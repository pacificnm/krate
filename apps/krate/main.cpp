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
#include "app/HostController.h"
#include "app/InstrumentRegistry.h"
#include "app/K303Instrument.h"
#include "app/KlickInstrument.h"
#include "app/MidiLearnController.h"
#include "app/ScreenshotHook.h"
#include "core/Midi.h"
#include "core/Transport.h"
#include "host/JackHost.h"
#include "rack/Rack.h"
#include "rack/RackProcessor.h"

Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(K303Plugin)
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
        klick::registryEntry(),
    };

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK), then the instruments, then rack and transport.
    krate::Transport transport;
    krate::Rack rack;
    std::vector<std::unique_ptr<krate::InstrumentInstance>> instances;
    std::vector<std::string> ids;

    // Each instrument keeps its own files, shared with its standalone app
    // (~/.config/<id>, ~/.local/share/<id>), so patterns carry over.
    // Rack sessions (rack.json) arrive in K5.
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

    // Interim (until rack-level learn in K5): MIDI Learn on the first instrument
    // that has a learn map, with its standalone mappings.
    std::unique_ptr<MidiLearnController> midiLearn;
    for (std::size_t i = 0; i < instances.size() && !midiLearn; ++i) {
        if (!instances[i]->instrument().midiLearnMap())
            continue;
        auto *instance = instances[i].get();
        midiLearn = std::make_unique<MidiLearnController>(
            instance->instrument(), QDir(config).filePath(entries[qsizetype(i)].id + QStringLiteral("/midimap.json")),
            nullptr, entries[qsizetype(i)].actions);
        QObject::connect(midiLearn.get(), &MidiLearnController::actionTriggered, &app,
                         [instance](const QString &action) { instance->triggerAction(action); });
    }

    krate::RackProcessor processor(rack, transport, ids);
    krate::JackHost jack(processor);
    krate::HostController host(jack, QStringLiteral("krate"), nullptr);
    host.setMidiInputMatch(midiLearn ? midiLearn->inputPortMatch() : QStringLiteral("Impact LX49"));

    RackController rackController(rack, transport, entries, nullptr);

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
