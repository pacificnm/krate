// krate: the Krate rack. Hosts every registered instrument on one transport,
// routes the MIDI controller to them, mixes them, one JACK client.

#include <memory>
#include <string>
#include <vector>

#include <QDir>
#include <QHash>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "ArrangeController.h"
#include "AudioTracks.h"
#include "PluginSlots.h"
#include "SoundsController.h"
#include "CrashReportsController.h"
#include "ExportController.h"
#include "SettingsController.h"
#include "UpdateController.h"
#include "InstrumentCopies.h"
#include "BackingInstrument.h"
#include "RackController.h"
#include "RackMidiLearn.h"
#include "RackRunControl.h"
#include "RackSession.h"
#include "SongController.h"
#include "app/CrashReports.h"
#include "app/Locations.h"
#include "app/HostController.h"
#include "app/InstrumentRegistry.h"
#include "app/KorrodeInstrument.h"
#include "kessel/app/KesselInstrument.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "krimson/app/KrimsonInstrument.h"
#include "kobalt/app/KobaltInstrument.h"
#include "klang/app/KlangInstrument.h"
#include "konnekt/app/KonnektInstrument.h"
#include "krunch/app/KrunchInstrument.h"
#include "app/KlickInstrument.h"
#include "app/ScreenshotHook.h"
#include "core/Midi.h"
#include "core/Transport.h"
#include "host/JackHost.h"
#include "rack/Rack.h"
#include "rack/RackProcessor.h"
#include "song/SongPlayer.h"
#include "core/EventMerge.h"
#include "seq/Sequencer.h"
#include "NoteTargets.h"
#include "SequencerController.h"

Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KorrodePlugin)
Q_IMPORT_QML_PLUGIN(KesselPlugin)
Q_IMPORT_QML_PLUGIN(KalliopePlugin)
Q_IMPORT_QML_PLUGIN(KrimsonPlugin)
Q_IMPORT_QML_PLUGIN(KobaltPlugin)
Q_IMPORT_QML_PLUGIN(KlangPlugin)
Q_IMPORT_QML_PLUGIN(KonnektPlugin)
Q_IMPORT_QML_PLUGIN(KrunchPlugin)
Q_IMPORT_QML_PLUGIN(KlickPlugin)

// The host gives the rack the master and every device its own stereo output.
static_assert(krate::JackHost::kMaxChannels >= 2 + 2 * krate::Rack::kMaxDevices);

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned, so start from the plainest style. Set after
    // the app exists: the desktop's platform theme (KDE: Breeze) sets its own
    // style while the app is constructed.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("krate"));
    QGuiApplication::setDesktopFileName(QStringLiteral("krate")); // its menu entry and icon (packaging/)
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));
    // The instruments in this rack: one registry line each
    // (docs/SHARED_FOUNDATIONS.MD#adding-instruments).
    QList<krate::InstrumentEntry> entries = {
        korrode::registryEntry(),
        kessel::registryEntry(),
        kalliope::registryEntry(),
        krimson::registryEntry(),
        kobalt::registryEntry(),
        klang::registryEntry(),
        konnekt::registryEntry(),
        krunch::registryEntry(),
        klick::registryEntry(),
    };
    // Copies (docs/DAW.md#copies): up to 16 of each instrument, each copy a
    // device right after the instrument itself, built once it's first added.
    {
        QList<krate::InstrumentEntry> withCopies;
        for (const auto &entry : std::as_const(entries))
            withCopies << entry << copyEntries(entry);
        entries = withCopies;
    }


    // Settings (docs/SETTINGS.md), first: they say where everything is, and a
    // folder moved on the Settings screen moves now, before anything opens it.
    QStringList instrumentIds;
    for (const auto &entry : std::as_const(entries))
        instrumentIds << entry.id;
    SettingsController settings(krate::locations::settingsFile(), instrumentIds);
    using krate::locations::Folder;
    // Crash reports (docs/CRASHES.md): this session's log, a crash's call
    // stack, a report at the next start; SIGTERM quits properly.
    const QString appData = settings.folder(Folder::Data);
    krate::crash::install(QStringLiteral("krate"), appData);
    CrashReportsController crashReports(appData, nullptr);
    // Updates from GitHub's releases (docs/UPDATES.md).
    UpdateController updates(QGuiApplication::applicationVersion());
    updates.setAutoCheck(settings.autoCheckUpdates());
    settings.onAutoCheckChanged = [&updates](bool on) { updates.setAutoCheck(on); };

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK), then the instruments, then rack and transport.
    krate::Transport transport;
    krate::Rack rack;
    krate::song::SongPlayer songPlayer; // plays a loaded song into the rack (docs/SONG_PLAYER.md)
    // The audio tracks (docs/DAW.md, D2): out of the rack until added from +;
    // their takes in the recordings folder (~/.local/share/krate/recordings).
    AudioTracksController audioTracks(settings.folder(Folder::Recordings), nullptr);
    audioTracks.setTransportPlaying([&transport] { return transport.playing(); });
    const qsizetype firstTrack = entries.size();
    entries.append(audioTrackEntries(audioTracks));
    // CLAP plugins (docs/DAW.md, D7): sixteen slots, out of the rack until added;
    // which plugin each holds, and its state, in ~/.config/krate/plugins.json.
    // Searched: the folders added in Settings, then the standard ones.
    const auto pluginSearchPaths = [](const QStringList &extra) {
        std::vector<std::string> dirs;
        for (const QString &dir : extra)
            dirs.push_back(dir.toStdString());
        for (const std::string &dir : krate::clap::defaultSearchPaths())
            dirs.push_back(dir);
        return dirs;
    };
    PluginSlotsController pluginSlots(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                                          + QStringLiteral("/plugins.json"),
                                      pluginSearchPaths(settings.pluginFolders()), nullptr);
    settings.onPluginFoldersChanged = [&pluginSlots, pluginSearchPaths](const QStringList &extra) {
        pluginSlots.setSearchPaths(pluginSearchPaths(extra));
    };
    const qsizetype firstPlugin = entries.size();
    entries.append(pluginSlotEntries(pluginSlots));
    // Last: the Song screen's backing track (a mixer strip, no panel).
    entries.append(backingEntry(songPlayer));
    std::vector<std::unique_ptr<krate::InstrumentInstance>> instances;
    std::vector<std::string> ids;
    // Each instrument's own RUN / STOP: just its sequencer, on the rack's transport.
    QStringList runIds;
    for (const auto &entry : entries)
        runIds << entry.id;
    RackRunControl runControl(rack, transport, runIds, nullptr);

    // Each instrument keeps its own files, shared with its standalone app
    // (~/.config/<id>, and <the instruments' folder>/<id>: ~/.local/share/<id>),
    // so patterns and knobs carry over; the rack's own settings are in rack.json.
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString data = settings.folder(Folder::Instruments);
    QHash<QString, krate::InstrumentInstance *> firsts; // each instrument, for its copies
    for (const auto &entry : entries) {
        krate::InstrumentContext context;
        context.transport = nullptr; // the transport bar owns play and tempo
        context.run = &runControl;
        context.configDir = QDir(config).filePath(entry.id);
        context.dataDir = QDir(data).filePath(entry.id);
        context.id = entry.id;
        const bool copy = entry.copy > 1;
        if (copy) // built when first added; its files start as the first one's
            instances.push_back(std::make_unique<CopyInstance>(entry, context, *firsts.value(entry.kind),
                                                               QDir(config).filePath(entry.kind),
                                                               QDir(data).filePath(entry.kind)));
        else
            instances.push_back(entry.create(context));
        firsts.insert(entry.id, instances.back().get());
        // A copy takes no MIDI channel of its own: those are the first one's.
        const int device = rack.addDevice(instances.back()->instrument(), copy ? -1 : entry.defaultMidiChannel);
        if (device < 0) // a device left out would be silent with nothing said (the backing track was, at 9 devices)
            qFatal("krate: the rack holds at most %d devices; %s doesn't fit", krate::Rack::kMaxDevices,
                   qPrintable(entry.name));
        if (!copy)
            for (int channel : entry.extraMidiChannels)
                rack.setChannelDevice(channel, device);
        ids.push_back(entry.id.toStdString());
    }

    for (int i = 0; i < AudioTracksController::kTracks; ++i)
        audioTracks.setDevice(i, int(firstTrack) + i); // devices are added in the entries' order
    for (int i = 0; i < PluginSlotsController::kSlots; ++i)
        pluginSlots.setDevice(i, int(firstPlugin) + i);

    RackController rackController(rack, transport, entries, nullptr);
    rackController.setBeforeAdding([&instances](int device) {
        if (auto *copy = dynamic_cast<CopyInstance *>(instances[std::size_t(device)].get()))
            copy->build();
    });
    rackController.setPanelProperties([&instances](int device) {
        return device >= 0 && device < int(instances.size()) ? instances[std::size_t(device)]->panelProperties() : QVariantMap{};
    });
    audioTracks.setRack(&rackController);
    rackController.setRunControl(&runControl);
    QObject::connect(&rackController, &RackController::playingChanged, &runControl, &RackRunControl::transportChanged);

    // Rack-wide MIDI Learn: panel keys refer to the focused instrument.
    std::vector<RackMidiLearn::Device> learnDevices;
    for (qsizetype i = 0; i < entries.size(); ++i)
        learnDevices.push_back({entries[i].id, instances[std::size_t(i)]->instrument().parameters(), entries[i].actions,
                                instances[std::size_t(i)].get()});
    SequencerController *sequencerActions = nullptr; // set below, once the sequencer exists
    RackMidiLearn midiLearn(rack, std::move(learnDevices),
                            [&rackController, &sequencerActions, &audioTracks](const QString &action) {
                                if (action == u"playstop")
                                    rackController.setPlaying(!rackController.playing());
                                else if (action == u"record")
                                    audioTracks.setRecording(!audioTracks.recording());
                                else if (action == u"mixer")
                                    rackController.setMixerShown(!rackController.mixerShown());
                                else if (action == u"song")
                                    rackController.setSongShown(!rackController.songShown());
                                else if (action == u"seq")
                                    rackController.setSeqShown(!rackController.seqShown());
                                else if (action == u"arrange")
                                    rackController.setArrangeShown(!rackController.arrangeShown());
                                else if (action == u"sounds")
                                    rackController.setSoundsShown(!rackController.soundsShown());
                                else if (action == u"settings")
                                    rackController.setSettingsShown(!rackController.settingsShown());
                                else if (SequencerController *s = sequencerActions; s && action.startsWith(u"seq")) {
                                    if (action == u"seqrun")
                                        s->toggleRun();
                                    else if (action == u"seqchain")
                                        s->setPlayMode(s->playMode() == 2 ? 0 : 2);
                                    else if (action == u"seqrange")
                                        s->setPlayMode(s->playMode() == 1 ? 0 : 1);
                                    else if (action == u"seqnext")
                                        s->stepPattern(1);
                                    else if (action == u"seqprevious")
                                        s->stepPattern(-1);
                                    else if (action.startsWith(u"seqpattern"))
                                        s->setPattern(action.mid(10).toInt() - 1);
                                }
                            },
                            nullptr);
    QObject::connect(&rackController, &RackController::focusChanged, &midiLearn, &RackMidiLearn::focusChanged);

    // ~/.config/krate/rack.json: tempo, focus, mixer, routing, mappings, MIDI device.
    QStringList deviceIds;
    for (const auto &entry : entries)
        deviceIds << entry.id;
    RackSession session(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/rack.json"),
                        rackController, rack, midiLearn, deviceIds, nullptr);
    session.load();

    // The rack sequencer (docs/SEQUENCER.md), from ~/.local/share/krate/sequencer.json.
    krate::seq::Sequencer sequencer;
    SequencerController sequencerController(
        sequencer, noteTargets(entries),
        appData + QStringLiteral("/sequencer.json"), nullptr);
    sequencerController.load();
    sequencerActions = &sequencerController;
    // The arranger (docs/DAW.md, D3): the sequence's song, start, loop and punch.
    ArrangeController arrangeController(sequencerController, transport, &audioTracks, nullptr);
    // The sound library (docs/SOUNDS.md): browse your sounds, audition them over
    // the master, send them to Klang's pads or Kessel's drums.
    krate::Previewer previewer;
    std::vector<SoundsController::Device> soundDevices;
    for (qsizetype i = 0; i < entries.size(); ++i)
        soundDevices.push_back({int(i), entries[i].name, instances[std::size_t(i)].get()});
    SoundsController soundsController(appData + QStringLiteral("/sounds.db"),
                                      previewer, rackController, std::move(soundDevices), nullptr);
    { // Krate's own folders of sounds, always in the library (docs/SOUNDS.md#krates-own-folders)
        QVariantMap own{{appData + QStringLiteral("/sounds"), QStringLiteral("Krate's sounds folder")}};
        for (qsizetype i = 0; i < entries.size(); ++i) { // the instruments that take samples: their samples folders
            const QString samples = QDir(data).filePath(entries[i].id) + QStringLiteral("/samples");
            if (!instances[std::size_t(i)]->sampleTargets().isEmpty() || QFileInfo(samples).isDir())
                own[samples] = entries[i].name + QStringLiteral("'s samples");
        }
        soundsController.setOwnFolders(own);
    }
    krate::MidiCapture midiCapture; // the keyboard's notes, for MIDI clips
    arrangeController.setMidiCapture(&midiCapture);
    // Automation (docs/DAW.md, D5): the instruments' knobs over the song.
    krate::AutomationPlayer automationPlayer;
    {
        QStringList names;
        std::vector<krate::IInstrument *> instruments;
        for (qsizetype i = 0; i < entries.size(); ++i) {
            names << entries[i].name;
            instruments.push_back(&instances[std::size_t(i)]->instrument());
        }
        arrangeController.setAutomationTargets(&automationPlayer, deviceIds, names, std::move(instruments));
    }
    QObject::connect(&audioTracks, &AudioTracksController::recordingChanged, &arrangeController,
                     [&] { arrangeController.setRecording(audioTracks.recording()); }); // one REC for both
    sequencerController.setRunControl(&runControl);
    sequencerController.setNoteLanes([&instances](int device) { // Klang's pads, by name
        return device >= 0 && std::size_t(device) < instances.size() ? instances[std::size_t(device)]->noteLanes() : QVariantList{};
    });
    // SONG on: PLAY plays the arrangement alone (an empty one: nothing).
    runControl.setSongMode([&arrangeController] { return arrangeController.songMode(); });
    runControl.setPlayedBySequencer([&sequencerController](int device) { return sequencerController.playsDevice(device); });
    QObject::connect(&arrangeController, &ArrangeController::changed, &runControl,
                     [&arrangeController, &runControl, song = arrangeController.songMode()]() mutable {
                         if (std::exchange(song, arrangeController.songMode()) != song)
                             runControl.songModeChanged();
                     });
    // An instrument's own RUN plays it alone, the Seq screen's tracks off; PLAY runs them all.
    runControl.setRackSequencer([&sequencerController] { return sequencerController.running(); },
                                [&sequencerController](bool on) { sequencerController.setRunning(on); });

    // The Song screen and the song library (~/.local/share/krate/library: the
    // songs' files, library.db with each song's set-up and sounds).
    SongLibrary library(settings.folder(Folder::Library));
    std::vector<krate::InstrumentInstance *> songInstances;
    for (const auto &instance : instances)
        songInstances.push_back(instance.get());
    SongController songController(songPlayer, rackController, entries, std::move(songInstances), library, nullptr);
    songController.setSequencer(&sequencerController);
    std::vector<krate::InstrumentInstance *> soundInstances;
    for (const auto &instance : instances)
        soundInstances.push_back(instance.get());
    const RackSounds rackSounds(rackController, deviceIds, std::move(soundInstances));
    sequencerController.setLibrary(&library, &rackSounds);
    songController.setBackingTrack(&static_cast<BackingInstance *>(instances.back().get())->track);
    if (const QString old = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/songs.json");
        QFileInfo::exists(old))
        songController.importSongsJson(old); // from before the library
    else
        songController.reopenLast();

    // The song player and the sequencer both play into the rack.
    krate::EventMerge<2> eventSources({&songPlayer, &sequencer});
    krate::RackProcessor processor(rack, transport, ids);
    processor.setEventSource(&eventSources);
    processor.setMidiCapture(&midiCapture);
    processor.setAutomation(&automationPlayer);
    processor.setPreview(&previewer);
    krate::JackHost jack(processor);
    krate::HostController host(jack, QStringLiteral("krate"), nullptr);
    // Export (docs/DAW.md, D6): the host hands the rack over while it renders.
    ExportController exporter(rack, processor, transport, sequencer, &automationPlayer, rackController,
                              sequencerController, arrangeController, &audioTracks,
                              settings.folder(Folder::Exports),
                              nullptr);
    exporter.setHost({[&jack] { jack.suspend(); }, [&jack] { jack.resume(); }, [&jack] { return double(jack.sampleRate()); }});
    // The plugins as they were, at the host's rate.
    pluginSlots.setHostRate([&jack] { return double(jack.sampleRate()); });
    QObject::connect(&pluginSlots, &PluginSlotsController::pluginsChanged, &arrangeController,
                     &ArrangeController::devicesChanged);
    pluginSlots.restore();
    audioTracks.setRoundTrip([&jack] { return int(jack.roundTripFrames()); });
    // A device's own output port appears when it's added to the rack.
    QObject::connect(&rackController, &RackController::presenceChanged, &rackController, [&jack] { jack.updateOutputs(); });
    host.setMidiInputMatch(settings.midiInputs());
    settings.onMidiInputsChanged = [&host](const QString &pattern) { host.setMidiInputMatch(pattern); };
    settings.onExportsChanged = [&exporter](const QString &folder) { exporter.setFolder(folder); };
    rack.setProgramChangeFocus(settings.programChangeFocus());
    settings.onProgramChangeFocusChanged = [&rack](bool on) { rack.setProgramChangeFocus(on); };
    host.setRequestedBufferSize(session.bufferSize());
    host.setInputPorts(session.audioInputs());
    host.setOutputDevice(session.outputDevice());
    QObject::connect(&host, &krate::HostController::outputDeviceChanged, &session,
                     [&session, &host] { session.setOutputDevice(host.outputDevice()); });
    QObject::connect(&host, &krate::HostController::inputPortsChanged, &session,
                     [&session, &host, &rackController] {
                         session.setAudioInputs(host.inputPorts());
                         rackController.setInterfaceInputs(host.inputLabels());
                     });
    QObject::connect(&host, &krate::HostController::requestedBufferSizeChanged, &session,
                     [&session, &host] { session.setBufferSize(host.requestedBufferSize()); });
    // Never chosen (or all "None"): the interface's jacks in order, once it's seen (and kept).
    const QStringList savedInputs = session.audioInputs();
    rackController.setInterfaceInputs(host.inputLabels()); // the mixer's IN 1, IN 2... named
    host.setAutoInputs(std::none_of(savedInputs.begin(), savedInputs.end(), [](const QString &p) { return !p.isEmpty(); }));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("KrateRack", "Main");

    krate::installScreenshotHook(engine);

    const int result = app.exec();
    audioTracks.finish(); // a take still recording is kept, while the rack and the host are here
    pluginSlots.finish(); // the plugins' state kept, each stopped while the host runs
    soundsController.finish(); // no preview playing, no folder being looked through
    if (!transport.following()) // a DAW leading over JACK keeps playing when this app closes
        transport.setPlaying(false);
    krate::crash::endedCleanly();
    return result;
}
