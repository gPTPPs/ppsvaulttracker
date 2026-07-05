// Unit tests for the I/O layer: .ubt serialisation (round-trip + hostile
// input) and MIDI export tick math.
#include <cstdio>
#include "io/ProjectIO.h"
#include "io/MidiExport.h"
#include "io/ModImportMapping.h"

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (! (cond)) {                                                    \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                    \
        }                                                                  \
    } while (false)

static Song makeSong()
{
    Song s;
    s.setNumChannels (3);
    s.getPattern (0)->at (0, 0) = { 45, 1, 64, 0, 0 };
    s.getPattern (0)->at (4, 1) = { 57, 2, 32, 3, 0x40 };
    s.getPattern (0)->at (8, 2) = { Cell::kNoteOff, 0, 64, 0, 0 };

    const int p1 = s.addPattern (32);
    s.getPattern (p1)->at (31, 2) = { 72, 1, 10, 0, 0 };

    s.orderLen = 4;
    s.order[0] = 0; s.order[1] = 1; s.order[2] = 1; s.order[3] = 0;
    return s;
}

static void testRoundTrip()
{
    const Song original = makeSong();

    // through actual JSON text, like a real file
    const auto json = juce::JSON::toString (ProjectIO::songToVar (original));
    Song restored;
    const auto err = ProjectIO::songFromVar (juce::JSON::parse (json), restored);
    CHECK (err.isEmpty());

    CHECK (restored.getNumChannels() == 3);
    CHECK (restored.getNumPatterns() == 2);
    CHECK (restored.getPattern (1)->getNumRows() == 32);
    CHECK (restored.getPattern (0)->at (0, 0) == original.getPattern (0)->at (0, 0));
    CHECK (restored.getPattern (0)->at (4, 1) == original.getPattern (0)->at (4, 1));
    CHECK (restored.getPattern (0)->at (8, 2).isNoteOff());
    CHECK (restored.getPattern (1)->at (31, 2).note == 72);
    CHECK (restored.getPattern (0)->at (3, 0) == Cell());   // empties stay empty

    CHECK (restored.orderLen == 4);
    CHECK (restored.order[2] == 1);
}

static void testRejectsGarbage()
{
    Song s;
    CHECK (ProjectIO::songFromVar (juce::var ("not an object"), s).isNotEmpty());
    CHECK (ProjectIO::songFromVar (juce::var(), s).isNotEmpty());

    // no patterns
    auto* o = new juce::DynamicObject();
    o->setProperty ("numChannels", 2);
    CHECK (ProjectIO::songFromVar (juce::var (o), s).isNotEmpty());
}

static void testClampsHostileValues()
{
    // hand-built hostile JSON: silly volume, out-of-grid cell, order pointing
    // to a pattern that doesn't exist
    const auto v = juce::JSON::parse (R"({
        "numChannels": 2,
        "patterns": [ { "rows": 8,
                        "cells": [ [0,0,60,0,200,99,300],
                                   [7,1,300,0,10,0,0],
                                   [50,0,60,0,64,0,0],
                                   [0,9,60,0,64,0,0] ] } ],
        "order": [0, 42, -3]
    })");

    Song s;
    const auto err = ProjectIO::songFromVar (v, s);
    CHECK (err.isEmpty());
    CHECK (s.getPattern (0)->at (0, 0).volume == 64);        // 200 -> clamped
    CHECK (s.getPattern (0)->at (0, 0).effect <= 15);        // 99 -> clamped
    CHECK (s.getPattern (0)->at (7, 1).note <= 127);         // 300 -> clamped
    CHECK (s.orderLen == 3);
    CHECK (s.order[1] == 0 && s.order[2] == 0);              // clamped to real patterns

    // invalid channel count is fatal
    const auto bad = juce::JSON::parse (R"({ "numChannels": 999, "patterns": [ { "rows": 8 } ] })");
    CHECK (ProjectIO::songFromVar (bad, s).isNotEmpty());

    // invalid row count is fatal
    const auto bad2 = juce::JSON::parse (R"({ "numChannels": 1, "patterns": [ { "rows": 100000 } ] })");
    CHECK (ProjectIO::songFromVar (bad2, s).isNotEmpty());
}

static void testMidiExport()
{
    Song s;
    s.setNumChannels (2);
    // ch0: note at row 0, replaced at row 4; ch1: note at row 2, "===" at row 6
    s.getPattern (0)->at (0, 0) = { 60, 0, 64, 0, 0 };
    s.getPattern (0)->at (4, 0) = { 62, 0, 32, 0, 0 };
    s.getPattern (0)->at (2, 1) = { 45, 0, 64, 0, 0 };
    s.getPattern (0)->at (6, 1) = { Cell::kNoteOff, 0, 64, 0, 0 };

    const auto mf = MidiExport::songToMidi (s, 125.0, 6, { "Lead", "Bass" });

    CHECK ((int) mf.getTimeFormat() == MidiExport::kPpq);
    CHECK (mf.getNumTracks() == 3);   // tempo + 2 channels

    const int rt = MidiExport::rowTicks (6);
    CHECK (rt == 240);   // PPQ 960: speed-6 row = 1/16 = 240 ticks

    // channel 0 track: on@0, off@4*240, on@4*240
    const auto* t0 = mf.getTrack (1);
    int ons = 0, offs = 0;
    double firstOn = -1, firstOff = -1;
    for (int i = 0; i < t0->getNumEvents(); ++i)
    {
        const auto& m = t0->getEventPointer (i)->message;
        if (m.isNoteOn())  { if (ons == 0)  firstOn  = m.getTimeStamp(); ++ons; }
        if (m.isNoteOff()) { if (offs == 0) firstOff = m.getTimeStamp(); ++offs; }
    }
    CHECK (ons == 2 && offs == 2);
    CHECK (firstOn == 0.0);
    CHECK (firstOff == 4.0 * rt);

    // channel 1 track: explicit note-off at row 6
    const auto* t1 = mf.getTrack (2);
    bool offAtRow6 = false;
    for (int i = 0; i < t1->getNumEvents(); ++i)
    {
        const auto& m = t1->getEventPointer (i)->message;
        if (m.isNoteOff() && m.getTimeStamp() == 6.0 * rt)
            offAtRow6 = true;
    }
    CHECK (offAtRow6);

    // tempo meta: 125 BPM -> 480000 us per quarter
    const auto* tt = mf.getTrack (0);
    bool tempoOk = false;
    for (int i = 0; i < tt->getNumEvents(); ++i)
    {
        const auto& m = tt->getEventPointer (i)->message;
        if (m.isTempoMetaEvent())
            tempoOk = std::abs (m.getTempoSecondsPerQuarterNote() - 0.48) < 1e-6;
    }
    CHECK (tempoOk);
}

static void testCcSlotsIo()
{
    // custom slots survive the JSON round-trip
    Song original = makeSong();
    original.ccSlots[0][0] = 21;
    original.ccSlots[15][7] = 127;

    const auto json = juce::JSON::toString (ProjectIO::songToVar (original));
    Song restored;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (json), restored).isEmpty());
    CHECK (restored.ccForSlot (0, 0) == 21);
    CHECK (restored.ccForSlot (15, 7) == 127);
    CHECK (restored.ccForSlot (1, 0) == 74);   // untouched slot keeps its default

    // pre-effect-column .ubt (no ccSlots property) -> defaults, not an error
    const auto legacy = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ] })");
    Song fromLegacy;
    CHECK (ProjectIO::songFromVar (legacy, fromLegacy).isEmpty());
    CHECK (fromLegacy.ccForSlot (0, 0) == 74);

    // malformed table is fatal, hostile values are clamped
    const auto bad = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ], "ccSlots": "nope" })");
    Song s;
    CHECK (ProjectIO::songFromVar (bad, s).isNotEmpty());

    const auto hostile = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ],
        "ccSlots": [ [9999, -5] ] })");
    CHECK (ProjectIO::songFromVar (hostile, s).isEmpty());
    CHECK (s.ccForSlot (0, 0) == 127 && s.ccForSlot (0, 1) == 0);   // clamped
    CHECK (s.ccForSlot (0, 2) == 1);                                // rest = defaults
}

static void testTrackStyleIo()
{
    // names + colours survive the JSON round-trip
    Song original = makeSong();
    original.trackNames[0] = "Bass";
    original.trackNames[15] = "Pads";
    original.trackColors[0] = 0xff00e5ff;

    const auto json = juce::JSON::toString (ProjectIO::songToVar (original));
    Song restored;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (json), restored).isEmpty());
    CHECK (restored.trackNames[0] == "Bass");
    CHECK (restored.trackNames[15] == "Pads");
    CHECK (restored.trackColors[0] == 0xff00e5ff);
    CHECK (restored.trackNames[1].empty() && restored.trackColors[1] == 0);

    // all-default song emits no "tracks" property at all (song.json unchanged)
    const auto plainJson = juce::JSON::toString (ProjectIO::songToVar (makeSong()));
    CHECK (! plainJson.contains ("\"tracks\""));

    // pre-track-style .ubt -> defaults, not an error
    const auto legacy = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ] })");
    Song fromLegacy;
    CHECK (ProjectIO::songFromVar (legacy, fromLegacy).isEmpty());
    CHECK (fromLegacy.trackNames[0].empty() && fromLegacy.trackColors[0] == 0);

    // malformed shapes are fatal
    Song s;
    const auto bad = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ], "tracks": "nope" })");
    CHECK (ProjectIO::songFromVar (bad, s).isNotEmpty());
    const auto bad2 = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ], "tracks": [ 42 ] })");
    CHECK (ProjectIO::songFromVar (bad2, s).isNotEmpty());

    // hostile values: control chars stripped + capped name, junk colour ignored,
    // alpha forced opaque
    const auto hostile = juce::JSON::parse (
        "{ \"numChannels\": 1, \"patterns\": [ { \"rows\": 8 } ],"
        "  \"tracks\": [ { \"name\": \"  Ba\\nss with a very very long name indeed  \","
        "                  \"color\": \"garbage\" },"
        "                { \"name\": 42, \"color\": \"0000e5ff\" } ] }");
    CHECK (ProjectIO::songFromVar (hostile, s).isEmpty());
    CHECK (s.trackNames[0] == juce::String ("Bass with a very very lo").toStdString());
    CHECK (s.trackColors[0] == 0);                 // junk colour -> theme default
    CHECK (s.trackNames[1].empty());               // non-string name ignored
    CHECK (s.trackColors[1] == 0xff00e5ffu);       // zero alpha forced to opaque

    // the sanitizer itself (shared with the UI editors)
    CHECK (ProjectIO::sanitizeTrackName ("  plain  ") == "plain");
    CHECK (ProjectIO::sanitizeTrackName ("a\tb\rc") == "abc");
    CHECK (ProjectIO::sanitizeTrackName ("") == "");
}

static void testOrderMutesIo()
{
    Song original = makeSong();   // orderLen = 4
    original.orderMutes[1][0] = true;
    original.orderMutes[1][15] = true;
    original.orderMutes[3][2] = true;

    const auto json = juce::JSON::toString (ProjectIO::songToVar (original));
    Song restored;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (json), restored).isEmpty());
    CHECK (restored.orderMuted (1, 0) && restored.orderMuted (1, 15));
    CHECK (restored.orderMuted (3, 2));
    CHECK (! restored.orderMuted (0, 0) && ! restored.orderMuted (1, 1));

    // all-clear matrix emits nothing (older files stay identical)
    const auto plainJson = juce::JSON::toString (ProjectIO::songToVar (makeSong()));
    CHECK (! plainJson.contains ("orderMutes"));
    Song plainBack;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (plainJson), plainBack).isEmpty());
    CHECK (! plainBack.orderMuted (0, 0));

    // hostile: wrong shape fatal, out-of-range masks clamped
    Song s;
    const auto bad = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ], "orderMutes": "nope" })");
    CHECK (ProjectIO::songFromVar (bad, s).isNotEmpty());

    const auto hostile = juce::JSON::parse (R"({
        "numChannels": 1, "patterns": [ { "rows": 8 } ],
        "order": [0, 0], "orderMutes": [99999999, -7] })");
    CHECK (ProjectIO::songFromVar (hostile, s).isEmpty());
    CHECK (s.orderMuted (0, 0) && s.orderMuted (0, 15));   // clamped to 0xFFFF
    CHECK (! s.orderMuted (1, 0));                         // negative -> 0

    // accessor bounds
    CHECK (! s.orderMuted (-1, 0) && ! s.orderMuted (0, -1)
           && ! s.orderMuted (Song::kMaxOrder, 0) && ! s.orderMuted (0, Song::kCcTracks));
}

static void testMidiExportOrderMutes()
{
    Song s;
    s.setNumChannels (1);
    s.getPattern (0)->at (0, 0) = { 60, 0, 64, 0, 0 };   // long note, no note-off
    s.orderLen = 3;
    s.order[0] = 0; s.order[1] = 0; s.order[2] = 0;
    s.orderMutes[1][0] = true;                            // middle block muted

    const auto mf = MidiExport::songToMidi (s, 125.0, 6, { "T" });
    const int rt = MidiExport::rowTicks (6);
    const auto* t = mf.getTrack (1);

    int ons = 0;
    bool cutAtBlock1 = false, onAtBlock2 = false;
    for (int i = 0; i < t->getNumEvents(); ++i)
    {
        const auto& m = t->getEventPointer (i)->message;
        if (m.isNoteOn())
        {
            ++ons;
            if (m.getTimeStamp() == 128.0 * rt)   // block 2 starts at row 128
                onAtBlock2 = true;
        }
        if (m.isNoteOff() && m.getTimeStamp() == 64.0 * rt)   // entering the muted block
            cutAtBlock1 = true;
    }
    CHECK (ons == 2);          // block 0 + block 2; nothing in the muted block
    CHECK (cutAtBlock1);       // the ringing note is cut at the muted block's door
    CHECK (onAtBlock2);
}

static void testPatternNames()
{
    Song original = makeSong();
    original.getPattern (0)->name = "Intro";
    original.getPattern (1)->name = "Drop";

    const auto json = juce::JSON::toString (ProjectIO::songToVar (original));
    Song restored;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (json), restored).isEmpty());
    CHECK (restored.getPattern (0)->name == "Intro");
    CHECK (restored.getPattern (1)->name == "Drop");

    // unnamed pattern emits no "name" property and loads back empty
    Song plain = makeSong();
    const auto plainJson = juce::JSON::toString (ProjectIO::songToVar (plain));
    CHECK (! plainJson.contains ("\"name\""));
    Song plainBack;
    CHECK (ProjectIO::songFromVar (juce::JSON::parse (plainJson), plainBack).isEmpty());
    CHECK (plainBack.getPattern (0)->name.empty());

    // hostile: non-string ignored, junk sanitized like track names
    const auto hostile = juce::JSON::parse (
        "{ \"numChannels\": 1, \"patterns\": [ { \"rows\": 8, \"name\": 42 },"
        "                                      { \"rows\": 8, \"name\": \" In\\ntro  \" } ] }");
    Song s;
    CHECK (ProjectIO::songFromVar (hostile, s).isEmpty());
    CHECK (s.getPattern (0)->name.empty());
    CHECK (s.getPattern (1)->name == "Intro");
}

static void testMidiExportEffects()
{
    Song s;
    s.setNumChannels (1);
    s.ccSlots[0][1] = 71;   // slot B

    // row 0: note + B50 (CC71 = 0x50 at the row start)
    s.getPattern (0)->at (0, 0) = { 60, 0, 64, FxCmd::kSlotA + 1, 0x50 };
    // row 2: P40 = centre pitch bend, no note
    s.getPattern (0)->at (2, 0) = { 0, 0, 64, FxCmd::kPitchBend, 0x40 };
    // row 4: new note delayed 3 ticks (kill of the old one moves with it)
    s.getPattern (0)->at (4, 0) = { 62, 0, 64, FxCmd::kNoteDelay, 3 };
    // row 6: cut at tick 2
    s.getPattern (0)->at (6, 0) = { 0, 0, 64, FxCmd::kNoteCut, 2 };

    const auto mf = MidiExport::songToMidi (s, 125.0, 6, { "T" });
    const int rt = MidiExport::rowTicks (6);
    const int tk = MidiExport::kTickTicks;

    bool ccOk = false, bendOk = false, delayedOnOk = false, delayedOffOk = false, cutOk = false;
    const auto* t = mf.getTrack (1);
    for (int i = 0; i < t->getNumEvents(); ++i)
    {
        const auto& m = t->getEventPointer (i)->message;
        const auto at = m.getTimeStamp();
        if (m.isController() && m.getControllerNumber() == 71
            && m.getControllerValue() == 0x50 && at == 0.0)
            ccOk = true;
        if (m.isPitchWheel() && m.getPitchWheelValue() == 0x2000 && at == 2.0 * rt)
            bendOk = true;
        if (m.isNoteOn() && m.getNoteNumber() == 62 && at == 4.0 * rt + 3 * tk)
            delayedOnOk = true;
        if (m.isNoteOff() && m.getNoteNumber() == 60 && at == 4.0 * rt + 3 * tk)
            delayedOffOk = true;
        if (m.isNoteOff() && m.getNoteNumber() == 62 && at == 6.0 * rt + 2 * tk)
            cutOk = true;
    }
    CHECK (ccOk);
    CHECK (bendOk);
    CHECK (delayedOnOk);
    CHECK (delayedOffOk);
    CHECK (cutOk);
}

static void testModMapping()
{
    using namespace ModImportMapping;
    CHECK (noteFromOpenMpt (0) == Cell::kEmpty);
    CHECK (noteFromOpenMpt (61) == 60);              // OpenMPT middle C -> MIDI 60
    CHECK (noteFromOpenMpt (1) == 1);                // clamped low end
    CHECK (noteFromOpenMpt (253) == Cell::kNoteOff); // fade
    CHECK (noteFromOpenMpt (254) == Cell::kNoteOff); // note cut
    CHECK (noteFromOpenMpt (255) == Cell::kNoteOff); // key off
    CHECK (noteFromOpenMpt (-5) == Cell::kEmpty);

    CHECK (volumeFromOpenMpt (1, 40) == 40);         // VOLCMD_VOLUME passes through
    CHECK (volumeFromOpenMpt (1, 999) == 64);        // clamped
    CHECK (volumeFromOpenMpt (0, 12) == 64);         // no volume command -> default
    CHECK (volumeFromOpenMpt (5, 12) == 64);         // other volcmd -> default
}

int main()
{
    testRoundTrip();
    testRejectsGarbage();
    testClampsHostileValues();
    testMidiExport();
    testCcSlotsIo();
    testTrackStyleIo();
    testPatternNames();
    testOrderMutesIo();
    testMidiExportOrderMutes();
    testMidiExportEffects();
    testModMapping();

    if (failures == 0)
        std::puts ("io tests: all passed");
    return failures == 0 ? 0 : 1;
}
