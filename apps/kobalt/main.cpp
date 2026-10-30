#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "app/HostController.h"
#include "kobalt/app/KobaltInstrument.h"
#include "app/InstrumentMidiLearn.h"
#include "app/ScreenshotHook.h"
#include "core/Transport.h"
#include "host/JackHost.h"

// Both QML modules are static libraries: pull in their plugins explicitly.
Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KobaltPlugin)

// The standalone Kobalt app: Kobalt built through its registry entry, exactly as
// the Krate rack builds it, but with its own transport and JACK host.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned, so start from the plainest style. Set after
    // the app exists: the desktop's platform theme (KDE: Breeze) sets its own
    // style while the app is constructed.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("kobalt"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    const krate::InstrumentEntry entry = kobalt::registryEntry();

    // Declaration order is destruction order in reverse: the QML engine goes
    // first, then the host (closing JACK), then the instrument, then the transport.
    krate::Transport transport;
    krate::InstrumentContext context;
    context.transport = &transport;
    context.configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // ~/.config/kobalt
    context.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);     // ~/.local/share/kobalt
    const std::unique_ptr<krate::InstrumentInstance> instrument = entry.create(context);

    // ~/.config/kobalt/midimap.json: controls plus the entry's learnable buttons.
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
