#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "ui/PatternEditor.h"

// CC lane (v1.1 session 3): a drawable curve editor over the EXISTING effect
// column — another view of the same data, no model/format/sequencer change.
// Context = the edited pattern x the track under the grid cursor. One bar per
// row; left-drag draws (writes command+value into the cells, row-quantised,
// linearly interpolating fast strokes), right-drag erases cells holding the
// selected command. Cells holding a DIFFERENT command are never overwritten —
// they show as grey ticks. Each stroke is one transaction in the grid's undo.
class CcLanePanel : public juce::Component,
                    private juce::Timer
{
public:
    CcLanePanel (HostEngine& e, PatternEditor& ed);

    void setTrack (int t);   // follows the grid cursor

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    Pattern* pattern() const { return engine.getEditPattern(); }
    uint8_t command() const;                 // selected slot A..H or pitch bend
    juce::String commandLabel() const;       // "A - CC74 Cutoff" / "Pitch bend"
    juce::Rectangle<int> plotArea() const;
    void applyStroke (const juce::MouseEvent&);
    void writeRow (int row, int value);      // honours the never-overwrite rule
    void refreshSlotButtons();

    static constexpr int kHeaderH = 24;
    static constexpr int kNumChoices = FxCmd::kNumSlots + 1;   // A..H + pitch bend

    HostEngine& engine;
    PatternEditor& editor;
    int track = 0;
    int choice = 0;          // 0..7 = slots A..H, 8 = pitch bend

    juce::TextButton choiceBtns[kNumChoices];

    // stroke state
    bool stroking = false, erasing = false;
    int lastRow = -1, lastValue = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CcLanePanel)
};
