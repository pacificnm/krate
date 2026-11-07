#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "CaptureController.h"
#include "dsp/Krunch.h"

using namespace krunch;

// The captures folder (docs/CAPTURES.md): which captures have their cabinet
// in them (their files say so), choosing one setting the channel's CABINET by
// it, and a chosen one whose file has gone.
class TestCaptures : public QObject
{
    Q_OBJECT

    // The capture engine's example model, saved as `name` with this gear type.
    static void writeCapture(const QString &folder, const QString &name, const QString &gearType)
    {
        QFile in(QStringLiteral(NAM_EXAMPLES "/wavenet_a1_standard.nam"));
        QVERIFY(in.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(in.readAll()).object();
        QJsonObject meta = json.value(u"metadata").toObject();
        meta[QStringLiteral("gear_type")] = gearType;
        json[QStringLiteral("metadata")] = meta;
        QFile out(folder + u'/' + name + QStringLiteral(".nam"));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    }

private slots:
    void fullRigsSwitchTheCabinetOff()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(QStringLiteral("captures"));
        QDir().mkpath(folder);
        writeCapture(folder, QStringLiteral("amp alone"), QStringLiteral("amp"));
        writeCapture(folder, QStringLiteral("full rig"), QStringLiteral("amp_cab"));
        Krunch engine;
        engine.prepare(48000.0, 128);
        CaptureController captures(engine, folder, dir.filePath(QStringLiteral("captures.json")), nullptr);
        QCOMPARE(captures.names(), (QStringList{QStringLiteral("amp alone"), QStringLiteral("full rig")}));
        QVERIFY(captures.hasCabinet(QStringLiteral("full rig")));
        QVERIFY(!captures.hasCabinet(QStringLiteral("amp alone")));

        const int cabinet = channelParam(1, ChannelParam::Cabinet);
        QCOMPARE(engine.param(cabinet), 1.0f);
        captures.choose(1, QStringLiteral("full rig"));
        QCOMPARE(engine.param(cabinet), 0.0f); // its own cabinet: none of Krunch's
        QVERIFY(engine.takeParamsChangedByAudio() != 0); // (the panel follows)
        captures.choose(1, QStringLiteral("amp alone"));
        QCOMPARE(engine.param(cabinet), 1.0f); // an amp alone wants one
        engine.setParam(cabinet, 0.0f);
        captures.choose(1, QString()); // none: the switch as it was
        QCOMPARE(engine.param(cabinet), 0.0f);
        QCOMPARE(engine.param(channelParam(0, ChannelParam::Cabinet)), 1.0f); // other channels untouched

        QVERIFY(!captures.missing(1));
        captures.choose(1, QStringLiteral("gone"));
        QVERIFY(captures.missing(1));
        QVERIFY(!captures.missing(0));
    }
};

QTEST_GUILESS_MAIN(TestCaptures)
#include "tst_captures.moc"
