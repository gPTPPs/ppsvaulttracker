#pragma once
#include <JuceHeader.h>
#include "ui/RVLookAndFeel.h"

// Synthwave splash: vector-drawn (crisp on any display scale), always on
// top, dismissed by clicking it. The owner deletes it via onFinished,
// called asynchronously so we never delete from inside a mouse callback.
class RVSplash : public juce::Component
{
public:
    std::function<void()> onFinished;

    explicit RVSplash (const juce::String& versionText) : version (versionText)
    {
        orbitron     = juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronMedium_otf,
                                                                BinaryData::OrbitronMedium_otfSize);
        orbitronBold = juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronBold_otf,
                                                                BinaryData::OrbitronBold_otfSize);

        setOpaque (true);
        setAlwaysOnTop (true);
        setSize (640, 360);
        addToDesktop (juce::ComponentPeer::windowIsTemporary);
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setCentrePosition (display->userBounds.getCentre().roundToInt());
        setVisible (true);
        toFront (false);
    }

    void mouseUp (const juce::MouseEvent&) override { finish(); }

    void paint (juce::Graphics& g) override
    {
        const auto w = (float) getWidth();
        const auto h = (float) getHeight();
        const float horizon = h * 0.62f;

        // sky: deep blue fading into a purple glow at the horizon
        g.setGradientFill (juce::ColourGradient (RV::bg, 0.0f, 0.0f,
                                                 juce::Colour (0xff1b1034), 0.0f, horizon, false));
        g.fillRect (0.0f, 0.0f, w, horizon);
        g.setColour (RV::gridBg);
        g.fillRect (0.0f, horizon, w, h - horizon);

        // synthwave sun sitting on the horizon, sliced by sky-coloured bars
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<int> (0, 0, (int) w, (int) horizon));
            const float r = 74.0f;
            const juce::Point<float> c (w * 0.5f, horizon);
            g.setGradientFill (juce::ColourGradient (RV::magenta.withAlpha (0.85f), c.x, c.y - r,
                                                     juce::Colour (0xffff8a3d).withAlpha (0.65f), c.x, c.y, false));
            g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            g.setColour (juce::Colour (0xff1b1034));
            for (int i = 0; i < 5; ++i)   // gaps widen toward the horizon
            {
                const float t = (float) i / 5.0f;
                const float y = c.y - r * 0.55f * (1.0f - t * t);
                g.fillRect (c.x - r, y, r * 2.0f, 2.0f + t * 5.0f);
            }
        }

        // horizon glow
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (RV::magenta.withAlpha (0.25f * (float) (3 - i)));
            g.fillRect (0.0f, horizon - 1.0f - (float) i, w, 1.0f);
        }
        g.setColour (RV::magenta);
        g.fillRect (0.0f, horizon, w, 2.0f);

        // perspective grid: horizontals ease toward the viewer, verticals fan
        // out from the vanishing point
        g.setColour (RV::cyan.withAlpha (0.28f));
        for (int i = 1; i <= 8; ++i)
        {
            const float t = (float) i / 8.0f;
            g.drawHorizontalLine ((int) (horizon + t * t * (h - horizon)), 0.0f, w);
        }
        for (int k = -9; k <= 9; ++k)
        {
            const float xBottom = w * 0.5f + (float) k * (w / 9.0f);
            g.drawLine (w * 0.5f + (float) k * 4.0f, horizon, xBottom, h, 1.0f);
        }

        // title with a slight chromatic-aberration glow, synthwave style
        const auto titleArea = juce::Rectangle<float> (0.0f, h * 0.16f, w, 52.0f);
        const juce::Font titleFont (juce::FontOptions (orbitronBold).withHeight (44.0f));
        g.setFont (titleFont);
        g.setColour (RV::magenta.withAlpha (0.55f));
        g.drawText ("PPsVaultTracker", titleArea.translated (2.0f, 1.5f), juce::Justification::centred);
        g.setColour (RV::cyan.withAlpha (0.55f));
        g.drawText ("PPsVaultTracker", titleArea.translated (-2.0f, -1.5f), juce::Justification::centred);
        g.setColour (juce::Colours::white);
        g.drawText ("PPsVaultTracker", titleArea, juce::Justification::centred);

        g.setFont (juce::Font (juce::FontOptions (orbitron).withHeight (14.0f)));
        g.setColour (RV::text.withAlpha (0.85f));
        g.drawText ("A modern VSTi tracker - by The Unborn / RetroVault",
                    juce::Rectangle<float> (0.0f, h * 0.16f + 58.0f, w, 20.0f),
                    juce::Justification::centred);

        g.setFont (juce::Font (juce::FontOptions (orbitron).withHeight (13.0f)));
        g.setColour (RV::textDim);
        g.drawText (version, juce::Rectangle<float> (0.0f, h - 26.0f, w * 0.5f - 4.0f, 18.0f),
                    juce::Justification::centredRight);
        g.drawText ("AGPLv3", juce::Rectangle<float> (w * 0.5f + 4.0f, h - 26.0f, w * 0.5f - 12.0f, 18.0f),
                    juce::Justification::centredLeft);

        g.setColour (RV::panelLine);
        g.drawRect (getLocalBounds(), 1);
    }

private:
    void finish()
    {
        setVisible (false);
        if (onFinished)
            juce::MessageManager::callAsync (std::exchange (onFinished, nullptr));
    }

    juce::String version;
    juce::Typeface::Ptr orbitron, orbitronBold;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RVSplash)
};
