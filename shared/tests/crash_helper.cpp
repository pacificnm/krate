// For tst_crash: an app that installs crash reports and then ends the way
// it's told: segv, abort, term (asked to quit), vanish (no goodbye), clean.

#include <csignal>

#include <sys/resource.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QTimer>

#include "app/CrashReports.h"

// Out of line and exported (-rdynamic), so the report names it.
extern "C++" __attribute__((noinline)) void crashHereOnPurpose(int *where)
{
    *static_cast<volatile int *>(where) = 42;
}

int main(int argc, char *argv[])
{
    const rlimit noCore{0, 0}; // no core files from the test
    setrlimit(RLIMIT_CORE, &noCore);
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("9.9.9"));
    const QString mode = app.arguments().value(1), dir = app.arguments().value(2);
    krate::crash::install(QStringLiteral("helper"), dir);
    qInfo("helper running: %s", qPrintable(mode));

    if (mode == u"segv")
        crashHereOnPurpose(nullptr);
    else if (mode == u"abort")
        qFatal("a check failed on purpose");
    else if (mode == u"vanish")
        _exit(3);
    else if (mode == u"term") {
        QTimer::singleShot(0, [] { raise(SIGTERM); });
        app.exec(); // the signal quits the loop
    }
    else if (mode == u"term-then-gone") { // asked to quit, then the screen goes before main() ends
        QTimer::singleShot(0, [] { raise(SIGTERM); });
        app.exec();
        _exit(1);
    }
    krate::crash::endedCleanly();
    return 0;
}
