#pragma once
#include <juce_core/juce_core.h>
#include "model/Song.h"

// Level-1 module import (MOD/XM/S3M/IT via libopenmpt): patterns, order list
// and initial tempo become a Song — notes only, no sample playback (you map
// VSTis onto the channels afterwards). Compiled in only when the build has
// libopenmpt (PVT_HAS_LIBOPENMPT); otherwise importFile() reports it.
namespace ModImport
{
    bool isAvailable();

    juce::String importFile (const juce::File&, Song& out, int maxChannels,
                             double& bpmOut, int& speedOut, juce::StringArray& warnings);
}
