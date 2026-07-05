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
    void mouseDown (const juce::MouseEvent&) override;   // right-click on a title = track colour
    void syncFromEngine();   // labels + faders + M/S states (after project load)

    static constexpr int kStripW = GridMetrics::kStride;   // aligned with the grid columns
    int activeChannels() const    { return engine.getSong().getNumChannels(); }
    int currentIdealWidth() const { return GridMetrics::kRowNumW + (activeChannels() + 1) * kStripW; }

private:
    static constexpr int kNumStrips = HostEngine::kMixChannels + 1;   // + master
    static constexpr int kMaster = HostEngine::kMixChannels;

    // strip innards: uniform breathing room; kFaderTop = everything above the
    // fader/VU zone (title, inst, FX slots, M/S/CC) — keep resized() and the
    // VU maths in paint() in sync
    static constexpr int kStripPad = 5;
    static constexpr int kRowGap   = 4;
    static constexpr int kFaderTop = 18 + kRowGap + 22 + kRowGap
                                   + (20 + kRowGap) * HostEngine::kMaxInserts
                                   + 22 + kRowGap;

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
    void refreshTrackTitles();   // names + colours (grid-side renames land here too)
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
