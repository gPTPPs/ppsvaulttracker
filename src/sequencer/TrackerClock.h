#pragma once
#include <algorithm>

// FT2-style clock, pure logic (no JUCE — unit-testable without audio).
// One tick lasts 2.5 / BPM seconds; one row lasts `speed` ticks.
// advance() walks a buffer and reports each row start with its exact sample
// offset inside that buffer — this is what makes playback sample-accurate.
class TrackerClock
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        samplePos = 0.0;
        nextRowAt = 0.0;
        row = 0;
    }

    void setTempo (double newBpm, int newSpeed)
    {
        bpm = newBpm;
        speed = newSpeed;
    }

    double samplesPerRow() const { return sampleRate * 2.5 / bpm * speed; }
    int getCurrentRow() const    { return row; }

    // 0..1 position inside the current row — the live-recording quantizer
    // rounds to the nearest row with this
    double phaseInRow() const
    {
        const double spr = samplesPerRow();
        if (spr <= 0.0)
            return 0.0;
        const double p = 1.0 - (nextRowAt - samplePos) / spr;
        return p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
    }

    // Walks numSamples; calls onRow(rowIndex, sampleOffsetInBuffer) for every
    // row boundary crossed. Loops over numRows. Tempo changes take effect at
    // the next row boundary.
    template <typename Fn>
    void advance (int numSamples, int numRows, Fn&& onRow)
    {
        const double end = samplePos + (double) numSamples;
        while (nextRowAt < end)
        {
            const int offset = (int) std::max (0.0, nextRowAt - samplePos);
            onRow (row, offset);
            row = (row + 1) % numRows;
            nextRowAt += samplesPerRow();
        }
        samplePos = end;
    }

private:
    double sampleRate = 44100.0;
    double bpm = 125.0;
    int speed = 6;
    int row = 0;
    double samplePos = 0.0;
    double nextRowAt = 0.0;
};
