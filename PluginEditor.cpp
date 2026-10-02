#include "PluginEditor.h"

//==============================================================================
// ResponseCurve
//==============================================================================
ResponseCurve::ResponseCurve (ParaEQAudioProcessor& p) : proc (p)
{
    refresh();
    startTimerHz (30);
}

ResponseCurve::~ResponseCurve() { stopTimer(); }

void ResponseCurve::timerCallback()
{
    const auto sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    bool changed = sr != sampleRate;

    for (int b = 0; b < numBands && ! changed; ++b)
    {
        const auto s = proc.getBandSettings (b);
        const auto& o = settings[(size_t) b];
        changed = s.on != o.on || s.freq != o.freq || s.gain != o.gain || s.q != o.q;
    }

    if (changed)
        refresh();
}

void ResponseCurve::refresh()
{
    sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;

    for (int b = 0; b < numBands; ++b)
    {
        settings[(size_t) b] = proc.getBandSettings (b);
        coeffs[(size_t) b] = makeBandCoefficients (bandInfos[(size_t) b].type, sampleRate, settings[(size_t) b]);
    }

    repaint();
}

void ResponseCurve::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (34).withTrimmedBottom (20).reduced (6);
}

float ResponseCurve::freqToX (float f) const
{
    const auto norm = std::log (f / 20.0f) / std::log (1000.0f);
    return plot.getX() + norm * plot.getWidth();
}

float ResponseCurve::xToFreq (float x) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return 20.0f * std::pow (1000.0f, norm);
}

float ResponseCurve::dbToY (float db) const
{
    return juce::jmap (db, minDb, maxDb, plot.getBottom(), plot.getY());
}

float ResponseCurve::yToDb (float y) const
{
    return juce::jlimit (minDb, maxDb, juce::jmap (y, plot.getBottom(), plot.getY(), minDb, maxDb));
}

juce::Point<float> ResponseCurve::nodePosition (int band) const
{
    const auto& s = settings[(size_t) band];
    const auto db = bandHasGain (bandInfos[(size_t) band].type) ? s.gain : 0.0f;
    return { freqToX (s.freq), dbToY (db) };
}

int ResponseCurve::findNodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 14.0f;

    for (int b = 0; b < numBands; ++b)
    {
        const auto d = nodePosition (b).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = b; }
    }

    return best;
}

juce::RangedAudioParameter* ResponseCurve::getParam (int band, const char* suffix) const
{
    return proc.apvts.getParameter (paramId (band, suffix));
}

void ResponseCurve::setParam (int band, const char* suffix, float value)
{
    if (auto* p = getParam (band, suffix))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void ResponseCurve::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // Grid + labels
    g.setFont (11.0f);
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto f : freqs)
    {
        const auto x = freqToX (f);
        g.setColour (Colours::grid);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (Colours::text.withAlpha (0.7f));
        const auto label = f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (x - 20, plot.getBottom() + 3, 40, 14),
                    juce::Justification::centred);
    }

    for (float db = minDb; db <= maxDb; db += 6.0f)
    {
        const auto y = dbToY (db);
        g.setColour (db == 0.0f ? Colours::grid.brighter (0.4f) : Colours::grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Colours::text.withAlpha (0.7f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (2, y - 7, 30, 14),
                    juce::Justification::centredRight);
    }

    // Per-band curves (faint) and combined curve
    juce::Path total;
    std::array<juce::Path, numBands> bandPaths;
    const auto zeroY = dbToY (0.0f);

    for (float x = plot.getX(); x <= plot.getRight(); x += 1.0f)
    {
        const auto f = (double) xToFreq (x);
        double mag = 1.0;

        for (int b = 0; b < numBands; ++b)
        {
            if (! settings[(size_t) b].on || coeffs[(size_t) b] == nullptr)
                continue;

            const auto m = coeffs[(size_t) b]->getMagnitudeForFrequency (f, sampleRate);
            mag *= m;

            const auto y = dbToY (juce::jlimit (minDb - 6.0f, maxDb + 6.0f,
                                                (float) juce::Decibels::gainToDecibels (m, -100.0)));
            if (bandPaths[(size_t) b].isEmpty()) bandPaths[(size_t) b].startNewSubPath (x, y);
            else                                  bandPaths[(size_t) b].lineTo (x, y);
        }

        const auto y = dbToY (juce::jlimit (minDb - 6.0f, maxDb + 6.0f,
                                            (float) juce::Decibels::gainToDecibels (mag, -100.0)));
        if (total.isEmpty()) total.startNewSubPath (x, y);
        else                 total.lineTo (x, y);
    }

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    for (int b = 0; b < numBands; ++b)
    {
        g.setColour (Colours::bands[(size_t) b].withAlpha (0.35f));
        g.strokePath (bandPaths[(size_t) b], juce::PathStrokeType (1.0f));
    }

    juce::Path fill (total);
    fill.lineTo (plot.getRight(), zeroY);
    fill.lineTo (plot.getX(), zeroY);
    fill.closeSubPath();
    g.setColour (Colours::curve.withAlpha (0.08f));
    g.fillPath (fill);

    g.setColour (Colours::curve);
    g.strokePath (total, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    g.restoreState();

    // Nodes
    for (int b = 0; b < numBands; ++b)
    {
        const auto pos = nodePosition (b);
        const auto c = Colours::bands[(size_t) b];
        const auto r = (b == dragBand || b == hoverBand) ? 9.0f : 7.0f;
        const auto circle = juce::Rectangle<float> (r * 2, r * 2).withCentre (pos);

        if (settings[(size_t) b].on)
        {
            g.setColour (c);
            g.fillEllipse (circle);
            g.setColour (Colours::background);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (juce::String (b + 1), circle, juce::Justification::centred);
        }
        else
        {
            g.setColour (c.withAlpha (0.5f));
            g.drawEllipse (circle, 1.5f);
        }
    }
}

void ResponseCurve::mouseMove (const juce::MouseEvent& e)
{
    const auto h = findNodeAt (e.position);
    if (h != hoverBand) { hoverBand = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void ResponseCurve::mouseDown (const juce::MouseEvent& e)
{
    dragBand = findNodeAt (e.position);
    if (dragBand < 0) return;

    if (auto* p = getParam (dragBand, "freq")) p->beginChangeGesture();
    if (auto* p = getParam (dragBand, "gain")) p->beginChangeGesture();
}

void ResponseCurve::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;

    setParam (dragBand, "freq", xToFreq (e.position.x));
    if (bandHasGain (bandInfos[(size_t) dragBand].type))
        setParam (dragBand, "gain", yToDb (e.position.y));

    refresh();
}

void ResponseCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragBand < 0) return;

    if (auto* p = getParam (dragBand, "freq")) p->endChangeGesture();
    if (auto* p = getParam (dragBand, "gain")) p->endChangeGesture();
    dragBand = -1;
    repaint();
}

void ResponseCurve::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto b = findNodeAt (e.position);
    if (b < 0) return;

    if (auto* p = getParam (b, "on"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
    refresh();
}

void ResponseCurve::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto b = dragBand >= 0 ? dragBand : findNodeAt (e.position);
    if (b < 0) return;

    const auto newQ = juce::jlimit (0.1f, 10.0f, settings[(size_t) b].q * std::pow (2.0f, w.deltaY * 2.0f));
    if (auto* p = getParam (b, "q"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (newQ));
        p->endChangeGesture();
    }
    refresh();
}

//==============================================================================
// Editor
//==============================================================================
ParaEQAudioProcessorEditor::ParaEQAudioProcessorEditor (ParaEQAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), curve (p)
{
    lnf.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, Colours::grid);
    lnf.setColour (juce::Label::textColourId, Colours::text);
    lnf.setColour (juce::ToggleButton::textColourId, Colours::text);
    lnf.setColour (juce::ToggleButton::tickColourId, Colours::curve);
    setLookAndFeel (&lnf);

    addAndMakeVisible (curve);

    title.setText ("ParaEQ", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::curve);
    addAndMakeVisible (title);

    for (int b = 0; b < numBands; ++b)
    {
        auto& s = strips[(size_t) b];
        const auto& info = bandInfos[(size_t) b];
        const auto c = Colours::bands[(size_t) b];

        s.name.setText (juce::String (b + 1) + "  " + info.name, juce::dontSendNotification);
        s.name.setJustificationType (juce::Justification::centred);
        s.name.setColour (juce::Label::textColourId, c);
        s.name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (s.name);

        s.on.setButtonText ("On");
        addAndMakeVisible (s.on);
        s.onAtt = std::make_unique<ButtonAttachment> (proc.apvts, paramId (b, "on"), s.on);

        setupKnob (s.freq, c);
        s.freqAtt = std::make_unique<SliderAttachment> (proc.apvts, paramId (b, "freq"), s.freq);

        setupKnob (s.q, c);
        s.qAtt = std::make_unique<SliderAttachment> (proc.apvts, paramId (b, "q"), s.q);

        if (bandHasGain (info.type))
        {
            setupKnob (s.gain, c);
            s.gainAtt = std::make_unique<SliderAttachment> (proc.apvts, paramId (b, "gain"), s.gain);
        }
    }

    outLabel.setText ("Output", juce::dontSendNotification);
    outLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outLabel);
    setupKnob (outKnob, Colours::curve);
    outAtt = std::make_unique<SliderAttachment> (proc.apvts, "out", outKnob);

    setResizable (true, true);
    setResizeLimits (760, 520, 1600, 1100);
    setSize (960, 640);
}

ParaEQAudioProcessorEditor::~ParaEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ParaEQAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Colour c)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, c);
    s.setColour (juce::Slider::thumbColourId, c);
    addAndMakeVisible (s);
}

void ParaEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    g.setColour (Colours::text.withAlpha (0.5f));
    g.setFont (12.0f);
    g.drawText ("Drag nodes to move  |  Scroll to change Q  |  Double-click to toggle",
                getLocalBounds().removeFromTop (40).reduced (14, 0), juce::Justification::centredRight);
}

void ParaEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    title.setBounds (area.removeFromTop (28).removeFromLeft (200));
    area.removeFromTop (4);

    const auto curveHeight = juce::roundToInt (area.getHeight() * 0.52f);
    curve.setBounds (area.removeFromTop (curveHeight));
    area.removeFromTop (10);

    auto outCol = area.removeFromRight (90);
    outLabel.setBounds (outCol.removeFromTop (22));
    outKnob.setBounds (outCol.removeFromTop (juce::jmin (100, outCol.getHeight())));

    const auto colWidth = area.getWidth() / numBands;
    for (int b = 0; b < numBands; ++b)
    {
        auto col = area.removeFromLeft (colWidth).reduced (4, 0);
        auto& s = strips[(size_t) b];

        s.name.setBounds (col.removeFromTop (20));
        s.on.setBounds (col.removeFromTop (22).withSizeKeepingCentre (56, 22));

        const auto knobH = col.getHeight() / 3;
        s.freq.setBounds (col.removeFromTop (knobH));
        auto gainArea = col.removeFromTop (knobH);
        if (s.gainAtt != nullptr) s.gain.setBounds (gainArea);
        s.q.setBounds (col);
    }
}
