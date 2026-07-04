#include "ui/MainComponent.h"

MainComponent::MainComponent()
{
    for (auto* b : { &audioBtn, &loadBtn, &editorBtn, &unloadBtn })
        addAndMakeVisible (b);

    audioBtn.onClick  = [this] { showAudioSettings(); };
    loadBtn.onClick   = [this] { chooseAndLoadPlugin(); };
    editorBtn.onClick = [this] { showEditor(); };
    unloadBtn.onClick = [this] { unload(); };

    // ---- transport ----
    for (auto* b : { &playBtn, &stopBtn })
        addAndMakeVisible (b);
    playBtn.onClick = [this] { engine.getSequencer().play(); patternEditor.grabKeyboardFocus(); };
    stopBtn.onClick = [this] { engine.getSequencer().stop(); patternEditor.grabKeyboardFocus(); };
    for (auto* b : { &playBtn, &stopBtn, &audioBtn, &loadBtn, &editorBtn, &unloadBtn })
        b->setWantsKeyboardFocus (false);

    // ---- edit-mode toggles ----
    auto setupToggle = [this] (juce::TextButton& b, bool initial, std::function<void (bool)> apply)
    {
        b.setClickingTogglesState (true);
        b.setToggleState (initial, juce::dontSendNotification);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb03060));
        b.setWantsKeyboardFocus (false);   // keep the editor's shortcuts alive
        b.onClick = [this, &b, apply] { apply (b.getToggleState()); patternEditor.grabKeyboardFocus(); };
        addAndMakeVisible (b);
    };
    setupToggle (recBtn,    false, [this] (bool on) { patternEditor.recEnabled = on; });
    setupToggle (followBtn, true,  [this] (bool on) { patternEditor.followPlayhead = on; });
    setupToggle (azertyBtn, true,  [this] (bool on) { patternEditor.azertyLayout = on;
                                                      azertyBtn.setButtonText (on ? "AZERTY" : "QWERTY"); });

    auto setupIncDec = [this] (juce::Slider& s, double min, double max, double value,
                               std::function<void (double)> apply)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.setWantsKeyboardFocus (false);
        s.onValueChange = [&s, apply] { apply (s.getValue()); };
        addAndMakeVisible (s);
    };
    setupIncDec (bpmSlider,   40.0, 300.0, 125.0, [this] (double)   { applyTempo(); });
    setupIncDec (speedSlider,  1.0,  31.0,   6.0, [this] (double)   { applyTempo(); });
    setupIncDec (stepSlider,   0.0,  16.0,   1.0, [this] (double v) { patternEditor.editStep = (int) v; });
    setupIncDec (octaveSlider, 1.0,   7.0,   3.0, [this] (double v) { patternEditor.editOctave = (int) v; });
    setupIncDec (chanSlider,   1.0,   8.0,   1.0, [this] (double v)
    {
        if (auto* p = engine.getSequencer().getMutablePattern())
            p->setNumChannels ((int) v);
    });

    for (auto* l : { &bpmLabel, &speedLabel, &stepLabel, &octaveLabel, &chanLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (patternEditor);
    addAndMakeVisible (keyboard);
    keyboard.setKeyPressBaseOctave (4);

    refreshStatus();
    setSize (1240, 720);
}

void MainComponent::applyTempo()
{
    engine.getSequencer().setTempo (bpmSlider.getValue(), (int) speedSlider.getValue());
}

MainComponent::~MainComponent()
{
    pluginWindow = nullptr;   // editor must go before the engine
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto bar = area.removeFromTop (32);
    for (auto* b : { &audioBtn, &loadBtn, &editorBtn, &unloadBtn })
    {
        b->setBounds (bar.removeFromLeft (110));
        bar.removeFromLeft (8);
    }

    area.removeFromTop (8);
    auto transport = area.removeFromTop (30);
    auto place = [&transport] (juce::Component& c, int w, int gapAfter = 8)
    {
        c.setBounds (transport.removeFromLeft (w));
        transport.removeFromLeft (gapAfter);
    };
    place (playBtn, 70);
    place (stopBtn, 70, 14);
    place (recBtn, 56);
    place (followBtn, 72);
    place (azertyBtn, 80, 14);
    place (bpmLabel, 38, 2);
    place (bpmSlider, 96, 10);
    place (speedLabel, 48, 2);
    place (speedSlider, 96, 10);
    place (stepLabel, 38, 2);
    place (stepSlider, 96, 10);
    place (octaveLabel, 30, 2);
    place (octaveSlider, 96, 10);
    place (chanLabel, 24, 2);
    place (chanSlider, 96);

    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromTop (24));
    keyboard.setBounds (area.removeFromBottom (96));
    area.removeFromBottom (8);
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

void MainComponent::chooseAndLoadPlugin()
{
    juce::File startDir ("C:/Program Files/Common Files/VST3");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Choose a VST3 plugin", startDir, "*.vst3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,   // .vst3 bundles are folders
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            pluginWindow = nullptr;   // never keep an editor on a dying instance
            const auto error = engine.loadPlugin (file);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Plugin load failed", error);
            else
                showEditor();
            refreshStatus();
        });
}

void MainComponent::showEditor()
{
    if (pluginWindow != nullptr)
    {
        pluginWindow->toFront (true);
        return;
    }
    if (auto* instance = engine.getPluginInstance())
        pluginWindow = std::make_unique<PluginWindow> (*instance, [this]
        {
            // async: we are inside the window's own close callback
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safe != nullptr)
                    safe->pluginWindow = nullptr;
            });
        });
}

void MainComponent::unload()
{
    pluginWindow = nullptr;
    engine.unloadPlugin();
    refreshStatus();
}

void MainComponent::refreshStatus()
{
    juce::String device = "no audio device";
    if (auto* d = engine.getDeviceManager().getCurrentAudioDevice())
        device = d->getName() + "  (" + juce::String (d->getCurrentSampleRate(), 0) + " Hz, "
               + juce::String (d->getCurrentBufferSizeSamples()) + " samples)";

    statusLabel.setText ((engine.hasPlugin() ? "Hosting: " + engine.getPluginName()
                                             : juce::String ("No plugin loaded"))
                             + "   |   " + device,
                         juce::dontSendNotification);
}
