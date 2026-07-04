#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "model/PatternOps.h"
#include "model/PatternUndo.h"

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
    int  editStep = 1;           // rows to advance after an entry (0..16)
    int  editOctave = 3;         // low piano row octave (1..7)

    int getCursorChannel() const { return cursorChannel; }
    std::function<void (int)> onCursorChannelChanged;   // live input follows the cursor

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override;
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
    void deleteAtCursor();
    void previewNote (int keyCode, int midiNote);
    PatternOps::Selection currentRegion() const;   // selection if active, else cursor cell
    int  noteForChar (juce::juce_wchar c) const;   // -1 if not a note key

    // geometry
    static constexpr int kRowH = 18, kRowNumW = 42, kChanGap = 10;
    static constexpr int kSubX[numSubCols]     = { 0, 48, 58, 74, 84, 100, 110, 120 };
    static constexpr int kSubW[numSubCols]     = { 44, 10, 10, 10, 10, 10, 10, 10 };
    static constexpr int kChanW = 134;
    int headerH() const { return 22; }

    HostEngine& engine;
    PatternUndo undo;
    PatternOps::Clipboard clipboard;

    int cursorRow = 0, cursorChannel = 0, cursorSub = subNote;
    int topRow = 0;
    bool hasSelection = false;
    int anchorRow = 0, anchorChannel = 0;

    struct HeldKey { int keyCode; int midiNote; int midiChannel; };
    juce::Array<HeldKey> heldKeys;
    bool focusGrabbed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternEditor)
};
