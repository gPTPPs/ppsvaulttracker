#pragma once
#include <JuceHeader.h>

// Boilerplate reducer for lightweight internal graph nodes.
class InternalNodeBase : public juce::AudioProcessor
{
public:
    using AudioProcessor::AudioProcessor;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override                     { return false; }
    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    double getTailLengthSeconds() const override        { return 0.0; }
    void releaseResources() override                    {}
};
