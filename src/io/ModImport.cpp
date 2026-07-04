#include "io/ModImport.h"
#include "io/ModImportMapping.h"

#if ! defined (PVT_HAS_LIBOPENMPT)
 #define PVT_HAS_LIBOPENMPT 0
#endif

#if PVT_HAS_LIBOPENMPT

#include <libopenmpt/libopenmpt.hpp>

bool ModImport::isAvailable() { return true; }

juce::String ModImport::importFile (const juce::File& file, Song& out, int maxChannels,
                                    double& bpmOut, int& speedOut, juce::StringArray& warnings)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() == 0)
        return "Cannot read " + file.getFullPathName();

    try
    {
        // module files are untrusted input: libopenmpt is fuzz-hardened, we
        // still catch everything and clamp all values on our side
        openmpt::module mod (data.getData(), data.getSize());

        const int srcChannels = mod.get_num_channels();
        const int numChannels = juce::jmin (srcChannels, maxChannels);
        if (srcChannels > maxChannels)
            warnings.add ("Module has " + juce::String (srcChannels) + " channels; only the first "
                          + juce::String (maxChannels) + " were imported");

        int numPatterns = mod.get_num_patterns();
        if (numPatterns > Song::kMaxPatterns)
        {
            warnings.add ("Module has " + juce::String (numPatterns) + " patterns; capped at "
                          + juce::String (Song::kMaxPatterns));
            numPatterns = Song::kMaxPatterns;
        }
        if (numPatterns < 1)
            return "Module has no patterns";

        out = Song();
        out.setNumChannels (numChannels);

        for (int pat = 0; pat < numPatterns; ++pat)
        {
            int rows = mod.get_pattern_num_rows (pat);
            if (rows > Pattern::kMaxRows)
            {
                warnings.add ("Pattern " + juce::String (pat) + ": " + juce::String (rows)
                              + " rows, truncated to " + juce::String (Pattern::kMaxRows));
                rows = Pattern::kMaxRows;
            }
            rows = juce::jmax (1, rows);

            Pattern* p = pat == 0 ? out.getPattern (0) : out.getPattern (out.addPattern (rows));
            p->setNumRows (rows);

            for (int r = 0; r < rows; ++r)
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const int rawNote = mod.get_pattern_row_channel_command (
                        pat, r, ch, openmpt::module::command_note);
                    const auto note = ModImportMapping::noteFromOpenMpt (rawNote);
                    if (note == Cell::kEmpty)
                        continue;

                    auto& cell = p->at (r, ch);
                    cell.note = note;
                    if (note != Cell::kNoteOff)
                        cell.volume = ModImportMapping::volumeFromOpenMpt (
                            mod.get_pattern_row_channel_command (pat, r, ch, openmpt::module::command_volumeffect),
                            mod.get_pattern_row_channel_command (pat, r, ch, openmpt::module::command_volume));
                }
        }

        // order list (skip markers / out-of-range entries)
        out.orderLen = 0;
        const int numOrders = mod.get_num_orders();
        for (int o = 0; o < numOrders && out.orderLen < Song::kMaxOrder; ++o)
        {
            const int pat = mod.get_order_pattern (o);
            if (pat >= 0 && pat < out.getNumPatterns())
                out.order[out.orderLen++] = pat;
        }
        if (out.orderLen == 0)
        {
            out.orderLen = 1;
            out.order[0] = 0;
        }

        bpmOut   = juce::jlimit (32.0, 999.0, mod.get_current_tempo2());
        speedOut = juce::jlimit (1, 31, (int) mod.get_current_speed());

        const auto title = juce::String (mod.get_metadata ("title")).trim();
        if (title.isNotEmpty())
            warnings.add ("Imported: \"" + title + "\" (" + juce::String (mod.get_metadata ("type_long")) + ")");
    }
    catch (const openmpt::exception& e)
    {
        return "libopenmpt: " + juce::String (e.what());
    }
    catch (const std::exception& e)
    {
        return "Import failed: " + juce::String (e.what());
    }

    return {};
}

#else

bool ModImport::isAvailable() { return false; }

juce::String ModImport::importFile (const juce::File&, Song&, int, double&, int&, juce::StringArray&)
{
    return "This build has no module import (compiled without libopenmpt)";
}

#endif
