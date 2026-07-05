#include "ui/ArrangementView.h"
#include "ui/RVLookAndFeel.h"
#include "ui/TrackStyle.h"
#include "io/ProjectIO.h"

ArrangementView::ArrangementView (HostEngine& e) : engine (e)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    nameEditor.setInputRestrictions (Song::kMaxTrackNameLen);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.onReturnKey = [this] { endRename (true); };
    nameEditor.onEscapeKey = [this] { endRename (false); };
    nameEditor.onFocusLost = [this] { endRename (true); };
    addChildComponent (nameEditor);

    startTimerHz (30);
}

// ---------------------------------------------------------------- layout

juce::Array<ArrangementView::Block> ArrangementView::computeBlocks() const
{
    juce::Array<Block> blocks;
    const auto& song = engine.getSong();
    int x = kMargin;
    for (int i = 0; i < song.orderLen; ++i)
    {
        const int patIdx = song.order[i];
        const auto* p = song.getPattern (patIdx);
        const int rows = p != nullptr ? p->getNumRows() : 64;
        const int w = juce::jlimit (kMinBlockW, kMaxBlockW, (int) ((float) rows * 1.5f));
        blocks.add ({ x, w, patIdx });
        x += w + kGap;
    }
    return blocks;
}

int ArrangementView::currentIdealWidth() const
{
    const auto blocks = computeBlocks();
    return blocks.isEmpty() ? 2 * kMargin
                            : blocks.getLast().x + blocks.getLast().w + kMargin;
}

int ArrangementView::entryAt (int x) const
{
    const auto blocks = computeBlocks();
    for (int i = 0; i < blocks.size(); ++i)
        if (x >= blocks[i].x && x < blocks[i].x + blocks[i].w)
            return i;
    return -1;
}

int ArrangementView::insertionIndexAt (int x) const
{
    const auto blocks = computeBlocks();
    for (int i = 0; i < blocks.size(); ++i)
        if (x < blocks[i].x + blocks[i].w / 2)
            return i;
    return blocks.size();
}

juce::String ArrangementView::patternLabel (const Song& song, int patIdx)
{
    const auto* p = song.getPattern (patIdx);
    if (p != nullptr && ! p->name.empty())
        return juce::String::fromUTF8 (p->name.c_str());
    return "P" + juce::String (patIdx);
}

// ---------------------------------------------------------------- painting

void ArrangementView::paint (juce::Graphics& g)
{
    g.fillAll (RV::gridBg);

    const auto& song = engine.getSong();
    auto& seq = engine.getSequencer();
    const auto blocks = computeBlocks();
    const bool songPlaying = seq.isPlaying() && seq.isSongMode();
    const int playPos = seq.getUiOrderPos();
    const int tracks = song.getNumChannels();

    // fixed-height lanes, compact blocks: the block hugs its content instead
    // of stretching to the view height
    constexpr int kLaneH = 14;
    const int blockH = juce::jmin (getHeight() - 2 * kMargin,
                                   kHeaderH + 8 + tracks * kLaneH + 10);

    for (int i = 0; i < blocks.size(); ++i)
    {
        const auto& b = blocks[i];
        const auto* p = song.getPattern (b.patIdx);
        if (p == nullptr)
            continue;

        const juce::Rectangle<int> r (b.x, kMargin, b.w, blockH);
        const bool isSel = i == selected;
        const bool isPlayingHere = songPlaying && playPos == i;

        g.setColour (RV::panel.brighter (isSel ? 0.10f : 0.0f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (isPlayingHere ? RV::magenta : isSel ? RV::cyan : RV::panelLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);

        // header: position + pattern number (dim), then the display name
        auto header = r.reduced (8, 0).removeFromTop (kHeaderH);
        g.setColour (RV::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (i).paddedLeft ('0', 2) + "  P" + juce::String (b.patIdx)
                        + "  (" + juce::String (p->getNumRows()) + ")",
                    header.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (RV::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
        g.drawText (patternLabel (song, b.patIdx), header, juce::Justification::centredLeft);

        // per-track mini-map: note density, 4-row segments, track colour
        const auto body = r.reduced (8).withTrimmedTop (kHeaderH);
        if (tracks > 0 && body.getHeight() > 0)
        {
            const int rows = p->getNumRows();
            const int segs = juce::jmax (1, (rows + 3) / 4);
            const float segW = (float) body.getWidth() / (float) segs;

            for (int t = 0; t < tracks; ++t)
            {
                const auto accent = TrackStyle::colourOr (song, t, RV::cyan);
                const float y = (float) body.getY() + (float) (kLaneH * t);
                const float h = (float) kLaneH - 3.0f;

                g.setColour (accent.withAlpha (0.07f));   // faint lane base
                g.fillRect ((float) body.getX(), y, (float) body.getWidth(), h);

                for (int s = 0; s < segs; ++s)
                {
                    int notes = 0;
                    const int r0 = s * 4, r1 = juce::jmin (rows, r0 + 4);
                    for (int row = r0; row < r1; ++row)
                        if (p->at (row, t).hasNote())
                            ++notes;
                    if (notes == 0)
                        continue;
                    g.setColour (accent.withAlpha (0.30f + 0.65f * juce::jmin (1.0f, (float) notes / 4.0f)));
                    g.fillRect ((float) body.getX() + segW * (float) s, y, juce::jmax (2.0f, segW - 1.0f), h);
                }
            }
        }

        // playhead inside the playing block
        if (isPlayingHere)
        {
            const int rowNow = seq.getUiRow();
            if (rowNow >= 0 && p->getNumRows() > 0)
            {
                const float px = (float) body.getX()
                               + (float) body.getWidth() * (float) rowNow / (float) p->getNumRows();
                g.setColour (RV::magenta.withAlpha (0.8f));
                g.fillRect (px, (float) r.getY() + 2.0f, 2.0f, (float) r.getHeight() - 4.0f);
            }
        }
    }

    // drop caret while dragging
    if (dragging && dropIndex >= 0)
    {
        const int cx = dropIndex < blocks.size()
                           ? blocks[dropIndex].x - kGap / 2
                           : (blocks.isEmpty() ? kMargin
                                               : blocks.getLast().x + blocks.getLast().w + kGap / 2);
        g.setColour (RV::cyan);
        g.fillRect (cx - 1, kMargin, 3, getHeight() - 2 * kMargin);
    }
}

void ArrangementView::timerCallback()
{
    const auto& song = engine.getSong();
    selected = juce::jlimit (0, juce::jmax (0, song.orderLen - 1), selected);

    const int w = juce::jmax (currentIdealWidth(), getParentWidth());
    if (getWidth() != w)
        setSize (w, getHeight());
    repaint();
}

// ---------------------------------------------------------------- editing

void ArrangementView::applyNewOrder (const juce::Array<int>& entries, int newSelected)
{
    engine.applyOrder (entries.data(), entries.size());
    selected = juce::jlimit (0, juce::jmax (0, entries.size() - 1), newSelected);
    repaint();
}

void ArrangementView::insertEntryAfter (int entry, int patIdx)
{
    const auto& song = engine.getSong();
    if (song.orderLen >= Song::kMaxOrder)
        return;
    juce::Array<int> entries;
    for (int i = 0; i < song.orderLen; ++i)
        entries.add (song.order[i]);
    entries.insert (entry + 1, patIdx);
    applyNewOrder (entries, entry + 1);
}

void ArrangementView::removeEntry (int entry)
{
    const auto& song = engine.getSong();
    if (song.orderLen <= 1 || entry < 0 || entry >= song.orderLen)
        return;
    juce::Array<int> entries;
    for (int i = 0; i < song.orderLen; ++i)
        if (i != entry)
            entries.add (song.order[i]);
    applyNewOrder (entries, entry);
}

// ---------------------------------------------------------------- mouse

void ArrangementView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    endRename (true);
    const int entry = entryAt (e.x);

    if (e.mods.isPopupMenu())
    {
        if (entry >= 0)
        {
            selected = entry;
            repaint();
            showContextMenu (entry);
        }
        return;
    }

    if (entry >= 0)
    {
        selected = entry;
        dragEntry = entry;
        dragging = false;
        engine.getSequencer().setEditPatternIndex (engine.getSong().order[entry]);
        repaint();
    }
}

void ArrangementView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEntry < 0)
        return;
    if (! dragging && e.getDistanceFromDragStart() > 6)
        dragging = true;
    if (dragging)
    {
        dropIndex = insertionIndexAt (e.x);
        repaint();
    }
}

void ArrangementView::mouseUp (const juce::MouseEvent& e)
{
    if (dragging && dropIndex >= 0 && dragEntry >= 0)
    {
        const auto& song = engine.getSong();
        juce::Array<int> entries;
        for (int i = 0; i < song.orderLen; ++i)
            entries.add (song.order[i]);

        if (e.mods.isCtrlDown())
        {
            // duplicate the dragged entry at the drop point
            if (entries.size() < Song::kMaxOrder)
            {
                entries.insert (dropIndex, entries[dragEntry]);
                applyNewOrder (entries, dropIndex);
            }
        }
        else
        {
            const int value = entries[dragEntry];
            entries.remove (dragEntry);
            const int target = dropIndex > dragEntry ? dropIndex - 1 : dropIndex;
            entries.insert (target, value);
            applyNewOrder (entries, target);
        }
    }
    dragEntry = -1;
    dragging = false;
    dropIndex = -1;
    repaint();
}

void ArrangementView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int entry = entryAt (e.x);
    if (entry < 0 || e.mods.isPopupMenu())
        return;
    selected = entry;
    engine.getSequencer().setEditPatternIndex (engine.getSong().order[entry]);
    if (onOpenPattern)
        onOpenPattern();
}

bool ArrangementView::keyPressed (const juce::KeyPress& kp)
{
    const auto code = kp.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        removeEntry (selected);
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        selected = juce::jlimit (0, juce::jmax (0, engine.getSong().orderLen - 1),
                                 selected + (code == juce::KeyPress::leftKey ? -1 : 1));
        engine.getSequencer().setEditPatternIndex (engine.getSong().order[selected]);
        repaint();
        return true;
    }
    return false;   // F12 & friends bubble up to MainComponent
}

// ---------------------------------------------------------------- menu / rename

void ArrangementView::showContextMenu (int entry)
{
    const auto& song = engine.getSong();

    juce::PopupMenu insertMenu;
    for (int pi = 0; pi < song.getNumPatterns(); ++pi)
        insertMenu.addItem (1000 + pi, "P" + juce::String (pi)
                                           + (song.getPattern (pi)->name.empty()
                                                  ? juce::String()
                                                  : " - " + patternLabel (song, pi)));
    insertMenu.addSeparator();
    insertMenu.addItem (999, "New pattern");

    juce::PopupMenu m;
    m.addSubMenu ("Insert after", insertMenu, song.orderLen < Song::kMaxOrder);
    m.addItem (1, "Duplicate entry", song.orderLen < Song::kMaxOrder);
    m.addItem (2, "Remove entry", song.orderLen > 1);
    m.addSeparator();
    m.addItem (3, "Rename pattern...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                         { juce::Desktop::getMousePosition().x, juce::Desktop::getMousePosition().y, 1, 1 }),
        [this, entry] (int r)
        {
            if (r == 0)
                return;
            const auto& s = engine.getSong();
            if (r == 1)       insertEntryAfter (entry, s.order[entry]);
            else if (r == 2)  removeEntry (entry);
            else if (r == 3)  beginRename (entry);
            else if (r == 999)
            {
                const int np = engine.addPattern();
                if (np >= 0)
                    insertEntryAfter (entry, np);
            }
            else if (r >= 1000)
                insertEntryAfter (entry, r - 1000);
        });
}

void ArrangementView::beginRename (int entry)
{
    const auto blocks = computeBlocks();
    if (entry < 0 || entry >= blocks.size())
        return;
    renameEntry = entry;
    const auto& b = blocks[entry];
    nameEditor.setText (engine.getSong().getPattern (b.patIdx)->name, juce::dontSendNotification);
    nameEditor.setBounds (b.x + 6, kMargin + 16, b.w - 12, 20);
    nameEditor.setVisible (true);
    nameEditor.grabKeyboardFocus();
}

void ArrangementView::endRename (bool commit)
{
    if (renameEntry < 0)
        return;   // re-entry guard: hiding the editor fires onFocusLost
    const int entry = renameEntry;
    renameEntry = -1;

    const auto& song = engine.getSong();
    if (commit && entry < song.orderLen)
        if (auto* p = song.getPattern (song.order[entry]))
            p->name = ProjectIO::sanitizeTrackName (nameEditor.getText()).toStdString();

    nameEditor.setVisible (false);
    grabKeyboardFocus();
    repaint();
}
