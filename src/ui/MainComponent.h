#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "ui/PatternEditor.h"
#include "ui/MixerView.h"

// Floating window hosting a plugin's own editor (or a generic one).
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& p, std::function<void()> onClose)
        : DocumentWindow (p.getName(), juce::Colours::darkgrey, DocumentWindow::closeButton),
          onCloseFn (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = p.createEditorAndMakeActive())
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

// Main view: toolbar, transport, song/order row, pattern editor, mixer,
// on-screen MIDI keyboard.
class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showAudioSettings();
    void showEditorFor (juce::AudioPluginInstance*);
    void applyTempo();
    void applyOrderFromText();
    void refreshStatus();

    HostEngine engine;

    // toolbar
    juce::TextButton audioBtn { "Audio/MIDI..." };

    // transport + edit controls
    juce::TextButton playBtn { "Play" }, stopBtn { "Stop" };
    juce::TextButton recBtn { "Rec" }, followBtn { "Follow" }, azertyBtn { "AZERTY" };
    juce::Slider bpmSlider, speedSlider, stepSlider, octaveSlider, chanSlider;
    juce::Label bpmLabel { {}, "BPM" }, speedLabel { {}, "Speed" },
                stepLabel { {}, "Step" }, octaveLabel { {}, "Oct" }, chanLabel { {}, "Ch" };

    // song / order controls
    juce::TextButton songModeBtn { "Song Mode" }, addPatBtn { "Add Pattern" }, orderApplyBtn { "Set Order" };
    juce::Slider patSlider;
    juce::Label patLabel { {}, "Pattern" }, orderLabel { {}, "Order" };
    juce::TextEditor orderEdit;

    juce::Label statusLabel;
    PatternEditor patternEditor { engine };
    MixerView mixer { engine, [this] (juce::AudioPluginInstance* p) { showEditorFor (p); } };
    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    std::unique_ptr<PluginWindow> pluginWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
