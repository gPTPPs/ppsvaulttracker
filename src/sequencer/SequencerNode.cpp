#include "sequencer/SequencerNode.h"

void SequencerNode::triggerClick (int offset, bool accent)
{
    if (numPendingClicks < 8)
        pendingClicks[numPendingClicks++] = { offset, accent };
}

void SequencerNode::renderClicks (juce::AudioBuffer<float>& audio)
{
    const int numSamples = audio.getNumSamples();
    const int numCh = juce::jmin (2, audio.getNumChannels());
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;

    int next = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        while (next < numPendingClicks && pendingClicks[next].offset <= i)
        {
            clickSamplesLeft = (int) (sr * 0.03);   // 30 ms tick
            clickPhase = 0.0;
            clickFreq = pendingClicks[next].accent ? 1567.0 : 1046.5;   // G6 / C6
            ++next;
        }
        if (clickSamplesLeft > 0)
        {
            const float env = (float) clickSamplesLeft / (float) (sr * 0.03);
            const float v = 0.25f * env * (float) std::sin (clickPhase * juce::MathConstants<double>::twoPi);
            clickPhase += clickFreq / sr;
            for (int ch = 0; ch < numCh; ++ch)
                audio.addSample (ch, i, v);
            --clickSamplesLeft;
        }
    }
    numPendingClicks = 0;
}

void SequencerNode::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();   // we own this bus: clicks only
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
        precountLeft = precountRowsCfg.load();
        precountCounter = 0.0;
        for (auto& n : activeNotes)
            n = 0;
    }
    else if (! playNow && wasPlaying)
    {
        killAllNotes (0);
        uiRow.store (-1);
        uiOrderPos.store (-1);
        precountLeft = 0;
    }
    wasPlaying = playNow;

    if (! playNow || song == nullptr)
        return;

    clock.setTempo (bpmAtomic.load(), speedAtomic.load());

    // ---- pre-count: clicks only, the pattern starts afterwards ----
    if (precountLeft > 0)
    {
        const double rowLen = clock.samplesPerRow();
        for (int i = 0; i < numSamples && precountLeft > 0; ++i)
        {
            if (precountCounter <= 0.0)
            {
                const int rowsDone = precountRowsCfg.load() - precountLeft;
                if (rowsDone % 4 == 0)                       // click every beat
                    triggerClick (i, rowsDone == 0);
                precountCounter += rowLen;
                --precountLeft;

                if (precountLeft == 0)
                    clock.reset();   // pattern starts clean right after
            }
            precountCounter -= 1.0;
        }
        renderClicks (audio);
        uiRow.store (-1);
        if (precountLeft > 0)
            return;
    }

    if (firstRowPending)
        curPattern = songMode.load() ? clampPatternIndex (song->order[0])
                                     : clampPatternIndex (editPatternIdx.load());

    Pattern* pat = song->getPattern (curPattern);
    if (pat == nullptr)
        return;

    const bool metro = metroOn.load();

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

        if (metro && row % 4 == 0)
            triggerClick (offset, row % 16 == 0);

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

    rowPhase.store ((float) clock.phaseInRow());
    renderClicks (audio);
}
