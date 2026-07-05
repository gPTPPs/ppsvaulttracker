#include "ui/CcLanePanel.h"
#include "ui/RVLookAndFeel.h"
#include "ui/TrackStyle.h"

CcLanePanel::CcLanePanel (HostEngine& e, PatternEditor& ed) : engine (e), editor (ed)
{
    setOpaque (true);

    for (int i = 0; i < kNumChoices; ++i)
    {
        auto& b = choiceBtns[i];
        b.setButtonText (i < FxCmd::kNumSlots ? juce::String::charToString ((juce::juce_wchar) ('A' + i))
                                              : juce::String ("P"));
        b.setColour (juce::TextButton::buttonOnColourId, RV::cyan);
        b.setColour (juce::TextButton::textColourOnId, RV::cyan);
        b.setWantsKeyboardFocus (false);
        b.onClick = [this, i] { choice = i; refreshSlotButtons(); repaint(); };
        addAndMakeVisible (b);
    }
    refreshSlotButtons();
    startTimerHz (30);
}

void CcLanePanel::refreshSlotButtons()
{
    for (int i = 0; i < kNumChoices; ++i)
        choiceBtns[i].setToggleState (i == choice, juce::dontSendNotification);
}

void CcLanePanel::setTrack (int t)
{
    const int clamped = juce::jlimit (0, Song::kCcTracks - 1, t);
    if (clamped != track)
    {
        track = clamped;
        repaint();
    }
}

uint8_t CcLanePanel::command() const
{
    return choice < FxCmd::kNumSlots ? (uint8_t) (FxCmd::kSlotA + choice)
                                     : (uint8_t) FxCmd::kPitchBend;
}

juce::String CcLanePanel::commandLabel() const
{
    if (choice >= FxCmd::kNumSlots)
        return "P - Pitch bend (40 = centre)";
    const int cc = (int) engine.getSong().ccForSlot (track, choice);
    const char* known = FxCmd::ccName (cc);
    return juce::String::charToString ((juce::juce_wchar) ('A' + choice))
         + " - CC" + juce::String (cc) + (known != nullptr ? " " + juce::String (known) : juce::String());
}

juce::Rectangle<int> CcLanePanel::plotArea() const
{
    return getLocalBounds().reduced (8, 4).withTrimmedTop (kHeaderH);
}

void CcLanePanel::resized()
{
    auto header = getLocalBounds().reduced (8, 4).removeFromTop (kHeaderH).reduced (0, 1);
    for (int i = kNumChoices - 1; i >= 0; --i)
    {
        choiceBtns[i].setBounds (header.removeFromRight (26));
        header.removeFromRight (3);
    }
}

void CcLanePanel::paint (juce::Graphics& g)
{
    g.fillAll (RV::bg);
    g.setColour (RV::panelLine);
    g.fillRect (0, 0, getWidth(), 1);

    const auto& song = engine.getSong();
    auto* p = pattern();
    const auto plot = plotArea();

    // header: track + resolved command
    g.setColour (TrackStyle::colourOr (song, track, RV::cyan));
    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    auto header = getLocalBounds().reduced (8, 4).removeFromTop (kHeaderH);
    g.drawText (TrackStyle::nameOr (song, track, "CH " + juce::String (track + 1)),
                header.removeFromLeft (150), juce::Justification::centredLeft);
    g.setColour (RV::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (commandLabel(), header, juce::Justification::centredLeft);

    if (p == nullptr || plot.getHeight() < 8)
        return;

    g.setColour (RV::gridBg);
    g.fillRect (plot);

    const int rows = p->getNumRows();
    const float rowW = (float) plot.getWidth() / (float) rows;
    const auto accent = TrackStyle::colourOr (song, track, RV::cyan);
    const uint8_t cmd = command();

    // beat gridlines (every 4 rows, stronger every 16) — matches the grid
    for (int r = 0; r < rows; r += 4)
    {
        g.setColour (r % 16 == 0 ? RV::panelLine : RV::gridSep);
        g.fillRect ((int) ((float) plot.getX() + rowW * (float) r), plot.getY(), 1, plot.getHeight());
    }

    // pitch bend: centre reference line (value 0x40)
    if (cmd == FxCmd::kPitchBend)
    {
        const float cy = (float) plot.getBottom() - (float) plot.getHeight() * (64.0f / 127.0f);
        g.setColour (RV::textDim.withAlpha (0.35f));
        g.fillRect ((float) plot.getX(), cy, (float) plot.getWidth(), 1.0f);
    }

    for (int r = 0; r < rows; ++r)
    {
        const Cell& c = p->at (r, track);
        const float x = (float) plot.getX() + rowW * (float) r;
        const float w = juce::jmax (1.0f, rowW - 1.0f);

        if (c.effect == cmd)
        {
            const float h = juce::jmax (2.0f, (float) plot.getHeight() * (float) c.effectValue / 127.0f);
            g.setColour (accent.withAlpha (0.85f));
            g.fillRect (x, (float) plot.getBottom() - h, w, h);
        }
        else if (c.effect != FxCmd::kNone)
        {
            // cell owned by another command: shown, never overwritten
            g.setColour (RV::textDim.withAlpha (0.7f));
            g.fillRect (x, (float) plot.getBottom() - 4.0f, w, 4.0f);
        }
    }

    // grid cursor row (cyan) + playhead (magenta) markers
    const int cursorRow = editor.getCursorRow();
    if (cursorRow >= 0 && cursorRow < rows)
    {
        g.setColour (RV::cyan.withAlpha (0.35f));
        g.fillRect ((int) ((float) plot.getX() + rowW * (float) cursorRow), plot.getY(), 1, plot.getHeight());
    }
    auto& seq = engine.getSequencer();
    if (seq.isPlaying() && seq.getUiPatternIndex() == seq.getEditPatternIndex())
    {
        const int playRow = seq.getUiRow();
        if (playRow >= 0 && playRow < rows)
        {
            g.setColour (RV::magenta.withAlpha (0.8f));
            g.fillRect ((int) ((float) plot.getX() + rowW * (float) playRow), plot.getY(), 2, plot.getHeight());
        }
    }
}

// ---------------------------------------------------------------- editing

void CcLanePanel::writeRow (int row, int value)
{
    auto* p = pattern();
    if (p == nullptr || row < 0 || row >= p->getNumRows() || track >= p->getNumChannels())
        return;

    Cell& c = p->at (row, track);
    const uint8_t cmd = command();

    if (erasing)
    {
        if (c.effect == cmd)
        {
            c.effect = FxCmd::kNone;
            c.effectValue = 0;
        }
        return;
    }

    // never overwrite a cell owned by another command
    if (c.effect != FxCmd::kNone && c.effect != cmd)
        return;
    c.effect = cmd;
    c.effectValue = (uint8_t) juce::jlimit (0, 127, value);
}

void CcLanePanel::applyStroke (const juce::MouseEvent& e)
{
    auto* p = pattern();
    if (p == nullptr)
        return;

    const auto plot = plotArea();
    const int rows = p->getNumRows();
    const int row = juce::jlimit (0, rows - 1,
        (int) ((float) (e.x - plot.getX()) / juce::jmax (0.001f, (float) plot.getWidth() / (float) rows)));
    const int value = juce::jlimit (0, 127,
        (int) (127.0f * (float) (plot.getBottom() - e.y) / juce::jmax (1.0f, (float) plot.getHeight())));

    if (lastRow >= 0 && std::abs (row - lastRow) > 1)
    {
        // fast stroke skipped rows: interpolate linearly between samples
        const int dir = row > lastRow ? 1 : -1;
        for (int r = lastRow + dir; r != row; r += dir)
            writeRow (r, lastValue + (value - lastValue) * (r - lastRow) / (row - lastRow));
    }
    writeRow (row, value);

    // audition while drawing: the plugin hears the value immediately (which
    // also makes the plugin's MIDI Learn catch the CC without playing back)
    if (! erasing)
    {
        if (command() == FxCmd::kPitchBend)
            engine.sendLivePitchBend (track, value);
        else
            engine.sendLiveController (track, (int) engine.getSong().ccForSlot (track, choice), value);
    }

    lastRow = row;
    lastValue = value;
    repaint();
}

void CcLanePanel::mouseDown (const juce::MouseEvent& e)
{
    if (! plotArea().contains (e.getPosition()) || pattern() == nullptr)
        return;
    stroking = true;
    erasing = e.mods.isPopupMenu();
    lastRow = -1;
    editor.beginExternalEdit (track);   // one stroke = one grid-undo transaction
    applyStroke (e);
}

void CcLanePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (stroking)
        applyStroke (e);
}

void CcLanePanel::mouseUp (const juce::MouseEvent&)
{
    if (! stroking)
        return;
    stroking = false;
    lastRow = -1;
    editor.commitExternalEdit();
}
