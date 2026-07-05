#include "io/MidiExport.h"
#include "sequencer/CcInterp.h"

juce::MidiFile MidiExport::songToMidi (const Song& song, double bpm, int speed,
                                       const juce::StringArray& trackNames)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kPpq);

    // track 0: tempo map (single tempo in v1)
    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
    tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    mf.addTrack (tempoTrack);

    const int rt = rowTicks (speed);
    const int numChannels = song.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::MidiMessageSequence seq;
        const auto name = ch < trackNames.size() && trackNames[ch].isNotEmpty()
                              ? trackNames[ch]
                              : "Track " + juce::String (ch + 1);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3 /* track name */, name), 0);

        int activeNote = -1;
        juce::int64 rowBase = 0;
        int lastSmooth[FxCmd::kNumSlots + 1];   // dedup per smooth command, whole timeline
        for (int& v : lastSmooth) v = -1;

        // linearise the order list into one timeline
        for (int oi = 0; oi < song.orderLen; ++oi)
        {
            const auto* p = song.getPattern (song.order[oi]);
            if (p == nullptr)
                continue;

            // arrangement matrix: a muted block is silent for this track and
            // cuts whatever was ringing, mirroring playback
            if (song.orderMuted (oi, ch))
            {
                if (activeNote >= 0)
                {
                    seq.addEvent (juce::MidiMessage::noteOff (1, activeNote), (double) (rowBase * rt));
                    activeNote = -1;
                }
                rowBase += p->getNumRows();
                continue;
            }

            // smooth lanes: tick-resolution ramps, mirroring emitSmoothRamps
            for (int slot = 0; slot <= FxCmd::kNumSlots; ++slot)
            {
                const uint8_t cmd = slot < FxCmd::kNumSlots ? (uint8_t) (FxCmd::kSlotA + slot)
                                                            : (uint8_t) FxCmd::kPitchBend;
                if (! song.isSmooth (ch, cmd) || ! CcInterp::hasAnyPoint (*p, ch, cmd))
                    continue;
                for (int r = 0; r < p->getNumRows(); ++r)
                    for (int t = 0; t < speed; ++t)
                    {
                        const int v = CcInterp::valueAt (*p, ch, cmd, (double) r + (double) t / (double) speed);
                        if (v < 0 || v == lastSmooth[slot])
                            continue;
                        lastSmooth[slot] = v;
                        const double at = (double) ((rowBase + r) * rt + (juce::int64) t * kTickTicks);
                        seq.addEvent (cmd == FxCmd::kPitchBend
                                          ? juce::MidiMessage::pitchWheel (1, v << 7)
                                          : juce::MidiMessage::controllerEvent (1,
                                                (int) song.ccForSlot (ch, slot), v), at);
                    }
            }

            for (int r = 0; r < p->getNumRows(); ++r)
            {
                const Cell& c = p->at (r, ch);
                const auto tick = (double) ((rowBase + r) * rt);
                const uint8_t fx = FxCmd::sanitize (c.effect);
                const int fxVal = juce::jmin ((int) c.effectValue, 127);

                // effect column -> native MIDI, mirroring playback. Smooth
                // commands are handled by the tick-resolution pass below.
                if (FxCmd::isSlot (fx) && ! song.isSmooth (ch, fx))
                    seq.addEvent (juce::MidiMessage::controllerEvent (1,
                                      (int) song.ccForSlot (ch, FxCmd::slotIndex (fx)), fxVal), tick);
                else if (fx == FxCmd::kPitchBend && ! song.isSmooth (ch, fx))
                    seq.addEvent (juce::MidiMessage::pitchWheel (1, fxVal << 7), tick);

                // Nxx delays the whole cell by x ticks (one tracker tick = kTickTicks)
                double cellTick = tick;
                if (fx == FxCmd::kNoteDelay && (c.hasNote() || c.isNoteOff()))
                    cellTick += juce::jmin (fxVal, speed - 1) * kTickTicks;

                // tracker semantics: next note (or "===") ends the previous one
                if ((c.hasNote() || c.isNoteOff()) && activeNote >= 0)
                {
                    seq.addEvent (juce::MidiMessage::noteOff (1, activeNote), cellTick);
                    activeNote = -1;
                }
                if (c.hasNote())
                {
                    const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) c.volume * 2);
                    seq.addEvent (juce::MidiMessage::noteOn (1, (int) c.note, vel), cellTick);
                    activeNote = c.note;
                }

                // Kxx stops whatever plays on this channel at tick x
                if (fx == FxCmd::kNoteCut && activeNote >= 0)
                {
                    seq.addEvent (juce::MidiMessage::noteOff (1, activeNote),
                                  tick + juce::jmin (fxVal, speed - 1) * kTickTicks);
                    activeNote = -1;
                }
            }
            rowBase += p->getNumRows();
        }

        if (activeNote >= 0)
            seq.addEvent (juce::MidiMessage::noteOff (1, activeNote), (double) (rowBase * rt));

        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }

    return mf;
}

juce::String MidiExport::writeMidiFile (const Song& song, double bpm, int speed,
                                        const juce::StringArray& trackNames, const juce::File& dest)
{
    auto mf = songToMidi (song, bpm, speed, trackNames);

    dest.deleteFile();
    juce::FileOutputStream out (dest);
    if (! out.openedOk())
        return "Cannot write " + dest.getFullPathName();
    if (! mf.writeTo (out))
        return "MIDI write failed";
    out.flush();
    return {};
}
