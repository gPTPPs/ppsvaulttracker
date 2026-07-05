#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "model/Song.h"
#include "sequencer/TrackerClock.h"

// MIDI-only graph node that plays the song. Pattern MIDI is generated inside
// the audio callback at exact sample offsets (see class notes below); each tracker
// channel is emitted on MIDI channel index+1 and fanned out to per-channel
// instruments by MidiRouterNode.
//
// Two play modes: Pattern (loops the edited pattern) and Song (follows the
// order list, advancing at each pattern boundary).
class SequencerNode : public juce::AudioProcessor
{
public:
    // the audio output carries ONLY the metronome/pre-count clicks; it is
    // wired straight to the device output, bypassing the mix bus
    SequencerNode()
        : AudioProcessor (BusesProperties()
                              .withOutput ("Click", juce::AudioChannelSet::stereo())) {}

    void setSong (Song* s) { song = s; }   // once, before audio starts

    // ---- transport (any thread) ----
    void play()            { playRequested.store (true); }
    void stop()            { playRequested.store (false); }
    bool isPlaying() const { return playRequested.load(); }

    void setTempo (double newBpm, int newSpeed)
    {
        bpmAtomic.store (juce::jlimit (32.0, 999.0, newBpm));
        speedAtomic.store (juce::jlimit (1, 31, newSpeed));
    }
    double getBpm() const { return bpmAtomic.load(); }
    int getSpeed() const  { return speedAtomic.load(); }

    void setSongMode (bool s)        { songMode.store (s); }
    bool isSongMode() const          { return songMode.load(); }
    void setEditPatternIndex (int i) { editPatternIdx.store (i); }
    int  getEditPatternIndex() const { return editPatternIdx.load(); }

    int getUiRow() const          { return uiRow.load(); }          // -1 when stopped
    int getUiPatternIndex() const { return uiPatternIdx.load(); }   // playing pattern
    int getUiOrderPos() const     { return uiOrderPos.load(); }     // -1 in pattern mode
    float getRowPhase() const     { return rowPhase.load(); }       // 0..1, for quantized recording

    // ---- metronome / pre-count (6b) ----
    void setMetronome (bool on)      { metroOn.store (on); }
    bool isMetronomeOn() const       { return metroOn.load(); }
    void setPrecountRows (int rows)  { precountRowsCfg.store (rows); }   // applied at next play()

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int) override { clock.prepare (sampleRate); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    const juce::String getName() const override  { return "Sequencer"; }
    bool acceptsMidi() const override            { return false; }
    bool producesMidi() const override           { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override              { return false; }
    int getNumPrograms() override                { return 1; }
    int getCurrentProgram() override             { return 0; }
    void setCurrentProgram (int) override        {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    int clampPatternIndex (int i) const
    {
        return juce::jlimit (0, juce::jmax (0, song->getNumPatterns() - 1), i);
    }

    TrackerClock clock;
    Song* song = nullptr;

    std::atomic<bool>   playRequested { false };
    std::atomic<bool>   songMode { false };
    std::atomic<double> bpmAtomic { 125.0 };
    std::atomic<int>    speedAtomic { 6 };
    std::atomic<int>    editPatternIdx { 0 };
    std::atomic<int>    uiRow { -1 };
    std::atomic<int>    uiPatternIdx { 0 };
    std::atomic<int>    uiOrderPos { -1 };
    std::atomic<float>  rowPhase { 0.0f };
    std::atomic<bool>   metroOn { false };
    std::atomic<int>    precountRowsCfg { 0 };

    void triggerClick (int offset, bool accent);
    void renderClicks (juce::AudioBuffer<float>&);

    // audio thread only
    bool wasPlaying = false;
    bool firstRowPending = true;
    int  curOrderPos = 0;
    int  curPattern = 0;
    int  activeNotes[Pattern::kMaxChannels] = {};   // 0 = none, else note + 1

    // note events pushed past the current block by Nxx (delay) / Kxx (cut)
    struct PendingMidi { int samplesUntil; bool noteOn; uint8_t midiCh, note, vel; };
    static constexpr int kMaxPendingMidi = 64;
    PendingMidi pendingMidi[kMaxPendingMidi];
    int numPendingMidi = 0;
    void flushPending (juce::MidiBuffer&, int numSamples);
    void cancelPending (juce::MidiBuffer&);   // on stop: emit note-offs now, drop note-ons

    // click synth + pre-count (audio thread only)
    struct PendingClick { int offset; bool accent; };
    PendingClick pendingClicks[8];
    int    numPendingClicks = 0;
    int    clickSamplesLeft = 0;
    double clickPhase = 0.0, clickFreq = 880.0;
    int    precountLeft = 0;
    double precountCounter = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerNode)
};
