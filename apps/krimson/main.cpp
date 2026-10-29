#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "app/HostController.h"
#include "krimson/app/KrimsonInstrument.h"
#include "app/InstrumentMidiLearn.h"
#include "app/ScreenshotHook.h"
#include "core/Transport.h"
#include "host/JackHost.h"

// Both QML modules are static libraries: pull in their plugins explicitly.
Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KrimsonPlugin)

// The standalone Krimson app: Krimson built through its registry entry, exactly as
// the Krate rack builds it, but with its own transport and JACK host.
int main(int argc, char *argv[])
{
    // Every control is re-skinned, so start from the plainest style.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("krimson"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    const krate::InstrumentEntry entry = krimson::registryEntry();

    // Declaration order is destruction order in reverse: the QML engine goes
    // first, then the host (closing JACK), then the instrument, then the transport.
    krate::Transport transport;
    krate::InstrumentContext context;
    context.transport = &transport;
    context.configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // ~/.config/krimson
    context.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);     // ~/.local/share/krimson
    const std::unique_ptr<krate::InstrumentInstance> instrument = entry.create(context);

    // ~/.config/krimson/midimap.json: controls plus the entry's learnable buttons.
    krate::InstrumentMidiLearn midiLearn(instrument->instrument(), context.configDir + QStringLiteral("/midimap.json"),
                                  nullptr, entry.actions);
    QObject::connect(&midiLearn, &krate::InstrumentMidiLearn::actionTriggered, &app,
                     [&instrument](const QString &action) { instrument->triggerAction(action); });

    krate::InstrumentProcessor processor(instrument->instrument(), transport);
    krate::JackHost jack(processor);
    krate::HostController host(jack, entry.id, nullptr);
    host.setMidiInputMatch(midiLearn.inputPortMatch());

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule(entry.qmlModule, "Main");

    krate::installScreenshotHook(engine);

    const int result = app.exec();
    transport.setPlaying(false);
    return result;
}
