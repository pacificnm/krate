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

#include "BackingInstrument.h"
#include "RackController.h"
#include "RackMidiLearn.h"
#include "RackRunControl.h"
#include "RackSession.h"
#include "SongController.h"
#include "app/HostController.h"
#include "app/InstrumentRegistry.h"
#include "app/K303Instrument.h"
#include "k909/app/K909Instrument.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "krimson/app/KrimsonInstrument.h"
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
Q_IMPORT_QML_PLUGIN(K303Plugin)
Q_IMPORT_QML_PLUGIN(K909Plugin)
Q_IMPORT_QML_PLUGIN(KalliopePlugin)
Q_IMPORT_QML_PLUGIN(KrimsonPlugin)
Q_IMPORT_QML_PLUGIN(KrunchPlugin)
Q_IMPORT_QML_PLUGIN(KlickPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned, so start from the plainest style. Set after
    // the app exists: the desktop's platform theme (KDE: Breeze) sets its own
    // style while the app is constructed.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("krate"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // The instruments in this rack: one registry line each
    // (docs/SHARED_FOUNDATIONS.MD#adding-instruments).
    QList<krate::InstrumentEntry> entries = {
        k303::registryEntry(),
        k909::registryEntry(),
        kalliope::registryEntry(),
        krimson::registryEntry(),
        krunch::registryEntry(),
        klick::registryEntry(),
    };

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK), then the instruments, then rack and transport.
    krate::Transport transport;
    krate::Rack rack;
    krate::song::SongPlayer songPlayer; // plays a loaded song into the rack (docs/SONG_PLAYER.md)
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
        for (int channel : entry.extraMidiChannels)
            rack.setChannelDevice(channel, device);
        ids.push_back(entry.id.toStdString());
    }

    RackController rackController(rack, transport, entries, nullptr);
    rackController.setRunControl(&runControl);
    QObject::connect(&rackController, &RackController::playingChanged, &runControl, &RackRunControl::transportChanged);

    // Rack-wide MIDI Learn: panel keys refer to the focused instrument.
    std::vector<RackMidiLearn::Device> learnDevices;
    for (qsizetype i = 0; i < entries.size(); ++i)
        learnDevices.push_back({entries[i].id, instances[std::size_t(i)]->instrument().parameters(), entries[i].actions,
                                instances[std::size_t(i)].get()});
    RackMidiLearn midiLearn(rack, std::move(learnDevices),
                            [&rackController](const QString &action) {
                                if (action == u"playstop")
                                    rackController.setPlaying(!rackController.playing());
                                else if (action == u"mixer")
                                    rackController.setMixerShown(!rackController.mixerShown());
                                else if (action == u"song")
                                    rackController.setSongShown(!rackController.songShown());
                                else if (action == u"seq")
                                    rackController.setSeqShown(!rackController.seqShown());
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

    // The Song screen and the song library (~/.local/share/krate/library: the
    // songs' files, library.db with each song's set-up and sounds).
    SongLibrary library(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library"));
    std::vector<krate::InstrumentInstance *> songInstances;
    for (const auto &instance : instances)
        songInstances.push_back(instance.get());
    SongController songController(songPlayer, rackController, entries, std::move(songInstances), library, nullptr);
    songController.setSequencer(&sequencerController);
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
    krate::JackHost jack(processor);
    krate::HostController host(jack, QStringLiteral("krate"), nullptr);
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
    transport.setPlaying(false);
    return result;
}
