#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "model/Pattern.h"
#include "sequencer/TrackerClock.h"

// MIDI-only graph node that plays the current pattern in a loop.
//
// Architecture (amendment validated 2026-07-04): pattern MIDI is generated
// INSIDE the audio callback at exact sample offsets. Transport, tempo and the
// playhead cross the thread boundary as lock-free atomics — no locks and no
// allocation on the audio thread.
//
// Phase-2 constraint: setPattern() may only be called from the message thread
// while the engine is not yet running (the demo pattern is installed once at
// startup). The deferred-update path for live edits arrives with the editor.
class SequencerNode : public juce::AudioProcessor
{
public:
    SequencerNode() : AudioProcessor (BusesProperties()) {}

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
    int getUiRow() const  { return uiRow.load(); }   // -1 when stopped

    void setPattern (std::unique_ptr<Pattern> p) { pattern = std::move (p); }
    const Pattern* getPattern() const            { return pattern.get(); }

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
    TrackerClock clock;
    std::unique_ptr<Pattern> pattern;

    std::atomic<bool>   playRequested { false };
    std::atomic<double> bpmAtomic { 125.0 };
    std::atomic<int>    speedAtomic { 6 };
    std::atomic<int>    uiRow { -1 };

    bool wasPlaying = false;   // audio thread only
    int  activeNote = -1;      // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerNode)
};
