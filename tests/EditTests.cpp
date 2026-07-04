// Unit tests for the phase-3 edit layer: PatternOps + PatternUndo.
#include <cstdio>
#include "model/PatternOps.h"
#include "model/PatternUndo.h"

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (! (cond)) {                                                    \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                    \
        }                                                                  \
    } while (false)

using PatternOps::Selection;

static void testTranspose()
{
    Pattern p (16, 2);
    p.at (0, 0).note = 60;
    p.at (1, 0).note = Cell::kNoteOff;
    p.at (2, 1).note = 126;

    PatternOps::transpose (p, { 0, 15, 0, 1 }, 2);
    CHECK (p.at (0, 0).note == 62);
    CHECK (p.at (1, 0).note == Cell::kNoteOff);   // note-off untouched
    CHECK (p.at (2, 1).note == 127);              // clamped at 127
    CHECK (p.at (3, 0).note == Cell::kEmpty);     // empty untouched

    PatternOps::transpose (p, { 0, 0, 0, 0 }, -120);
    CHECK (p.at (0, 0).note == 1);                // clamped at 1
}

static void testInterpolate()
{
    Pattern p (16, 1);
    for (int r = 0; r <= 8; ++r)
        p.at (r, 0).note = 60;
    p.at (0, 0).volume = 0;
    p.at (8, 0).volume = 64;

    PatternOps::interpolateVolume (p, { 0, 8, 0, 0 });
    CHECK (p.at (0, 0).volume == 0);
    CHECK (p.at (4, 0).volume == 32);
    CHECK (p.at (8, 0).volume == 64);
    CHECK (p.at (2, 0).volume == 16);
}

static void testCopyPasteClear()
{
    Pattern p (32, 2);
    p.at (0, 0).note = 60;
    p.at (1, 0).note = 62;
    p.at (0, 1).note = 64;

    auto cb = PatternOps::copy (p, { 0, 1, 0, 1 });
    CHECK (cb.rows == 2 && cb.channels == 2);

    PatternOps::paste (p, cb, 16, 0);
    CHECK (p.at (16, 0).note == 60);
    CHECK (p.at (17, 0).note == 62);
    CHECK (p.at (16, 1).note == 64);

    // paste partially out of bounds must clip, not crash
    PatternOps::paste (p, cb, 31, 1);
    CHECK (p.at (31, 1).note == 60);

    PatternOps::clear (p, { 0, 1, 0, 1 });
    CHECK (p.at (0, 0).note == Cell::kEmpty);
    CHECK (p.at (0, 1).note == Cell::kEmpty);
    CHECK (p.at (16, 0).note == 60);   // outside selection untouched
}

static void testSelectionNormalise()
{
    Pattern p (64, 4);
    Selection s { 20, 5, 3, 1 };
    auto n = Selection::clipped (s, p);
    CHECK (n.startRow == 5 && n.endRow == 20);
    CHECK (n.startChannel == 1 && n.endChannel == 3);
    CHECK (n.numRows() == 16 && n.numChannels() == 3);
}

static void testUndoRedo()
{
    Pattern p (16, 1);
    PatternUndo undo;

    undo.begin (p, { 0, 0, 0, 0 });
    p.at (0, 0).note = 60;
    undo.commit (p);

    undo.begin (p, { 1, 1, 0, 0 });
    p.at (1, 0).note = 62;
    undo.commit (p);

    CHECK (undo.canUndo());
    CHECK (undo.undo (p));
    CHECK (p.at (1, 0).note == Cell::kEmpty);
    CHECK (p.at (0, 0).note == 60);

    CHECK (undo.undo (p));
    CHECK (p.at (0, 0).note == Cell::kEmpty);
    CHECK (! undo.canUndo());

    CHECK (undo.redo (p));
    CHECK (p.at (0, 0).note == 60);
    CHECK (undo.redo (p));
    CHECK (p.at (1, 0).note == 62);
    CHECK (! undo.canRedo());

    // a no-op transaction must not pollute the stack
    undo.begin (p, { 2, 2, 0, 0 });
    undo.commit (p);
    CHECK (undo.undo (p));                        // undoes the note-62 edit
    CHECK (p.at (1, 0).note == Cell::kEmpty);

    // a new edit clears the redo stack
    undo.begin (p, { 3, 3, 0, 0 });
    p.at (3, 0).note = 65;
    undo.commit (p);
    CHECK (! undo.canRedo());
}

int main()
{
    testTranspose();
    testInterpolate();
    testCopyPasteClear();
    testSelectionNormalise();
    testUndoRedo();

    if (failures == 0)
        std::puts ("edit tests: all passed");
    return failures == 0 ? 0 : 1;
}
