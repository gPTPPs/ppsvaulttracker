#pragma once
#include <JuceHeader.h>

// RetroVault identity: deep dark blue, cyan/magenta neon, Orbitron for the
// chrome (the pattern grid keeps its monospace). Deliberately a THIN layer
// over LookAndFeel_V4 — colours, fonts and two custom draws, nothing more.
namespace RV
{
    inline const juce::Colour bg        { 0xff0a0d16 };
    inline const juce::Colour panel     { 0xff11172a };
    inline const juce::Colour panelLine { 0xff1e2a4a };
    inline const juce::Colour cyan      { 0xff00e5ff };
    inline const juce::Colour magenta   { 0xffff2da6 };
    inline const juce::Colour text      { 0xffd7e4ff };
    inline const juce::Colour textDim   { 0xff5d6b8c };
    inline const juce::Colour gridBg    { 0xff0c1020 };
    inline const juce::Colour gridBar   { 0xff131a30 };
    inline const juce::Colour gridEmpty { 0xff33405f };
}

class RVLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RVLookAndFeel()
    {
        orbitron     = juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronMedium_otf,
                                                                BinaryData::OrbitronMedium_otfSize);
        orbitronBold = juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronBold_otf,
                                                                BinaryData::OrbitronBold_otfSize);

        setColour (juce::ResizableWindow::backgroundColourId, RV::bg);
        setColour (juce::DocumentWindow::textColourId, RV::text);

        setColour (juce::TextButton::buttonColourId, RV::panel);
        setColour (juce::TextButton::buttonOnColourId, RV::magenta.withAlpha (0.8f));
        setColour (juce::TextButton::textColourOffId, RV::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        setColour (juce::Label::textColourId, RV::text);

        setColour (juce::Slider::backgroundColourId, RV::panelLine);
        setColour (juce::Slider::trackColourId, RV::cyan);
        setColour (juce::Slider::thumbColourId, RV::cyan);
        setColour (juce::Slider::textBoxTextColourId, RV::text);
        setColour (juce::Slider::textBoxBackgroundColourId, RV::panel);
        setColour (juce::Slider::textBoxOutlineColourId, RV::panelLine);

        setColour (juce::TextEditor::backgroundColourId, RV::panel);
        setColour (juce::TextEditor::textColourId, RV::text);
        setColour (juce::TextEditor::outlineColourId, RV::panelLine);
        setColour (juce::TextEditor::focusedOutlineColourId, RV::cyan);

        setColour (juce::ComboBox::backgroundColourId, RV::panel);
        setColour (juce::ComboBox::textColourId, RV::text);
        setColour (juce::ComboBox::outlineColourId, RV::panelLine);
        setColour (juce::ComboBox::arrowColourId, RV::cyan);

        setColour (juce::PopupMenu::backgroundColourId, RV::panel);
        setColour (juce::PopupMenu::textColourId, RV::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, RV::cyan.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour (juce::ScrollBar::thumbColourId, RV::panelLine.brighter (0.4f));
        setColour (juce::AlertWindow::backgroundColourId, RV::panel);
        setColour (juce::AlertWindow::textColourId, RV::text);
        setColour (juce::AlertWindow::outlineColourId, RV::panelLine);

        setColour (juce::ToggleButton::textColourId, RV::text);
        setColour (juce::ToggleButton::tickColourId, RV::cyan);
        setColour (juce::ToggleButton::tickDisabledColourId, RV::textDim);

        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, RV::cyan.withAlpha (0.6f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, RV::cyan.withAlpha (0.25f));

        setColour (juce::ProgressBar::foregroundColourId, RV::cyan);
        setColour (juce::ProgressBar::backgroundColourId, RV::panelLine);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        // the pattern grid asks for the monospace face by name — leave it alone
        if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
            return LookAndFeel_V4::getTypefaceForFont (f);
        return f.isBold() ? orbitronBold : orbitron;
    }

    // sober button: flat panel, 1 px border, hover = lifted border. Toggled
    // state = slightly lifted fill + a thin accent bar along the bottom (the
    // per-button buttonOnColourId), instead of a loud solid fill.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        // inc/dec buttons inside a value pill: no chrome of their own, just a
        // subtle tint on interaction (the pill draws the shared border)
        if (dynamic_cast<juce::Slider*> (b.getParentComponent()) != nullptr)
        {
            if (down || highlighted)
            {
                g.setColour (RV::cyan.withAlpha (down ? 0.18f : 0.08f));
                g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (1.0f), 2.0f);
            }
            return;
        }

        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        auto base = RV::panel;
        if (on)               base = base.brighter (0.13f);
        if (down)             base = base.brighter (0.18f);
        else if (highlighted) base = base.brighter (0.07f);

        g.setColour (base);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (highlighted ? RV::panelLine.brighter (0.6f) : RV::panelLine);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        if (on)
        {
            g.setColour (b.findColour (juce::TextButton::buttonOnColourId));
            g.fillRoundedRectangle (r.getX() + 5.0f, r.getBottom() - 3.5f,
                                    r.getWidth() - 10.0f, 2.0f, 1.0f);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }

        // neon fader: dark track, cyan->magenta fill up to the thumb
        const float cx = (float) x + (float) w * 0.5f;
        const juce::Rectangle<float> track (cx - 2.0f, (float) y, 4.0f, (float) h);
        g.setColour (RV::panelLine);
        g.fillRoundedRectangle (track, 2.0f);

        const juce::Rectangle<float> fill (cx - 2.0f, sliderPos, 4.0f, (float) y + (float) h - sliderPos);
        g.setGradientFill (juce::ColourGradient (RV::magenta, cx, (float) y,
                                                 RV::cyan, cx, (float) (y + h), false));
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (cx - 9.0f, sliderPos - 2.0f, 18.0f, 4.0f, 2.0f);
    }

private:
    juce::Typeface::Ptr orbitron, orbitronBold;
};
