#pragma once
#include "model/Pattern.h"
#include "model/EffectCommands.h"

// Shared CC-lane interpolation (session 4): linear ramp between consecutive
// cells carrying the same effect-column command, within a single pattern.
// Used identically by the sequencer (audio thread) and the MIDI export so
// playback and the .mid match to the value. Pure, JUCE-free, header-only.
//
// A "control point" is a row whose cell holds command `cmd`; its value is the
// cell's effectValue. Before the first point / after the last, the value is
// held (no extrapolation). fracRow may be fractional (mid-row, for tick-level
// sampling) — the value is linearly interpolated in row space.
namespace CcInterp
{
    // value at fractional row position, or -1 if the track has no control
    // point for this command anywhere in the pattern (caller emits nothing)
    inline int valueAt (const Pattern& p, int track, uint8_t cmd, double fracRow)
    {
        const int rows = p.getNumRows();

        // nearest control point at or before fracRow
        int prevRow = -1, prevVal = 0;
        for (int r = (int) fracRow; r >= 0; --r)
            if (p.at (r, track).effect == cmd) { prevRow = r; prevVal = p.at (r, track).effectValue; break; }

        // nearest control point strictly after fracRow's integer row
        int nextRow = -1, nextVal = 0;
        for (int r = (int) fracRow + 1; r < rows; ++r)
            if (p.at (r, track).effect == cmd) { nextRow = r; nextVal = p.at (r, track).effectValue; break; }

        if (prevRow < 0 && nextRow < 0) return -1;          // no points at all
        if (prevRow < 0) return nextVal;                    // before the first: hold
        if (nextRow < 0) return prevVal;                    // after the last: hold

        const double t = (fracRow - prevRow) / (double) (nextRow - prevRow);
        const double v = prevVal + (nextVal - prevVal) * t;
        return (int) (v + 0.5);
    }

    // does this track carry at least one control point for cmd in the pattern?
    inline bool hasAnyPoint (const Pattern& p, int track, uint8_t cmd)
    {
        for (int r = 0; r < p.getNumRows(); ++r)
            if (p.at (r, track).effect == cmd)
                return true;
        return false;
    }
}
