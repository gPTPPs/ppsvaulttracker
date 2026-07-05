#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "model/PatternOps.h"
#include "model/PatternUndo.h"
#include "ui/GridMetrics.h"

// FT2-style pattern editor (phase 3): cursor with sub-columns
// (note | instrument | volume | effect | effect value), piano-key note entry
// (AZERTY or QWERTY layout), Rec mode with step advance, block selection,
// copy/cut/paste, transpose, volume interpolation, unlimited undo/redo,
// playhead following.
class PatternEditor : public juce::Component,
                      private juce::Timer
{
public:
    explicit PatternEditor (HostEngine& e);

    // toolbar-facing state (owned here, driven by MainComponent's controls)
    bool recEnabled = false;
    bool followPlayhead = true;
    bool azertyLayout = true;    // BE/FR default
    bool ptKeys = false;         // false = FT2 keymap, true = ProTracker keymap
    int  editStep = 1;           // rows to advance after an entry (0..16)
    int  editOctave = 3;         // low piano row octave (1..7)

    int getCursorChannel() const { return cursorChannel; }
    int getFirstChannel() const  { return firstChannel; }
    std::function<void (int)> onCursorChannelChanged;   // live input follows the cursor
    std::function<void()> onViewChanged;                // mixer scroll follows the grid

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // sub-columns within a channel
    enum SubCol { subNote = 0, subInstrHi, subInstrLo, subVolHi, subVolLo,
                  subFx, subFxValHi, subFxValLo, numSubCols };

    Pattern* pattern() const { return engine.getEditPattern(); }
    void notifyChannelChange (int previousChannel);

    void timerCallback() override;
    void moveCursor (int rowDelta, int subDelta, bool extendSelection);
    void setCursorRow (int row, bool extendSelection);
    void ensureCursorVisible();
    void advanceAfterEntry();
    void enterNote (int midiNote);
    void enterNoteOff();
    void enterHexDigit (int value);
    void enterFxCommand (uint8_t cmd);
    void deleteAtCursor();
    void previewNote (int keyCode, int midiNote);
    PatternOps::Selection currentRegion() const;   // selection if active, else cursor cell
    int  noteForChar (juce::juce_wchar c) const;   // -1 if not a note key

    // track identity (header double-click = rename, right-click = colour)
    int headerChannelAt (int x) const;             // -1 outside the header cells
    juce::Rectangle<int> headerCellBounds (int ch) const;
    void beginNameEdit (int ch);
    void endNameEdit (bool commit);

    // geometry (horizontal metrics shared with the mixer)
    static constexpr int kRowH = 18;
    static constexpr int kRowNumW = GridMetrics::kRowNumW;
    static constexpr int kChanGap = GridMetrics::kChanGap;
    static constexpr int kChanW   = GridMetrics::kChanW;
    static constexpr int kSubX[numSubCols]     = { 0, 48, 58, 74, 84, 100, 110, 120 };
    static constexpr int kSubW[numSubCols]     = { 44, 10, 10, 10, 10, 10, 10, 10 };
    int headerH() const { return 22; }
    void setFirstChannel (int fc);

    HostEngine& engine;
    PatternUndo undo;
    PatternOps::Clipboard clipboard;

    int visibleChannelCount() const;

    int cursorRow = 0, cursorChannel = 0, cursorSub = subNote;
    int topRow = 0;
    int firstChannel = 0;   // leftmost visible channel (horizontal scroll)
    bool hasSelection = false;
    int anchorRow = 0, anchorChannel = 0;

    struct HeldKey { int keyCode; int midiNote; int midiChannel; };
    juce::Array<HeldKey> heldKeys;
    bool focusGrabbed = false;

    juce::TextEditor nameEditor;   // in-place track rename, hidden when idle
    int editingChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternEditor)
};
