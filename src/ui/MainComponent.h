#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "ui/PatternEditor.h"

// Floating window hosting the plugin's own editor (or a generic one).
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& p, std::function<void()> onClose)
        : DocumentWindow (p.getName(), juce::Colours::darkgrey, DocumentWindow::closeButton),
          onCloseFn (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = p.createEditorIfNeeded())
            setContentOwned (ed, true);
        else
            setContentOwned (new juce::GenericAudioProcessorEditor (p), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        if (onCloseFn)
            onCloseFn();   // the owner deletes us asynchronously
    }

private:
    std::function<void()> onCloseFn;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

// Main view: toolbar (audio settings / load VST3 / editor / unload),
// transport bar (play/stop, BPM, speed), pattern view, status line,
// on-screen MIDI keyboard.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showAudioSettings();
    void chooseAndLoadPlugin();
    void showEditor();
    void unload();
    void refreshStatus();
    void applyTempo();

    HostEngine engine;

    juce::TextButton audioBtn  { "Audio/MIDI..." };
    juce::TextButton loadBtn   { "Load VST3..." };
    juce::TextButton editorBtn { "Editor" };
    juce::TextButton unloadBtn { "Unload" };

    // transport + edit controls
    juce::TextButton playBtn { "Play" }, stopBtn { "Stop" };
    juce::TextButton recBtn { "Rec" }, followBtn { "Follow" }, azertyBtn { "AZERTY" };
    juce::Slider bpmSlider, speedSlider, stepSlider, octaveSlider, chanSlider;
    juce::Label bpmLabel { {}, "BPM" }, speedLabel { {}, "Speed" },
                stepLabel { {}, "Step" }, octaveLabel { {}, "Oct" }, chanLabel { {}, "Ch" };

    juce::Label statusLabel;
    PatternEditor patternEditor { engine };
    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    std::unique_ptr<PluginWindow> pluginWindow;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
