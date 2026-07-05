#pragma once
#include <JuceHeader.h>
#include "engine/HostEngine.h"
#include "ui/GridMetrics.h"

// Mixer: one strip per ACTIVE tracker channel + master, column-aligned with
// the pattern grid (same stride and left offset; the parent viewport scrolls
// in sync with the grid). Click "Inst"/empty FX slot = load a VST3; click a
// loaded one = menu (open editor / remove).
class MixerView : public juce::Component,
                  private juce::Timer
{
public:
    MixerView (HostEngine& e, std::function<void (juce::AudioPluginInstance*)> showEditorFn);

    void paint (juce::Graphics&) override;
    void resized() override;
    void syncFromEngine();   // labels + faders + M/S states (after project load)

    static constexpr int kStripW = GridMetrics::kStride;   // aligned with the grid columns
    int activeChannels() const    { return engine.getSong().getNumChannels(); }
    int currentIdealWidth() const { return GridMetrics::kRowNumW + (activeChannels() + 1) * kStripW; }

private:
    static constexpr int kNumStrips = HostEngine::kMixChannels + 1;   // + master
    static constexpr int kMaster = HostEngine::kMixChannels;

    struct StripUi
    {
        juce::Label title;
        juce::TextButton instBtn { "Inst..." };
        juce::TextButton fxBtn[HostEngine::kMaxInserts];
        juce::TextButton muteBtn { "M" }, soloBtn { "S" }, ccBtn { "CC" };
        juce::Slider gain;
    };

    void timerCallback() override;
    void refreshLabels();
    void showCcSlotsFor (int ch);
    void loadInstrumentFor (int ch);
    void addInsertFor (int chOrMaster);
    void showPluginMenu (juce::AudioPluginInstance*, std::function<void()> removeFn, juce::Component* target);
    juce::Rectangle<int> stripArea (int index) const;

    HostEngine& engine;
    std::function<void (juce::AudioPluginInstance*)> showEditor;
    StripUi strips[kNumStrips];
    float vu[kNumStrips] = {};
    int lastActive = -1;   // relayout when the channel count changes
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
