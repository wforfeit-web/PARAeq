#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace Colours
{
    const juce::Colour background { 0xff15171c };
    const juce::Colour panel      { 0xff1d2027 };
    const juce::Colour grid       { 0xff2b2f38 };
    const juce::Colour text       { 0xffb8bfcc };
    const juce::Colour curve      { 0xfff2f4f8 };

    const std::array<juce::Colour, numBands> bands {{
        juce::Colour (0xffe06c75), juce::Colour (0xffe5c07b), juce::Colour (0xff98c379),
        juce::Colour (0xff56b6c2), juce::Colour (0xff61afef), juce::Colour (0xffc678dd) }};
}

//==============================================================================
/** Frequency response display with draggable band nodes.
    Drag a node: move frequency / gain.  Mouse wheel over a node: change Q.
    Double-click a node: turn the band on/off. */
class ResponseCurve : public juce::Component, private juce::Timer
{
public:
    explicit ResponseCurve (ParaEQAudioProcessor&);
    ~ResponseCurve() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refresh();

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    juce::Point<float> nodePosition (int band) const;
    int findNodeAt (juce::Point<float> p) const;
    void setParam (int band, const char* suffix, float value);
    juce::RangedAudioParameter* getParam (int band, const char* suffix) const;

    ParaEQAudioProcessor& proc;
    std::array<BandSettings, numBands> settings {};
    std::array<Coeffs::Ptr, numBands> coeffs {};
    double sampleRate = 48000.0;
    juce::Rectangle<float> plot;
    int dragBand = -1, hoverBand = -1;

    static constexpr float minDb = -24.0f, maxDb = 24.0f;
};

//==============================================================================
class ParaEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ParaEQAudioProcessorEditor (ParaEQAudioProcessor&);
    ~ParaEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct BandStrip
    {
        juce::Label name;
        juce::ToggleButton on;
        juce::Slider freq, gain, q;
        std::unique_ptr<ButtonAttachment> onAtt;
        std::unique_ptr<SliderAttachment> freqAtt, gainAtt, qAtt;
    };

    void setupKnob (juce::Slider&, juce::Colour);

    ParaEQAudioProcessor& proc;
    juce::LookAndFeel_V4 lnf;
    ResponseCurve curve;
    std::array<BandStrip, numBands> strips;

    juce::Label title, outLabel;
    juce::Slider outKnob;
    std::unique_ptr<SliderAttachment> outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParaEQAudioProcessorEditor)
};
