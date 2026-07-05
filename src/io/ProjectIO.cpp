#include "io/ProjectIO.h"

juce::var ProjectIO::songToVar (const Song& s)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("numChannels", s.getNumChannels());

    juce::Array<juce::var> pats;
    for (int pi = 0; pi < s.getNumPatterns(); ++pi)
    {
        const auto* p = s.getPattern (pi);
        auto* po = new juce::DynamicObject();
        po->setProperty ("rows", p->getNumRows());

        juce::Array<juce::var> cells;
        for (int r = 0; r < p->getNumRows(); ++r)
            for (int c = 0; c < p->getNumChannels(); ++c)
            {
                const Cell& cell = p->at (r, c);
                if (cell == Cell())
                    continue;   // sparse storage: default cells are omitted
                juce::Array<juce::var> e { r, c, (int) cell.note, (int) cell.instrument,
                                           (int) cell.volume, (int) cell.effect, (int) cell.effectValue };
                cells.add (juce::var (e));
            }
        po->setProperty ("cells", juce::var (cells));
        pats.add (juce::var (po));
    }
    root->setProperty ("patterns", juce::var (pats));

    juce::Array<juce::var> ord;
    for (int i = 0; i < s.orderLen; ++i)
        ord.add (s.order[i]);
    root->setProperty ("order", juce::var (ord));

    juce::Array<juce::var> ccTable;
    for (int t = 0; t < Song::kCcTracks; ++t)
    {
        juce::Array<juce::var> track;
        for (int slot = 0; slot < FxCmd::kNumSlots; ++slot)
            track.add ((int) s.ccSlots[t][slot]);
        ccTable.add (juce::var (track));
    }
    root->setProperty ("ccSlots", juce::var (ccTable));

    return juce::var (root);
}

juce::String ProjectIO::songFromVar (const juce::var& v, Song& out)
{
    if (! v.isObject())
        return "song: not an object";

    const int nc = (int) v.getProperty ("numChannels", 1);
    if (nc < 1 || nc > Pattern::kMaxChannels)
        return "song: invalid channel count (" + juce::String (nc) + ")";

    const auto* pats = v.getProperty ("patterns", {}).getArray();
    if (pats == nullptr || pats->isEmpty())
        return "song: no patterns";
    if (pats->size() > Song::kMaxPatterns)
        return "song: too many patterns (" + juce::String (pats->size()) + ")";

    out = Song();
    out.setNumChannels (nc);

    for (int pi = 0; pi < pats->size(); ++pi)
    {
        const auto& pv = pats->getReference (pi);
        if (! pv.isObject())
            return "pattern " + juce::String (pi) + ": not an object";

        const int rows = (int) pv.getProperty ("rows", 0);
        if (rows < 1 || rows > Pattern::kMaxRows)
            return "pattern " + juce::String (pi) + ": invalid row count (" + juce::String (rows) + ")";

        Pattern* p = pi == 0 ? out.getPattern (0) : out.getPattern (out.addPattern (rows));
        p->setNumRows (rows);
        p->setNumChannels (nc);

        if (const auto* cells = pv.getProperty ("cells", {}).getArray())
        {
            for (const auto& cv : *cells)
            {
                const auto* e = cv.getArray();
                if (e == nullptr || e->size() < 7)
                    return "pattern " + juce::String (pi) + ": malformed cell";

                const int r = (int) e->getReference (0);
                const int c = (int) e->getReference (1);
                if (r < 0 || r >= rows || c < 0 || c >= nc)
                    continue;   // out-of-grid cell: skipped, not fatal

                Cell cell;
                const int note = (int) e->getReference (2);
                cell.note        = (uint8_t) (note == (int) Cell::kNoteOff ? (int) Cell::kNoteOff
                                                                           : juce::jlimit (0, 127, note));
                cell.instrument  = (uint8_t) juce::jlimit (0, 255, (int) e->getReference (3));
                cell.volume      = (uint8_t) juce::jlimit (0, 64,  (int) e->getReference (4));
                cell.effect      = FxCmd::sanitize ((uint8_t) juce::jlimit (0, 255, (int) e->getReference (5)));
                cell.effectValue = (uint8_t) juce::jlimit (0, 127, (int) e->getReference (6));
                p->at (r, c) = cell;
            }
        }
    }

    if (const auto* ord = v.getProperty ("order", {}).getArray(); ord != nullptr && ! ord->isEmpty())
    {
        out.orderLen = juce::jmin ((int) ord->size(), Song::kMaxOrder);
        for (int i = 0; i < out.orderLen; ++i)
            out.order[i] = juce::jlimit (0, out.getNumPatterns() - 1, (int) ord->getReference (i));
    }
    else
    {
        out.orderLen = 1;
        out.order[0] = 0;
    }

    // CC slot table: optional (older .ubt files keep the defaults), but if
    // present it must be an array of per-track arrays
    if (const auto& ccv = v.getProperty ("ccSlots", {}); ! ccv.isVoid())
    {
        const auto* ccTable = ccv.getArray();
        if (ccTable == nullptr)
            return "ccSlots: not an array";

        for (int t = 0; t < juce::jmin ((int) ccTable->size(), Song::kCcTracks); ++t)
        {
            const auto* track = ccTable->getReference (t).getArray();
            if (track == nullptr)
                return "ccSlots: track " + juce::String (t) + " is not an array";
            for (int slot = 0; slot < juce::jmin ((int) track->size(), FxCmd::kNumSlots); ++slot)
                out.ccSlots[t][slot] = (uint8_t) juce::jlimit (0, 127, (int) track->getReference (slot));
        }
    }

    return {};
}
