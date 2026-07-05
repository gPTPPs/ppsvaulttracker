#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"

// Arrangement view (v1.1 session 1): the order list as a horizontal timeline.
// One block per order entry, width proportional to the pattern length, body =
// per-track mini-map (note density in each track's colour). Click = select +
// make it the edited pattern, double-click = open it in the tracker view,
// drag = reorder (Ctrl+drag = duplicate), Delete = remove entry,
// right-click = insert/duplicate/remove/rename menu.
class ArrangementView : public juce::Component,
                        private juce::Timer
{
public:
    explicit ArrangementView (HostEngine& e);

    std::function<void()> onOpenPattern;   // double-click -> back to the tracker

    void resetSelection();   // after project load/new: back to the song start

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    int currentIdealWidth() const;

private:
    struct Block { int x, w, patIdx; };

    // an order entry with its mute row, so reorder/duplicate/insert move the
    // arrangement-matrix mutes together with the entry
    struct Entry { int pat; bool mutes[Song::kCcTracks]; };

    void timerCallback() override;
    juce::Array<Block> computeBlocks() const;
    int trackCount() const;
    int blockHeight() const;
    int entryAt (int x) const;            // -1 outside any block
    int laneAt (int entry, juce::Point<int> pos) const;   // track lane under the mouse, -1 outside
    int insertionIndexAt (int x) const;   // 0..orderLen
    juce::Array<Entry> snapshotOrder() const;
    void setSelected (int s);   // clamps + feeds the sequencer's start position
    void applyEntries (const juce::Array<Entry>&, int newSelected);
    void insertEntryAfter (int entry, int patIdx);
    void removeEntry (int entry);
    void showContextMenu (int entry);
    void beginRename (int entry);
    void endRename (bool commit);
    static juce::String patternLabel (const Song&, int patIdx);

    static constexpr int kMargin = 10, kGap = 8, kHeaderH = 36, kLaneH = 14;
    static constexpr int kMinBlockW = 96, kMaxBlockW = 384;

    HostEngine& engine;
    int selected = 0;

    // drag-to-reorder state
    int dragEntry = -1;
    bool dragging = false;
    int dropIndex = -1;
    int pressLane = -1;   // lane under the initial press: click = mute toggle

    juce::TextEditor nameEditor;   // in-place pattern rename, hidden when idle
    int renameEntry = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementView)
};
