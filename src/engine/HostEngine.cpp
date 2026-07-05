#include "engine/HostEngine.h"
#include "model/DemoPattern.h"
#include "io/ProjectIO.h"
#include "io/MidiExport.h"
#include "io/ModImport.h"

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
        // MIDI: the router also feeds each insert, so the effect column can
        // automate FX plugins by CC (inserts without a MIDI input just ignore it)
        Node::Ptr prev = s.instrument;
        for (auto& ins : s.inserts)
            if (ins != nullptr)
            {
                graph.addConnection ({ { s.router->nodeID, midiIdx }, { ins->nodeID, midiIdx } });
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

    // metronome clicks: straight to the device, bypassing the mix bus
    for (int a = 0; a < 2; ++a)
        graph.addConnection ({ { seqNode->nodeID, a }, { audioOutNode->nodeID, a } });
}

std::unique_ptr<juce::AudioPluginInstance> HostEngine::createInstance (const juce::File& file,
                                                                       juce::String& error,
                                                                       juce::PluginDescription* usedDesc)
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
    if (instance != nullptr && usedDesc != nullptr)
        *usedDesc = *descs[0];
    return instance;
}

juce::String HostEngine::loadInstrument (int ch, const juce::File& file)
{
    autosaveBackup();   // design rule: always autosave before instantiating a plugin

    juce::PluginDescription desc;
    juce::String error;
    auto instance = createInstance (file, error, &desc);
    if (instance == nullptr)
        return error;

    ScopedDetach detach (*this);
    auto& s = slots[ch];
    if (s.instrument != nullptr)
        graph.removeNode (s.instrument->nodeID);
    s.instrumentName = instance->getName();
    s.instrumentDesc = desc;
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
    s.instrumentDesc = {};
    rebuildConnections();
}

juce::String HostEngine::addInsert (int chOrMaster, const juce::File& file)
{
    autosaveBackup();

    auto* nodes = chOrMaster < 0 ? masterInserts : slots[chOrMaster].inserts;
    auto* names = chOrMaster < 0 ? masterInsertNames : slots[chOrMaster].insertNames;
    auto* descs = chOrMaster < 0 ? masterInsertDescs : slots[chOrMaster].insertDescs;

    int slot = -1;
    for (int i = 0; i < kMaxInserts; ++i)
        if (nodes[i] == nullptr) { slot = i; break; }
    if (slot < 0)
        return "All insert slots are full";

    juce::PluginDescription desc;
    juce::String error;
    auto instance = createInstance (file, error, &desc);
    if (instance == nullptr)
        return error;

    ScopedDetach detach (*this);
    names[slot] = instance->getName();
    descs[slot] = desc;
    nodes[slot] = graph.addNode (std::move (instance));
    rebuildConnections();
    return {};
}

void HostEngine::removeInsert (int chOrMaster, int index)
{
    auto* nodes = chOrMaster < 0 ? masterInserts : slots[chOrMaster].inserts;
    auto* names = chOrMaster < 0 ? masterInsertNames : slots[chOrMaster].insertNames;
    auto* descs = chOrMaster < 0 ? masterInsertDescs : slots[chOrMaster].insertDescs;
    if (index < 0 || index >= kMaxInserts || nodes[index] == nullptr)
        return;

    ScopedDetach detach (*this);
    graph.removeNode (nodes[index]->nodeID);
    for (int i = index; i < kMaxInserts - 1; ++i)   // keep the chain compact
    {
        nodes[i] = nodes[i + 1];
        names[i] = names[i + 1];
        descs[i] = descs[i + 1];
    }
    nodes[kMaxInserts - 1] = nullptr;
    names[kMaxInserts - 1].clear();
    descs[kMaxInserts - 1] = {};
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

// ---------------------------------------------------------------- project I/O

void HostEngine::unloadAllPluginsDetached()
{
    for (int ch = 0; ch < kMixChannels; ++ch)
    {
        auto& s = slots[ch];
        if (s.instrument != nullptr)
            graph.removeNode (s.instrument->nodeID);
        s.instrument = nullptr;
        s.instrumentName.clear();
        s.instrumentDesc = {};
        for (int i = 0; i < kMaxInserts; ++i)
        {
            if (s.inserts[i] != nullptr)
                graph.removeNode (s.inserts[i]->nodeID);
            s.inserts[i] = nullptr;
            s.insertNames[i].clear();
            s.insertDescs[i] = {};
        }
        strips[ch]->setGain (1.0f);
        mutes[ch] = solos[ch] = false;
    }
    for (int i = 0; i < kMaxInserts; ++i)
    {
        if (masterInserts[i] != nullptr)
            graph.removeNode (masterInserts[i]->nodeID);
        masterInserts[i] = nullptr;
        masterInsertNames[i].clear();
        masterInsertDescs[i] = {};
    }
    masterStrip->setGain (1.0f);
    updateMuteStates();
}

juce::var HostEngine::buildProjectVar (bool writeStates)
{
    auto pluginsDir = projectDir.getChildFile ("plugins");
    if (writeStates)
        pluginsDir.createDirectory();

    auto pluginRef = [&] (const juce::PluginDescription& desc, Node::Ptr node,
                          const juce::String& stateFileName) -> juce::var
    {
        if (node == nullptr)
            return {};
        auto* o = new juce::DynamicObject();
        o->setProperty ("path", desc.fileOrIdentifier);
        o->setProperty ("name", desc.name);
        o->setProperty ("uid",  desc.createIdentifierString());
        o->setProperty ("state", stateFileName);
        if (writeStates)
            if (auto* inst = dynamic_cast<juce::AudioPluginInstance*> (node->getProcessor()))
            {
                juce::MemoryBlock mb;
                inst->getStateInformation (mb);
                pluginsDir.getChildFile (stateFileName).replaceWithData (mb.getData(), mb.getSize());
            }
        return juce::var (o);
    };

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", ProjectIO::kFormatTag);
    root->setProperty ("bpm",   sequencer->getBpm());
    root->setProperty ("speed", sequencer->getSpeed());
    root->setProperty ("song",  ProjectIO::songToVar (song));

    juce::Array<juce::var> channels;
    for (int ch = 0; ch < kMixChannels; ++ch)
    {
        auto& s = slots[ch];
        auto* o = new juce::DynamicObject();
        o->setProperty ("gain", strips[ch]->getGain());
        o->setProperty ("mute", mutes[ch]);
        o->setProperty ("solo", solos[ch]);
        o->setProperty ("instrument",
                        pluginRef (s.instrumentDesc, s.instrument,
                                   "ch" + juce::String (ch) + "-inst.state"));
        juce::Array<juce::var> ins;
        for (int i = 0; i < kMaxInserts; ++i)
            ins.add (pluginRef (s.insertDescs[i], s.inserts[i],
                                "ch" + juce::String (ch) + "-fx" + juce::String (i) + ".state"));
        o->setProperty ("inserts", juce::var (ins));
        channels.add (juce::var (o));
    }
    root->setProperty ("channels", juce::var (channels));

    auto* m = new juce::DynamicObject();
    m->setProperty ("gain", masterStrip->getGain());
    juce::Array<juce::var> mins;
    for (int i = 0; i < kMaxInserts; ++i)
        mins.add (pluginRef (masterInsertDescs[i], masterInserts[i],
                             "master-fx" + juce::String (i) + ".state"));
    m->setProperty ("inserts", juce::var (mins));
    root->setProperty ("master", juce::var (m));

    return juce::var (root);
}

juce::String HostEngine::saveProject (const juce::File& ubtDir)
{
    if (! ubtDir.createDirectory().wasOk() && ! ubtDir.isDirectory())
        return "Cannot create project folder";
    ubtDir.getChildFile ("backups").createDirectory();
    projectDir = ubtDir;

    const auto root = buildProjectVar (true);
    if (! ubtDir.getChildFile ("song.json").replaceWithText (juce::JSON::toString (root)))
        return "Cannot write song.json";
    return {};
}

juce::String HostEngine::restorePlugins (const juce::var& root, juce::StringArray& warnings)
{
    auto pluginsDir = projectDir.getChildFile ("plugins");

    auto loadRef = [&] (const juce::var& ref, const juce::String& what)
        -> std::pair<std::unique_ptr<juce::AudioPluginInstance>, juce::PluginDescription>
    {
        if (! ref.isObject())
            return {};

        const juce::String path = ref.getProperty ("path", juce::String()).toString();
        const juce::String uid  = ref.getProperty ("uid",  juce::String()).toString();
        juce::File file (path);
        if (! file.exists())
        {
            warnings.add (what + ": plugin not found (" + path + ")");
            return {};
        }

        juce::PluginDescription desc;
        juce::String error;
        auto instance = createInstance (file, error, &desc);
        if (instance == nullptr)
        {
            warnings.add (what + ": " + error);
            return {};
        }
        if (uid.isNotEmpty() && desc.createIdentifierString() != uid)
            warnings.add (what + ": plugin identity changed (" + desc.name + ")");

        const juce::String stateName = ref.getProperty ("state", juce::String()).toString();
        if (stateName.isNotEmpty())
        {
            auto stateFile = pluginsDir.getChildFile (stateName);
            // never follow a path outside plugins/ from a hostile song.json
            if (! stateFile.getFullPathName().startsWith (pluginsDir.getFullPathName()))
                warnings.add (what + ": suspicious state path ignored (" + stateName + ")");
            else if (stateFile.existsAsFile())
            {
                if (stateFile.getSize() > (juce::int64) kMaxStateBytes)
                    warnings.add (what + ": state file too large, skipped");
                else
                {
                    juce::MemoryBlock mb;
                    if (stateFile.loadFileAsData (mb) && mb.getSize() > 0)
                        instance->setStateInformation (mb.getData(), (int) mb.getSize());
                }
            }
        }
        return { std::move (instance), desc };
    };

    const auto* channels = root.getProperty ("channels", {}).getArray();
    if (channels != nullptr)
    {
        for (int ch = 0; ch < juce::jmin (kMixChannels, channels->size()); ++ch)
        {
            const auto& cv = channels->getReference (ch);
            if (! cv.isObject())
                continue;
            auto& s = slots[ch];

            strips[ch]->setGain (juce::jlimit (0.0f, 1.5f, (float) (double) cv.getProperty ("gain", 1.0)));
            mutes[ch] = (bool) cv.getProperty ("mute", false);
            solos[ch] = (bool) cv.getProperty ("solo", false);

            auto [inst, desc] = loadRef (cv.getProperty ("instrument", {}), "CH" + juce::String (ch + 1));
            if (inst != nullptr)
            {
                s.instrumentName = inst->getName();
                s.instrumentDesc = desc;
                s.instrument = graph.addNode (std::move (inst));
            }

            if (const auto* ins = cv.getProperty ("inserts", {}).getArray())
                for (int i = 0, slot = 0; i < ins->size() && slot < kMaxInserts; ++i)
                {
                    auto [fx, fdesc] = loadRef (ins->getReference (i),
                                                "CH" + juce::String (ch + 1) + " FX" + juce::String (i + 1));
                    if (fx != nullptr)
                    {
                        s.insertNames[slot] = fx->getName();
                        s.insertDescs[slot] = fdesc;
                        s.inserts[slot] = graph.addNode (std::move (fx));
                        ++slot;
                    }
                }
        }
    }

    const auto mv = root.getProperty ("master", {});
    if (mv.isObject())
    {
        masterStrip->setGain (juce::jlimit (0.0f, 1.5f, (float) (double) mv.getProperty ("gain", 1.0)));
        if (const auto* ins = mv.getProperty ("inserts", {}).getArray())
            for (int i = 0, slot = 0; i < ins->size() && slot < kMaxInserts; ++i)
            {
                auto [fx, fdesc] = loadRef (ins->getReference (i), "Master FX" + juce::String (i + 1));
                if (fx != nullptr)
                {
                    masterInsertNames[slot] = fx->getName();
                    masterInsertDescs[slot] = fdesc;
                    masterInserts[slot] = graph.addNode (std::move (fx));
                    ++slot;
                }
            }
    }

    updateMuteStates();
    return {};
}

juce::String HostEngine::loadProject (const juce::File& ubtDir, juce::StringArray& warnings)
{
    auto jsonFile = ubtDir.getChildFile ("song.json");
    if (! jsonFile.existsAsFile())
        return "song.json not found in " + ubtDir.getFullPathName();

    const auto root = juce::JSON::parse (jsonFile.loadFileAsString());
    if (! root.isObject())
        return "song.json: invalid JSON";
    if (root.getProperty ("format", juce::String()).toString() != ProjectIO::kFormatTag)
        return "song.json: unknown format";

    Song loaded;
    if (auto err = ProjectIO::songFromVar (root.getProperty ("song", {}), loaded); err.isNotEmpty())
        return err;

    if (loaded.getNumChannels() > kMixChannels)
    {
        warnings.add ("project uses " + juce::String (loaded.getNumChannels())
                      + " channels, mixer handles " + juce::String (kMixChannels));
        loaded.setNumChannels (kMixChannels);
    }

    sequencer->stop();
    ScopedDetach detach (*this);

    unloadAllPluginsDetached();
    song = std::move (loaded);
    sequencer->setSong (&song);   // same address, but keeps intent explicit
    sequencer->setEditPatternIndex (0);
    sequencer->setTempo ((double) root.getProperty ("bpm", 125.0),
                         (int) root.getProperty ("speed", 6));

    projectDir = ubtDir;
    restorePlugins (root, warnings);
    rebuildConnections();
    return {};
}

void HostEngine::newProject()
{
    sequencer->stop();
    ScopedDetach detach (*this);
    unloadAllPluginsDetached();
    song = Song();
    sequencer->setSong (&song);
    sequencer->setEditPatternIndex (0);
    sequencer->setTempo (125.0, 6);
    projectDir = juce::File();
    rebuildConnections();
}

void HostEngine::autosaveBackup()
{
    if (! hasProject())
        return;

    auto backups = projectDir.getChildFile ("backups");
    backups.createDirectory();
    backups.getChildFile ("song-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".json")
           .replaceWithText (juce::JSON::toString (buildProjectVar (false)));

    auto files = backups.findChildFiles (juce::File::findFiles, false, "song-*.json");
    std::sort (files.begin(), files.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
    while (files.size() > kMaxBackups)
    {
        files.getReference (0).deleteFile();
        files.remove (0);
    }
}

// ---------------------------------------------------------------- exports

// custom track name if set, empty string otherwise
static juce::String customTrackName (const Song& song, int ch)
{
    if (ch >= 0 && ch < Song::kCcTracks && ! song.trackNames[ch].empty())
        return juce::String::fromUTF8 (song.trackNames[ch].c_str());
    return {};
}

juce::String HostEngine::exportMidi (const juce::File& dest)
{
    juce::StringArray names;
    for (int ch = 0; ch < song.getNumChannels(); ++ch)
    {
        const auto custom = customTrackName (song, ch);
        names.add (custom.isNotEmpty() ? custom : slots[ch].instrumentName);
    }
    return MidiExport::writeMidiFile (song, sequencer->getBpm(), sequencer->getSpeed(), names, dest);
}

juce::String HostEngine::exportTracklist (const juce::File& destTxt)
{
    juce::String txt = "PPsVaultTracker tracklist";
    if (hasProject())
        txt += " - " + projectDir.getFileNameWithoutExtension();
    txt += "\n\n";

    juce::Array<juce::var> jsonTracks;
    for (int ch = 0; ch < song.getNumChannels(); ++ch)
    {
        const auto& s = slots[ch];
        const auto custom = customTrackName (song, ch);
        const auto name = s.instrumentName.isNotEmpty() ? s.instrumentName : juce::String ("(empty)");
        txt << "Track " << juce::String (ch + 1).paddedLeft ('0', 2) << ": ";
        if (custom.isNotEmpty())
            txt << custom << "  |  ";
        txt << name;
        if (s.instrument != nullptr)
            txt << "  |  " << s.instrumentDesc.manufacturerName
                << "  |  " << s.instrumentDesc.fileOrIdentifier
                << "  |  " << s.instrumentDesc.createIdentifierString();
        txt << "\n";

        auto* o = new juce::DynamicObject();
        o->setProperty ("track", ch + 1);
        if (custom.isNotEmpty())
            o->setProperty ("name", custom);
        o->setProperty ("instrument", name);
        if (s.instrument != nullptr)
        {
            o->setProperty ("manufacturer", s.instrumentDesc.manufacturerName);
            o->setProperty ("path", s.instrumentDesc.fileOrIdentifier);
            o->setProperty ("uid", s.instrumentDesc.createIdentifierString());
        }
        jsonTracks.add (juce::var (o));
    }

    if (! destTxt.replaceWithText (txt))
        return "Cannot write " + destTxt.getFullPathName();
    destTxt.withFileExtension (".json").replaceWithText (juce::JSON::toString (juce::var (jsonTracks)));
    return {};
}

juce::String HostEngine::renderOffline (const juce::File& wavFile, int soloChannel,
                                        double sampleRate, int bitDepth, const Progress& progress)
{
    const int block = 512;
    graph.setPlayConfigDetails (0, 2, sampleRate, block);
    graph.prepareToPlay (sampleRate, block);

    // solo the target channel directly on the strips (user solo/mute state
    // untouched — restored via updateMuteStates() afterwards)
    if (soloChannel >= 0)
        for (int ch = 0; ch < kMixChannels; ++ch)
            strips[ch]->setEffectiveMute (ch != soloChannel);
    else
        updateMuteStates();

    juce::int64 totalRows = 0;
    for (int i = 0; i < song.orderLen; ++i)
        if (auto* p = song.getPattern (song.order[i]))
            totalRows += p->getNumRows();
    const double rowSec = 2.5 * sequencer->getSpeed() / sequencer->getBpm();
    const auto songSamples = (juce::int64) ((double) totalRows * rowSec * sampleRate);
    const auto tailSamples = (juce::int64) (3.0 * sampleRate);
    const auto total = songSamples + tailSamples;

    wavFile.deleteFile();
    auto stream = wavFile.createOutputStream();
    if (stream == nullptr)
        return "Cannot write " + wavFile.getFullPathName();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (stream.get(), sampleRate, 2, bitDepth, {}, 0));
    if (writer == nullptr)
        return "Cannot create WAV writer (" + juce::String (bitDepth) + " bit)";
    stream.release();   // the writer owns it now

    const bool prevSongMode = sequencer->isSongMode();
    const bool prevMetro = sequencer->isMetronomeOn();
    sequencer->setSongMode (true);
    sequencer->setMetronome (false);   // never render clicks into stems
    sequencer->setPrecountRows (0);
    sequencer->play();

    juce::AudioBuffer<float> buf (2, block);
    juce::String error;
    for (juce::int64 done = 0; done < total; done += block)
    {
        if (done >= songSamples && sequencer->isPlaying())
            sequencer->stop();   // note-offs, then let the tail ring out

        buf.clear();
        juce::MidiBuffer midi;
        graph.processBlock (buf, midi);
        writer->writeFromAudioSampleBuffer (buf, 0, block);

        if (progress && ! progress ((double) done / (double) total))
        {
            error = "Cancelled";
            break;
        }
    }

    sequencer->stop();
    sequencer->setSongMode (prevSongMode);
    sequencer->setMetronome (prevMetro);
    writer.reset();
    updateMuteStates();
    if (error.isNotEmpty())
        wavFile.deleteFile();
    return error;
}

juce::String HostEngine::renderStems (const juce::File& destDir, Progress progress)
{
    destDir.createDirectory();
    ScopedDetach detach (*this);
    graph.setNonRealtime (true);

    const int active = song.getNumChannels();
    const int totalPasses = active + 1;
    juce::String error;

    for (int ch = 0; ch < active && error.isEmpty(); ++ch)
    {
        const auto base = slots[ch].instrumentName.isNotEmpty() ? slots[ch].instrumentName
                                                                : juce::String ("empty");
        const auto name = "stem-" + juce::String (ch + 1).paddedLeft ('0', 2) + "-"
                        + juce::File::createLegalFileName (base) + ".wav";
        error = renderOffline (destDir.getChildFile (name), ch, 48000.0, 24,
            [&] (double p) { return progress == nullptr || progress ((ch + p) / totalPasses); });
    }
    if (error.isEmpty())
        error = renderOffline (destDir.getChildFile ("master.wav"), -1, 48000.0, 24,
            [&] (double p) { return progress == nullptr || progress ((active + p) / totalPasses); });

    graph.setNonRealtime (false);
    return error;
}

juce::String HostEngine::renderMasterWav (const juce::File& dest, Progress progress)
{
    ScopedDetach detach (*this);
    graph.setNonRealtime (true);
    const auto error = renderOffline (dest, -1, 48000.0, 24, progress);
    graph.setNonRealtime (false);
    return error;
}

juce::String HostEngine::exportMp3 (const juce::File& dest, const juce::File& lameExe, Progress progress)
{
    if (! lameExe.existsAsFile())
        return "lame.exe not found: " + lameExe.getFullPathName();

    auto tmpWav = dest.getSiblingFile (dest.getFileNameWithoutExtension() + "-render.wav");
    if (auto error = renderMasterWav (tmpWav, progress); error.isNotEmpty())
    {
        tmpWav.deleteFile();
        return error;
    }

    juce::ChildProcess lame;
    if (! lame.start (juce::StringArray { lameExe.getFullPathName(), "-b", "320",
                                          tmpWav.getFullPathName(), dest.getFullPathName() }))
    {
        tmpWav.deleteFile();
        return "Cannot start lame.exe";
    }
    lame.waitForProcessToFinish (300'000);
    const bool ok = lame.getExitCode() == 0 && dest.existsAsFile();
    tmpWav.deleteFile();
    return ok ? juce::String() : "lame.exe failed (exit " + juce::String (lame.getExitCode()) + ")";
}

// ---------------------------------------------------------------- import

juce::String HostEngine::importModule (const juce::File& modFile, juce::StringArray& warnings)
{
    Song imported;
    double bpm = 125.0;
    int speed = 6;
    if (auto err = ModImport::importFile (modFile, imported, kMixChannels, bpm, speed, warnings);
        err.isNotEmpty())
        return err;

    sequencer->stop();
    ScopedDetach detach (*this);
    song = std::move (imported);
    sequencer->setSong (&song);
    sequencer->setEditPatternIndex (0);
    sequencer->setTempo (bpm, speed);
    return {};
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

void HostEngine::startPlayback()
{
    sequencer->setPrecountRows (precountEnabled && liveRec.load() ? 16 : 0);
    sequencer->play();
}

void HostEngine::recordLiveEvent (const juce::MidiMessage& m)
{
    if (! liveRec.load() || ! sequencer->isPlaying())
        return;
    auto* pat = song.getPattern (sequencer->getUiPatternIndex());
    if (pat == nullptr)
        return;
    int row = sequencer->getUiRow();
    if (row < 0)
        return;   // pre-count still running

    // quantize to the NEAREST row
    if (sequencer->getRowPhase() > 0.5f)
        row = (row + 1) % pat->getNumRows();
    const int ch = juce::jlimit (0, pat->getNumChannels() - 1, liveChannel.load());
    auto& cell = pat->at (row, ch);   // direct write, benign race by design

    if (m.isNoteOn())
    {
        cell.note = (uint8_t) juce::jlimit (1, 127, m.getNoteNumber());
        cell.volume = (uint8_t) juce::jlimit (1, 64, (int) std::lround (m.getFloatVelocity() * 64.0f));
        cell.instrument = 0;
    }
    else if (m.isNoteOff() && ! cell.hasNote())
    {
        cell.note = Cell::kNoteOff;   // never stomp a fresh note with "==="
    }
}

void HostEngine::handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    const auto m = juce::MidiMessage::noteOn (channel, note, velocity);
    recordLiveEvent (m);
    pushLiveMessage (m);
}

void HostEngine::handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    const auto m = juce::MidiMessage::noteOff (channel, note, velocity);
    recordLiveEvent (m);
    pushLiveMessage (m);
}

void HostEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    // hardware plays the channel under the edit cursor
    juce::MidiMessage remapped (m);
    if (remapped.getChannel() > 0)
        remapped.setChannel (liveChannel.load() + 1);
    if (remapped.isNoteOn() || remapped.isNoteOff())
        recordLiveEvent (remapped);
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
