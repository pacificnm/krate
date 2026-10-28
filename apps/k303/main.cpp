#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QStandardPaths>

#include "app/MidiLearnController.h"
#include "app/PatternModel.h"
#include "app/ScreenshotHook.h"
#include "app/SynthController.h"

// Both QML modules are static libraries: pull in their plugins explicitly.
Q_IMPORT_QML_PLUGIN(KratePlugin)
Q_IMPORT_QML_PLUGIN(K303Plugin)

int main(int argc, char *argv[])
{
    // Every control is re-skinned, so start from the plainest style.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("k303"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // Controllers are created before the QML engine (which looks them up as
    // singletons) and destroyed after it, so audio outlives every binding.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // ~/.config/k303/panel.json: knob and switch positions
    SynthController synth(configDir + QStringLiteral("/panel.json"), nullptr);
    // ~/.local/share/k303/patterns.json
    PatternModel pattern(synth.engine(),
                         QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/patterns.json"),
                         nullptr);
    // ~/.config/k303/midimap.json. Learnable buttons besides the knobs: pattern
    // select 1-8 and run/stop (keys match the panel's LearnOverlays).
    const QStringList actions = {
        QStringLiteral("pattern1"), QStringLiteral("pattern2"), QStringLiteral("pattern3"), QStringLiteral("pattern4"),
        QStringLiteral("pattern5"), QStringLiteral("pattern6"), QStringLiteral("pattern7"), QStringLiteral("pattern8"),
        QStringLiteral("runstop"),
    };
    MidiLearnController midiLearn(synth.engine(), configDir + QStringLiteral("/midimap.json"), nullptr, actions);
    synth.setMidiInputMatch(midiLearn.inputPortMatch());
    QObject::connect(&midiLearn, &MidiLearnController::actionTriggered, &app, [&](const QString &action) {
        if (action == u"runstop")
            synth.setPlaying(!synth.playing());
        else if (action.startsWith(u"pattern"))
            pattern.setPatternIndex(action.mid(7).toInt() - 1);
    });

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("K303", "Main");

    krate::installScreenshotHook(engine);

    return app.exec();
}
