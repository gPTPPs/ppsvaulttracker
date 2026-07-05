#pragma once
#include <JuceHeader.h>

// One home for the user-facing version string (window title, splash).
// JUCE_APPLICATION_VERSION_STRING comes from the CMake project VERSION.
namespace AppVersion
{
    inline juce::String display() { return "v" JUCE_APPLICATION_VERSION_STRING "-beta"; }
}
