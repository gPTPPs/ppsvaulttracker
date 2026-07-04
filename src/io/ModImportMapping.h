#pragma once
#include <cstdint>
#include "model/Cell.h"

// Pure mapping helpers for module import (no libopenmpt dependency here, so
// they unit-test everywhere).
namespace ModImportMapping
{
    // OpenMPT raw note -> cell note. OpenMPT: 0 = none, 1..120 = C-0..B-9
    // (middle C = 61 -> MIDI 60), 253/254/255 = fade / note-cut / key-off.
    inline uint8_t noteFromOpenMpt (int v)
    {
        if (v <= 0)
            return Cell::kEmpty;
        if (v >= 253)
            return Cell::kNoteOff;
        const int midi = v - 1;
        return (uint8_t) (midi < 1 ? 1 : (midi > 127 ? 127 : midi));
    }

    // volume column: VOLCMD_VOLUME (raw volumeffect == 1) carries 0..64
    inline uint8_t volumeFromOpenMpt (int volEffect, int volParam)
    {
        if (volEffect == 1)
            return (uint8_t) (volParam < 0 ? 0 : (volParam > 64 ? 64 : volParam));
        return 64;
    }
}
