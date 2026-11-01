// Crash reports (docs/CRASHES.md): an app that crashes, aborts, vanishes,
// is asked to quit or ends well, and the report (or none) the next start makes.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "app/CrashReports.h"

using namespace krate;

class TestCrash : public QObject
{
    Q_OBJECT

    // Runs the helper; returns its exit status (crashed or not) and code.
    static QProcess::ExitStatus run(const QString &mode, const QString &dir, int *code = nullptr)
    {
        QProcess p;
        p.start(QStringLiteral(CRASH_HELPER), {mode, dir});
        p.waitForFinished(10000);
        if (code)
            *code = p.exitCode();
        return p.exitStatus();
    }

    static QString read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

private slots:
    // A segmentation fault: the app still dies of it; the next start's report
    // says what, where (the function, by name), when, and the log before it.
    void segfault()
    {
        QTemporaryDir dir;
        QCOMPARE(run(QStringLiteral("segv"), dir.path()), QProcess::CrashExit);
        const QString path = crash::collect(QStringLiteral("helper"), dir.path());
        QVERIFY(!path.isEmpty());
        const QString report = read(path);
        QVERIFY2(report.contains(u"What happened: Crashed: a segmentation fault"), qPrintable(report));
        QVERIFY(report.contains(u"in the main (screen) thread"));
        QVERIFY(report.contains(u"Signal: SIGSEGV"));
        QVERIFY(report.contains(u"Version: 9.9.9"));
        QVERIFY(report.contains(u"Address: 0x0"));
        QVERIFY2(report.contains(u"#0  crashHereOnPurpose(int*)"), qPrintable(report)); // demangled, innermost first
        QVERIFY(report.contains(u"helper running: segv"));
        QVERIFY(!QFile::exists(dir.filePath("crashes/pending.txt")));
        QVERIFY(!QFile::exists(dir.filePath("helper.running")));
        QVERIFY(crash::collect(QStringLiteral("helper"), dir.path()).isEmpty()); // once

        const auto list = crash::reports(dir.path());
        QCOMPARE(list.size(), 1);
        QVERIFY(list[0].summary.startsWith(u"Crashed: a segmentation fault"));
        QVERIFY(list[0].when.isValid());
    }

    // qFatal (or a failed assert): an abort, with the message in the log.
    void abort()
    {
        QTemporaryDir dir;
        QCOMPARE(run(QStringLiteral("abort"), dir.path()), QProcess::CrashExit);
        const QString report = read(crash::collect(QStringLiteral("helper"), dir.path()));
        QVERIFY2(report.contains(u"Crashed: an abort"), qPrintable(report));
        QVERIFY(report.contains(u"fatal default: a check failed on purpose") || report.contains(u"a check failed on purpose"));
    }

    // Killed without a word (or frozen, or the power went): a report saying so.
    void vanished()
    {
        QTemporaryDir dir;
        int code = 0;
        QCOMPARE(run(QStringLiteral("vanish"), dir.path(), &code), QProcess::NormalExit);
        QCOMPARE(code, 3);
        const QString report = read(crash::collect(QStringLiteral("helper"), dir.path()));
        QVERIFY2(report.contains(u"Ended without finishing"), qPrintable(report));
        QVERIFY(!report.contains(u"Call stack"));
    }

    // SIGTERM (a shutdown, the service stopping): the app quits properly; no report.
    void askedToQuit()
    {
        QTemporaryDir dir;
        int code = -1;
        QCOMPARE(run(QStringLiteral("term"), dir.path(), &code), QProcess::NormalExit);
        QCOMPARE(code, 0);
        QVERIFY(crash::collect(QStringLiteral("helper"), dir.path()).isEmpty());
        QVERIFY(read(dir.filePath("helper.log")).contains(u"asked to quit"));
    }

    // Asked to quit, then gone before main() ends (the screen went first, as
    // when the player shuts down): not a crash either.
    void askedToQuitThenGone()
    {
        QTemporaryDir dir;
        int code = 0;
        QCOMPARE(run(QStringLiteral("term-then-gone"), dir.path(), &code), QProcess::NormalExit);
        QCOMPARE(code, 1);
        QVERIFY(crash::collect(QStringLiteral("helper"), dir.path()).isEmpty());
    }

    void cleanEnd()
    {
        QTemporaryDir dir;
        QCOMPARE(run(QStringLiteral("clean"), dir.path()), QProcess::NormalExit);
        QVERIFY(crash::collect(QStringLiteral("helper"), dir.path()).isEmpty());
        QVERIFY(crash::reports(dir.path()).isEmpty());
    }

    // The next session keeps the last one's log beside its own.
    void logsRotate()
    {
        QTemporaryDir dir;
        run(QStringLiteral("clean"), dir.path());
        run(QStringLiteral("term"), dir.path());
        QVERIFY(read(dir.filePath("helper.log.1")).contains(u"helper running: clean"));
        QVERIFY(read(dir.filePath("helper.log")).contains(u"helper running: term"));
    }

    void frames()
    {
        QCOMPARE(crash::readableFrame(QStringLiteral("/usr/bin/krate(_ZN5krate4Rack7processERKNS_9RackBlockE+0x1c) [0x55d0]")),
                 QStringLiteral("krate::Rack::process(krate::RackBlock const&) +0x1c  (krate)"));
        QCOMPARE(crash::readableFrame(QStringLiteral("/lib/x86_64-linux-gnu/libc.so.6(+0x3fdf0) [0x7f00]")),
                 QStringLiteral("libc.so.6 +0x3fdf0"));
    }
};

QTEST_GUILESS_MAIN(TestCrash)
#include "tst_crash.moc"
