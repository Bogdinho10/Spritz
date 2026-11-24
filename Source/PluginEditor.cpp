#include "PluginEditor.h"

namespace
{
    const juce::Colour bgCol    (0xff15151a);
    const juce::Colour yellow   (0xffffc800);
    const juce::Colour redCol   (0xffe0202a);
    const juce::Colour panelCol (0xff24242c);
}

SpritzEditor::SpritzEditor (SpritzProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (560, 440);
    auto& apvts = proc.apvts;

    for (auto* b : { &satBtn, &panBtn, &ricoBtn, &pitchBtn })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId, panelCol);
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff7a00));
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (*b);
    }
    satAtt   = std::make_unique<ButtonAtt> (apvts, "sat",   satBtn);
    panAtt   = std::make_unique<ButtonAtt> (apvts, "pan",   panBtn);
    ricoAtt  = std::make_unique<ButtonAtt> (apvts, "rico",  ricoBtn);
    pitchAtt = std::make_unique<ButtonAtt> (apvts, "pitch", pitchBtn);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, yellow);
        s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, yellow);
        l.attachToComponent (&s, false);
        addAndMakeVisible (l);
    };
    setupKnob (chaosKnob, chaosLbl, "CHAOS");
    setupKnob (mixKnob,   mixLbl,   "MIX");
    setupKnob (depthKnob, depthLbl, "PAN DEPTH");
    chaosAtt = std::make_unique<SliderAtt> (apvts, "chaos",    chaosKnob);
    mixAtt   = std::make_unique<SliderAtt> (apvts, "mix",      mixKnob);
    depthAtt = std::make_unique<SliderAtt> (apvts, "pandepth", depthKnob);

    auto setupBox = [this, &apvts] (juce::ComboBox& box, juce::Label& l, const juce::String& name,
                                    const char* paramId)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            box.addItemList (cp->choices, 1);
        addAndMakeVisible (box);
        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, juce::Colours::white);
        l.attachToComponent (&box, true);
        addAndMakeVisible (l);
    };
    setupBox (rateBox,    rateLbl,    "Trigger rate", "rate");
    setupBox (panRateBox, panRateLbl, "Pan rate",     "panrate");
    setupBox (ricoBox,    ricoLbl,    "Ricochet time","ricotime");
    rateAtt     = std::make_unique<ComboAtt> (apvts, "rate",     rateBox);
    panRateAtt  = std::make_unique<ComboAtt> (apvts, "panrate",  panRateBox);
    ricoTimeAtt = std::make_unique<ComboAtt> (apvts, "ricotime", ricoBox);

    // hold-to-fire trigger (also automatable from the host)
    triggerAtt = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("trigger"),
                                                              [] (float) {});
    triggerBtn.setButtonText ("TRIGGER (hold)");
    triggerBtn.setColour (juce::TextButton::buttonColourId, redCol);
    triggerBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    triggerBtn.onHold = [this] (bool down)
    {
        if (down) { triggerAtt->beginGesture(); triggerAtt->setValueAsPartOfGesture (1.0f); }
        else      { triggerAtt->setValueAsPartOfGesture (0.0f); triggerAtt->endGesture(); }
    };
    addAndMakeVisible (triggerBtn);

    startTimerHz (30);
}

SpritzEditor::~SpritzEditor()
{
    stopTimer();
}

void SpritzEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgCol);

    // header
    g.setColour (yellow);
    g.fillRect (0, 0, getWidth(), 48);
    g.setColour (juce::Colours::black);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("SPRITZ", 16, 0, 200, 48, juce::Justification::centredLeft);

    // flash area
    auto area = juce::Rectangle<int> (20, 60, getWidth() - 40, 90);
    g.setColour (panelCol);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);

    const float lvl = juce::jlimit (0.0f, 1.0f, proc.fireLevel.load());
    g.setColour (yellow.withAlpha (0.15f + 0.85f * lvl));
    const float w = (float) (area.getWidth() - 20) * lvl;
    g.fillRoundedRectangle ((float) area.getX() + 10.0f, (float) area.getCentreY() - 12.0f, w, 24.0f, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (13.0f);
    g.drawText (lvl > 0.02f ? "FIRING" : "SAFE", area.reduced (12, 6), juce::Justification::topLeft);
}

void SpritzEditor::resized()
{
    auto bounds = getLocalBounds();

    // modes row
    const int bw = (getWidth() - 40 - 3 * 8) / 4;
    int x = 20;
    for (auto* b : { &satBtn, &panBtn, &ricoBtn, &pitchBtn })
    {
        b->setBounds (x, 165, bw, 34);
        x += bw + 8;
    }

    // knobs
    const int ky = 240, ks = 100;
    chaosKnob.setBounds (30,  ky, ks, ks + 20);
    mixKnob.setBounds   (150, ky, ks, ks + 20);
    depthKnob.setBounds (270, ky, ks, ks + 20);

    // combo boxes (labels sit on their left)
    const int bx = bounds.getRight() - 20 - 90;
    rateBox.setBounds    (bx, 250, 90, 26);
    panRateBox.setBounds (bx, 290, 90, 26);
    ricoBox.setBounds    (bx, 330, 90, 26);

    triggerBtn.setBounds (20, 380, getWidth() - 40, 44);
}
