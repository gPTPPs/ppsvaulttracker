#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "model/Song.h"

// Song -> Standard MIDI File type 1 (one track per channel + tempo track).
// PPQ 960 so one row is EXACTLY 40 * speed integer ticks — no drift, clean
// import into Ableton.
namespace MidiExport
{
    inline constexpr int kPpq = 960;
    inline int rowTicks (int speed) { return 40 * speed; }

    juce::MidiFile songToMidi (const Song&, double bpm, int speed,
                               const juce::StringArray& trackNames);

    // returns an error message, or {} on success
    juce::String writeMidiFile (const Song&, double bpm, int speed,
                                const juce::StringArray& trackNames, const juce::File& dest);
}
