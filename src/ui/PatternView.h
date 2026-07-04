#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"

// Phase-2 read-only pattern display with a following playhead.
// The full FT2-style editor (keyboard navigation, hex entry, undo) is phase 3.
class PatternView : public juce::Component,
                    private juce::Timer
{
public:
    explicit PatternView (HostEngine& e) : engine (e)
    {
        setOpaque (true);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff16181c));

        const auto* pattern = engine.getSequencer().getPattern();
        if (pattern == nullptr)
            return;

        const int playRow = engine.getSequencer().getUiRow();
        const int rowH = 18;
        const int visible = juce::jmax (1, getHeight() / rowH);
        const juce::Font mono (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));
        g.setFont (mono);

        // centre the view on the playhead while playing, top when stopped
        const int centreRow = playRow >= 0 ? playRow : visible / 2;
        int first = centreRow - visible / 2;

        for (int line = 0; line <= visible; ++line)
        {
            const int row = first + line;
            if (row < 0 || row >= pattern->getNumRows())
                continue;

            const int y = line * rowH;
            const bool isPlayhead = row == playRow;

            if (isPlayhead)
            {
                g.setColour (juce::Colour (0xff2d3644));
                g.fillRect (0, y, getWidth(), rowH);
            }
            else if (row % 16 == 0)
            {
                g.setColour (juce::Colour (0xff1c2026));
                g.fillRect (0, y, getWidth(), rowH);
            }

            const auto& c = pattern->at (row, 0);
            juce::String noteTxt = "---";
            if (c.isNoteOff())     noteTxt = "===";
            else if (c.hasNote())  noteTxt = noteName (c.note);

            const juce::String volTxt = c.hasNote()
                ? juce::String::toHexString ((int) c.volume).paddedLeft ('0', 2).toUpperCase()
                : juce::String ("..");

            g.setColour (row % 4 == 0 ? juce::Colour (0xff9aa4b2) : juce::Colour (0xff5f6976));
            g.drawText (juce::String::toHexString (row).paddedLeft ('0', 2).toUpperCase(),
                        8, y, 30, rowH, juce::Justification::centredLeft);

            g.setColour (isPlayhead ? juce::Colour (0xffb07cf0)
                                    : (c.hasNote() ? juce::Colour (0xffd8dee6) : juce::Colour (0xff4a525e)));
            g.drawText (noteTxt + "  " + volTxt, 52, y, getWidth() - 60, rowH,
                        juce::Justification::centredLeft);
        }
    }

private:
    static juce::String noteName (uint8_t midiNote)
    {
        static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-",
                                       "F#", "G-", "G#", "A-", "A#", "B-" };
        return juce::String (names[midiNote % 12]) + juce::String (midiNote / 12 - 1);
    }

    void timerCallback() override { repaint(); }

    HostEngine& engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternView)
};
