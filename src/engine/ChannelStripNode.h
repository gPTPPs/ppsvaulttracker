#pragma once
#include <atomic>
#include "engine/InternalNodeBase.h"

// Mixer strip: smoothed gain + effective mute (solo logic resolved by the
// engine on the message thread) + peak metering for the VU display.
// Everything crosses the thread boundary as atomics.
class ChannelStripNode : public InternalNodeBase
{
public:
    ChannelStripNode()
        : InternalNodeBase (BusesProperties()
                                .withInput  ("In",  juce::AudioChannelSet::stereo())
                                .withOutput ("Out", juce::AudioChannelSet::stereo())) {}

    const juce::String getName() const override { return "Strip"; }
    bool acceptsMidi() const override           { return false; }
    bool producesMidi() const override          { return false; }

    void setGain (float g)              { gain.store (g); }
    float getGain() const               { return gain.load(); }
    void setEffectiveMute (bool m)      { effectiveMute.store (m); }
    float readAndResetPeak()            { return peak.exchange (0.0f); }

    void prepareToPlay (double, int) override
    {
        smoothedGain = effectiveMute.load() ? 0.0f : gain.load();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const float target = effectiveMute.load() ? 0.0f : gain.load();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        const int n = buffer.getNumSamples();

        float g = smoothedGain;
        float blockPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            g += (target - g) * 0.004f;   // ~10 ms ramp, click-free
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                data[i] *= g;
                const float a = std::abs (data[i]);
                if (a > blockPeak)
                    blockPeak = a;
            }
        }
        smoothedGain = g;

        if (blockPeak > peak.load())
            peak.store (blockPeak);
    }

private:
    std::atomic<float> gain { 1.0f };
    std::atomic<bool>  effectiveMute { false };
    std::atomic<float> peak { 0.0f };
    float smoothedGain = 1.0f;   // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripNode)
};
