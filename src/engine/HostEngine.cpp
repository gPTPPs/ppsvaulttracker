#include "engine/HostEngine.h"
#include "model/DemoPattern.h"

HostEngine::HostEngine()
{
    // JUCE 8.0.14: addDefaultFormats() was replaced by this free function
    juce::addDefaultFormatsToManager (formatManager);   // VST3 is the only format compiled in

    using IO = juce::AudioProcessorGraph::AudioGraphIOProcessor;
    audioOutNode = graph.addNode (std::make_unique<IO> (IO::audioOutputNode));
    midiInNode   = graph.addNode (std::make_unique<IO> (IO::midiInputNode));

    // demo content: pattern 0 = the synthwave bassline
    if (auto* p0 = song.getPattern (0))
        *p0 = *makeDemoPattern();

    auto seq = std::make_unique<SequencerNode>();
    sequencer = seq.get();
    sequencer->setSong (&song);
    seqNode = graph.addNode (std::move (seq));

    for (int ch = 0; ch < kMixChannels; ++ch)
    {
        slots[ch].router = graph.addNode (std::make_unique<MidiRouterNode> (ch));
        auto strip = std::make_unique<ChannelStripNode>();
        strips[ch] = strip.get();
        slots[ch].strip = graph.addNode (std::move (strip));
    }

    auto master = std::make_unique<ChannelStripNode>();
    masterStrip = master.get();
    masterStripNode = graph.addNode (std::move (master));

    rebuildConnections();

    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setProcessor (&graph);
    deviceManager.addAudioCallback (&player);

    // hardware MIDI comes through US (not the player) so we can retarget it
    // to the channel under the edit cursor
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.addMidiInputDeviceCallback (d.identifier, this);
    }

    keyboardState.addListener (this);
}

HostEngine::~HostEngine()
{
    keyboardState.removeListener (this);
    for (auto& d : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (d.identifier, this);
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
}

// ---------------------------------------------------------------- graph

void HostEngine::rebuildConnections()
{
    for (auto& c : graph.getConnections())
        graph.removeConnection (c);

    constexpr auto midiIdx = juce::AudioProcessorGraph::midiChannelIndex;

    for (int ch = 0; ch < kMixChannels; ++ch)
    {
        auto& s = slots[ch];

        graph.addConnection ({ { midiInNode->nodeID, midiIdx }, { s.router->nodeID, midiIdx } });
        graph.addConnection ({ { seqNode->nodeID,    midiIdx }, { s.router->nodeID, midiIdx } });

        if (s.instrument == nullptr)
            continue;

        graph.addConnection ({ { s.router->nodeID, midiIdx }, { s.instrument->nodeID, midiIdx } });

        // audio: instrument -> inserts... -> strip
        Node::Ptr prev = s.instrument;
        for (auto& ins : s.inserts)
            if (ins != nullptr)
            {
                for (int a = 0; a < 2; ++a)
                    graph.addConnection ({ { prev->nodeID, a }, { ins->nodeID, a } });
                prev = ins;
            }
        for (int a = 0; a < 2; ++a)
            graph.addConnection ({ { prev->nodeID, a }, { s.strip->nodeID, a } });
    }

    // strips -> master inserts... -> master strip -> out
    Node::Ptr masterHead = masterStripNode;
    for (int i = kMaxInserts; --i >= 0;)
        if (masterInserts[i] != nullptr)
        {
            for (int a = 0; a < 2; ++a)
                graph.addConnection ({ { masterInserts[i]->nodeID, a },
                                       { masterHead->nodeID, a } });
            masterHead = masterInserts[i];
        }

    for (int ch = 0; ch < kMixChannels; ++ch)
        for (int a = 0; a < 2; ++a)
            graph.addConnection ({ { slots[ch].strip->nodeID, a }, { masterHead->nodeID, a } });

    for (int a = 0; a < 2; ++a)
        graph.addConnection ({ { masterStripNode->nodeID, a }, { audioOutNode->nodeID, a } });
}

std::unique_ptr<juce::AudioPluginInstance> HostEngine::createInstance (const juce::File& file,
                                                                       juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> descs;
    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (file.getFullPathName()))
            format->findAllTypesForFile (descs, file.getFullPathName());

    if (descs.isEmpty())
    {
        error = "No VST3 plugin found in this file";
        return nullptr;
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    const double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    const int    blockSize  = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    auto instance = formatManager.createPluginInstance (*descs[0], sampleRate, blockSize, error);
    if (instance == nullptr && error.isEmpty())
        error = "Plugin instantiation failed";
    return instance;
}

juce::String HostEngine::loadInstrument (int ch, const juce::File& file)
{
    juce::String error;
    auto instance = createInstance (file, error);
    if (instance == nullptr)
        return error;

    ScopedDetach detach (*this);
    auto& s = slots[ch];
    if (s.instrument != nullptr)
        graph.removeNode (s.instrument->nodeID);
    s.instrumentName = instance->getName();
    s.instrument = graph.addNode (std::move (instance));
    rebuildConnections();
    return {};
}

void HostEngine::unloadInstrument (int ch)
{
    auto& s = slots[ch];
    if (s.instrument == nullptr)
        return;
    ScopedDetach detach (*this);
    graph.removeNode (s.instrument->nodeID);
    s.instrument = nullptr;
    s.instrumentName.clear();
    rebuildConnections();
}

juce::String HostEngine::addInsert (int chOrMaster, const juce::File& file)
{
    auto* nodes = chOrMaster < 0 ? masterInserts : slots[chOrMaster].inserts;
    auto* names = chOrMaster < 0 ? masterInsertNames : slots[chOrMaster].insertNames;

    int slot = -1;
    for (int i = 0; i < kMaxInserts; ++i)
        if (nodes[i] == nullptr) { slot = i; break; }
    if (slot < 0)
        return "All insert slots are full";

    juce::String error;
    auto instance = createInstance (file, error);
    if (instance == nullptr)
        return error;

    ScopedDetach detach (*this);
    names[slot] = instance->getName();
    nodes[slot] = graph.addNode (std::move (instance));
    rebuildConnections();
    return {};
}

void HostEngine::removeInsert (int chOrMaster, int index)
{
    auto* nodes = chOrMaster < 0 ? masterInserts : slots[chOrMaster].inserts;
    auto* names = chOrMaster < 0 ? masterInsertNames : slots[chOrMaster].insertNames;
    if (index < 0 || index >= kMaxInserts || nodes[index] == nullptr)
        return;

    ScopedDetach detach (*this);
    graph.removeNode (nodes[index]->nodeID);
    for (int i = index; i < kMaxInserts - 1; ++i)   // keep the chain compact
    {
        nodes[i] = nodes[i + 1];
        names[i] = names[i + 1];
    }
    nodes[kMaxInserts - 1] = nullptr;
    names[kMaxInserts - 1].clear();
    rebuildConnections();
}

int HostEngine::addPattern()
{
    ScopedDetach detach (*this);
    return song.addPattern();
}

void HostEngine::applyOrder (const int* entries, int count)
{
    ScopedDetach detach (*this);
    song.orderLen = juce::jlimit (1, Song::kMaxOrder, count);
    for (int i = 0; i < song.orderLen; ++i)
        song.order[i] = juce::jlimit (0, song.getNumPatterns() - 1, entries[i]);
}

void HostEngine::setNumChannels (int n)
{
    song.setNumChannels (juce::jlimit (1, kMixChannels, n));
}

// ---------------------------------------------------------------- mixer

void  HostEngine::setChannelGain (int ch, float g) { strips[ch]->setGain (g); }
float HostEngine::getChannelGain (int ch) const    { return strips[ch]->getGain(); }
float HostEngine::readChannelPeak (int ch)         { return strips[ch]->readAndResetPeak(); }
void  HostEngine::setMasterGain (float g)          { masterStrip->setGain (g); }
float HostEngine::getMasterGain() const            { return masterStrip->getGain(); }
float HostEngine::readMasterPeak()                 { return masterStrip->readAndResetPeak(); }

void HostEngine::setChannelMute (int ch, bool m)
{
    mutes[ch] = m;
    updateMuteStates();
}

void HostEngine::setChannelSolo (int ch, bool s)
{
    solos[ch] = s;
    updateMuteStates();
}

void HostEngine::updateMuteStates()
{
    bool effective[kMixChannels];
    computeEffectiveMutes (mutes, solos, kMixChannels, effective);
    for (int ch = 0; ch < kMixChannels; ++ch)
        strips[ch]->setEffectiveMute (effective[ch]);
}

juce::AudioPluginInstance* HostEngine::getInstrument (int ch) const
{
    return slots[ch].instrument != nullptr
        ? dynamic_cast<juce::AudioPluginInstance*> (slots[ch].instrument->getProcessor())
        : nullptr;
}

juce::AudioPluginInstance* HostEngine::getInsert (int chOrMaster, int index) const
{
    auto* nodes = chOrMaster < 0 ? masterInserts : slots[chOrMaster].inserts;
    return nodes[index] != nullptr
        ? dynamic_cast<juce::AudioPluginInstance*> (nodes[index]->getProcessor())
        : nullptr;
}

juce::String HostEngine::getInsertName (int chOrMaster, int index) const
{
    return chOrMaster < 0 ? masterInsertNames[index] : slots[chOrMaster].insertNames[index];
}

// ---------------------------------------------------------------- live MIDI

void HostEngine::handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    pushLiveMessage (juce::MidiMessage::noteOn (channel, note, velocity));
}

void HostEngine::handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    pushLiveMessage (juce::MidiMessage::noteOff (channel, note, velocity));
}

void HostEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    // hardware plays the channel under the edit cursor
    juce::MidiMessage remapped (m);
    if (remapped.getChannel() > 0)
        remapped.setChannel (liveChannel.load() + 1);
    if (deviceManager.getCurrentAudioDevice() != nullptr)
        player.getMidiMessageCollector().addMessageToQueue (remapped);
}

void HostEngine::pushLiveMessage (juce::MidiMessage m)
{
    if (deviceManager.getCurrentAudioDevice() == nullptr)
        return;
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    player.getMidiMessageCollector().addMessageToQueue (m);
}
