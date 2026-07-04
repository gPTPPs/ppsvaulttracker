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
    playBtn.onClick = [this] { engine.getSequencer().play(); };
    stopBtn.onClick = [this] { engine.getSequencer().stop(); };

    auto setupTempoSlider = [this] (juce::Slider& s, double min, double max, double value)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 22);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.onValueChange = [this] { applyTempo(); };
        addAndMakeVisible (s);
    };
    setupTempoSlider (bpmSlider,   40.0, 300.0, 125.0);
    setupTempoSlider (speedSlider,  1.0,  31.0,   6.0);
    for (auto* l : { &bpmLabel, &speedLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (patternView);
    addAndMakeVisible (keyboard);
    keyboard.setKeyPressBaseOctave (4);   // QWERTY row plays notes too

    refreshStatus();
    setSize (900, 640);
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
    playBtn.setBounds (transport.removeFromLeft (80));
    transport.removeFromLeft (8);
    stopBtn.setBounds (transport.removeFromLeft (80));
    transport.removeFromLeft (16);
    bpmLabel.setBounds (transport.removeFromLeft (44));
    bpmSlider.setBounds (transport.removeFromLeft (110));
    transport.removeFromLeft (16);
    speedLabel.setBounds (transport.removeFromLeft (54));
    speedSlider.setBounds (transport.removeFromLeft (110));

    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromTop (24));
    keyboard.setBounds (area.removeFromBottom (96));
    area.removeFromBottom (8);
    patternView.setBounds (area);
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
