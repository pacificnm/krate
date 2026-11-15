#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "app/CrashReports.h"
#include "app/CrashReportsController.h"
#include "app/HostController.h"
#include "app/InstrumentMidiLearn.h"
#include "app/Locations.h"
#include "app/ScreenshotHook.h"
#include "app/StandaloneSession.h"
#include "app/UpdateController.h"
#include "core/Transport.h"
#include "host/JackHost.h"
#include "krunch/app/KrunchInstrument.h"

// Both QML modules are static libraries: pull in their plugins explicitly.
Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(KrunchPlugin)

// The standalone Krunch app: Krunch built through its registry entry, exactly
// as the Krate rack builds it (the same files: ~/.config/krunch and its data
// folder, so presets, captures and IRs are the same in both), with its own
// transport and JACK host, and what the rack gives it around the panel: the
// audio inputs, outputs and buffer (kept in ~/.config/krunch/app.json), MIDI
// Learn, updates from Krate's releases, crash reports.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Every control is re-skinned, so start from the plainest style. Set after
    // the app exists: the desktop's platform theme (KDE: Breeze) sets its own
    // style while the app is constructed.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setApplicationName(QStringLiteral("krunch"));
    QGuiApplication::setDesktopFileName(QStringLiteral("krunch")); // its menu entry and icon (packaging/)
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    const krate::InstrumentEntry entry = krunch::registryEntry();
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // ~/.config/krunch

    // Crash reports (docs/CRASHES.md): this session's log, a crash's call
    // stack, a report at the next start (~/.local/share/krunch); SIGTERM quits properly.
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    krate::crash::install(QStringLiteral("krunch"), appData);
    CrashReportsController crashReports(appData, nullptr);
    // Updates from Krate's GitHub releases (docs/UPDATES.md): Krunch comes in its package.
    UpdateController updates(QGuiApplication::applicationVersion());
    updates.setAppName(QStringLiteral("Krunch"));

    // Declaration order is destruction order in reverse: the QML engine goes
    // first, then the host (closing JACK), then the instrument, then the transport.
    krate::Transport transport;
    krate::InstrumentContext context;
    context.transport = &transport;
    context.configDir = configDir;
    // ~/.local/share/krunch, or where Krate's Settings put the instruments' folder (docs/SETTINGS.md)
    context.dataDir = krate::locations::instrumentData(entry.id);
    context.id = entry.id;
    const std::unique_ptr<krate::InstrumentInstance> instrument = entry.create(context);

    // ~/.config/krunch/midimap.json: the knobs plus the channel buttons,
    // footswitches, presets and tuner (learnable).
    krate::InstrumentMidiLearn midiLearn(instrument->instrument(), configDir + QStringLiteral("/midimap.json"), nullptr,
                                         entry.actions);
    QObject::connect(&midiLearn, &krate::InstrumentMidiLearn::actionTriggered, &app,
                     [&instrument](const QString &action) { instrument->triggerAction(action); });

    krate::InstrumentProcessor processor(instrument->instrument(), transport);
    krate::JackHost jack(processor);
    krate::HostController host(jack, entry.id, nullptr);
    host.setMidiInputMatch(midiLearn.inputPortMatch());
    // The guitar's input, the outputs, the buffer, checking for updates; a
    // first start takes the audio as Krate has it (~/.config/krate/rack.json).
    const QString krateConfig = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                + QStringLiteral("/krate/rack.json");
    krate::StandaloneSession session(configDir + QStringLiteral("/app.json"), host, &updates, krateConfig);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.setInitialProperties(instrument->panelProperties()); // its controllers, for the panel
    engine.loadFromModule(entry.qmlModule, "Main");

    krate::installScreenshotHook(engine);

    const int result = app.exec();
    if (!transport.following()) // a DAW leading over JACK keeps playing when this app closes
        transport.setPlaying(false);
    krate::crash::endedCleanly();
    return result;
}
