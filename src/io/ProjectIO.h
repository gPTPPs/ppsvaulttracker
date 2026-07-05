#pragma once
#include <juce_core/juce_core.h>
#include "model/Song.h"

// Song <-> JSON (juce::var) serialisation for the .ubt project format.
// Loading VALIDATES everything (design rule: never trust a third-party .ubt):
// wrong shapes are fatal errors, out-of-range values are clamped or skipped.
namespace ProjectIO
{
    inline constexpr const char* kFormatTag = "ubt-1";

    juce::var songToVar (const Song&);

    // strict validation; returns an error message, or {} on success (song filled)
    juce::String songFromVar (const juce::var&, Song&);

    // track-name policy, shared by loading and the UI editors: control chars
    // stripped, trimmed, capped at Song::kMaxTrackNameLen code points
    juce::String sanitizeTrackName (const juce::String&);
}
