#pragma once
#include <memory>
#include "model/Pattern.h"

// Song = patterns + order list (indices into the pattern pool), fixed
// capacity like Pattern so the audio thread can read while the UI edits.
// Structural changes (adding patterns, resizing the order) happen on the
// message thread with the audio player detached.
class Song
{
public:
    static constexpr int kMaxPatterns = 64;
    static constexpr int kMaxOrder    = 128;

    Song()
    {
        patterns[0] = std::make_unique<Pattern> (64, 1);
    }

    Pattern* getPattern (int i) const
    {
        return i >= 0 && i < numPatterns ? patterns[i].get() : nullptr;
    }

    int getNumPatterns() const { return numPatterns; }

    // returns the new pattern's index, or -1 when full
    int addPattern (int rows = 64)
    {
        if (numPatterns >= kMaxPatterns)
            return -1;
        patterns[numPatterns] = std::make_unique<Pattern> (rows, numChannels);
        return numPatterns++;
    }

    int getNumChannels() const { return numChannels; }

    void setNumChannels (int c)
    {
        numChannels = c < 1 ? 1 : (c > Pattern::kMaxChannels ? Pattern::kMaxChannels : c);
        for (int i = 0; i < numPatterns; ++i)
            patterns[i]->setNumChannels (numChannels);
    }

    // order list: plain arrays, read by the audio thread at pattern boundaries
    int order[kMaxOrder] = { 0 };
    int orderLen = 1;

private:
    std::unique_ptr<Pattern> patterns[kMaxPatterns];
    int numPatterns = 1;
    int numChannels = 1;
};
