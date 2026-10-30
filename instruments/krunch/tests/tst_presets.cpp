#include <QtTest>

#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "PresetController.h"

namespace {

// The amp as far as presets care: a whole-sound object.
struct FakeAmp
{
    QJsonObject sound{{"gain", 0.5}};
    bool dropsPedals = false; // like an amp without one of the preset's pedals
    PresetController::Save save() { return [this] { return sound; }; }
    PresetController::Load load()
    {
        return [this](const QJsonObject &s) {
            sound = s;
            if (dropsPedals)
                sound.remove("pedals");
        };
    }
};

void settle() { QTest::qWait(150); } // the "modified" check runs after a burst of changes

} // namespace

class TestPresets : public QObject
{
    Q_OBJECT

private slots:
    void saveLoadStep()
    {
        QTemporaryDir dir;
        FakeAmp amp;
        PresetController presets(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
        QVERIFY(presets.names().isEmpty());
        QVERIFY(!presets.saveAs("  "));

        QVERIFY(presets.saveAs("Clean"));
        amp.sound = {{"gain", 0.9}};
        QVERIFY(presets.saveAs("  Lead "));
        QCOMPARE(presets.names(), (QStringList{"Clean", "Lead"}));
        QCOMPARE(presets.current(), QString("Lead"));

        presets.load("Clean");
        QCOMPARE(amp.sound.value("gain").toDouble(), 0.5);
        QCOMPARE(presets.current(), QString("Clean"));
        presets.step(1);
        QCOMPARE(presets.current(), QString("Lead"));
        presets.step(1); // round the list
        QCOMPARE(presets.current(), QString("Clean"));
        presets.step(-1);
        QCOMPARE(presets.current(), QString("Lead"));
    }

    void modifiedAndBack()
    {
        QTemporaryDir dir;
        FakeAmp amp;
        PresetController presets(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
        presets.saveAs("Crunch");
        QVERIFY(!presets.modified());
        amp.sound["gain"] = 0.8;
        presets.ampChanged();
        settle();
        QVERIFY(presets.modified());
        amp.sound["gain"] = 0.5; // turned back
        presets.ampChanged();
        settle();
        QVERIFY(!presets.modified());
        amp.sound["gain"] = 0.7;
        presets.ampChanged();
        settle();
        QVERIFY(presets.save()); // over the current one
        QVERIFY(!presets.modified());
        presets.load("Crunch");
        QCOMPARE(amp.sound.value("gain").toDouble(), 0.7);
    }

    // A preset with something this amp can't have (a pedal type it doesn't
    // know, an IR no longer in the library) isn't "modified" as soon as it loads.
    void loadsWhatItCan()
    {
        QTemporaryDir dir;
        FakeAmp amp;
        PresetController presets(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
        amp.sound = {{"gain", 0.4}, {"pedals", "a pedal from the future"}};
        presets.saveAs("Odd");
        amp.dropsPedals = true;
        presets.load("Odd");
        presets.ampChanged();
        settle();
        QVERIFY(!presets.modified());
    }

    void renameAndRemove()
    {
        QTemporaryDir dir;
        FakeAmp amp;
        PresetController presets(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
        presets.saveAs("A");
        presets.saveAs("B");
        QVERIFY(!presets.rename("A", "B")); // taken
        QVERIFY(!presets.rename("A", " "));
        QVERIFY(presets.rename("B", "Bee"));
        QCOMPARE(presets.current(), QString("Bee"));
        QCOMPARE(presets.names(), (QStringList{"A", "Bee"}));
        presets.remove("Bee");
        QCOMPARE(presets.names(), QStringList{"A"});
        QCOMPARE(presets.current(), QString());
    }

    void keptAcrossRuns()
    {
        QTemporaryDir dir;
        FakeAmp amp;
        {
            PresetController presets(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
            presets.saveAs("One");
            amp.sound = {{"gain", 0.1}};
            presets.saveAs("Two");
        }
        PresetController again(dir.filePath("presets.json"), amp.save(), amp.load(), nullptr);
        QCOMPARE(again.names(), (QStringList{"One", "Two"}));
        QCOMPARE(again.current(), QString("Two"));
        QVERIFY(!again.modified()); // the amp is as "Two" left it
        again.load("One");
        QCOMPARE(amp.sound.value("gain").toDouble(), 0.5);
    }
};

QTEST_GUILESS_MAIN(TestPresets)
#include "tst_presets.moc"
