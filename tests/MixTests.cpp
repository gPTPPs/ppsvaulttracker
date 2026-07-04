// Unit tests for the phase-4 pure logic: Song model + solo/mute resolution.
#include <cstdio>
#include "model/Song.h"
#include "engine/MixLogic.h"

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (! (cond)) {                                                    \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                    \
        }                                                                  \
    } while (false)

static void testSongDefaults()
{
    Song s;
    CHECK (s.getNumPatterns() == 1);
    CHECK (s.getPattern (0) != nullptr);
    CHECK (s.getPattern (1) == nullptr);
    CHECK (s.getPattern (-1) == nullptr);
    CHECK (s.orderLen == 1);
    CHECK (s.order[0] == 0);
}

static void testSongAddPattern()
{
    Song s;
    const int idx = s.addPattern (32);
    CHECK (idx == 1);
    CHECK (s.getNumPatterns() == 2);
    CHECK (s.getPattern (1)->getNumRows() == 32);

    // new patterns inherit the song's channel count
    s.setNumChannels (4);
    const int idx2 = s.addPattern();
    CHECK (s.getPattern (idx2)->getNumChannels() == 4);
    CHECK (s.getPattern (0)->getNumChannels() == 4);   // existing ones follow too

    // fill to capacity
    while (s.addPattern() >= 0) {}
    CHECK (s.getNumPatterns() == Song::kMaxPatterns);
    CHECK (s.addPattern() == -1);
}

static void testMuteSoloLogic()
{
    constexpr int n = 4;
    bool mute[n] = { false, true, false, false };
    bool solo[n] = {};
    bool out[n];

    // no solo: effective = mute
    computeEffectiveMutes (mute, solo, n, out);
    CHECK (! out[0] && out[1] && ! out[2] && ! out[3]);

    // solo ch0: everything else muted
    solo[0] = true;
    computeEffectiveMutes (mute, solo, n, out);
    CHECK (! out[0] && out[1] && out[2] && out[3]);

    // solo + mute on the same channel: mute wins
    mute[0] = true;
    computeEffectiveMutes (mute, solo, n, out);
    CHECK (out[0]);

    // two solos: both audible, rest muted
    mute[0] = false;
    solo[2] = true;
    computeEffectiveMutes (mute, solo, n, out);
    CHECK (! out[0] && out[1] && ! out[2] && out[3]);
}

int main()
{
    testSongDefaults();
    testSongAddPattern();
    testMuteSoloLogic();

    if (failures == 0)
        std::puts ("mix/song tests: all passed");
    return failures == 0 ? 0 : 1;
}
