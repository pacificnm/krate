#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#include "app/MidiLearnController.h"
#include "app/PatternModel.h"
#include "app/SynthController.h"

int main(int argc, char *argv[])
{
    // Every control is re-skinned, so start from the plainest style (docs/UI.md).
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("k303"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // Created before the QML engine (which looks it up as the Synth singleton)
    // and destroyed after it, so audio outlives every binding to it.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // ~/.config/k303/panel.json: knob and switch positions
    SynthController synth(configDir + QStringLiteral("/panel.json"), nullptr);
    // ~/.local/share/k303/patterns.json (docs/SEQUENCER.md#pattern-storage)
    PatternModel pattern(synth.engine(),
                         QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/patterns.json"),
                         nullptr);
    // ~/.config/k303/midimap.json (docs/MIDI.md#mapping-file)
    MidiLearnController midiLearn(synth.engine(), configDir + QStringLiteral("/midimap.json"), nullptr);
    Q_ASSERT(synth.engine().midiLearnMap());
    synth.setMidiInputMatch(midiLearn.inputPortMatch());

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("K303", "Main");

    // Dev hook (docs/BUILD.md): K303_SCREENSHOT=<file.png> saves the window and quits
    // (after K303_SCREENSHOT_DELAY_MS, default 500).
    if (const QString shot = qEnvironmentVariable("K303_SCREENSHOT"); !shot.isEmpty()) {
        const int delayMs = qEnvironmentVariableIntValue("K303_SCREENSHOT_DELAY_MS");
        QTimer::singleShot(delayMs > 0 ? delayMs : 500, &app, [&engine, shot] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            if (!window || !window->grabWindow().save(shot))
                qWarning("K303_SCREENSHOT: could not save %s", qPrintable(shot));
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
