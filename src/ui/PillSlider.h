#pragma once
#include <JuceHeader.h>
#include "ui/RVLookAndFeel.h"

// IncDec value field drawn as ONE unified pill: the text box and the -/+
// buttons share a single rounded border. The buttons themselves paint flat —
// RVLookAndFeel::drawButtonBackground skips chrome for buttons whose parent
// is a Slider — and thin separators mark them off inside the pill.
class PillSlider : public juce::Slider
{
public:
    PillSlider()
    {
        setSliderStyle (juce::Slider::IncDecButtons);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (RV::panel);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (RV::panelLine);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (RV::panelLine);
        for (auto* c : getChildren())
            if (c->isVisible() && dynamic_cast<juce::Button*> (c) != nullptr)
                g.fillRect (c->getX(), 4, 1, getHeight() - 8);
    }
};
