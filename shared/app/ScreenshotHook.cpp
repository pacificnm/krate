#include "app/ScreenshotHook.h"

#include <QCoreApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

namespace krate {

void installScreenshotHook(QQmlApplicationEngine &engine)
{
    const QString shot = qEnvironmentVariable("K303_SCREENSHOT");
    if (shot.isEmpty())
        return;
    const int delayMs = qEnvironmentVariableIntValue("K303_SCREENSHOT_DELAY_MS");
    QTimer::singleShot(delayMs > 0 ? delayMs : 500, &engine, [&engine, shot] {
        const auto roots = engine.rootObjects();
        auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
        if (!window || !window->grabWindow().save(shot))
            qWarning("K303_SCREENSHOT: could not save %s", qPrintable(shot));
        QCoreApplication::quit();
    });
}

} // namespace krate
