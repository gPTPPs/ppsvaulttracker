// Unit tests for the JUCE-free core: Cell/Pattern model, effect commands,
// CC slot table and the FT2 clock.
#include <cstdio>
#include <cmath>
#include <vector>
#include "model/Cell.h"
#include "model/EffectCommands.h"
#include "model/Pattern.h"
#include "model/Song.h"
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

static void testClockPhase()
{
    TrackerClock c;
    c.prepare (48000.0);
    c.setTempo (125.0, 6);   // 5760 samples per row

    c.advance (1440, 64, [] (int, int) {});   // quarter of a row
    CHECK (std::abs (c.phaseInRow() - 0.25) < 1e-9);

    c.advance (3600, 64, [] (int, int) {});   // now at 5040 = 0.875
    CHECK (c.phaseInRow() > 0.5);             // quantizer would pick the NEXT row

    // landing exactly on the boundary: the row has not fired yet, phase
    // reads 1.0 — the nearest-row quantizer correctly picks the next row
    c.advance (720, 64, [] (int, int) {});
    CHECK (c.phaseInRow() > 0.999);

    // next block fires the row at offset 0, phase drops back
    c.advance (576, 64, [] (int, int) {});    // 576/5760 = 0.1 into the row
    CHECK (std::abs (c.phaseInRow() - 0.1) < 1e-9);
}

static void testFxCommands()
{
    using namespace FxCmd;
    CHECK (fromChar ('a') == kSlotA && fromChar ('A') == kSlotA);
    CHECK (fromChar ('h') == kSlotH);
    CHECK (fromChar ('p') == kPitchBend);
    CHECK (fromChar ('n') == kNoteDelay);
    CHECK (fromChar ('k') == kNoteCut);
    CHECK (fromChar ('.') == kNone && fromChar ('0') == kNone);
    CHECK (fromChar ('z') == -1 && fromChar ('5') == -1 && fromChar (' ') == -1);

    // display letter and key entry agree for every command
    for (int fx = kSlotA; fx < kNumCommands; ++fx)
        CHECK (fromChar (letter ((uint8_t) fx)) == fx);
    CHECK (letter (kNone) == 0);

    CHECK (slotIndex (kSlotA) == 0 && slotIndex (kSlotH) == 7);
    CHECK (slotIndex (kPitchBend) == -1 && slotIndex (kNone) == -1);
    CHECK (sanitize (200) == kNone && sanitize (kNumCommands) == kNone);
    CHECK (sanitize (kNoteCut) == kNoteCut);
}

static void testCcSlotTable()
{
    Song s;
    CHECK (s.ccForSlot (0, 0) == 74);    // A = cutoff
    CHECK (s.ccForSlot (15, 7) == 64);   // H = sustain
    CHECK (s.ccForSlot (-4, 99) == s.ccForSlot (0, 7));   // clamped access

    s.ccSlots[3][2] = 11;
    CHECK (s.ccForSlot (3, 2) == 11);
    CHECK (s.ccForSlot (4, 2) == 1);     // other tracks untouched
    s.resetCcSlots();
    CHECK (s.ccForSlot (3, 2) == 1);
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
    testFxCommands();
    testCcSlotTable();
    testClockMath();
    testClockAdvance();
    testClockLoop();
    testClockPhase();
    testClockOffsetsInsideBuffer();

    if (failures == 0)
        std::puts ("model/clock tests: all passed");
    return failures == 0 ? 0 : 1;
}
