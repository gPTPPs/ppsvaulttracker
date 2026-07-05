#pragma once
#include <JuceHeader.h>
#include "model/Song.h"

// Per-track identity shared by the grid and the mixer: custom name (fallback
// = the caller's default label) and accent colour picked from a fixed neon
// palette (fallback = theme colour). Message thread only.
namespace TrackStyle
{
    struct PaletteEntry { const char* name; uint32_t argb; };

    inline constexpr PaletteEntry kPalette[] = {
        { "Cyan",    0xff00e5ff },
        { "Magenta", 0xffff2da6 },
        { "Purple",  0xffb388ff },
        { "Blue",    0xff4f7bff },
        { "Green",   0xff00ff9c },
        { "Yellow",  0xffffe14f },
        { "Orange",  0xffff9130 },
        { "Red",     0xffff5252 },
    };

    inline juce::String nameOr (const Song& s, int track, const juce::String& fallback)
    {
        if (track >= 0 && track < Song::kCcTracks && ! s.trackNames[track].empty())
            return juce::String::fromUTF8 (s.trackNames[track].c_str());
        return fallback;
    }

    inline juce::Colour colourOr (const Song& s, int track, juce::Colour fallback)
    {
        if (track >= 0 && track < Song::kCcTracks && s.trackColors[track] != 0)
            return juce::Colour (s.trackColors[track]);
        return fallback;
    }

    inline bool hasColour (const Song& s, int track)
    {
        return track >= 0 && track < Song::kCcTracks && s.trackColors[track] != 0;
    }

    // swatch menu (8 palette entries + Default); writes into the Song and
    // calls onChanged afterwards
    inline void showColourMenu (Song& song, int track, const juce::PopupMenu::Options& options,
                                std::function<void()> onChanged)
    {
        if (track < 0 || track >= Song::kCcTracks)
            return;

        juce::PopupMenu m;
        for (int i = 0; i < juce::numElementsInArray (kPalette); ++i)
            m.addColouredItem (i + 1, kPalette[i].name, juce::Colour (kPalette[i].argb),
                               true, song.trackColors[track] == kPalette[i].argb);
        m.addSeparator();
        m.addItem (100, "Default", true, song.trackColors[track] == 0);

        m.showMenuAsync (options, [&song, track, onChanged = std::move (onChanged)] (int r)
        {
            if (r == 0)
                return;
            song.trackColors[track] = r == 100 ? 0 : kPalette[r - 1].argb;
            if (onChanged)
                onChanged();
        });
    }
}
