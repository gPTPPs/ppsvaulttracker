#pragma once
#include <JuceHeader.h>
#include "model/Song.h"
#include "sequencer/SequencerNode.h"
#include "engine/ChannelStripNode.h"
#include "engine/MidiRouterNode.h"
#include "engine/MixLogic.h"

// Host engine: per-channel instruments + mixer + .ubt project I/O.
//
// Graph topology, per tracker channel ch (0-based):
//   midiIn ─┐
//   seq ────┴→ router[ch] (keeps MIDI ch+1) → instrument[ch] → inserts… → strip[ch] ─┐
//                                                       master inserts… → masterStrip → audioOut
//
// Structural changes (instruments, inserts, patterns, order, project load)
// happen on the message thread with the player briefly detached. Mixer
// parameters (gain/mute/solo) and metering are lock-free atomics.
class HostEngine : private juce::MidiKeyboardState::Listener,
                   private juce::MidiInputCallback
{
public:
    static constexpr int kMixChannels = 16;   // hard ceiling: 1 track = 1 MIDI channel
    static constexpr int kMaxInserts  = 2;
    static constexpr int kMaxStateBytes = 32 * 1024 * 1024;   // plugin state cap on load
    static constexpr int kMaxBackups  = 10;

    HostEngine();
    ~HostEngine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::MidiKeyboardState&  getKeyboardState() { return keyboardState; }
    SequencerNode&            getSequencer()     { return *sequencer; }
    Song&                     getSong()          { return song; }
    Pattern*                  getEditPattern()   { return song.getPattern (sequencer->getEditPatternIndex()); }

    // live input (virtual keyboard + hardware MIDI) targets this channel
    void setLiveChannel (int ch) { liveChannel.store (juce::jlimit (0, kMixChannels - 1, ch)); }

    // ---- live recording / metronome (6b) ----
    void setLiveRecording (bool on)   { liveRec.store (on); }
    void setMetronome (bool on)       { sequencer->setMetronome (on); }
    void setPrecountEnabled (bool on) { precountEnabled = on; }
    void startPlayback();             // arms the pre-count when Rec is on
    void stopPlayback()               { sequencer->stop(); }

    // ---- project I/O (.ubt folder) ----
    juce::String saveProject (const juce::File& ubtDir);                           // {} on success
    juce::String loadProject (const juce::File& ubtDir, juce::StringArray& warnings);
    void newProject();
    void autosaveBackup();     // rotating song.json snapshot into backups/
    bool hasProject() const          { return projectDir != juce::File(); }
    juce::File getProjectDir() const { return projectDir; }
    juce::String getStateAsJson()    { return juce::JSON::toString (buildProjectVar (false)); }   // dirty-check

    // ---- exports (5b) ----
    // progress callback: return false to cancel. Render functions may run on
    // a worker thread (the player is detached for the duration).
    using Progress = std::function<bool (double)>;
    juce::String exportMidi (const juce::File& dest);
    juce::String exportTracklist (const juce::File& destTxt);              // writes .txt + .json sibling
    juce::String renderStems (const juce::File& destDir, Progress p);      // one WAV per channel + master
    juce::String renderMasterWav (const juce::File& dest, Progress p);
    juce::String exportMp3 (const juce::File& dest, const juce::File& lameExe, Progress p);

    // ---- import (5c): module -> Song, instruments stay loaded ----
    juce::String importModule (const juce::File& modFile, juce::StringArray& warnings);

    // ---- structural operations (message thread; audio briefly detached) ----
    juce::String loadInstrument (int ch, const juce::File& vst3File);
    void unloadInstrument (int ch);
    juce::String addInsert (int chOrMaster, const juce::File& vst3File);   // -1 = master bus
    void removeInsert (int chOrMaster, int index);
    int  addPattern();                                                     // -1 when full
    void applyOrder (const int* entries, int count);
    void setNumChannels (int n);

    // ---- mixer (lock-free) ----
    void  setChannelGain (int ch, float g);
    float getChannelGain (int ch) const;
    void  setChannelMute (int ch, bool m);
    void  setChannelSolo (int ch, bool s);
    bool  getChannelMute (int ch) const { return mutes[ch]; }
    bool  getChannelSolo (int ch) const { return solos[ch]; }
    float readChannelPeak (int ch);
    void  setMasterGain (float g);
    float getMasterGain() const;
    float readMasterPeak();

    juce::AudioPluginInstance* getInstrument (int ch) const;
    juce::String getInstrumentName (int ch) const  { return slots[ch].instrumentName; }
    juce::AudioPluginInstance* getInsert (int chOrMaster, int index) const;
    juce::String getInsertName (int chOrMaster, int index) const;

private:
    using Node = juce::AudioProcessorGraph::Node;

    // detaches the player for the lifetime of a structural change
    struct ScopedDetach
    {
        explicit ScopedDetach (HostEngine& e) : eng (e) { eng.player.setProcessor (nullptr); }
        ~ScopedDetach()                                 { eng.player.setProcessor (&eng.graph); }
        HostEngine& eng;
    };

    struct Slot
    {
        Node::Ptr router, instrument, strip;
        Node::Ptr inserts[kMaxInserts];
        juce::String instrumentName, insertNames[kMaxInserts];
        juce::PluginDescription instrumentDesc, insertDescs[kMaxInserts];
    };

    std::unique_ptr<juce::AudioPluginInstance> createInstance (const juce::File&, juce::String& error,
                                                               juce::PluginDescription* usedDesc = nullptr);
    void rebuildConnections();   // full rewire, call detached
    void updateMuteStates();
    void unloadAllPluginsDetached();
    juce::var buildProjectVar (bool writeStates);   // writeStates -> plugins/*.state next to projectDir
    juce::String renderOffline (const juce::File& wavFile, int soloChannel /* -1 = full mix */,
                                double sampleRate, int bitDepth, const Progress& progress);
    juce::String restorePlugins (const juce::var& root, juce::StringArray& warnings);   // call detached
    void handleNoteOn  (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void pushLiveMessage (juce::MidiMessage m);
    void recordLiveEvent (const juce::MidiMessage& m);   // quantized write into the playing pattern

    // song BEFORE graph: graph nodes hold raw pointers into it
    Song song;
    bool mutes[kMixChannels] = {}, solos[kMixChannels] = {};
    juce::File projectDir;

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> liveChannel { 0 };
    std::atomic<bool> liveRec { false };
    bool precountEnabled = false;   // message thread only

    Node::Ptr audioOutNode, midiInNode, seqNode, masterStripNode;
    Node::Ptr masterInserts[kMaxInserts];
    juce::String masterInsertNames[kMaxInserts];
    juce::PluginDescription masterInsertDescs[kMaxInserts];
    Slot slots[kMixChannels];

    SequencerNode* sequencer = nullptr;                 // owned by seqNode
    ChannelStripNode* strips[kMixChannels] = {};        // owned by slots[i].strip
    ChannelStripNode* masterStrip = nullptr;            // owned by masterStripNode

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostEngine)
};
