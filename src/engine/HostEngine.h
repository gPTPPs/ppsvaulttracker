#pragma once
#include <JuceHeader.h>
#include "model/Song.h"
#include "sequencer/SequencerNode.h"
#include "engine/ChannelStripNode.h"
#include "engine/MidiRouterNode.h"
#include "engine/MixLogic.h"

// Host engine, phase 4: per-channel instruments + mixer.
//
// Graph topology, per tracker channel ch (0-based):
//   midiIn ─┐
//   seq ────┴→ router[ch] (keeps MIDI ch+1) → instrument[ch] → inserts… → strip[ch] ─┐
//                                                       master inserts… → masterStrip → audioOut
//
// Structural changes (instruments, inserts, patterns, order) happen on the
// message thread with the player briefly detached. Mixer parameters
// (gain/mute/solo) and metering are lock-free atomics on the strips.
class HostEngine : private juce::MidiKeyboardState::Listener,
                   private juce::MidiInputCallback
{
public:
    static constexpr int kMixChannels = 8;
    static constexpr int kMaxInserts  = 2;

    HostEngine();
    ~HostEngine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::MidiKeyboardState&  getKeyboardState() { return keyboardState; }
    SequencerNode&            getSequencer()     { return *sequencer; }
    Song&                     getSong()          { return song; }
    Pattern*                  getEditPattern()   { return song.getPattern (sequencer->getEditPatternIndex()); }

    // live input (virtual keyboard + hardware MIDI) targets this channel
    void setLiveChannel (int ch) { liveChannel.store (juce::jlimit (0, kMixChannels - 1, ch)); }

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
    };

    std::unique_ptr<juce::AudioPluginInstance> createInstance (const juce::File&, juce::String& error);
    void rebuildConnections();   // full rewire, call detached
    void updateMuteStates();
    void handleNoteOn  (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void pushLiveMessage (juce::MidiMessage m);

    // song BEFORE graph: graph nodes hold raw pointers into it
    Song song;
    bool mutes[kMixChannels] = {}, solos[kMixChannels] = {};

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> liveChannel { 0 };

    Node::Ptr audioOutNode, midiInNode, seqNode, masterStripNode;
    Node::Ptr masterInserts[kMaxInserts];
    juce::String masterInsertNames[kMaxInserts];
    Slot slots[kMixChannels];

    SequencerNode* sequencer = nullptr;                 // owned by seqNode
    ChannelStripNode* strips[kMixChannels] = {};        // owned by slots[i].strip
    ChannelStripNode* masterStrip = nullptr;            // owned by masterStripNode

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostEngine)
};
