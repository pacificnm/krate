// Updates (docs/UPDATES.md): reading GitHub's latest release, comparing
// versions, the updater's status line, and what the controller says when the
// updater isn't installed.

#include <QtTest>

#include "UpdateController.h"

class TestUpdates : public QObject
{
    Q_OBJECT

private slots:
    void release()
    {
        const QByteArray full = "{\"tag_name\": \"v0.3.0\", \"draft\": false, \"prerelease\": false,"
                                " \"body\": \"## What's new\\n- copies\\n\","
                                " \"assets\": [{\"name\": \"krate_0.3.0_amd64.deb\"}, {\"name\": \"SHA256SUMS\"}]}";
        const QByteArray building = "{\"tag_name\": \"v0.4.0\", \"assets\": []}";
        const QByteArray notFound = "{\"message\": \"Not Found\"}";
        const QByteArray pre = "{\"tag_name\": \"v0.5.0\", \"prerelease\": true}";
        UpdateController::Release r;
        QVERIFY(UpdateController::parseRelease(full, &r));
        QCOMPARE(r.version, QStringLiteral("0.3.0"));
        QCOMPARE(r.notes, QStringLiteral("## What's new\n- copies"));
        QVERIFY(r.hasPackage);

        QVERIFY(UpdateController::parseRelease(building, &r)); // still being built
        QVERIFY(!r.hasPackage);
        QVERIFY(!UpdateController::parseRelease(notFound, &r));
        QVERIFY(!UpdateController::parseRelease(pre, &r));
        QVERIFY(!UpdateController::parseRelease("not json", &r));
    }

    void versions()
    {
        QVERIFY(UpdateController::newer(QStringLiteral("0.3.0"), QStringLiteral("0.2.0")));
        QVERIFY(UpdateController::newer(QStringLiteral("0.10.0"), QStringLiteral("0.9.1"))); // as numbers, not text
        QVERIFY(UpdateController::newer(QStringLiteral("1.0.0"), QStringLiteral("0.99.9")));
        QVERIFY(!UpdateController::newer(QStringLiteral("0.2.0"), QStringLiteral("0.2.0")));
        QVERIFY(!UpdateController::newer(QStringLiteral("0.1.0"), QStringLiteral("0.2.0")));
        QVERIFY(!UpdateController::newer(QStringLiteral("junk"), QStringLiteral("0.2.0")));
    }

    void status()
    {
        QString state, text;
        UpdateController::parseStatus(QStringLiteral("downloading Downloading Krate 0.3.0\n"), &state, &text);
        QCOMPARE(state, QStringLiteral("downloading"));
        QCOMPARE(text, QStringLiteral("Downloading Krate 0.3.0"));
        UpdateController::parseStatus(QStringLiteral("current Krate 0.3.0 is the latest"), &state, &text);
        QCOMPARE(state, QStringLiteral("current"));
    }

    // Run from a build (no updater installed): it can't install, and says so by canInstall.
    void withoutTheUpdater()
    {
        UpdateController updates(QStringLiteral("0.2.0"), QStringLiteral("/nonexistent/krate-update"),
                                 QStringLiteral("/nonexistent/status"));
        QVERIFY(!updates.canInstall());
        QVERIFY(!updates.available());
        updates.install(); // nothing happens
        QCOMPARE(updates.state(), QString());
    }
};

QTEST_GUILESS_MAIN(TestUpdates)
#include "tst_updates.moc"
