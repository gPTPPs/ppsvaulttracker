#include "sequencer/SequencerNode.h"
#include "model/EffectCommands.h"

// deliver deferred note events that fall inside this block, age the rest
void SequencerNode::flushPending (juce::MidiBuffer& midi, int numSamples)
{
    int keep = 0;
    for (int i = 0; i < numPendingMidi; ++i)
    {
        auto e = pendingMidi[i];
        if (e.samplesUntil < numSamples)
        {
            midi.addEvent (e.noteOn ? juce::MidiMessage::noteOn ((int) e.midiCh, (int) e.note, e.vel)
                                    : juce::MidiMessage::noteOff ((int) e.midiCh, (int) e.note),
                           e.samplesUntil);
        }
        else
        {
            e.samplesUntil -= numSamples;
            pendingMidi[keep++] = e;
        }
    }
    numPendingMidi = keep;
}

void SequencerNode::cancelPending (juce::MidiBuffer& midi)
{
    for (int i = 0; i < numPendingMidi; ++i)
        if (! pendingMidi[i].noteOn)   // pending cuts must still silence their note
            midi.addEvent (juce::MidiMessage::noteOff ((int) pendingMidi[i].midiCh,
                                                       (int) pendingMidi[i].note), 0);
    numPendingMidi = 0;
}

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
        curOrderPos = song != nullptr
                          ? juce::jlimit (0, juce::jmax (0, song->orderLen - 1), startOrderPos.load())
                          : 0;
        precountLeft = precountRowsCfg.load();
        precountCounter = 0.0;
        numPendingMidi = 0;
        for (auto& n : activeNotes)
            n = 0;
    }
    else if (! playNow && wasPlaying)
    {
        cancelPending (midi);
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
        curPattern = songMode.load() ? clampPatternIndex (song->order[curOrderPos])
                                     : clampPatternIndex (editPatternIdx.load());

    Pattern* pat = song->getPattern (curPattern);
    if (pat == nullptr)
        return;

    const bool metro = metroOn.load();

    flushPending (midi, numSamples);

    // note events land in this block or join the pending queue (Nxx / Kxx)
    auto emitOrDefer = [&] (bool on, int midiCh, int note, juce::uint8 vel, int at)
    {
        if (at < numSamples)
            midi.addEvent (on ? juce::MidiMessage::noteOn (midiCh, note, vel)
                              : juce::MidiMessage::noteOff (midiCh, note), at);
        else if (numPendingMidi < kMaxPendingMidi)
            pendingMidi[numPendingMidi++] = { at - numSamples, on,
                                              (uint8_t) midiCh, (uint8_t) note, vel };
    };

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

                // entering a block: tracks muted here get their hanging note
                // cut, so a sustained pad doesn't ring through its muted block
                for (int ch = 0; ch < Pattern::kMaxChannels; ++ch)
                    if (activeNotes[ch] != 0 && song->orderMuted (curOrderPos, ch))
                    {
                        midi.addEvent (juce::MidiMessage::noteOff (juce::jmin (16, ch + 1),
                                                                   activeNotes[ch] - 1), offset);
                        activeNotes[ch] = 0;
                    }
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
        const int speedNow = juce::jmax (1, speedAtomic.load());
        const bool inSongMode = songMode.load();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // arrangement matrix: a muted track emits nothing in this block
            if (inSongMode && song->orderMuted (curOrderPos, ch))
                continue;

            const Cell c = pat->at (row, ch);   // by value: one coherent read
            const int midiCh = ch + 1;
            const uint8_t fx = FxCmd::sanitize (c.effect);
            const int fxVal = juce::jmin ((int) c.effectValue, 127);

            // CC and pitch bend fire at the row start, even without a note
            if (FxCmd::isSlot (fx))
                midi.addEvent (juce::MidiMessage::controllerEvent (midiCh,
                                   (int) song->ccForSlot (ch, FxCmd::slotIndex (fx)), fxVal),
                               offset);
            else if (fx == FxCmd::kPitchBend)
                midi.addEvent (juce::MidiMessage::pitchWheel (midiCh, fxVal << 7), offset);

            // Nxx delays the whole cell: previous-note kill AND trigger move together
            int cellAt = offset;
            if (fx == FxCmd::kNoteDelay && (c.hasNote() || c.isNoteOff()))
                cellAt = offset + (int) ((double) juce::jmin (fxVal, speedNow - 1)
                                         * clock.samplesPerTick());

            if ((c.hasNote() || c.isNoteOff()) && activeNotes[ch] != 0)
            {
                emitOrDefer (false, midiCh, activeNotes[ch] - 1, 0, cellAt);
                activeNotes[ch] = 0;
            }

            if (c.hasNote())
            {
                const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) c.volume * 2);
                emitOrDefer (true, midiCh, (int) c.note, vel, cellAt);
                activeNotes[ch] = (int) c.note + 1;
            }

            // Kxx: whatever plays on this channel (incl. a note from this row) stops at tick x
            if (fx == FxCmd::kNoteCut && activeNotes[ch] != 0)
            {
                const int cutAt = offset + (int) ((double) juce::jmin (fxVal, speedNow - 1)
                                                  * clock.samplesPerTick());
                emitOrDefer (false, midiCh, activeNotes[ch] - 1, 0, cutAt);
                activeNotes[ch] = 0;
            }
        }

        uiRow.store (row);
        uiPatternIdx.store (curPattern);
        uiOrderPos.store (songMode.load() ? curOrderPos : -1);
    });

    rowPhase.store ((float) clock.phaseInRow());
    renderClicks (audio);
}
