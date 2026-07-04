#include "ui/MixerView.h"

MixerView::MixerView (HostEngine& e, std::function<void (juce::AudioPluginInstance*)> showEditorFn)
    : engine (e), showEditor (std::move (showEditorFn))
{
    setOpaque (true);

    for (int i = 0; i < kNumStrips; ++i)
    {
        auto& s = strips[i];
        const bool isMaster = i == kMaster;
        const int ch = i;

        s.title.setText (isMaster ? "MASTER" : "CH " + juce::String (i + 1), juce::dontSendNotification);
        s.title.setJustificationType (juce::Justification::centred);
        s.title.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
        s.title.setColour (juce::Label::textColourId,
                           isMaster ? juce::Colour (0xffb07cf0) : juce::Colour (0xff9aa4b2));
        addAndMakeVisible (s.title);

        if (! isMaster)
        {
            s.instBtn.setWantsKeyboardFocus (false);
            s.instBtn.onClick = [this, ch]
            {
                if (juce::ModifierKeys::getCurrentModifiers().isCtrlDown())
                {
                    engine.unloadInstrument (ch);
                    refreshLabels();
                }
                else if (auto* inst = engine.getInstrument (ch))
                {
                    showEditor (inst);
                }
                else
                {
                    loadInstrumentFor (ch);
                }
            };
            addAndMakeVisible (s.instBtn);

            s.muteBtn.setClickingTogglesState (true);
            s.muteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb03060));
            s.muteBtn.setWantsKeyboardFocus (false);
            s.muteBtn.onClick = [this, ch, &s] { engine.setChannelMute (ch, s.muteBtn.getToggleState()); };
            addAndMakeVisible (s.muteBtn);

            s.soloBtn.setClickingTogglesState (true);
            s.soloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2f8f5b));
            s.soloBtn.setWantsKeyboardFocus (false);
            s.soloBtn.onClick = [this, ch, &s] { engine.setChannelSolo (ch, s.soloBtn.getToggleState()); };
            addAndMakeVisible (s.soloBtn);
        }

        for (int f = 0; f < HostEngine::kMaxInserts; ++f)
        {
            auto& b = s.fxBtn[f];
            b.setWantsKeyboardFocus (false);
            const int target = isMaster ? -1 : ch;
            b.onClick = [this, target, f]
            {
                if (engine.getInsert (target, f) != nullptr)
                {
                    if (juce::ModifierKeys::getCurrentModifiers().isCtrlDown())
                    {
                        engine.removeInsert (target, f);
                        refreshLabels();
                    }
                    else
                    {
                        showEditor (engine.getInsert (target, f));
                    }
                }
                else
                {
                    addInsertFor (target);
                }
            };
            addAndMakeVisible (b);
        }

        s.gain.setSliderStyle (juce::Slider::LinearVertical);
        s.gain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.gain.setRange (0.0, 1.5, 0.01);
        s.gain.setValue (1.0, juce::dontSendNotification);
        s.gain.setWantsKeyboardFocus (false);
        s.gain.onValueChange = [this, ch, isMaster, &s]
        {
            const auto v = (float) s.gain.getValue();
            if (isMaster) engine.setMasterGain (v);
            else          engine.setChannelGain (ch, v);
        };
        addAndMakeVisible (s.gain);
    }

    refreshLabels();
    startTimerHz (30);
}

juce::Rectangle<int> MixerView::stripArea (int index) const
{
    const int w = getWidth() / kNumStrips;
    return { index * w, 0, w, getHeight() };
}

void MixerView::resized()
{
    for (int i = 0; i < kNumStrips; ++i)
    {
        auto& s = strips[i];
        auto r = stripArea (i).reduced (4);

        s.title.setBounds (r.removeFromTop (18));
        if (i != kMaster)
            s.instBtn.setBounds (r.removeFromTop (22));
        else
            r.removeFromTop (22);
        r.removeFromTop (2);
        for (auto& b : s.fxBtn)
        {
            b.setBounds (r.removeFromTop (20));
            r.removeFromTop (2);
        }

        if (i != kMaster)
        {
            auto ms = r.removeFromTop (22);
            s.muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (1, 0));
            s.soloBtn.setBounds (ms.reduced (1, 0));
        }
        else
            r.removeFromTop (22);

        r.removeFromTop (2);
        s.gain.setBounds (r.removeFromLeft (r.getWidth() / 2));
        // remaining right half of r = VU meter, painted in paint()
    }
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161a));

    for (int i = 0; i < kNumStrips; ++i)
    {
        const auto area = stripArea (i);
        g.setColour (juce::Colour (0xff23272e));
        g.drawRect (area);

        // VU: right half of the fader zone
        auto r = area.reduced (4);
        r.removeFromTop (18 + 22 + 2 + (20 + 2) * HostEngine::kMaxInserts + 22 + 2);
        auto vuArea = r.removeFromRight (r.getWidth() / 2).reduced (8, 4);

        g.setColour (juce::Colour (0xff1c2026));
        g.fillRect (vuArea);

        const float level = juce::jlimit (0.0f, 1.0f, vu[i]);
        const int h = (int) (level * (float) vuArea.getHeight());
        auto bar = vuArea.removeFromBottom (h);
        g.setColour (level > 0.9f ? juce::Colour (0xffd04a4a)
                                  : (i == kMaster ? juce::Colour (0xffb07cf0) : juce::Colour (0xff2f8f5b)));
        g.fillRect (bar);
    }
}

void MixerView::timerCallback()
{
    for (int i = 0; i < kNumStrips; ++i)
    {
        const float peak = i == kMaster ? engine.readMasterPeak() : engine.readChannelPeak (i);
        vu[i] = juce::jmax (peak, vu[i] * 0.85f);   // fast attack, smooth decay
    }
    repaint();
}

void MixerView::refreshLabels()
{
    for (int i = 0; i < kNumStrips; ++i)
    {
        auto& s = strips[i];
        if (i != kMaster)
        {
            const auto name = engine.getInstrumentName (i);
            s.instBtn.setButtonText (name.isNotEmpty() ? name : "Inst...");
        }
        for (int f = 0; f < HostEngine::kMaxInserts; ++f)
        {
            const auto name = engine.getInsertName (i == kMaster ? -1 : i, f);
            s.fxBtn[f].setButtonText (name.isNotEmpty() ? name : "+ FX");
        }
    }
}

void MixerView::loadInstrumentFor (int ch)
{
    juce::File startDir ("C:/Program Files/Common Files/VST3");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Instrument for CH " + juce::String (ch + 1),
                                                   startDir, "*.vst3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this, ch] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;
            const auto error = engine.loadInstrument (ch, file);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Instrument load failed", error);
            refreshLabels();
        });
}

void MixerView::addInsertFor (int chOrMaster)
{
    juce::File startDir ("C:/Program Files/Common Files/VST3");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Insert effect", startDir, "*.vst3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this, chOrMaster] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;
            const auto error = engine.addInsert (chOrMaster, file);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Insert load failed", error);
            refreshLabels();
        });
}
