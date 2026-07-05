#include "ui/MainComponent.h"
#include "AppVersion.h"
#include "io/ModImport.h"

MainComponent::MainComponent()
{
    // ---- app settings (lame.exe path, later: prefs) ----
    juce::PropertiesFile::Options opts;
    opts.applicationName = "PPsVaultTracker";
    opts.filenameSuffix = ".settings";
    opts.folderName = "PPsVaultTracker";
    opts.osxLibrarySubFolder = "Application Support";
    appProps.setStorageParameters (opts);

    // ---- toolbar ----
    for (auto* b : { &audioBtn, &newBtn, &openBtn, &saveBtn, &saveAsBtn, &exportBtn, &importBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    audioBtn.onClick  = [this] { showAudioSettings(); };
    newBtn.onClick    = [this] { newProject(); };
    openBtn.onClick   = [this] { openProject(); };
    saveBtn.onClick   = [this] { saveProject(); };
    saveAsBtn.onClick = [this] { saveProjectAs(); };
    exportBtn.onClick = [this] { showExportMenu(); };
    importBtn.onClick = [this] { importModuleFlow(); };
    importBtn.setEnabled (ModImport::isAvailable());
    if (! ModImport::isAvailable())
        importBtn.setTooltip ("Built without libopenmpt");

    // ---- transport ----
    for (auto* b : { &playBtn, &stopBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    playBtn.onClick = [this] { engine.startPlayback(); patternEditor.grabKeyboardFocus(); };
    stopBtn.onClick = [this] { engine.stopPlayback(); patternEditor.grabKeyboardFocus(); };

    auto setupToggle = [this] (juce::TextButton& b, bool initial, std::function<void (bool)> apply)
    {
        b.setClickingTogglesState (true);
        b.setToggleState (initial, juce::dontSendNotification);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb03060));
        b.setWantsKeyboardFocus (false);
        b.onClick = [this, &b, apply] { apply (b.getToggleState()); patternEditor.grabKeyboardFocus(); };
        addAndMakeVisible (b);
    };
    setupToggle (recBtn,      false, [this] (bool on) { patternEditor.recEnabled = on;
                                                        engine.setLiveRecording (on); });
    setupToggle (followBtn,   true,  [this] (bool on) { patternEditor.followPlayhead = on; });
    setupToggle (azertyBtn,   true,  [this] (bool on) { patternEditor.azertyLayout = on;
                                                        azertyBtn.setButtonText (on ? "AZERTY" : "QWERTY"); });
    setupToggle (songModeBtn, false, [this] (bool on) { engine.getSequencer().setSongMode (on); });
    setupToggle (metroBtn,    false, [this] (bool on) { engine.setMetronome (on); });
    setupToggle (precountBtn, false, [this] (bool on) { engine.setPrecountEnabled (on); });
    setupToggle (keymapBtn,   false, [this] (bool on) { patternEditor.ptKeys = on;
                                                        keymapBtn.setButtonText (on ? "PT" : "FT2"); });

    auto setupIncDec = [this] (juce::Slider& s, double min, double max, double value,
                               std::function<void (double)> apply)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.setWantsKeyboardFocus (false);
        for (auto* child : s.getChildren())      // the inc/dec buttons steal focus too
            child->setWantsKeyboardFocus (false);
        s.onValueChange = [this, &s, apply] { apply (s.getValue()); patternEditor.grabKeyboardFocus(); };
        addAndMakeVisible (s);
    };
    setupIncDec (bpmSlider,   40.0, 300.0, 125.0, [this] (double)   { applyTempo(); });
    setupIncDec (speedSlider,  1.0,  31.0,   6.0, [this] (double)   { applyTempo(); });
    setupIncDec (stepSlider,   0.0,  16.0,   1.0, [this] (double v) { patternEditor.editStep = (int) v; });
    setupIncDec (octaveSlider, 1.0,   7.0,   3.0, [this] (double v) { patternEditor.editOctave = (int) v; });
    setupIncDec (chanSlider,   1.0,  16.0,   1.0, [this] (double v) { engine.setNumChannels ((int) v); });
    setupIncDec (patSlider,    0.0,   1.0,   0.0, [this] (double v)
    {
        // the range can exceed the real pattern count (JUCE sliders need
        // max > min) — clamp here
        const int idx = juce::jlimit (0, engine.getSong().getNumPatterns() - 1, (int) v);
        if (idx != (int) v)
            patSlider.setValue ((double) idx, juce::dontSendNotification);
        engine.getSequencer().setEditPatternIndex (idx);
        patternEditor.repaint();
    });

    addPatBtn.setWantsKeyboardFocus (false);
    addPatBtn.onClick = [this]
    {
        const int idx = engine.addPattern();
        if (idx >= 0)
        {
            patSlider.setRange (0.0, juce::jmax (1.0, (double) (engine.getSong().getNumPatterns() - 1)), 1.0);
            patSlider.setValue ((double) idx);   // jump to the new pattern
        }
    };
    addAndMakeVisible (addPatBtn);

    orderEdit.setText ("0");
    orderEdit.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    orderEdit.onReturnKey = [this] { applyOrderFromText(); };
    addAndMakeVisible (orderEdit);

    orderApplyBtn.setWantsKeyboardFocus (false);
    orderApplyBtn.onClick = [this] { applyOrderFromText(); };
    addAndMakeVisible (orderApplyBtn);

    for (auto* l : { &bpmLabel, &speedLabel, &stepLabel, &octaveLabel, &chanLabel, &patLabel, &orderLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (patternEditor);
    // the mixer follows the grid, and keeps its own scrollbar so the MASTER
    // strip (one column past the last channel) stays reachable
    mixerViewport.setViewedComponent (&mixer, false);
    mixerViewport.setScrollBarsShown (false, true);
    mixerViewport.setScrollBarThickness (10);
    addAndMakeVisible (mixerViewport);
    patternEditor.onViewChanged = [this]
    {
        mixerViewport.setViewPosition (patternEditor.getFirstChannel() * GridMetrics::kStride, 0);
    };
    addAndMakeVisible (keyboard);
    keyboard.setKeyPressBaseOctave (4);

    // live input (virtual keyboard + hardware) follows the edit cursor
    patternEditor.onCursorChannelChanged = [this] (int ch)
    {
        engine.setLiveChannel (ch);
        keyboard.setMidiChannel (ch + 1);
    };
    keyboard.setMidiChannel (1);

    refreshStatus();
    markStateSaved();   // startup state = reference for the dirty check
    startTimerHz (4);
    setSize (1240, 940);
}

void MainComponent::confirmAndQuit()
{
    if (! hasUnsavedChanges())
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return;
    }

    juce::AlertWindow::showYesNoCancelBox (
        juce::MessageBoxIconType::QuestionIcon,
        "Unsaved changes",
        "The project has unsaved changes.",
        "Save & quit", "Quit without saving", "Cancel", this,
        juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1)          // save & quit
            {
                if (engine.hasProject())
                {
                    saveProject();
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                }
                else
                {
                    quitAfterSave = true;
                    saveProjectAs();   // quits in the chooser callback on success
                }
            }
            else if (result == 2)     // quit without saving
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            // 0 = cancel: stay
        }));
}

MainComponent::~MainComponent()
{
    pluginWindow = nullptr;   // plugin editors must go before the engine
}

void MainComponent::timerCallback()
{
    // keep the pattern selector in sync when the song playback moves it
    const int edited = engine.getSequencer().getEditPatternIndex();
    if ((int) patSlider.getValue() != edited)
        patSlider.setValue ((double) edited, juce::dontSendNotification);

    // autosave every 3 minutes once a project exists
    const auto now = juce::Time::getMillisecondCounter();
    if (lastAutosaveMs == 0)
        lastAutosaveMs = now;
    if (engine.hasProject() && now - lastAutosaveMs > 180'000)
    {
        lastAutosaveMs = now;
        engine.autosaveBackup();
    }

    refreshStatus();
}

// ---------------------------------------------------------------- project

void MainComponent::newProject()
{
    engine.newProject();
    syncFromEngine();
    markStateSaved();
}

void MainComponent::openProject()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Open project (.ubt folder)",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.ubt");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File())
                return;

            juce::StringArray warnings;
            const auto error = engine.loadProject (dir, warnings);
            if (error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Open failed", error);
                return;
            }
            syncFromEngine();
            markStateSaved();
            if (! warnings.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                        "Project loaded with warnings",
                                                        warnings.joinIntoString ("\n"));
        });
}

void MainComponent::saveProject()
{
    if (! engine.hasProject())
    {
        saveProjectAs();
        return;
    }
    const auto error = engine.saveProject (engine.getProjectDir());
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", error);
    else
        markStateSaved();
    updateTitle();
}

void MainComponent::saveProjectAs()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Save project as (.ubt folder)",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.ubt");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto target = fc.getResult();
            if (target == juce::File())
                return;
            if (! target.getFileName().endsWithIgnoreCase (".ubt"))
                target = target.withFileExtension (".ubt");

            const auto error = engine.saveProject (target);
            if (error.isNotEmpty())
            {
                quitAfterSave = false;
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Save failed", error);
            }
            else
            {
                markStateSaved();
                if (quitAfterSave)
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }
            updateTitle();
        });
}

void MainComponent::syncFromEngine()
{
    auto& seq = engine.getSequencer();
    bpmSlider.setValue (seq.getBpm(), juce::dontSendNotification);
    speedSlider.setValue ((double) seq.getSpeed(), juce::dontSendNotification);
    chanSlider.setValue ((double) engine.getSong().getNumChannels(), juce::dontSendNotification);

    patSlider.setRange (0.0, juce::jmax (1.0, (double) (engine.getSong().getNumPatterns() - 1)), 1.0);
    patSlider.setValue (0.0, juce::dontSendNotification);
    seq.setEditPatternIndex (0);

    juce::StringArray ord;
    for (int i = 0; i < engine.getSong().orderLen; ++i)
        ord.add (juce::String (engine.getSong().order[i]));
    orderEdit.setText (ord.joinIntoString (" "), juce::dontSendNotification);

    songModeBtn.setToggleState (seq.isSongMode(), juce::dontSendNotification);
    mixer.syncFromEngine();
    patternEditor.repaint();
    updateTitle();
    refreshStatus();
}

// ---------------------------------------------------------------- exports

namespace
{
    void reportExportResult (const juce::String& error, const juce::String& doneMessage)
    {
        if (error.isNotEmpty() && error != "Cancelled")
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Export failed", error);
        else if (error.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Export", doneMessage);
    }

    // runs an engine render job with a progress dialog + cancel, async
    // (JUCE 8 disallows modal loops by default). Self-deleting.
    struct RenderJob : juce::ThreadWithProgressWindow
    {
        RenderJob (const juce::String& title,
                   std::function<juce::String (HostEngine::Progress)> fn,
                   juce::String doneMsg)
            : ThreadWithProgressWindow (title, true, true),
              job (std::move (fn)), doneMessage (std::move (doneMsg)) {}

        void run() override
        {
            error = job ([this] (double p)
            {
                setProgress (p);
                return ! threadShouldExit();
            });
        }

        void threadComplete (bool userPressedCancel) override
        {
            if (! userPressedCancel)
                reportExportResult (error, doneMessage);
            delete this;
        }

        std::function<juce::String (HostEngine::Progress)> job;
        juce::String error, doneMessage;
    };
}

void MainComponent::showExportMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "MIDI (SMF type 1)...");
    m.addItem (2, "Stems WAV (24-bit / 48 kHz)...");
    m.addItem (3, "Master WAV (24-bit / 48 kHz)...");
    m.addItem (4, "Master MP3 (320 kbps, via lame.exe)...");
    m.addItem (5, "Tracklist (.txt + .json)...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (exportBtn),
        [this] (int r)
        {
            switch (r)
            {
                case 1: exportMidiFlow(); break;
                case 2: exportStemsFlow(); break;
                case 3: exportMasterFlow(); break;
                case 4: exportMp3Flow(); break;
                case 5: exportTracklistFlow(); break;
                default: break;
            }
        });
}

void MainComponent::exportMidiFlow()
{
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto error = engine.exportMidi (f.withFileExtension (".mid"));
            reportExportResult (error, "MIDI exported.");
        });
}

void MainComponent::exportTracklistFlow()
{
    chooser = std::make_unique<juce::FileChooser> ("Export tracklist",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto error = engine.exportTracklist (f.withFileExtension (".txt"));
            reportExportResult (error, "Tracklist exported (.txt + .json).");
        });
}

void MainComponent::exportStemsFlow()
{
    chooser = std::make_unique<juce::FileChooser> ("Export stems into folder",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir == juce::File()) return;
            (new RenderJob ("Rendering stems...",
                            [this, dir] (HostEngine::Progress p) { return engine.renderStems (dir, std::move (p)); },
                            "Stems rendered into " + dir.getFullPathName()))->launchThread();
        });
}

void MainComponent::exportMasterFlow()
{
    chooser = std::make_unique<juce::FileChooser> ("Export master WAV",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            (new RenderJob ("Rendering master...",
                            [this, f] (HostEngine::Progress p)
                            { return engine.renderMasterWav (f.withFileExtension (".wav"), std::move (p)); },
                            "Master WAV rendered."))->launchThread();
        });
}

juce::File MainComponent::getLamePath()
{
    return juce::File (appProps.getUserSettings()->getValue ("lamePath"));
}

void MainComponent::exportMp3Flow()
{
    if (! getLamePath().existsAsFile())
    {
        chooser = std::make_unique<juce::FileChooser> ("Locate lame.exe (one-time setup)",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.exe");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto exe = fc.getResult();
                if (! exe.existsAsFile()) return;
                appProps.getUserSettings()->setValue ("lamePath", exe.getFullPathName());
                appProps.getUserSettings()->saveIfNeeded();
                exportMp3Flow();   // resume with the path configured
            });
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Export MP3",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            (new RenderJob ("Rendering MP3...",
                            [this, f] (HostEngine::Progress p)
                            { return engine.exportMp3 (f.withFileExtension (".mp3"), getLamePath(), std::move (p)); },
                            "MP3 exported."))->launchThread();
        });
}

void MainComponent::importModuleFlow()
{
    chooser = std::make_unique<juce::FileChooser> ("Import module (level 1: notes only)",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.mod;*.xm;*.s3m;*.it");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile())
                return;

            juce::StringArray warnings;
            const auto error = engine.importModule (f, warnings);
            if (error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Import failed", error);
                return;
            }
            syncFromEngine();
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                "Module imported",
                "Notes, order list and tempo were imported.\n"
                "Load an instrument on each channel to hear it.\n\n"
                + warnings.joinIntoString ("\n"));
        });
}

void MainComponent::updateTitle()
{
    const auto base = "PPsVaultTracker " + AppVersion::display();
    if (auto* dw = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
        dw->setName (engine.hasProject()
                         ? base + " - " + engine.getProjectDir().getFileNameWithoutExtension()
                         : base);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto bar = area.removeFromTop (32);
    auto placeBar = [&bar] (juce::Component& c, int w)
    {
        c.setBounds (bar.removeFromLeft (w));
        bar.removeFromLeft (8);
    };
    placeBar (audioBtn, 110);
    placeBar (newBtn, 64);
    placeBar (openBtn, 84);
    placeBar (saveBtn, 70);
    placeBar (saveAsBtn, 96);
    placeBar (exportBtn, 90);
    placeBar (importBtn, 90);

    area.removeFromTop (6);
    auto transport = area.removeFromTop (30);
    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w, int gapAfter = 8)
    {
        c.setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gapAfter);
    };
    place (transport, playBtn, 70);
    place (transport, stopBtn, 70, 14);
    place (transport, recBtn, 56);
    place (transport, followBtn, 72);
    place (transport, azertyBtn, 80, 14);
    place (transport, bpmLabel, 38, 2);
    place (transport, bpmSlider, 96, 10);
    place (transport, speedLabel, 48, 2);
    place (transport, speedSlider, 96, 10);
    place (transport, stepLabel, 46, 2);
    place (transport, stepSlider, 96, 10);
    place (transport, octaveLabel, 46, 2);
    place (transport, octaveSlider, 96, 10);
    place (transport, chanLabel, 40, 2);
    place (transport, chanSlider, 96);

    area.removeFromTop (6);
    auto songRow = area.removeFromTop (30);
    place (songRow, songModeBtn, 96, 10);
    place (songRow, metroBtn, 70, 8);
    place (songRow, precountBtn, 56, 8);
    place (songRow, keymapBtn, 56, 14);
    place (songRow, patLabel, 56, 2);
    place (songRow, patSlider, 96, 10);
    place (songRow, addPatBtn, 100, 14);
    place (songRow, orderLabel, 46, 2);
    orderApplyBtn.setBounds (songRow.removeFromRight (84));
    songRow.removeFromRight (8);
    orderEdit.setBounds (songRow);

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (22));

    keyboard.setBounds (area.removeFromBottom (90));
    area.removeFromBottom (6);
    mixerViewport.setBounds (area.removeFromBottom (200));
    mixer.setSize (mixer.currentIdealWidth(), mixerViewport.getHeight());
    area.removeFromBottom (6);
    patternEditor.setBounds (area);
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        engine.getDeviceManager(), 0, 0, 0, 2, true, false, true, false);
    selector->setSize (520, 460);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (selector.release());
    opts.dialogTitle = "Audio/MIDI Settings";
    opts.componentToCentreAround = this;
    opts.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}

void MainComponent::showEditorFor (juce::AudioPluginInstance* instance)
{
    if (instance == nullptr)
        return;
    pluginWindow = nullptr;   // one editor window at a time
    pluginWindow = std::make_unique<PluginWindow> (*instance, [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr)
                safe->pluginWindow = nullptr;
        });
    });
}

void MainComponent::applyTempo()
{
    engine.getSequencer().setTempo (bpmSlider.getValue(), (int) speedSlider.getValue());
}

void MainComponent::applyOrderFromText()
{
    juce::StringArray tokens;
    tokens.addTokens (orderEdit.getText(), " ,;", {});
    tokens.removeEmptyStrings();

    int entries[Song::kMaxOrder];
    int count = 0;
    for (auto& t : tokens)
        if (count < Song::kMaxOrder)
            entries[count++] = t.getIntValue();

    if (count > 0)
        engine.applyOrder (entries, count);
    patternEditor.grabKeyboardFocus();
}

void MainComponent::refreshStatus()
{
    juce::String device = "no audio device";
    if (auto* d = engine.getDeviceManager().getCurrentAudioDevice())
        device = d->getName() + " (" + juce::String (d->getCurrentSampleRate(), 0) + " Hz, "
               + juce::String (d->getCurrentBufferSizeSamples()) + " smp)";

    auto& seq = engine.getSequencer();
    juce::String pos = "Pat " + juce::String (seq.getEditPatternIndex());
    if (seq.isPlaying())
    {
        pos = "Playing pat " + juce::String (seq.getUiPatternIndex());
        if (seq.getUiOrderPos() >= 0)
            pos += "  (order " + juce::String (seq.getUiOrderPos() + 1) + "/"
                 + juce::String (engine.getSong().orderLen) + ")";
    }

    statusLabel.setText ("CH " + juce::String (patternEditor.getCursorChannel() + 1)
                             + "   |   " + pos + "   |   " + device,
                         juce::dontSendNotification);
}
