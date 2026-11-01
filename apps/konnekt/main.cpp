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
#include "konnekt/app/KonnektInstrument.h"

Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KonnektPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned: start from the plainest style (set after the
    // app exists: the desktop's platform theme sets its own while it's built).
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("konnekt"));
    QGuiApplication::setDesktopFileName(QStringLiteral("konnekt")); // its menu entry and icon (packaging/)
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // Declaration order is destruction order in reverse: the QML engine first,
    // then the host (closing JACK: the engine stops running modules), then the
    // instrument (its controller freeing them, then the engine).
    const krate::InstrumentEntry entry = konnekt::registryEntry();
    krate::Transport transport; // unused: Konnekt keeps its own time (its Clock modules)
    krate::InstrumentContext context;
    context.configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // ~/.config/konnekt
    context.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);     // ~/.local/share/konnekt
    const std::unique_ptr<krate::InstrumentInstance> instrument = entry.create(context);
    krate::InstrumentProcessor processor(instrument->instrument(), transport);
    krate::JackHost jack(processor);
    krate::HostController host(jack, entry.id, nullptr);
    host.setMidiInputMatch(QStringLiteral(".")); // every MIDI controller plays it

    QQmlApplicationEngine qml;
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    qml.setInitialProperties(instrument->panelProperties()); // its controller, for the panel
    qml.loadFromModule("Konnekt", "Main");
    krate::installScreenshotHook(qml);
    const int result = app.exec();
    static_cast<konnekt::Instance *>(instrument.get())->controller.save();
    return result;
}
