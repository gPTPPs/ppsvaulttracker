#pragma once
#include <vector>
#include "model/Cell.h"

// A rows × channels grid of cells. Phase 2 uses a single channel, but the
// grid is generic from the start.
class Pattern
{
public:
    explicit Pattern (int rows = 64, int channels = 1)
        : numRows (rows), numChannels (channels),
          cells ((size_t) rows * (size_t) channels) {}

    int getNumRows() const      { return numRows; }
    int getNumChannels() const  { return numChannels; }

    Cell&       at (int row, int channel)       { return cells[index (row, channel)]; }
    const Cell& at (int row, int channel) const { return cells[index (row, channel)]; }

private:
    size_t index (int row, int channel) const
    {
        return (size_t) row * (size_t) numChannels + (size_t) channel;
    }

    int numRows, numChannels;
    std::vector<Cell> cells;
};
