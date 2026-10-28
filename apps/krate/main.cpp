// krate: the Krate rack. Hosts every registered instrument on one transport,
// routes the MIDI controller to them, mixes them, one JACK client.

#include <memory>
#include <string>
#include <vector>

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "RackController.h"
#include "RackMidiLearn.h"
#include "RackSession.h"
#include "app/HostController.h"
#include "app/InstrumentRegistry.h"
#include "app/K303Instrument.h"
#include "k909/app/K909Instrument.h"
#include "app/KlickInstrument.h"
#include "app/ScreenshotHook.h"
#include "core/Midi.h"
#include "core/Transport.h"
#include "host/JackHost.h"
#include "rack/Rack.h"
#include "rack/RackProcessor.h"

Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(K303Plugin)
Q_IMPORT_QML_PLUGIN(K909Plugin)
Q_IMPORT_QML_PLUGIN(KlickPlugin)

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("krate"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // The instruments in this rack: one registry line each
    // (docs/SHARED_FOUNDATIONS.MD#adding-instruments).
    const QList<krate::InstrumentEntry> entries = {
        k303::registryEntry(),
        k909::registryEntry(),
        klick::registryEntry(),
    };

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK), then the instruments, then rack and transport.
    krate::Transport transport;
    krate::Rack rack;
    std::vector<std::unique_ptr<krate::InstrumentInstance>> instances;
    std::vector<std::string> ids;

    // Each instrument keeps its own files, shared with its standalone app
    // (~/.config/<id>, ~/.local/share/<id>), so patterns and knobs carry over;
    // the rack's own settings are in rack.json.
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    for (const auto &entry : entries) {
        krate::InstrumentContext context;
        context.transport = nullptr; // the transport bar owns play and tempo
        context.configDir = QDir(config).filePath(entry.id);
        context.dataDir = QDir(data).filePath(entry.id);
        instances.push_back(entry.create(context));
        rack.addDevice(instances.back()->instrument(), entry.defaultMidiChannel);
        ids.push_back(entry.id.toStdString());
    }

    RackController rackController(rack, transport, entries, nullptr);

    // Rack-wide MIDI Learn: panel keys refer to the focused instrument.
    std::vector<RackMidiLearn::Device> learnDevices;
    for (qsizetype i = 0; i < entries.size(); ++i)
        learnDevices.push_back({entries[i].id, instances[std::size_t(i)]->instrument().parameters(), entries[i].actions,
                                instances[std::size_t(i)].get()});
    RackMidiLearn midiLearn(rack, std::move(learnDevices),
                            [&rackController] { rackController.setPlaying(!rackController.playing()); }, nullptr);
    QObject::connect(&rackController, &RackController::focusChanged, &midiLearn, &RackMidiLearn::focusChanged);

    // ~/.config/krate/rack.json: tempo, focus, mixer, routing, mappings, MIDI device.
    QStringList deviceIds;
    for (const auto &entry : entries)
        deviceIds << entry.id;
    RackSession session(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/rack.json"),
                        rackController, rack, midiLearn, deviceIds, nullptr);
    session.load();

    krate::RackProcessor processor(rack, transport, ids);
    krate::JackHost jack(processor);
    krate::HostController host(jack, QStringLiteral("krate"), nullptr);
    host.setMidiInputMatch(session.midiInput());

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
