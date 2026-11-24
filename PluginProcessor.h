#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

class SpritzProcessor : public juce::AudioProcessor
{
public:
    SpritzProcessor();
    ~SpritzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Spritz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> fireLevel { 0.0f }; // read by the editor for the flash

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void startPulse (float chaos);

    double sr = 44100.0;

    // burst clock
    double phase = 0.0;
    bool wasTrig = false;
    float envS = 0.0f;
    float envCoef = 0.01f;
    float pulseVel = 1.0f, pulseDriveMul = 1.0f, pulsePanSign = 1.0f, pulseDrop = 0.5f;
    juce::Random rng;

    // pitch drop buffer
    std::array<std::vector<float>, 2> pitchBuf;
    int pitchN = 0, pitchWp = 0;
    double readPos = 0.0;

    // auto pan
    double lfoPhase = 0.0;

    // ricochet delay
    std::array<std::vector<float>, 2> dbuf;
    int delayN = 0, delayWp = 0;
    float lpL = 0.0f, lpR = 0.0f;

    juce::SmoothedValue<float> mixS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpritzProcessor)
};
