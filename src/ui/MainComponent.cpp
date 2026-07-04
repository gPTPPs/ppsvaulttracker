#include "ui/MainComponent.h"

MainComponent::MainComponent()
{
    // ---- toolbar ----
    addAndMakeVisible (audioBtn);
    audioBtn.setWantsKeyboardFocus (false);
    audioBtn.onClick = [this] { showAudioSettings(); };

    // ---- transport ----
    for (auto* b : { &playBtn, &stopBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    playBtn.onClick = [this] { engine.getSequencer().play(); patternEditor.grabKeyboardFocus(); };
    stopBtn.onClick = [this] { engine.getSequencer().stop(); patternEditor.grabKeyboardFocus(); };

    auto setupToggle = [this] (juce::TextButton& b, bool initial, std::function<void (bool)> apply)
    {
        b.setClickingTogglesState (true);
        b.setToggleState (initial, juce::dontSendNotification);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb03060));
        b.setWantsKeyboardFocus (false);
        b.onClick = [this, &b, apply] { apply (b.getToggleState()); patternEditor.grabKeyboardFocus(); };
        addAndMakeVisible (b);
    };
    setupToggle (recBtn,      false, [this] (bool on) { patternEditor.recEnabled = on; });
    setupToggle (followBtn,   true,  [this] (bool on) { patternEditor.followPlayhead = on; });
    setupToggle (azertyBtn,   true,  [this] (bool on) { patternEditor.azertyLayout = on;
                                                        azertyBtn.setButtonText (on ? "AZERTY" : "QWERTY"); });
    setupToggle (songModeBtn, false, [this] (bool on) { engine.getSequencer().setSongMode (on); });

    auto setupIncDec = [this] (juce::Slider& s, double min, double max, double value,
                               std::function<void (double)> apply)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.setWantsKeyboardFocus (false);
        for (auto* child : s.getChildren())      // the inc/dec buttons steal focus too
            child->setWantsKeyboardFocus (false);
        s.onValueChange = [this, &s, apply] { apply (s.getValue()); patternEditor.grabKeyboardFocus(); };
        addAndMakeVisible (s);
    };
    setupIncDec (bpmSlider,   40.0, 300.0, 125.0, [this] (double)   { applyTempo(); });
    setupIncDec (speedSlider,  1.0,  31.0,   6.0, [this] (double)   { applyTempo(); });
    setupIncDec (stepSlider,   0.0,  16.0,   1.0, [this] (double v) { patternEditor.editStep = (int) v; });
    setupIncDec (octaveSlider, 1.0,   7.0,   3.0, [this] (double v) { patternEditor.editOctave = (int) v; });
    setupIncDec (chanSlider,   1.0,   8.0,   1.0, [this] (double v) { engine.setNumChannels ((int) v); });
    setupIncDec (patSlider,    0.0,   1.0,   0.0, [this] (double v)
    {
        // the range can exceed the real pattern count (JUCE sliders need
        // max > min) — clamp here
        const int idx = juce::jlimit (0, engine.getSong().getNumPatterns() - 1, (int) v);
        if (idx != (int) v)
            patSlider.setValue ((double) idx, juce::dontSendNotification);
        engine.getSequencer().setEditPatternIndex (idx);
        patternEditor.repaint();
    });

    addPatBtn.setWantsKeyboardFocus (false);
    addPatBtn.onClick = [this]
    {
        const int idx = engine.addPattern();
        if (idx >= 0)
        {
            patSlider.setRange (0.0, juce::jmax (1.0, (double) (engine.getSong().getNumPatterns() - 1)), 1.0);
            patSlider.setValue ((double) idx);   // jump to the new pattern
        }
    };
    addAndMakeVisible (addPatBtn);

    orderEdit.setText ("0");
    orderEdit.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    orderEdit.onReturnKey = [this] { applyOrderFromText(); };
    addAndMakeVisible (orderEdit);

    orderApplyBtn.setWantsKeyboardFocus (false);
    orderApplyBtn.onClick = [this] { applyOrderFromText(); };
    addAndMakeVisible (orderApplyBtn);

    for (auto* l : { &bpmLabel, &speedLabel, &stepLabel, &octaveLabel, &chanLabel, &patLabel, &orderLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (patternEditor);
    addAndMakeVisible (mixer);
    addAndMakeVisible (keyboard);
    keyboard.setKeyPressBaseOctave (4);

    // live input (virtual keyboard + hardware) follows the edit cursor
    patternEditor.onCursorChannelChanged = [this] (int ch)
    {
        engine.setLiveChannel (ch);
        keyboard.setMidiChannel (ch + 1);
    };
    keyboard.setMidiChannel (1);

    refreshStatus();
    startTimerHz (4);
    setSize (1240, 940);
}

MainComponent::~MainComponent()
{
    pluginWindow = nullptr;   // plugin editors must go before the engine
}

void MainComponent::timerCallback()
{
    // keep the pattern selector in sync when the song playback moves it
    const int edited = engine.getSequencer().getEditPatternIndex();
    if ((int) patSlider.getValue() != edited)
        patSlider.setValue ((double) edited, juce::dontSendNotification);

    refreshStatus();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto bar = area.removeFromTop (32);
    audioBtn.setBounds (bar.removeFromLeft (110));

    area.removeFromTop (6);
    auto transport = area.removeFromTop (30);
    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w, int gapAfter = 8)
    {
        c.setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gapAfter);
    };
    place (transport, playBtn, 70);
    place (transport, stopBtn, 70, 14);
    place (transport, recBtn, 56);
    place (transport, followBtn, 72);
    place (transport, azertyBtn, 80, 14);
    place (transport, bpmLabel, 38, 2);
    place (transport, bpmSlider, 96, 10);
    place (transport, speedLabel, 48, 2);
    place (transport, speedSlider, 96, 10);
    place (transport, stepLabel, 38, 2);
    place (transport, stepSlider, 96, 10);
    place (transport, octaveLabel, 30, 2);
    place (transport, octaveSlider, 96, 10);
    place (transport, chanLabel, 24, 2);
    place (transport, chanSlider, 96);

    area.removeFromTop (6);
    auto songRow = area.removeFromTop (30);
    place (songRow, songModeBtn, 96, 14);
    place (songRow, patLabel, 56, 2);
    place (songRow, patSlider, 96, 10);
    place (songRow, addPatBtn, 100, 14);
    place (songRow, orderLabel, 46, 2);
    orderApplyBtn.setBounds (songRow.removeFromRight (84));
    songRow.removeFromRight (8);
    orderEdit.setBounds (songRow);

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (22));

    keyboard.setBounds (area.removeFromBottom (90));
    area.removeFromBottom (6);
    mixer.setBounds (area.removeFromBottom (190));
    area.removeFromBottom (6);
    patternEditor.setBounds (area);
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        engine.getDeviceManager(), 0, 0, 0, 2, true, false, true, false);
    selector->setSize (520, 460);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (selector.release());
    opts.dialogTitle = "Audio/MIDI Settings";
    opts.componentToCentreAround = this;
    opts.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}

void MainComponent::showEditorFor (juce::AudioPluginInstance* instance)
{
    if (instance == nullptr)
        return;
    pluginWindow = nullptr;   // one editor window at a time
    pluginWindow = std::make_unique<PluginWindow> (*instance, [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr)
                safe->pluginWindow = nullptr;
        });
    });
}

void MainComponent::applyTempo()
{
    engine.getSequencer().setTempo (bpmSlider.getValue(), (int) speedSlider.getValue());
}

void MainComponent::applyOrderFromText()
{
    juce::StringArray tokens;
    tokens.addTokens (orderEdit.getText(), " ,;", {});
    tokens.removeEmptyStrings();

    int entries[Song::kMaxOrder];
    int count = 0;
    for (auto& t : tokens)
        if (count < Song::kMaxOrder)
            entries[count++] = t.getIntValue();

    if (count > 0)
        engine.applyOrder (entries, count);
    patternEditor.grabKeyboardFocus();
}

void MainComponent::refreshStatus()
{
    juce::String device = "no audio device";
    if (auto* d = engine.getDeviceManager().getCurrentAudioDevice())
        device = d->getName() + " (" + juce::String (d->getCurrentSampleRate(), 0) + " Hz, "
               + juce::String (d->getCurrentBufferSizeSamples()) + " smp)";

    auto& seq = engine.getSequencer();
    juce::String pos = "Pat " + juce::String (seq.getEditPatternIndex());
    if (seq.isPlaying())
    {
        pos = "Playing pat " + juce::String (seq.getUiPatternIndex());
        if (seq.getUiOrderPos() >= 0)
            pos += "  (order " + juce::String (seq.getUiOrderPos() + 1) + "/"
                 + juce::String (engine.getSong().orderLen) + ")";
    }

    statusLabel.setText ("CH " + juce::String (patternEditor.getCursorChannel() + 1)
                             + "   |   " + pos + "   |   " + device,
                         juce::dontSendNotification);
}
