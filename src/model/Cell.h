#pragma once
#include <cstdint>

// One pattern cell. The effect column is native MIDI per CLAUDE.md; it exists
// in the model from day one but stays inert until the effect phase.
struct Cell
{
    static constexpr uint8_t kEmpty   = 0;     // no note in this cell
    static constexpr uint8_t kNoteOff = 255;   // explicit note-off (FT2 "===")

    uint8_t note        = kEmpty;   // 1..127 = MIDI note
    uint8_t instrument  = 0;        // 0 = none (single instrument in phase 2)
    uint8_t volume      = 64;       // 0..64 FT2-style, mapped to MIDI velocity
    uint8_t effect      = 0;        // inert in phase 2
    uint8_t effectValue = 0;        // inert in phase 2

    bool hasNote() const   { return note >= 1 && note <= 127; }
    bool isNoteOff() const { return note == kNoteOff; }
};
