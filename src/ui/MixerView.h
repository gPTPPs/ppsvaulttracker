#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"

// Mixer: one strip per tracker channel (instrument, gain fader, mute/solo,
// 2 insert slots, VU) + a master strip (gain, inserts, VU).
// Click "Inst"/empty FX slot = load a VST3; click a loaded FX = open its
// editor; Ctrl+click = remove/unload.
class MixerView : public juce::Component,
                  private juce::Timer
{
public:
    MixerView (HostEngine& e, std::function<void (juce::AudioPluginInstance*)> showEditorFn);

    void paint (juce::Graphics&) override;
    void resized() override;
    void syncFromEngine();   // labels + faders + M/S states (after project load)

private:
    static constexpr int kNumStrips = HostEngine::kMixChannels + 1;   // + master
    static constexpr int kMaster = HostEngine::kMixChannels;

    struct StripUi
    {
        juce::Label title;
        juce::TextButton instBtn { "Inst..." };
        juce::TextButton fxBtn[HostEngine::kMaxInserts];
        juce::TextButton muteBtn { "M" }, soloBtn { "S" };
        juce::Slider gain;
    };

    void timerCallback() override;
    void refreshLabels();
    void loadInstrumentFor (int ch);
    void addInsertFor (int chOrMaster);
    juce::Rectangle<int> stripArea (int index) const;

    HostEngine& engine;
    std::function<void (juce::AudioPluginInstance*)> showEditor;
    StripUi strips[kNumStrips];
    float vu[kNumStrips] = {};
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
