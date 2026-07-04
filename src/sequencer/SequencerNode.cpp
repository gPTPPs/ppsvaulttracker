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
                midi.addEvent (juce::MidiMessage::noteOff (juce::jmin (16, ch + 1), activeNotes[ch] - 1), offset);
                activeNotes[ch] = 0;
            }
    };

    if (playNow && ! wasPlaying)
    {
        clock.reset();
        firstRowPending = true;
        curOrderPos = 0;
        for (auto& n : activeNotes)
            n = 0;
    }
    else if (! playNow && wasPlaying)
    {
        killAllNotes (0);
        uiRow.store (-1);
        uiOrderPos.store (-1);
    }
    wasPlaying = playNow;

    if (! playNow || song == nullptr)
        return;

    if (firstRowPending)
        curPattern = songMode.load() ? clampPatternIndex (song->order[0])
                                     : clampPatternIndex (editPatternIdx.load());

    Pattern* pat = song->getPattern (curPattern);
    if (pat == nullptr)
        return;

    clock.setTempo (bpmAtomic.load(), speedAtomic.load());

    clock.advance (numSamples, pat->getNumRows(), [&] (int row, int offset)
    {
        // pattern boundary: advance the order list (song mode) or re-read the
        // edited pattern (pattern mode). NB: if the next pattern has a
        // different length, the wrap that lands exactly on this block used the
        // previous length — a one-block approximation, inaudible in practice.
        if (row == 0 && ! firstRowPending)
        {
            if (songMode.load())
            {
                curOrderPos = (curOrderPos + 1) % juce::jmax (1, song->orderLen);
                curPattern = clampPatternIndex (song->order[curOrderPos]);
            }
            else
            {
                curPattern = clampPatternIndex (editPatternIdx.load());
            }
            pat = song->getPattern (curPattern);
        }
        firstRowPending = false;

        const int numChannels = juce::jmin (pat->getNumChannels(), 16);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const Cell c = pat->at (row, ch);   // by value: one coherent read
            const int midiCh = ch + 1;

            if ((c.hasNote() || c.isNoteOff()) && activeNotes[ch] != 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (midiCh, activeNotes[ch] - 1), offset);
                activeNotes[ch] = 0;
            }

            if (c.hasNote())
            {
                const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) c.volume * 2);
                midi.addEvent (juce::MidiMessage::noteOn (midiCh, (int) c.note, vel), offset);
                activeNotes[ch] = (int) c.note + 1;
            }
        }

        uiRow.store (row);
        uiPatternIdx.store (curPattern);
        uiOrderPos.store (songMode.load() ? curOrderPos : -1);
    });
}
