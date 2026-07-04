#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "model/Pattern.h"

// Pure edit operations on a Pattern — no UI, no JUCE, unit-tested.
namespace PatternOps
{

struct Selection
{
    int startRow = 0, endRow = 0;           // inclusive
    int startChannel = 0, endChannel = 0;   // inclusive

    Selection normalised() const
    {
        return { std::min (startRow, endRow),         std::max (startRow, endRow),
                 std::min (startChannel, endChannel), std::max (startChannel, endChannel) };
    }

    static Selection clipped (const Selection& s, const Pattern& p)
    {
        Selection r = s.normalised();
        r.startRow     = std::clamp (r.startRow,     0, p.getNumRows() - 1);
        r.endRow       = std::clamp (r.endRow,       0, p.getNumRows() - 1);
        r.startChannel = std::clamp (r.startChannel, 0, p.getNumChannels() - 1);
        r.endChannel   = std::clamp (r.endChannel,   0, p.getNumChannels() - 1);
        return r;
    }

    int numRows() const     { return endRow - startRow + 1; }
    int numChannels() const { return endChannel - startChannel + 1; }
};

struct Clipboard
{
    int rows = 0, channels = 0;
    std::vector<Cell> cells;
    bool empty() const { return cells.empty(); }
};

inline void transpose (Pattern& p, const Selection& selIn, int semitones)
{
    const auto sel = Selection::clipped (selIn, p);
    for (int r = sel.startRow; r <= sel.endRow; ++r)
        for (int c = sel.startChannel; c <= sel.endChannel; ++c)
        {
            auto& cell = p.at (r, c);
            if (cell.hasNote())
                cell.note = (uint8_t) std::clamp ((int) cell.note + semitones, 1, 127);
        }
}

// FT2 Ctrl+L: linear volume ramp between the first and last row of the
// selection, per channel.
inline void interpolateVolume (Pattern& p, const Selection& selIn)
{
    const auto sel = Selection::clipped (selIn, p);
    if (sel.numRows() < 2)
        return;

    for (int c = sel.startChannel; c <= sel.endChannel; ++c)
    {
        const double v0 = p.at (sel.startRow, c).volume;
        const double v1 = p.at (sel.endRow, c).volume;
        for (int r = sel.startRow; r <= sel.endRow; ++r)
        {
            const double t = (double) (r - sel.startRow) / (double) (sel.endRow - sel.startRow);
            p.at (r, c).volume = (uint8_t) std::clamp ((int) std::lround (v0 + (v1 - v0) * t), 0, 64);
        }
    }
}

inline Clipboard copy (const Pattern& p, const Selection& selIn)
{
    const auto sel = Selection::clipped (selIn, p);
    Clipboard cb { sel.numRows(), sel.numChannels(), {} };
    cb.cells.reserve ((size_t) cb.rows * (size_t) cb.channels);
    for (int r = sel.startRow; r <= sel.endRow; ++r)
        for (int c = sel.startChannel; c <= sel.endChannel; ++c)
            cb.cells.push_back (p.at (r, c));
    return cb;
}

inline void clear (Pattern& p, const Selection& selIn)
{
    const auto sel = Selection::clipped (selIn, p);
    for (int r = sel.startRow; r <= sel.endRow; ++r)
        for (int c = sel.startChannel; c <= sel.endChannel; ++c)
            p.at (r, c) = Cell();
}

// Pastes the clipboard with its top-left corner at (atRow, atChannel),
// clipped at the pattern's active bounds.
inline void paste (Pattern& p, const Clipboard& cb, int atRow, int atChannel)
{
    if (cb.empty())
        return;
    for (int r = 0; r < cb.rows; ++r)
        for (int c = 0; c < cb.channels; ++c)
        {
            const int dr = atRow + r, dc = atChannel + c;
            if (dr >= 0 && dr < p.getNumRows() && dc >= 0 && dc < p.getNumChannels())
                p.at (dr, dc) = cb.cells[(size_t) r * (size_t) cb.channels + (size_t) c];
        }
}

} // namespace PatternOps
