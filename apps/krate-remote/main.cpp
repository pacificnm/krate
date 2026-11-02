// krate-remote: Krate Remote, managing a Krate player from another computer
// over SSH (docs/REMOTE.md): its settings, its files, Krate restarted.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "RemoteController.h"
#include "RemoteFiles.h"
#include "Shell.h"
#include "app/ScreenshotHook.h"

int main(int argc, char *argv[])
{
    // ssh asking for the player's password runs this app as its helper.
    if (Shell::isAskpass())
        return Shell::askpassMain(argc, argv);

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion")); // a desktop tool: the plain desktop look
    QGuiApplication::setOrganizationName(QStringLiteral("krate"));
    QGuiApplication::setApplicationName(QStringLiteral("krate-remote")); // ~/.config/krate/krate-remote.conf
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Krate Remote"));
    QGuiApplication::setDesktopFileName(QStringLiteral("krate-remote"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("krate-remote")));

    Shell shell;
    RemoteController remote(shell);
    RemoteFiles files(shell);
    QObject::connect(&remote, &RemoteController::connected, &files, [&] { files.open(remote.home()); });
    QObject::connect(&remote, &RemoteController::stateChanged, &files, [&] {
        if (remote.state() == QLatin1String("disconnected"))
            files.reset();
    });

    QQmlApplicationEngine qml;
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    qml.loadFromModule("KrateRemote", "Main");
    krate::installScreenshotHook(qml);
    return app.exec();
}
