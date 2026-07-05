#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "ui/PatternEditor.h"
#include "ui/MixerView.h"
#include "ui/PillSlider.h"

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

    // close-button path: prompts when there are unsaved changes
    void confirmAndQuit();

private:
    void timerCallback() override;
    void showAudioSettings();
    void showEditorFor (juce::AudioPluginInstance*);
    void applyTempo();
    void applyOrderFromText();
    void refreshStatus();
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void syncFromEngine();   // UI <- engine after project load/new
    void updateTitle();

    HostEngine engine;

    // toolbar
    juce::TextButton audioBtn { "Audio/MIDI..." };
    juce::TextButton newBtn { "New" }, openBtn { "Open..." },
                     saveBtn { "Save" }, saveAsBtn { "Save As..." },
                     exportBtn { "Export..." }, importBtn { "Import..." };
    void importModuleFlow();
    juce::ApplicationProperties appProps;   // lame.exe path etc.
    void showExportMenu();
    void exportMidiFlow();
    void exportStemsFlow();
    void exportMasterFlow();
    void exportMp3Flow();
    void exportTracklistFlow();
    juce::File getLamePath();
    std::unique_ptr<juce::FileChooser> chooser;
    juce::uint32 lastAutosaveMs = 0;
    juce::String savedStateJson;   // last saved/loaded state, for the dirty check
    bool quitAfterSave = false;
    bool hasUnsavedChanges() { return engine.getStateAsJson() != savedStateJson; }
    void markStateSaved()    { savedStateJson = engine.getStateAsJson(); }

    // transport + edit controls
    juce::TextButton playBtn { "Play" };   // single transport button: Play <-> Stop
    juce::TextButton recBtn { "Rec" }, followBtn { "Follow" }, azertyBtn { "AZERTY" };
    juce::TextButton metroBtn { "Metro" }, precountBtn { "Pre" }, keymapBtn { "FT2" };
    PillSlider bpmSlider, speedSlider, stepSlider, octaveSlider, chanSlider;
    juce::Label bpmLabel { {}, "BPM" }, speedLabel { {}, "Speed" },
                stepLabel { {}, "Step" }, octaveLabel { {}, "Oct" }, chanLabel { {}, "Ch" };

    // song / order controls
    juce::TextButton songModeBtn { "Pattern" },   // mode selector: label = current mode
                     addPatBtn { "Add Pattern" }, orderApplyBtn { "Set Order" };
    PillSlider patSlider;
    juce::Label patLabel { {}, "Pat" }, orderLabel { {}, "Order" };   // "Pat" like the status bar
    juce::TextEditor orderEdit;

    juce::Label statusLabel;
    juce::Array<juce::Rectangle<int>> toolbarSeparators;   // computed in resized(), drawn in paint()
    PatternEditor patternEditor { engine };
    MixerView mixer { engine, [this] (juce::AudioPluginInstance* p) { showEditorFor (p); } };
    juce::Viewport mixerViewport;   // 17 strips don't fit: horizontal scroll
    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    std::unique_ptr<PluginWindow> pluginWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
