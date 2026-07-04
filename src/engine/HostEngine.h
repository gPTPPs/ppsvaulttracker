#pragma once
#include <JuceHeader.h>

// Phase-1 host engine: AudioProcessorGraph (MIDI in -> plugin -> audio out)
// driven by an AudioProcessorPlayer.
//
// Realtime rules (no alloc / lock / IO in the audio callback) hold by
// construction here: every graph change happens on the message thread with
// the player detached from the graph. The deferred-message scheme described
// in CLAUDE.md arrives with the sequencer phase.
class HostEngine : private juce::MidiKeyboardState::Listener
{
public:
    HostEngine();
    ~HostEngine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::MidiKeyboardState&  getKeyboardState() { return keyboardState; }

    // Loads the first VST3 found in the given file, replacing any current one.
    // Returns an empty string on success, an error message otherwise.
    juce::String loadPlugin (const juce::File& vst3File);
    void unloadPlugin();

    bool hasPlugin() const { return pluginNode != nullptr; }
    juce::AudioPluginInstance* getPluginInstance() const;
    juce::String getPluginName() const { return pluginName; }

private:
    void unloadDetached();   // graph edit only — caller detaches/reattaches the player
    void handleNoteOn  (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void pushKeyboardMessage (juce::MidiMessage m);

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::MidiKeyboardState keyboardState;

    juce::AudioProcessorGraph::Node::Ptr audioOutNode, midiInNode, pluginNode;
    juce::String pluginName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostEngine)
};
