#pragma once
#include "PluginProcessor.h"
#include <memory>

class SpritzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpritzEditor (SpritzProcessor&);
    ~SpritzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint (20, 60, getWidth() - 40, 90); }

    struct HoldButton : juce::TextButton
    {
        std::function<void (bool)> onHold;
        void buttonStateChanged() override
        {
            const bool d = isDown();
            if (d != wasDown) { wasDown = d; if (onHold) onHold (d); }
        }
        bool wasDown = false;
    };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    SpritzProcessor& proc;

    juce::TextButton satBtn { "SATURATION" }, panBtn { "AUTO PAN" },
                     ricoBtn { "RICOCHET" }, pitchBtn { "PITCH DROP" };
    HoldButton triggerBtn;

    juce::Slider chaosKnob, mixKnob, depthKnob;
    juce::ComboBox rateBox, panRateBox, ricoBox;
    juce::Label chaosLbl, mixLbl, depthLbl, rateLbl, panRateLbl, ricoLbl;

    std::unique_ptr<SliderAtt> chaosAtt, mixAtt, depthAtt;
    std::unique_ptr<ButtonAtt> satAtt, panAtt, ricoAtt, pitchAtt;
    std::unique_ptr<ComboAtt> rateAtt, panRateAtt, ricoTimeAtt;
    std::unique_ptr<juce::ParameterAttachment> triggerAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpritzEditor)
};
