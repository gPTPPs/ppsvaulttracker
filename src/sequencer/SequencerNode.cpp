#include "sequencer/SequencerNode.h"

void SequencerNode::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    const int numSamples = audio.getNumSamples();
    const bool playNow = playRequested.load();

    if (playNow && ! wasPlaying)
    {
        clock.reset();       // always start from row 0
        activeNote = -1;
    }
    else if (! playNow && wasPlaying)
    {
        if (activeNote >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, activeNote), 0);
            activeNote = -1;
        }
        uiRow.store (-1);
    }
    wasPlaying = playNow;

    if (! playNow || pattern == nullptr)
        return;

    clock.setTempo (bpmAtomic.load(), speedAtomic.load());

    clock.advance (numSamples, pattern->getNumRows(), [&] (int row, int offset)
    {
        const Cell& c = pattern->at (row, 0);

        // tracker semantics: a new note (or an explicit "===") ends the
        // previous one on this track
        if ((c.hasNote() || c.isNoteOff()) && activeNote >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, activeNote), offset);
            activeNote = -1;
        }

        if (c.hasNote())
        {
            const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) c.volume * 2);
            midi.addEvent (juce::MidiMessage::noteOn (1, (int) c.note, vel), offset);
            activeNote = c.note;
        }

        uiRow.store (row);
    });
}
