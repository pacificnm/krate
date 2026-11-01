// krate: the Krate rack. Hosts every registered instrument on one transport,
// routes the MIDI controller to them, mixes them, one JACK client.

#include <memory>
#include <string>
#include <vector>

#include <QDir>
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
#include "ExportController.h"
#include "BackingInstrument.h"
#include "RackController.h"
#include "RackMidiLearn.h"
#include "RackRunControl.h"
#include "RackSession.h"
#include "SongController.h"
#include "app/HostController.h"
#include "app/InstrumentRegistry.h"
#include "app/KorrodeInstrument.h"
#include "kessel/app/KesselInstrument.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "krimson/app/KrimsonInstrument.h"
#include "kobalt/app/KobaltInstrument.h"
#include "klang/app/KlangInstrument.h"
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
        krunch::registryEntry(),
        klick::registryEntry(),
    };

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK), then the instruments, then rack and transport.
    krate::Transport transport;
    krate::Rack rack;
    krate::song::SongPlayer songPlayer; // plays a loaded song into the rack (docs/SONG_PLAYER.md)
    // The audio tracks (docs/DAW.md, D2): out of the rack until added from +;
    // their takes in ~/.local/share/krate/recordings.
    AudioTracksController audioTracks(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/recordings"), nullptr);
    const qsizetype firstTrack = entries.size();
    entries.append(audioTrackEntries(audioTracks));
    // CLAP plugins (docs/DAW.md, D7): eight slots, out of the rack until added;
    // which plugin each holds, and its state, in ~/.config/krate/plugins.json.
    PluginSlotsController pluginSlots(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                                          + QStringLiteral("/plugins.json"),
                                      krate::clap::defaultSearchPaths(), nullptr);
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
    // (~/.config/<id>, ~/.local/share/<id>), so patterns and knobs carry over;
    // the rack's own settings are in rack.json.
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    for (const auto &entry : entries) {
        krate::InstrumentContext context;
        context.transport = nullptr; // the transport bar owns play and tempo
        context.run = &runControl;
        context.configDir = QDir(config).filePath(entry.id);
        context.dataDir = QDir(data).filePath(entry.id);
        instances.push_back(entry.create(context));
        const int device = rack.addDevice(instances.back()->instrument(), entry.defaultMidiChannel);
        if (device < 0) // a device left out would be silent with nothing said (the backing track was, at 9 devices)
            qFatal("krate: the rack holds at most %d devices; %s doesn't fit", krate::Rack::kMaxDevices,
                   qPrintable(entry.name));
        for (int channel : entry.extraMidiChannels)
            rack.setChannelDevice(channel, device);
        ids.push_back(entry.id.toStdString());
    }

    for (int i = 0; i < AudioTracksController::kTracks; ++i)
        audioTracks.setDevice(i, int(firstTrack) + i); // devices are added in the entries' order
    for (int i = 0; i < PluginSlotsController::kSlots; ++i)
        pluginSlots.setDevice(i, int(firstPlugin) + i);

    RackController rackController(rack, transport, entries, nullptr);
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
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/sequencer.json"), nullptr);
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
    SoundsController soundsController(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                          + QStringLiteral("/sounds.db"),
                                      previewer, rackController, std::move(soundDevices), nullptr);
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

    // The Song screen and the song library (~/.local/share/krate/library: the
    // songs' files, library.db with each song's set-up and sounds).
    SongLibrary library(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library"));
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
                              QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + QStringLiteral("/Krate"),
                              nullptr);
    exporter.setHost({[&jack] { jack.suspend(); }, [&jack] { jack.resume(); }, [&jack] { return double(jack.sampleRate()); }});
    // The plugins as they were, at the host's rate.
    pluginSlots.setHostRate([&jack] { return double(jack.sampleRate()); });
    QObject::connect(&pluginSlots, &PluginSlotsController::pluginsChanged, &arrangeController,
                     &ArrangeController::devicesChanged);
    pluginSlots.restore();
    audioTracks.setRoundTrip([&jack] { return int(jack.roundTripFrames()); });
    host.setMidiInputMatch(session.midiInput());
    host.setRequestedBufferSize(session.bufferSize());
    host.setInputPorts(session.audioInputs());
    host.setOutputDevice(session.outputDevice());
    QObject::connect(&host, &krate::HostController::outputDeviceChanged, &session,
                     [&session, &host] { session.setOutputDevice(host.outputDevice()); });
    QObject::connect(&host, &krate::HostController::inputPortsChanged, &session,
                     [&session, &host] { session.setAudioInputs(host.inputPorts()); });
    QObject::connect(&host, &krate::HostController::requestedBufferSizeChanged, &session,
                     [&session, &host] { session.setBufferSize(host.requestedBufferSize()); });

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
    return result;
}
