#pragma once
#include <vector>
#include "model/Cell.h"

// A rows × channels grid of cells.
//
// Storage is preallocated at fixed capacity with a CONSTANT stride
// (kMaxChannels), so cell addresses never move: the UI thread edits cells by
// direct write while the audio thread reads them, lock-free (benign-race
// documented design decision). Changing the active dimensions never
// reallocates.
class Pattern
{
public:
    static constexpr int kMaxRows     = 256;
    static constexpr int kMaxChannels = 32;

    explicit Pattern (int rows = 64, int channels = 1)
        : numRows (clampRows (rows)), numChannels (clampChannels (channels)),
          cells ((size_t) kMaxRows * (size_t) kMaxChannels) {}

    int getNumRows() const     { return numRows; }
    int getNumChannels() const { return numChannels; }

    void setNumRows (int r)     { numRows = clampRows (r); }
    void setNumChannels (int c) { numChannels = clampChannels (c); }

    Cell&       at (int row, int channel)       { return cells[index (row, channel)]; }
    const Cell& at (int row, int channel) const { return cells[index (row, channel)]; }

private:
    static int clampRows (int r)     { return r < 1 ? 1 : (r > kMaxRows ? kMaxRows : r); }
    static int clampChannels (int c) { return c < 1 ? 1 : (c > kMaxChannels ? kMaxChannels : c); }

    size_t index (int row, int channel) const
    {
        return (size_t) row * (size_t) kMaxChannels + (size_t) channel;
    }

    int numRows, numChannels;
    std::vector<Cell> cells;
};
