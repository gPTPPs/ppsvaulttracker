#pragma once
#include <memory>
#include <string>
#include <utility>
#include "model/EffectCommands.h"
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
    static constexpr int kCcTracks    = 16;   // == the engine's MIDI-channel ceiling

    // per-track CC slot defaults (A..H): cutoff, resonance, mod wheel, volume,
    // pan, reverb, chorus, sustain
    static constexpr uint8_t kDefaultCcSlots[FxCmd::kNumSlots] = { 74, 71, 1, 7, 10, 91, 93, 64 };

    Song()
    {
        patterns[0] = std::make_unique<Pattern> (64, 1);
        resetCcSlots();
    }

    void resetCcSlots()
    {
        for (auto& track : ccSlots)
            for (int s = 0; s < FxCmd::kNumSlots; ++s)
                track[s] = kDefaultCcSlots[s];
    }

    // clamped accessor (audio thread reads while the UI edits — single bytes,
    // same benign race as pattern cells)
    uint8_t ccForSlot (int track, int slot) const
    {
        track = track < 0 ? 0 : (track >= kCcTracks ? kCcTracks - 1 : track);
        slot  = slot  < 0 ? 0 : (slot >= FxCmd::kNumSlots ? FxCmd::kNumSlots - 1 : slot);
        return ccSlots[track][slot];
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

    // swap two tracks across the whole song: every pattern column (all rows,
    // including rows hidden by a smaller numRows), CC slots, name and colour.
    // Cells swap one by one while the audio thread may be reading — same
    // benign race as live cell editing.
    void swapTracks (int a, int b)
    {
        if (a == b || a < 0 || b < 0 || a >= kCcTracks || b >= kCcTracks)
            return;
        for (int i = 0; i < numPatterns; ++i)
            for (int r = 0; r < Pattern::kMaxRows; ++r)
                std::swap (patterns[i]->at (r, a), patterns[i]->at (r, b));
        for (int s = 0; s < FxCmd::kNumSlots; ++s)
            std::swap (ccSlots[a][s], ccSlots[b][s]);
        std::swap (trackNames[a], trackNames[b]);
        std::swap (trackColors[a], trackColors[b]);
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

    // CC slot table (A..H per track), values 0..127
    uint8_t ccSlots[kCcTracks][FxCmd::kNumSlots] = {};

    // track identity (message thread only — the audio thread never reads
    // these): empty name = default "CH n", colour 0 = theme default (ARGB)
    static constexpr int kMaxTrackNameLen = 24;
    std::string trackNames[kCcTracks];
    uint32_t    trackColors[kCcTracks] = {};

private:
    std::unique_ptr<Pattern> patterns[kMaxPatterns];
    int numPatterns = 1;
    int numChannels = 1;
};
