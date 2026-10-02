#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

enum class BandType { LowCut, LowShelf, Peak, HighShelf, HighCut };

struct BandInfo
{
    const char* id;
    const char* name;
    BandType type;
    float defaultFreq;
    float defaultQ;
    bool defaultOn;
};

constexpr int numBands = 6;

constexpr std::array<BandInfo, numBands> bandInfos {{
    { "lc", "Low Cut",    BandType::LowCut,    30.0f,    0.707f, false },
    { "ls", "Low Shelf",  BandType::LowShelf,  100.0f,   0.707f, true  },
    { "p1", "Peak 1",     BandType::Peak,      400.0f,   1.0f,   true  },
    { "p2", "Peak 2",     BandType::Peak,      2000.0f,  1.0f,   true  },
    { "hs", "High Shelf", BandType::HighShelf, 8000.0f,  0.707f, true  },
    { "hc", "High Cut",   BandType::HighCut,   18000.0f, 0.707f, false },
}};

inline bool bandHasGain (BandType t) { return t != BandType::LowCut && t != BandType::HighCut; }

inline juce::String paramId (int band, const char* suffix)
{
    return juce::String (bandInfos[(size_t) band].id) + "_" + suffix;
}

struct BandSettings
{
    bool on = false;
    float freq = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
};

using Coeffs = juce::dsp::IIR::Coefficients<float>;
Coeffs::Ptr makeBandCoefficients (BandType type, double sampleRate, const BandSettings& s);

//==============================================================================
class ParaEQAudioProcessor : public juce::AudioProcessor
{
public:
    ParaEQAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    BandSettings getBandSettings (int band) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateFilters (bool force);

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, Coeffs>;
    std::array<Filter, numBands> filters;
    std::array<BandSettings, numBands> lastSettings {};
    juce::dsp::Gain<float> outputGain;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParaEQAudioProcessor)
};
