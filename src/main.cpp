#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

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
    SynthController synth(nullptr);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("K303", "Main");

    // Dev hook (docs/BUILD.md): K303_SCREENSHOT=<file.png> saves the window and quits.
    if (const QString shot = qEnvironmentVariable("K303_SCREENSHOT"); !shot.isEmpty()) {
        QTimer::singleShot(500, &app, [&engine, shot] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            if (!window || !window->grabWindow().save(shot))
                qWarning("K303_SCREENSHOT: could not save %s", qPrintable(shot));
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
