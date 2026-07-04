// Unit tests for the .ubt serialisation layer (ProjectIO): full round-trip
// plus strict validation of malformed / hostile input.
#include <cstdio>
#include "io/ProjectIO.h"

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

int main()
{
    testRoundTrip();
    testRejectsGarbage();
    testClampsHostileValues();

    if (failures == 0)
        std::puts ("io tests: all passed");
    return failures == 0 ? 0 : 1;
}
