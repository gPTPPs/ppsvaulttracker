// Unit tests for the JUCE-free core: Cell/Pattern model and the FT2 clock.
#include <cstdio>
#include <cmath>
#include <vector>
#include "model/Cell.h"
#include "model/Pattern.h"
#include "sequencer/TrackerClock.h"

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (! (cond)) {                                                    \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                    \
        }                                                                  \
    } while (false)

static void testCell()
{
    Cell c;
    CHECK (! c.hasNote());
    CHECK (! c.isNoteOff());
    c.note = 60;
    CHECK (c.hasNote());
    c.note = Cell::kNoteOff;
    CHECK (c.isNoteOff());
    CHECK (! c.hasNote());
}

static void testPattern()
{
    Pattern p (64, 2);
    CHECK (p.getNumRows() == 64);
    CHECK (p.getNumChannels() == 2);
    p.at (10, 1).note = 45;
    CHECK (p.at (10, 1).note == 45);
    CHECK (p.at (10, 0).note == Cell::kEmpty);   // neighbour untouched
    const Pattern& cp = p;
    CHECK (cp.at (10, 1).hasNote());
}

static void testClockMath()
{
    TrackerClock c;
    c.prepare (48000.0);
    c.setTempo (125.0, 6);
    // FT2: tick = 2.5/125 s = 20 ms; row = 6 ticks = 120 ms = 5760 samples @48k
    CHECK (std::abs (c.samplesPerRow() - 5760.0) < 1e-9);
}

static void testClockAdvance()
{
    TrackerClock c;
    c.prepare (48000.0);
    c.setTempo (125.0, 6);

    std::vector<int> rows, offsets;
    c.advance (11520, 64, [&] (int r, int o) { rows.push_back (r); offsets.push_back (o); });

    // rows 0 and 1 fire inside [0, 11520): at samples 0 and 5760
    CHECK (rows.size() == 2);
    CHECK (rows[0] == 0 && offsets[0] == 0);
    CHECK (rows[1] == 1 && offsets[1] == 5760);

    // next buffer starts exactly on row 2
    rows.clear(); offsets.clear();
    c.advance (512, 64, [&] (int r, int o) { rows.push_back (r); offsets.push_back (o); });
    CHECK (rows.size() == 1);
    CHECK (rows[0] == 2 && offsets[0] == 0);
}

static void testClockLoop()
{
    TrackerClock c;
    c.prepare (48000.0);
    c.setTempo (125.0, 6);

    std::vector<int> rows;
    // 6 rows worth of samples over a 4-row pattern -> must wrap 0..3, 0, 1
    c.advance ((int) (5760 * 6), 4, [&] (int r, int) { rows.push_back (r); });
    CHECK (rows.size() == 6);
    const int expected[6] = { 0, 1, 2, 3, 0, 1 };
    for (int i = 0; i < 6; ++i)
        CHECK (rows[(size_t) i] == expected[i]);
}

static void testClockOffsetsInsideBuffer()
{
    TrackerClock c;
    c.prepare (44100.0);
    c.setTempo (137.0, 5);   // awkward tempo on purpose

    bool ok = true;
    for (int block = 0; block < 200; ++block)
        c.advance (480, 64, [&] (int, int o) { ok = ok && o >= 0 && o < 480; });
    CHECK (ok);
}

int main()
{
    testCell();
    testPattern();
    testClockMath();
    testClockAdvance();
    testClockLoop();
    testClockOffsetsInsideBuffer();

    if (failures == 0)
        std::puts ("model/clock tests: all passed");
    return failures == 0 ? 0 : 1;
}
