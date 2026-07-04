#pragma once
#include <memory>
#include "model/Pattern.h"

// Phase-2 demo: a 4-bar synthwave bassline (Am / F / C / G), 16 rows per bar,
// eighth-note pump with octave pops. Row = 1/16 at the classic 125 BPM / speed 6.
inline std::unique_ptr<Pattern> makeDemoPattern()
{
    auto p = std::make_unique<Pattern> (64, 1);
    const int roots[4] = { 45, 41, 48, 43 };   // A2, F2, C3, G2

    for (int bar = 0; bar < 4; ++bar)
    {
        const int root = roots[bar] - 12;      // pump an octave below
        for (int step = 0; step < 16; step += 2)
        {
            auto& c  = p->at (bar * 16 + step, 0);
            c.note   = (uint8_t) (step % 8 == 6 ? root + 12 : root);
            c.volume = (uint8_t) (step % 4 == 0 ? 64 : 44);
        }
    }
    return p;
}
