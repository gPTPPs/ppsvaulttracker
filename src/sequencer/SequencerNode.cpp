#include "sequencer/SequencerNode.h"

void SequencerNode::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    const int numSamples = audio.getNumSamples();
    const bool playNow = playRequested.load();

    auto killAllNotes = [this, &midi] (int offset)
    {
        for (int ch = 0; ch < Pattern::kMaxChannels; ++ch)
            if (activeNotes[ch] != 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, activeNotes[ch] - 1), offset);
                activeNotes[ch] = 0;
            }
    };

    if (playNow && ! wasPlaying)
    {
        clock.reset();       // always start from row 0
        for (auto& n : activeNotes)
            n = 0;
    }
    else if (! playNow && wasPlaying)
    {
        killAllNotes (0);
        uiRow.store (-1);
    }
    wasPlaying = playNow;

    if (! playNow || pattern == nullptr)
        return;

    clock.setTempo (bpmAtomic.load(), speedAtomic.load());

    const int numChannels = pattern->getNumChannels();

    clock.advance (numSamples, pattern->getNumRows(), [&] (int row, int offset)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const Cell c = pattern->at (row, ch);   // by value: one coherent read

            // tracker semantics: a new note (or an explicit "===") ends the
            // previous one on this track
            if ((c.hasNote() || c.isNoteOff()) && activeNotes[ch] != 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, activeNotes[ch] - 1), offset);
                activeNotes[ch] = 0;
            }

            if (c.hasNote())
            {
                const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) c.volume * 2);
                midi.addEvent (juce::MidiMessage::noteOn (1, (int) c.note, vel), offset);
                activeNotes[ch] = (int) c.note + 1;
            }
        }
        uiRow.store (row);
    });
}
