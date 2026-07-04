#include "engine/HostEngine.h"
#include "model/DemoPattern.h"

HostEngine::HostEngine()
{
    // JUCE 8.0.14: addDefaultFormats() was replaced by this free function
    juce::addDefaultFormatsToManager (formatManager);   // VST3 is the only format compiled in

    using IO = juce::AudioProcessorGraph::AudioGraphIOProcessor;
    audioOutNode = graph.addNode (std::make_unique<IO> (IO::audioOutputNode));
    midiInNode   = graph.addNode (std::make_unique<IO> (IO::midiInputNode));

    auto seq = std::make_unique<SequencerNode>();
    sequencer = seq.get();
    sequencer->setPattern (makeDemoPattern());   // installed before audio starts
    seqNode = graph.addNode (std::move (seq));

    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setProcessor (&graph);
    deviceManager.addAudioCallback (&player);

    // hardware MIDI: enable every available input and route it to the player
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.addMidiInputDeviceCallback (d.identifier, &player);
    }

    keyboardState.addListener (this);   // on-screen keyboard -> audio callback
}

HostEngine::~HostEngine()
{
    keyboardState.removeListener (this);
    for (auto& d : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (d.identifier, &player);
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
}

juce::String HostEngine::loadPlugin (const juce::File& vst3File)
{
    juce::OwnedArray<juce::PluginDescription> descs;
    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (vst3File.getFullPathName()))
            format->findAllTypesForFile (descs, vst3File.getFullPathName());

    if (descs.isEmpty())
        return "No VST3 plugin found in this file";

    auto* device = deviceManager.getCurrentAudioDevice();
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    const int    blockSize  = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    juce::String error;
    auto instance = formatManager.createPluginInstance (*descs[0], sampleRate, blockSize, error);
    if (instance == nullptr)
        return error.isNotEmpty() ? error : "Plugin instantiation failed";

    // swap the plugin into the graph with the audio callback detached
    player.setProcessor (nullptr);
    unloadDetached();

    pluginName = instance->getName();
    pluginNode = graph.addNode (std::move (instance));

    // live MIDI and sequencer MIDI both feed the plugin (the graph merges them)
    graph.addConnection ({ { midiInNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex },
                           { pluginNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    graph.addConnection ({ { seqNode->nodeID,    juce::AudioProcessorGraph::midiChannelIndex },
                           { pluginNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
    const int chans = juce::jmin (2, pluginNode->getProcessor()->getTotalNumOutputChannels());
    for (int ch = 0; ch < chans; ++ch)
        graph.addConnection ({ { pluginNode->nodeID, ch }, { audioOutNode->nodeID, ch } });

    player.setProcessor (&graph);
    return {};
}

void HostEngine::unloadPlugin()
{
    if (pluginNode == nullptr)
        return;
    player.setProcessor (nullptr);
    unloadDetached();
    player.setProcessor (&graph);
}

void HostEngine::unloadDetached()
{
    if (pluginNode != nullptr)
    {
        graph.removeNode (pluginNode->nodeID);
        pluginNode = nullptr;
        pluginName.clear();
    }
}

juce::AudioPluginInstance* HostEngine::getPluginInstance() const
{
    return pluginNode != nullptr
        ? dynamic_cast<juce::AudioPluginInstance*> (pluginNode->getProcessor())
        : nullptr;
}

void HostEngine::handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    pushKeyboardMessage (juce::MidiMessage::noteOn (channel, note, velocity));
}

void HostEngine::handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    pushKeyboardMessage (juce::MidiMessage::noteOff (channel, note, velocity));
}

void HostEngine::pushKeyboardMessage (juce::MidiMessage m)
{
    // the collector is only valid once a device has started
    if (deviceManager.getCurrentAudioDevice() == nullptr)
        return;
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    player.getMidiMessageCollector().addMessageToQueue (m);
}
