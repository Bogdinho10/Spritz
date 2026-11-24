#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    const double rateDens[] = { 4, 8, 16, 32, 64 };   // trigger rate
    const double panDens[]  = { 1, 2, 4, 8, 16 };     // auto pan rate
    const double ricoDens[] = { 32, 16, 8, 4 };       // ricochet time

    float pulseShape (double p)
    {
        const double attack = 0.02, length = 0.65;
        if (p < attack) return (float) (p / attack);
        const double x = (p - attack) / (length - attack);
        if (x >= 1.0) return 0.0f;
        const double k = 1.0 - x;
        return (float) (k * k);
    }
}

SpritzProcessor::SpritzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SpritzProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "trigger", 1 }, "Trigger", false));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "sat",     1 }, "Saturation", true));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "pan",     1 }, "Auto Pan", false));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "rico",    1 }, "Ricochet", false));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "pitch",   1 }, "Pitch Drop", false));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos",   1 }, "Chaos",
                                                  NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix",     1 }, "Mix",
                                                  NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "pandepth",1 }, "Pan Depth",
                                                  NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate",    1 }, "Trigger Rate",
                                                   StringArray { "1/4", "1/8", "1/16", "1/32", "1/64" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "panrate", 1 }, "Pan Rate",
                                                   StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "ricotime",1 }, "Ricochet Time",
                                                   StringArray { "1/32", "1/16", "1/8", "1/4" }, 2));
    return l;
}

bool SpritzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SpritzProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    pitchN = (int) (sr * 2.0) + 8;
    delayN = (int) (sr * 2.6) + 8;
    for (auto& b : pitchBuf) b.assign ((size_t) pitchN, 0.0f);
    for (auto& b : dbuf)     b.assign ((size_t) delayN, 0.0f);

    pitchWp = delayWp = 0;
    readPos = 0.0;
    phase = 0.0;
    lfoPhase = 0.0;
    envS = 0.0f;
    lpL = lpR = 0.0f;
    wasTrig = false;

    envCoef = 1.0f - std::exp (-1.0f / (0.0008f * (float) sr));
    mixS.reset (sr, 0.02);
    mixS.setCurrentAndTargetValue (*apvts.getRawParameterValue ("mix"));
}

void SpritzProcessor::startPulse (float chaos)
{
    pulseVel      = 1.0f - chaos * 0.5f * rng.nextFloat();
    pulseDriveMul = 1.0f - chaos * 0.4f * rng.nextFloat();
    pulsePanSign  = (rng.nextFloat() < chaos * 0.5f) ? -1.0f : 1.0f;
    pulseDrop     = juce::jlimit (0.2f, 0.9f, 0.5f + chaos * (rng.nextFloat() - 0.5f));
}

void SpritzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh < 1 || pitchN == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // parameters
    const bool trig    = apvts.getRawParameterValue ("trigger")->load() > 0.5f;
    const bool satOn   = apvts.getRawParameterValue ("sat")->load()   > 0.5f;
    const bool panOn   = apvts.getRawParameterValue ("pan")->load()   > 0.5f;
    const bool ricoOn  = apvts.getRawParameterValue ("rico")->load()  > 0.5f;
    const bool pitchOn = apvts.getRawParameterValue ("pitch")->load() > 0.5f;
    const float chaos    = apvts.getRawParameterValue ("chaos")->load();
    const float panDepth = apvts.getRawParameterValue ("pandepth")->load();
    const int rateIdx  = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("rate")->load());
    const int panIdx   = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("panrate")->load());
    const int ricoIdx  = juce::jlimit (0, 3, (int) apvts.getRawParameterValue ("ricotime")->load());
    mixS.setTargetValue (apvts.getRawParameterValue ("mix")->load());

    // tempo
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    bpm = juce::jlimit (30.0, 300.0, bpm);

    const double quartersPerSec = bpm / 60.0;
    const double phaseInc = quartersPerSec / (4.0 / rateDens[rateIdx]) / sr;
    const double lfoInc   = quartersPerSec / (4.0 / panDens[panIdx]) / sr;
    const int delaySamples = juce::jlimit (1, delayN - 1,
        (int) std::lround ((4.0 / ricoDens[ricoIdx]) * (60.0 / bpm) * sr));
    const float fb = juce::jmin (0.85f, 0.45f + 0.4f * chaos);
    const float lpCoef = 0.35f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float quarterPi = juce::MathConstants<float>::pi * 0.25f;

    // rising edge: the first shot fires right away
    if (trig && ! wasTrig)
    {
        phase = 0.0;
        lfoPhase = 0.0;
        startPulse (chaos);
        readPos = (double) ((pitchWp - 1 + pitchN) % pitchN);
    }
    wasTrig = trig;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = l[i];
        const float inR = r != nullptr ? r[i] : inL;

        // keep the pitch buffer filled
        pitchBuf[0][(size_t) pitchWp] = inL;
        pitchBuf[1][(size_t) pitchWp] = inR;

        // burst clock + envelope
        float envTarget = 0.0f;
        if (trig)
        {
            phase += phaseInc;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                startPulse (chaos);
                readPos = (double) pitchWp;
            }
            envTarget = pulseShape (phase) * pulseVel;
        }
        envS += (envTarget - envS) * envCoef;

        float yL = inL, yR = inR;

        // pitch drop: playback speed falls during each shot
        if (pitchOn && trig)
        {
            const int i0 = (int) readPos;
            const float fr = (float) (readPos - (double) i0);
            const int i1 = (i0 + 1) % pitchN;
            yL = pitchBuf[0][(size_t) i0] * (1.0f - fr) + pitchBuf[0][(size_t) i1] * fr;
            yR = pitchBuf[1][(size_t) i0] * (1.0f - fr) + pitchBuf[1][(size_t) i1] * fr;

            const double rate = 1.0 - (double) pulseDrop * phase;
            readPos += rate;
            if (readPos >= (double) pitchN) readPos -= (double) pitchN;
        }

        // saturation
        if (satOn)
        {
            const float d = 1.0f + (3.0f + 27.0f * chaos) * pulseDriveMul;
            const float norm = 1.0f / std::tanh (d);
            yL = std::tanh (yL * d) * norm;
            yR = std::tanh (yR * d) * norm;
        }

        // auto pan (equal power)
        if (panOn)
        {
            const float p = std::sin (twoPi * (float) lfoPhase) * panDepth * pulsePanSign;
            const float ang = (p + 1.0f) * quarterPi;
            yL *= std::cos (ang) * 1.41421356f;
            yR *= std::sin (ang) * 1.41421356f;
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        }

        const float wL = yL * envS;
        const float wR = yR * envS;

        // ricochet: ping-pong echoes of the shots
        const int rp = ((delayWp - delaySamples) % delayN + delayN) % delayN;
        const float dl = dbuf[0][(size_t) rp];
        const float dr = dbuf[1][(size_t) rp];
        lpL += lpCoef * (dl - lpL);
        lpR += lpCoef * (dr - lpR);
        const float mono = ricoOn ? (wL + wR) * 0.5f : 0.0f;
        dbuf[0][(size_t) delayWp] = mono + fb * lpR;
        dbuf[1][(size_t) delayWp] = fb * lpL;
        delayWp = (delayWp + 1) % delayN;

        // final mix
        const float m = mixS.getNextValue();
        const float outL = inL * (1.0f - m * envS) + (wL + dl) * m;
        const float outR = inR * (1.0f - m * envS) + (wR + dr) * m;

        l[i] = outL;
        if (r != nullptr) r[i] = outR;

        pitchWp = (pitchWp + 1) % pitchN;
    }

    fireLevel.store (envS);
}

juce::AudioProcessorEditor* SpritzProcessor::createEditor()
{
    return new SpritzEditor (*this);
}

void SpritzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SpritzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpritzProcessor();
}
