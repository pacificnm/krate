// konnekt: the modular synthesizer, its own app (instruments/konnekt/README.md).

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "app/HostController.h"
#include "app/ScreenshotHook.h"
#include "core/Transport.h"
#include "host/JackHost.h"
#include "konnekt/app/KonnektController.h"
#include "konnekt/dsp/Engine.h"

Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KonnektPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned: start from the plainest style (set after the
    // app exists: the desktop's platform theme sets its own while it's built).
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("konnekt"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); // ~/.local/share/konnekt
    QDir().mkpath(dataDir);

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK: the engine stops running modules), then the
    // controller (freeing them), then the engine.
    konnekt::Engine engine;
    krate::Transport transport; // unused: Konnekt keeps its own time (its Clock modules)
    KonnektController controller(engine, dataDir, nullptr);
    krate::InstrumentProcessor processor(engine, transport);
    krate::JackHost jack(processor);
    krate::HostController host(jack, QStringLiteral("konnekt"), nullptr);
    host.setMidiInputMatch(QStringLiteral(".")); // every MIDI controller plays it

    QQmlApplicationEngine qml;
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    qml.loadFromModule("Konnekt", "Main");
    krate::installScreenshotHook(qml);
    const int result = app.exec();
    controller.save();
    return result;
}
