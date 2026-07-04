#pragma once
#include "engine/InternalNodeBase.h"

// Keeps only the MIDI messages of one tracker channel (MIDI channel index+1)
// and rewrites them to channel 1 for the instrument behind it. This is how
// one sequencer stream fans out to per-channel instruments.
class MidiRouterNode : public InternalNodeBase
{
public:
    explicit MidiRouterNode (int channelIndex)
        : InternalNodeBase (BusesProperties()), midiChannel (channelIndex + 1) {}

    const juce::String getName() const override { return "Router " + juce::String (midiChannel); }
    bool acceptsMidi() const override           { return true; }
    bool producesMidi() const override          { return true; }
    void prepareToPlay (double, int) override   {}

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi) override
    {
        filtered.clear();
        for (const auto meta : midi)
        {
            auto m = meta.getMessage();
            if (m.getChannel() == midiChannel)
            {
                m.setChannel (1);
                filtered.addEvent (m, meta.samplePosition);
            }
        }
        midi.swapWith (filtered);
    }

private:
    int midiChannel;
    juce::MidiBuffer filtered;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRouterNode)
};
