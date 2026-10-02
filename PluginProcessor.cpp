#include "PluginProcessor.h"
#include "PluginEditor.h"

Coeffs::Ptr makeBandCoefficients (BandType type, double sampleRate, const BandSettings& s)
{
    const auto freq = juce::jlimit (10.0f, (float) (sampleRate * 0.49), s.freq);
    const auto q    = juce::jmax (0.05f, s.q);
    const auto gain = juce::Decibels::decibelsToGain (s.gain);

    switch (type)
    {
        case BandType::LowCut:    return Coeffs::makeHighPass  (sampleRate, freq, q);
        case BandType::HighCut:   return Coeffs::makeLowPass   (sampleRate, freq, q);
        case BandType::LowShelf:  return Coeffs::makeLowShelf  (sampleRate, freq, q, gain);
        case BandType::HighShelf: return Coeffs::makeHighShelf (sampleRate, freq, q, gain);
        case BandType::Peak:
        default:                  return Coeffs::makePeakFilter (sampleRate, freq, q, gain);
    }
}

//==============================================================================
ParaEQAudioProcessor::ParaEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ParaEQAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto freqText = [] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    };
    auto dbText = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto qText  = [] (float v, int) { return "Q " + juce::String (v, 2); };

    for (int b = 0; b < numBands; ++b)
    {
        const auto& info = bandInfos[(size_t) b];
        const juce::String name (info.name);

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { paramId (b, "on"), 1 }, name + " On", info.defaultOn));

        juce::NormalisableRange<float> freqRange (20.0f, 20000.0f);
        freqRange.setSkewForCentre (632.0f); // geometric centre of 20..20k = log-like feel
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId (b, "freq"), 1 }, name + " Freq", freqRange, info.defaultFreq,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (freqText)));

        if (bandHasGain (info.type))
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { paramId (b, "gain"), 1 }, name + " Gain",
                juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        }

        juce::NormalisableRange<float> qRange (0.1f, 10.0f);
        qRange.setSkewForCentre (1.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId (b, "q"), 1 }, name + " Q", qRange, info.defaultQ,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (qText)));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "out", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    return layout;
}

BandSettings ParaEQAudioProcessor::getBandSettings (int band) const
{
    BandSettings s;
    s.on   = apvts.getRawParameterValue (paramId (band, "on"))->load() > 0.5f;
    s.freq = apvts.getRawParameterValue (paramId (band, "freq"))->load();
    s.q    = apvts.getRawParameterValue (paramId (band, "q"))->load();

    if (auto* g = apvts.getRawParameterValue (paramId (band, "gain")))
        s.gain = g->load();

    return s;
}

void ParaEQAudioProcessor::updateFilters (bool force)
{
    for (int b = 0; b < numBands; ++b)
    {
        const auto s = getBandSettings (b);
        auto& last = lastSettings[(size_t) b];

        // Only rebuild coefficients when something changed (avoids work on the audio thread)
        if (force || s.freq != last.freq || s.gain != last.gain || s.q != last.q)
            *filters[(size_t) b].state = *makeBandCoefficients (bandInfos[(size_t) b].type, currentSampleRate, s);

        if (s.on && ! last.on)
            filters[(size_t) b].reset(); // clear stale state when a band is switched back on

        last = s;
    }
}

void ParaEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };

    updateFilters (true);

    for (auto& f : filters)
    {
        f.prepare (spec);
        f.reset();
    }

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);
}

bool ParaEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void ParaEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    updateFilters (false);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    for (int b = 0; b < numBands; ++b)
        if (lastSettings[(size_t) b].on)
            filters[(size_t) b].process (context);

    outputGain.setGainDecibels (apvts.getRawParameterValue ("out")->load());
    outputGain.process (context);
}

juce::AudioProcessorEditor* ParaEQAudioProcessor::createEditor()
{
    return new ParaEQAudioProcessorEditor (*this);
}

void ParaEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ParaEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParaEQAudioProcessor();
}
