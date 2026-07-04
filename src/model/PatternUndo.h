#pragma once
#include <vector>
#include "model/PatternOps.h"

// Region-snapshot undo/redo, unlimited depth (a cell is 5 bytes — memory is
// a non-issue). Usage: begin() before mutating a region, commit() after; a
// no-op edit (before == after) is discarded.
class PatternUndo
{
public:
    void begin (const Pattern& p, const PatternOps::Selection& sel)
    {
        pending.sel = PatternOps::Selection::clipped (sel, p);
        pending.before = capture (p, pending.sel);
        inTransaction = true;
    }

    void commit (const Pattern& p)
    {
        if (! inTransaction)
            return;
        inTransaction = false;
        pending.after = capture (p, pending.sel);
        if (pending.before == pending.after)
            return;   // nothing actually changed
        undoStack.push_back (pending);
        redoStack.clear();
    }

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    bool undo (Pattern& p)
    {
        if (undoStack.empty())
            return false;
        auto e = undoStack.back();
        undoStack.pop_back();
        apply (p, e.sel, e.before);
        redoStack.push_back (std::move (e));
        return true;
    }

    bool redo (Pattern& p)
    {
        if (redoStack.empty())
            return false;
        auto e = redoStack.back();
        redoStack.pop_back();
        apply (p, e.sel, e.after);
        undoStack.push_back (std::move (e));
        return true;
    }

private:
    struct Edit
    {
        PatternOps::Selection sel;
        std::vector<Cell> before, after;
    };

    static std::vector<Cell> capture (const Pattern& p, const PatternOps::Selection& sel)
    {
        std::vector<Cell> out;
        out.reserve ((size_t) sel.numRows() * (size_t) sel.numChannels());
        for (int r = sel.startRow; r <= sel.endRow; ++r)
            for (int c = sel.startChannel; c <= sel.endChannel; ++c)
                out.push_back (p.at (r, c));
        return out;
    }

    static void apply (Pattern& p, const PatternOps::Selection& sel, const std::vector<Cell>& cells)
    {
        size_t i = 0;
        for (int r = sel.startRow; r <= sel.endRow; ++r)
            for (int c = sel.startChannel; c <= sel.endChannel; ++c)
                if (r < p.getNumRows() && c < p.getNumChannels() && i < cells.size())
                    p.at (r, c) = cells[i++];
                else
                    ++i;
    }

    std::vector<Edit> undoStack, redoStack;
    Edit pending;
    bool inTransaction = false;
};
