#include "ui/PatternEditor.h"
#include "ui/RVLookAndFeel.h"
#include "ui/TrackStyle.h"
#include "io/ProjectIO.h"

namespace
{
    struct KeyNote { juce::juce_wchar ch; int semitone; };   // relative to the low row's C

    // FT2 piano rows. Lower row = editOctave, upper row = editOctave + 1.
    const KeyNote kQwerty[] = {
        { 'z', 0 },  { 's', 1 },  { 'x', 2 },  { 'd', 3 },  { 'c', 4 },  { 'v', 5 },
        { 'g', 6 },  { 'b', 7 },  { 'h', 8 },  { 'n', 9 },  { 'j', 10 }, { 'm', 11 },
        { 'q', 12 }, { '2', 13 }, { 'w', 14 }, { '3', 15 }, { 'e', 16 }, { 'r', 17 },
        { '5', 18 }, { 't', 19 }, { '6', 20 }, { 'y', 21 }, { '7', 22 }, { 'u', 23 },
        { 'i', 24 }, { '9', 25 }, { 'o', 26 }, { '0', 27 }, { 'p', 28 },
    };

    // BE/FR AZERTY equivalents of the same physical keys (unshifted).
    const KeyNote kAzerty[] = {
        { 'w', 0 },    { 's', 1 },    { 'x', 2 },    { 'd', 3 },    { 'c', 4 },  { 'v', 5 },
        { 'g', 6 },    { 'b', 7 },    { 'h', 8 },    { 'n', 9 },    { 'j', 10 }, { ',', 11 },
        { 'a', 12 },   { 0x00E9, 13 } /* é */,       { 'z', 14 },   { '"', 15 },
        { 'e', 16 },   { 'r', 17 },   { '(', 18 },   { 't', 19 },   { 0x00A7, 20 } /* § */,
        { 'y', 21 },   { 0x00E8, 22 } /* è */,       { 'u', 23 },   { 'i', 24 },
        { 0x00E7, 25 } /* ç */,       { 'o', 26 },   { 0x00E0, 27 } /* à */,     { 'p', 28 },
    };

    juce::String noteName (uint8_t midiNote)
    {
        static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-",
                                       "F#", "G-", "G#", "A-", "A#", "B-" };
        return juce::String (names[midiNote % 12]) + juce::String (midiNote / 12 - 1);
    }

    juce::String hex2 (int v) { return juce::String::toHexString (v).paddedLeft ('0', 2).toUpperCase(); }
}

PatternEditor::PatternEditor (HostEngine& e) : engine (e)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    nameEditor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                       13.0f, juce::Font::plain)));
    nameEditor.setInputRestrictions (Song::kMaxTrackNameLen);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.onReturnKey  = [this] { endNameEdit (true); };
    nameEditor.onEscapeKey  = [this] { endNameEdit (false); };
    nameEditor.onFocusLost  = [this] { endNameEdit (true); };
    addChildComponent (nameEditor);

    startTimerHz (30);
}

// ---------------------------------------------------------------- painting

int PatternEditor::visibleChannelCount() const
{
    return juce::jmax (1, (getWidth() - kRowNumW) / (kChanW + kChanGap));
}

void PatternEditor::setFirstChannel (int fc)
{
    if (auto* p = pattern())
        fc = juce::jlimit (0, juce::jmax (0, p->getNumChannels() - visibleChannelCount()), fc);
    if (fc != firstChannel)
    {
        endNameEdit (true);   // don't leave the rename editor over the wrong column
        firstChannel = fc;
        if (onViewChanged)
            onViewChanged();
        repaint();
    }
}

void PatternEditor::paint (juce::Graphics& g)
{
    g.fillAll (RV::gridBg);

    auto* p = pattern();
    if (p == nullptr)
        return;

    const int playRow = engine.getSequencer().getUiRow();
    const int visible = juce::jmax (1, (getHeight() - headerH()) / kRowH);
    const int visCh   = visibleChannelCount();
    const int lastCh  = juce::jmin (p->getNumChannels(), firstChannel + visCh);
    const juce::Font mono (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));

    // ---- channel headers ----
    const auto& song = engine.getSong();
    g.setFont (mono.withHeight (13.0f));
    for (int ch = firstChannel; ch < lastCh; ++ch)
    {
        const int x = kRowNumW + (ch - firstChannel) * (kChanW + kChanGap);
        const auto accent = TrackStyle::colourOr (song, ch, RV::cyan);

        g.setColour (ch == cursorChannel ? accent
                     : TrackStyle::hasColour (song, ch) ? accent.withAlpha (0.7f)
                                                        : RV::textDim);
        g.drawText (TrackStyle::nameOr (song, ch, "CH " + hex2 (ch + 1)),
                    x, 0, kChanW, headerH(), juce::Justification::centredLeft);

        if (TrackStyle::hasColour (song, ch))
        {
            g.setColour (accent.withAlpha (ch == cursorChannel ? 0.9f : 0.5f));
            g.fillRect (x, headerH() - 4, kChanW, 2);
        }
    }
    if (firstChannel > 0)
    {
        g.setColour (RV::cyan);
        g.drawText ("<", 2, 0, 16, headerH(), juce::Justification::centred);
    }
    if (lastCh < p->getNumChannels())
    {
        g.setColour (RV::cyan);
        g.drawText (">", getWidth() - 16, 0, 14, headerH(), juce::Justification::centred);
    }
    g.setColour (RV::panelLine);
    g.fillRect (0, headerH() - 2, getWidth(), 1);

    g.setFont (mono);

    const auto sel = hasSelection
        ? PatternOps::Selection::clipped ({ anchorRow, cursorRow, anchorChannel, cursorChannel }, *p)
        : PatternOps::Selection { -1, -1, -1, -1 };

    for (int line = 0; line < visible; ++line)
    {
        const int row = topRow + line;
        if (row < 0 || row >= p->getNumRows())
            continue;

        const int y = headerH() + line * kRowH;
        const bool isPlayhead = row == playRow;

        if (isPlayhead)
        {
            g.setColour (RV::magenta.withAlpha (0.16f));
            g.fillRect (0, y, getWidth(), kRowH);
        }
        else if (row % 16 == 0)
        {
            g.setColour (RV::gridBar);
            g.fillRect (0, y, getWidth(), kRowH);
        }

        // row number
        g.setColour (row % 4 == 0 ? RV::cyan.withAlpha (0.75f) : RV::textDim);
        g.drawText (hex2 (row), 8, y, kRowNumW - 10, kRowH, juce::Justification::centredLeft);

        for (int ch = firstChannel; ch < lastCh; ++ch)
        {
            const int x0 = kRowNumW + (ch - firstChannel) * (kChanW + kChanGap);
            const auto& c = p->at (row, ch);

            // selection overlay
            if (hasSelection
                && row >= sel.startRow && row <= sel.endRow
                && ch >= sel.startChannel && ch <= sel.endChannel)
            {
                g.setColour (RV::cyan.withAlpha (0.18f));
                g.fillRect (x0 - 2, y, kChanW + 4, kRowH);
            }

            // cursor
            if (row == cursorRow && ch == cursorChannel)
            {
                g.setColour (juce::Colour (0xff0d4356));
                g.fillRect (x0 + kSubX[cursorSub] - 2, y, kSubW[cursorSub] + 4, kRowH);
                g.setColour (RV::cyan);
                g.drawRect (x0 + kSubX[cursorSub] - 2, y, kSubW[cursorSub] + 4, kRowH, 1);
            }

            juce::String noteTxt = "---";
            if (c.isNoteOff())     noteTxt = "===";
            else if (c.hasNote())  noteTxt = noteName (c.note);

            const bool empty = ! c.hasNote() && ! c.isNoteOff();
            const auto dim    = RV::gridEmpty;
            const auto normal = RV::text;

            g.setColour (isPlayhead ? RV::magenta : (empty ? dim : normal));
            g.drawText (noteTxt, x0 + kSubX[subNote], y, kSubW[subNote], kRowH, juce::Justification::centredLeft);

            g.setColour (c.instrument != 0 ? normal : dim);
            g.drawText (c.instrument != 0 ? hex2 (c.instrument) : juce::String (".."),
                        x0 + kSubX[subInstrHi], y, 20, kRowH, juce::Justification::centredLeft);

            g.setColour (empty ? dim : normal);
            g.drawText (empty ? juce::String ("..") : hex2 (c.volume),
                        x0 + kSubX[subVolHi], y, 20, kRowH, juce::Justification::centredLeft);

            const bool hasFx = c.effect != FxCmd::kNone || c.effectValue != 0;
            const char fxLetter = FxCmd::letter (c.effect);
            g.setColour (hasFx ? normal : dim);
            g.drawText (hasFx ? (fxLetter != 0 ? juce::String::charToString (fxLetter)
                                               : juce::String (".")) + hex2 (c.effectValue)
                              : juce::String ("..."),
                        x0 + kSubX[subFx], y, 30, kRowH, juce::Justification::centredLeft);
        }
    }
}

// ---------------------------------------------------------------- timer

void PatternEditor::timerCallback()
{
    if (! focusGrabbed && isShowing())
    {
        focusGrabbed = true;
        grabKeyboardFocus();
    }

    auto& seq = engine.getSequencer();
    const int playRow = seq.getUiRow();
    if (followPlayhead && playRow >= 0)
    {
        // FT2 behaviour: in song mode, the edited/displayed pattern follows
        // the one the order list is currently playing
        if (seq.isSongMode() && seq.getUiPatternIndex() != seq.getEditPatternIndex())
            seq.setEditPatternIndex (seq.getUiPatternIndex());

        const int visible = juce::jmax (1, (getHeight() - headerH()) / kRowH);
        topRow = juce::jlimit (0, juce::jmax (0, pattern()->getNumRows() - visible),
                               playRow - visible / 2);
    }
    repaint();
}

// ---------------------------------------------------------------- keyboard

bool PatternEditor::keyPressed (const juce::KeyPress& kp)
{
    auto* p = pattern();
    if (p == nullptr)
        return false;

    const auto mods = kp.getModifiers();
    const int code = kp.getKeyCode();

    // ---- global edit shortcuts ----
    // NB: compare key CODES here — with Ctrl held, getTextCharacter() yields
    // control characters on Windows, not letters.
    if (mods.isCtrlDown())
    {
        const auto c = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code);
        if (c == 'z') { undo.undo (*p);  repaint(); return true; }
        if (c == 'y') { undo.redo (*p);  repaint(); return true; }
        if (c == 'c') { clipboard = PatternOps::copy (*p, currentRegion()); return true; }
        if (c == 'x')
        {
            const auto region = currentRegion();
            clipboard = PatternOps::copy (*p, region);
            undo.begin (*p, region);
            PatternOps::clear (*p, region);
            undo.commit (*p);
            repaint();
            return true;
        }
        if (c == 'v')
        {
            if (! clipboard.empty())
            {
                const PatternOps::Selection target { cursorRow, cursorRow + clipboard.rows - 1,
                                                     cursorChannel, cursorChannel + clipboard.channels - 1 };
                undo.begin (*p, target);
                PatternOps::paste (*p, clipboard, cursorRow, cursorChannel);
                undo.commit (*p);
                repaint();
            }
            return true;
        }
        if (c == 'l')
        {
            if (hasSelection)
            {
                const auto region = currentRegion();
                undo.begin (*p, region);
                PatternOps::interpolateVolume (*p, region);
                undo.commit (*p);
                repaint();
            }
            return true;
        }
    }

    // ---- transpose: Shift+F1/F2 = ±1 semitone, Ctrl+F1/F2 = ±12 (both keymaps) ----
    if (code == juce::KeyPress::F1Key || code == juce::KeyPress::F2Key)
        if (mods.isShiftDown() || mods.isCtrlDown())
        {
            const int dir = code == juce::KeyPress::F2Key ? 1 : -1;
            const auto region = currentRegion();
            undo.begin (*p, region);
            PatternOps::transpose (*p, region, dir * (mods.isCtrlDown() ? 12 : 1));
            undo.commit (*p);
            repaint();
            return true;
        }

    // ---- octave: FT2 = F1/F2 down/up; ProTracker = F1..F5 select directly ----
    if (! ptKeys)
    {
        if (code == juce::KeyPress::F1Key) { editOctave = juce::jlimit (1, 7, editOctave - 1); return true; }
        if (code == juce::KeyPress::F2Key) { editOctave = juce::jlimit (1, 7, editOctave + 1); return true; }
    }
    else
    {
        for (int f = 0; f < 5; ++f)
            if (code == juce::KeyPress::F1Key + f)
            {
                editOctave = f + 1;
                return true;
            }
    }

    // ---- navigation ----
    const bool shift = mods.isShiftDown();

    // Ctrl+Shift+Left/Right = move the cursor's track (full swap with its
    // neighbour) — checked before plain arrow navigation
    if (mods.isCtrlDown() && shift
        && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey))
    {
        moveTrack (cursorChannel, code == juce::KeyPress::leftKey ? -1 : 1);
        return true;
    }

    if (code == juce::KeyPress::upKey)       { moveCursor (-1, 0, shift); return true; }
    if (code == juce::KeyPress::downKey)     { moveCursor ( 1, 0, shift); return true; }
    if (code == juce::KeyPress::leftKey)     { moveCursor (0, -1, shift); return true; }
    if (code == juce::KeyPress::rightKey)    { moveCursor (0,  1, shift); return true; }
    if (code == juce::KeyPress::pageUpKey)   { setCursorRow (cursorRow - 16, shift); return true; }
    if (code == juce::KeyPress::pageDownKey) { setCursorRow (cursorRow + 16, shift); return true; }
    if (code == juce::KeyPress::homeKey)     { setCursorRow (0, shift); return true; }
    if (code == juce::KeyPress::endKey)      { setCursorRow (p->getNumRows() - 1, shift); return true; }
    if (code == juce::KeyPress::tabKey)
    {
        const int prev = cursorChannel;
        const int d = shift ? -1 : 1;
        cursorChannel = (cursorChannel + d + p->getNumChannels()) % p->getNumChannels();
        cursorSub = subNote;
        hasSelection = false;
        ensureCursorVisible();
        notifyChannelChange (prev);
        repaint();
        return true;
    }
    if (code == juce::KeyPress::escapeKey)   { hasSelection = false; repaint(); return true; }
    if (code == juce::KeyPress::deleteKey)   { deleteAtCursor(); return true; }

    // space = play/stop (the sequencer follows the current Song/Pattern mode;
    // startPlayback arms the pre-count when recording)
    if (code == juce::KeyPress::spaceKey)
    {
        if (engine.getSequencer().isPlaying())
            engine.stopPlayback();
        else
            engine.startPlayback();
        return true;
    }

    // ---- data entry ----
    const auto ch = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) kp.getTextCharacter());

    // note-off: & (AZERTY), 1 (QWERTY), ² (BE key below Esc)
    if (cursorSub == subNote && (ch == '&' || ch == '1' || ch == 0x00B2))
    {
        enterNoteOff();
        return true;
    }

    if (cursorSub == subNote)
    {
        const int note = noteForChar (ch);
        if (note >= 0)
        {
            previewNote (code, note);
            if (recEnabled)
                enterNote (note);
            return true;
        }
    }
    else if (cursorSub == subFx)
    {
        // command letters only (A-H = CC slots, P = bend, N = delay, K = cut,
        // '.' or '0' clears) — not hex: C/D/E/F must mean slots, not digits
        const int cmd = FxCmd::fromChar ((char) ch);
        if (cmd >= 0)
        {
            enterFxCommand ((uint8_t) cmd);
            return true;
        }
    }
    else
    {
        // hex entry in instrument / volume / effect-value columns
        const int v = juce::CharacterFunctions::getHexDigitValue (ch);
        if (v >= 0)
        {
            enterHexDigit (v);
            return true;
        }
    }

    return false;
}

bool PatternEditor::keyStateChanged (bool)
{
    // release preview notes whose key went up
    for (int i = heldKeys.size(); --i >= 0;)
        if (! juce::KeyPress::isKeyCurrentlyDown (heldKeys[i].keyCode))
        {
            engine.getKeyboardState().noteOff (heldKeys[i].midiChannel, heldKeys[i].midiNote, 0.0f);
            heldKeys.remove (i);
        }
    return false;
}

void PatternEditor::notifyChannelChange (int previousChannel)
{
    if (cursorChannel != previousChannel && onCursorChannelChanged)
        onCursorChannelChanged (cursorChannel);
}

// ---------------------------------------------------------------- mouse

void PatternEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto* p = pattern();
    if (p == nullptr)
        return;

    if (e.y < headerH())
    {
        const int ch = headerChannelAt (e.x);
        if (ch >= 0 && e.mods.isPopupMenu())
            TrackStyle::showTrackMenu (engine.getSong(), ch, trackCount(),
                juce::PopupMenu::Options().withTargetScreenArea (
                    localAreaToGlobal (headerCellBounds (ch))),
                [this, ch] (int delta) { moveTrack (ch, delta); },
                [this] { repaint(); });
        return;
    }

    const int row = topRow + (e.y - headerH()) / kRowH;
    if (row < 0 || row >= p->getNumRows())
        return;

    int chAndX = e.x - kRowNumW;
    const int ch = juce::jlimit (0, p->getNumChannels() - 1,
                                 firstChannel + chAndX / (kChanW + kChanGap));
    const int xInChan = chAndX - (ch - firstChannel) * (kChanW + kChanGap);

    int sub = subNote;
    for (int s = 0; s < numSubCols; ++s)
        if (xInChan >= kSubX[s] && xInChan < kSubX[s] + kSubW[s] + 4)
            sub = s;

    if (e.mods.isShiftDown())
    {
        hasSelection = true;   // extend from anchor
    }
    else
    {
        hasSelection = false;
        anchorRow = row;
        anchorChannel = ch;
    }
    const int prev = cursorChannel;
    cursorRow = row;
    cursorChannel = ch;
    cursorSub = sub;
    notifyChannelChange (prev);
    repaint();
}

void PatternEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.y >= headerH() || e.mods.isPopupMenu())
        return;
    const int ch = headerChannelAt (e.x);
    if (ch >= 0)
        beginNameEdit (ch);
}

int PatternEditor::trackCount() const
{
    auto* p = pattern();
    return p != nullptr ? juce::jmin (p->getNumChannels(), Song::kCcTracks) : 0;
}

int PatternEditor::headerChannelAt (int x) const
{
    if (x < kRowNumW)
        return -1;
    const int ch = firstChannel + (x - kRowNumW) / (kChanW + kChanGap);
    return ch < trackCount() ? ch : -1;
}

void PatternEditor::moveTrack (int ch, int delta)
{
    const int to = ch + delta;
    if (ch < 0 || to < 0 || ch >= trackCount() || to >= trackCount())
        return;

    endNameEdit (true);
    engine.swapChannels (ch, to);   // fires onTrackLayoutChanged (mixer refresh)

    // the cursor sticks to the musical content it was on
    const int prev = cursorChannel;
    if (cursorChannel == ch)      cursorChannel = to;
    else if (cursorChannel == to) cursorChannel = ch;
    hasSelection = false;
    ensureCursorVisible();
    notifyChannelChange (prev);
    repaint();
}

juce::Rectangle<int> PatternEditor::headerCellBounds (int ch) const
{
    return { kRowNumW + (ch - firstChannel) * (kChanW + kChanGap), 0, kChanW, headerH() - 2 };
}

void PatternEditor::beginNameEdit (int ch)
{
    editingChannel = ch;
    nameEditor.setText (TrackStyle::nameOr (engine.getSong(), ch, {}), juce::dontSendNotification);
    nameEditor.setBounds (headerCellBounds (ch));
    nameEditor.setVisible (true);
    nameEditor.grabKeyboardFocus();
}

void PatternEditor::endNameEdit (bool commit)
{
    if (editingChannel < 0)
        return;   // re-entry guard: hiding the editor fires onFocusLost
    const int ch = editingChannel;
    editingChannel = -1;

    if (commit)
        engine.getSong().trackNames[ch] =
            ProjectIO::sanitizeTrackName (nameEditor.getText()).toStdString();

    nameEditor.setVisible (false);
    grabKeyboardFocus();
    repaint();
}

void PatternEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto* p = pattern();
    if (p == nullptr)
        return;

    // Shift+wheel (or a horizontal wheel) scrolls channels
    const float dx = wheel.deltaX != 0.0f ? wheel.deltaX
                   : (e.mods.isShiftDown() ? wheel.deltaY : 0.0f);
    if (dx != 0.0f)
    {
        setFirstChannel (firstChannel - (dx > 0 ? 1 : -1));
        return;
    }

    const int visible = juce::jmax (1, (getHeight() - headerH()) / kRowH);
    topRow = juce::jlimit (0, juce::jmax (0, p->getNumRows() - visible),
                           topRow - (int) (wheel.deltaY * 6.0f));
    repaint();
}

// ---------------------------------------------------------------- helpers

void PatternEditor::moveCursor (int rowDelta, int subDelta, bool extendSelection)
{
    auto* p = pattern();
    if (! extendSelection && ! hasSelection)
    {
        anchorRow = cursorRow;
        anchorChannel = cursorChannel;
    }
    if (extendSelection && ! hasSelection)
    {
        anchorRow = cursorRow;
        anchorChannel = cursorChannel;
        hasSelection = true;
    }
    if (! extendSelection)
        hasSelection = false;

    if (rowDelta != 0)
        cursorRow = (cursorRow + rowDelta + p->getNumRows()) % p->getNumRows();

    if (subDelta != 0)
    {
        const int prev = cursorChannel;
        int pos = cursorChannel * numSubCols + cursorSub + subDelta;
        const int total = p->getNumChannels() * numSubCols;
        pos = (pos + total) % total;
        cursorChannel = pos / numSubCols;
        cursorSub = pos % numSubCols;
        notifyChannelChange (prev);
    }
    ensureCursorVisible();
    repaint();
}

void PatternEditor::setCursorRow (int row, bool extendSelection)
{
    auto* p = pattern();
    if (extendSelection && ! hasSelection)
    {
        anchorRow = cursorRow;
        anchorChannel = cursorChannel;
        hasSelection = true;
    }
    if (! extendSelection)
        hasSelection = false;
    cursorRow = juce::jlimit (0, p->getNumRows() - 1, row);
    ensureCursorVisible();
    repaint();
}

void PatternEditor::ensureCursorVisible()
{
    const int visible = juce::jmax (1, (getHeight() - headerH()) / kRowH);
    if (cursorRow < topRow)
        topRow = cursorRow;
    else if (cursorRow >= topRow + visible)
        topRow = cursorRow - visible + 1;

    // horizontal: keep the cursor channel on screen
    const int visCh = visibleChannelCount();
    int fc = firstChannel;
    if (cursorChannel < fc)
        fc = cursorChannel;
    else if (cursorChannel >= fc + visCh)
        fc = cursorChannel - visCh + 1;
    setFirstChannel (fc);
}

void PatternEditor::advanceAfterEntry()
{
    if (editStep > 0)
        cursorRow = (cursorRow + editStep) % pattern()->getNumRows();
    ensureCursorVisible();
    repaint();
}

void PatternEditor::enterNote (int midiNote)
{
    auto* p = pattern();
    undo.begin (*p, { cursorRow, cursorRow, cursorChannel, cursorChannel });
    auto& c = p->at (cursorRow, cursorChannel);
    c.note = (uint8_t) juce::jlimit (1, 127, midiNote);
    c.volume = 64;
    undo.commit (*p);
    advanceAfterEntry();
}

void PatternEditor::enterNoteOff()
{
    if (! recEnabled)
        return;
    auto* p = pattern();
    undo.begin (*p, { cursorRow, cursorRow, cursorChannel, cursorChannel });
    p->at (cursorRow, cursorChannel).note = Cell::kNoteOff;
    undo.commit (*p);
    advanceAfterEntry();
}

void PatternEditor::enterHexDigit (int value)
{
    if (! recEnabled)
        return;
    auto* p = pattern();
    undo.begin (*p, { cursorRow, cursorRow, cursorChannel, cursorChannel });
    auto& c = p->at (cursorRow, cursorChannel);

    auto setNibble = [value] (uint8_t& field, bool high)
    {
        field = high ? (uint8_t) ((field & 0x0F) | (value << 4))
                     : (uint8_t) ((field & 0xF0) | value);
    };

    switch (cursorSub)
    {
        case subInstrHi: setNibble (c.instrument, true);  break;
        case subInstrLo: setNibble (c.instrument, false); break;
        case subVolHi:   setNibble (c.volume, true);  c.volume = (uint8_t) juce::jmin ((int) c.volume, 64); break;
        case subVolLo:   setNibble (c.volume, false); c.volume = (uint8_t) juce::jmin ((int) c.volume, 64); break;
        case subFxValHi: setNibble (c.effectValue, true);
                         c.effectValue = (uint8_t) juce::jmin ((int) c.effectValue, 127); break;
        case subFxValLo: setNibble (c.effectValue, false);
                         c.effectValue = (uint8_t) juce::jmin ((int) c.effectValue, 127); break;
        default: break;
    }
    undo.commit (*p);
    advanceAfterEntry();
}

void PatternEditor::enterFxCommand (uint8_t cmd)
{
    if (! recEnabled)
        return;
    auto* p = pattern();
    undo.begin (*p, { cursorRow, cursorRow, cursorChannel, cursorChannel });
    p->at (cursorRow, cursorChannel).effect = cmd;
    undo.commit (*p);
    advanceAfterEntry();
}

void PatternEditor::deleteAtCursor()
{
    auto* p = pattern();
    const auto region = currentRegion();
    undo.begin (*p, region);
    PatternOps::clear (*p, region);
    undo.commit (*p);
    if (! hasSelection)
        advanceAfterEntry();
    else
        repaint();
}

void PatternEditor::previewNote (int keyCode, int midiNote)
{
    for (auto& hk : heldKeys)
        if (hk.keyCode == keyCode)
            return;   // key auto-repeat
    const int midiCh = cursorChannel + 1;   // preview through this channel's instrument
    heldKeys.add ({ keyCode, midiNote, midiCh });
    engine.getKeyboardState().noteOn (midiCh, midiNote, 0.8f);
}

PatternOps::Selection PatternEditor::currentRegion() const
{
    if (hasSelection)
        return { anchorRow, cursorRow, anchorChannel, cursorChannel };
    return { cursorRow, cursorRow, cursorChannel, cursorChannel };
}

int PatternEditor::noteForChar (juce::juce_wchar c) const
{
    const KeyNote* map  = azertyLayout ? kAzerty : kQwerty;
    const size_t   size = azertyLayout ? std::size (kAzerty) : std::size (kQwerty);
    for (size_t i = 0; i < size; ++i)
        if (map[i].ch == c)
        {
            const int note = 12 * (editOctave + 1) + map[i].semitone;
            return note <= 127 ? note : -1;
        }
    return -1;
}
