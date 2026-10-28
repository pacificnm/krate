#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <vector>

#include "app/PatternModel.h"
#include "app/Persistence.h"
#include "dsp/Engine.h"

using namespace k303;
namespace kp = k303::persistence;

namespace {

// Let the engine drain its command queue.
void pump(Engine &engine)
{
    std::vector<float> l(64), r(64);
    engine.process(l.data(), r.data(), 64);
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

} // namespace

class TestPattern : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("patterns-%1.json").arg(++m_counter));
    }

    void defaultsWhenNoFile()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        QCOMPARE(model.patternIndex(), 0);
        QCOMPARE(model.length(), 16);
        QCOMPARE(model.rowCount(), 16);
        pump(engine);
        QVERIFY(engine.sequencer().pattern().steps == demoPattern().steps); // sent on startup
    }

    void editsReachTheEngine()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setPatternIndex(5); // empty pattern: all rests
        model.setSelectedStep(3);
        QCOMPARE(model.selectedNote(), -1);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setNote(5);   // a rest becomes a note
        model.octaveUp();
        model.toggleAccent();
        model.toggleSlide();
        QCOMPARE(changed.count(), 4);
        QCOMPARE(model.selectedNote(), 5);
        QCOMPARE(model.data(model.index(3), PatternModel::LabelRole).toString(), QStringLiteral("F↑")); // octave up

        pump(engine);
        const Step &s = engine.sequencer().pattern().steps[3];
        QCOMPARE(s, (Step{5, 1, true, true, Gate::Note}));

        model.octaveUp(); // already at +1: no change, nothing sent
        QCOMPARE(changed.count(), 4);
    }

    void gatesAndClear()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setSelectedStep(0);
        model.setGate(1);
        QCOMPARE(model.selectedGate(), 1);
        QCOMPARE(model.data(model.index(0), PatternModel::LabelRole).toString(), QStringLiteral("–"));
        model.setGate(2);
        QCOMPARE(model.selectedNote(), -1);

        model.clear();
        pump(engine);
        for (const auto &step : engine.sequencer().pattern().steps)
            QCOMPARE(int(step.gate), int(Gate::Rest));
        QCOMPARE(model.length(), 16); // length kept
    }

    void lengthAndActiveSteps()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setLength(4);
        model.setLength(99); // clamped
        QCOMPARE(model.length(), 16);
        model.setLength(4);
        QVERIFY(model.data(model.index(3), PatternModel::ActiveRole).toBool());
        QVERIFY(!model.data(model.index(4), PatternModel::ActiveRole).toBool());
        pump(engine);
        QCOMPARE(int(engine.sequencer().pattern().length), 4);

        model.setSelectedStep(3);
        model.selectNext(); // wraps within the length
        QCOMPARE(model.selectedStep(), 0);
        model.selectPrevious();
        QCOMPARE(model.selectedStep(), 3);
    }

    void switchingPatternsLoadsThem()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setSelectedStep(0);
        model.setNote(7);          // edit pattern 1
        model.setPatternIndex(4);  // switch to an empty one
        pump(engine);
        QCOMPARE(int(engine.sequencer().pattern().steps[0].gate), int(Gate::Rest));
        model.setPatternIndex(0);
        pump(engine);
        QCOMPARE(int(engine.sequencer().pattern().steps[0].note), 7);
    }

    void saveAndReload()
    {
        {
            Engine engine;
            PatternModel model(engine, m_path, nullptr);
            model.setPatternIndex(5);
            model.setSelectedStep(9);
            model.setNote(11);
            model.setLength(12);
            model.save();
        }
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        QCOMPARE(model.patternIndex(), 5);
        QCOMPARE(model.length(), 12);
        model.setSelectedStep(9);
        QCOMPARE(model.selectedNote(), 11);
    }

    void savesShortlyAfterAnEdit()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        QVERIFY(!QFile::exists(m_path));
        model.toggleAccent();
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_path), 4000);
    }

    void savesOnDestruction()
    {
        {
            Engine engine;
            PatternModel model(engine, m_path, nullptr);
            model.setLength(3);
        }
        kp::PatternBank bank;
        QCOMPARE(kp::loadPatterns(m_path, &bank), kp::LoadResult::Loaded);
        QCOMPARE(int(bank.patterns[0].length), 3);
    }

    void invalidFileFallsBackToDefaults()
    {
        writeFile(m_path, "{ not json");
        kp::PatternBank bank;
        QCOMPARE(kp::loadPatterns(m_path, &bank), kp::LoadResult::Invalid);
        QVERIFY(bank.patterns[0].steps == demoPattern().steps);
    }

    void unknownKeysIgnoredAndValuesClamped()
    {
        writeFile(m_path, R"({
            "version": 1, "current": 42, "futureFeature": true,
            "patterns": [ { "length": 40, "swing": 3,
                            "steps": [ { "note": 99, "octave": -5, "accent": true, "gate": "tie", "colour": "red" } ] } ]
        })");
        kp::PatternBank bank;
        QCOMPARE(kp::loadPatterns(m_path, &bank), kp::LoadResult::Loaded);
        QCOMPARE(bank.current, 7);
        QCOMPARE(int(bank.patterns[0].length), 16);
        QCOMPARE(bank.patterns[0].steps[0], (Step{12, -1, true, false, Gate::Tie}));
        QCOMPARE(int(bank.patterns[0].steps[1].gate), int(Gate::Rest)); // missing steps are rests
        // Missing patterns are empty (slots 1..3 get the v2 factory riffs: this is a v1 file).
        QCOMPARE(int(bank.patterns[5].steps[0].gate), int(Gate::Rest));
    }

    void newerVersionIsNotOverwritten()
    {
        const QByteArray newer = R"({ "version": 99, "patterns": [] })";
        writeFile(m_path, newer);
        {
            Engine engine;
            PatternModel model(engine, m_path, nullptr);
            model.toggleAccent();
            model.save();
        }
        QFile original(m_path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), newer);
        QVERIFY(QFile::exists(m_path + QStringLiteral(".v1")));
    }

    void newInstallHasFourFactoryRiffs()
    {
        const kp::PatternBank bank = kp::defaultBank();
        for (int i = 0; i < kFactoryPatternCount; ++i)
            QVERIFY(bank.patterns[i].steps == factoryPattern(i).steps);
        for (int i = kFactoryPatternCount; i < kp::kPatternCount; ++i)
            QVERIFY(kp::isEmpty(bank.patterns[i]));
        for (int i = 0; i < kFactoryPatternCount; ++i) {
            QVERIFY(!kp::isEmpty(factoryPattern(i)));
            for (const auto &s : factoryPattern(i).steps) {
                QVERIFY(s.note <= 12);
                QVERIFY(s.octave >= -1 && s.octave <= 1);
            }
        }
    }

    // A v1 file gains riffs 1..3 in its first empty slots; user patterns are kept.
    void upgradeFromV1FillsEmptySlotsOnly()
    {
        kp::PatternBank v1 = kp::defaultBank();
        for (int i = 1; i < kp::kPatternCount; ++i)
            v1.patterns[i] = kp::fromJson({}).patterns[i]; // empty
        v1.patterns[1].steps[0] = Step{9, 0, false, false, Gate::Note}; // the user's own pattern
        QJsonObject json = kp::toJson(v1);
        json[QStringLiteral("version")] = 1;
        writeFile(m_path, QJsonDocument(json).toJson());

        kp::PatternBank bank;
        QCOMPARE(kp::loadPatterns(m_path, &bank), kp::LoadResult::Loaded);
        QVERIFY(bank.patterns[0].steps == factoryPattern(0).steps);
        QCOMPARE(int(bank.patterns[1].steps[0].note), 9); // untouched
        QVERIFY(bank.patterns[2].steps == factoryPattern(1).steps);
        QVERIFY(bank.patterns[3].steps == factoryPattern(2).steps);
        QVERIFY(bank.patterns[4].steps == factoryPattern(3).steps);
        QVERIFY(kp::isEmpty(bank.patterns[5]));

        // Saved files are v2, so the upgrade runs only once.
        QVERIFY(kp::savePatterns(m_path, bank));
        QFile f(m_path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(f.readAll()).object().value(u"version").toInt(), 2);
    }

    void randomizeMakesAValidRiff()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setPatternIndex(6); // empty
        model.setLength(12);
        model.randomize();
        QCOMPARE(model.length(), 12); // kept
        pump(engine);
        const Pattern &p = engine.sequencer().pattern();
        QCOMPARE(int(p.steps[0].gate), int(Gate::Note));
        int notes = 0;
        for (const auto &s : p.steps) {
            QVERIFY(s.note <= 12);
            QVERIFY(s.octave >= -1 && s.octave <= 1);
            notes += s.gate == Gate::Note;
        }
        QVERIFY(notes >= 4);

        // Two rolls differ (16 steps of random choices: a collision is vanishingly unlikely).
        const auto first = p.steps;
        model.randomize();
        pump(engine);
        QVERIFY(engine.sequencer().pattern().steps != first);
    }

    void labelsShowOctave()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        model.setPatternIndex(7);
        model.setSelectedStep(0);
        model.setNote(3);
        QCOMPARE(model.data(model.index(0), PatternModel::LabelRole).toString(), QStringLiteral("D#"));
        model.octaveUp();
        QCOMPARE(model.data(model.index(0), PatternModel::LabelRole).toString(), QStringLiteral("D#↑"));
        model.octaveDown();
        model.octaveDown();
        QCOMPARE(model.data(model.index(0), PatternModel::LabelRole).toString(), QStringLiteral("D#↓"));
    }

    void jsonRoundTrip()
    {
        kp::PatternBank bank = kp::defaultBank();
        bank.current = 4;
        bank.patterns[4].length = 9;
        bank.patterns[4].steps[8] = Step{3, -1, false, true, Gate::Note};
        const kp::PatternBank back = kp::fromJson(kp::toJson(bank));
        QCOMPARE(back.current, 4);
        for (int i = 0; i < kp::kPatternCount; ++i) {
            QCOMPARE(back.patterns[i].length, bank.patterns[i].length);
            QVERIFY(back.patterns[i].steps == bank.patterns[i].steps);
        }
    }

    void auditionPlaysOnlyWhenStopped()
    {
        Engine engine;
        PatternModel model(engine, m_path, nullptr);
        pump(engine);
        engine.takePeak();
        model.auditionOn(0);
        std::vector<float> l(4800), r(4800);
        engine.process(l.data(), r.data(), 4800);
        QVERIFY(engine.takePeak() > 0.01f);
        model.auditionOff();
        engine.process(l.data(), r.data(), 4800); // 100 ms: release is ~9 ms
        engine.takePeak();                        // discard the release itself
        engine.process(l.data(), r.data(), 480);
        QVERIFY(engine.takePeak() < 0.001f);

        // While the sequencer runs, live notes are ignored (no restart of the voice).
        engine.setRunning(true);
        model.auditionOn(0);
        pump(engine);
        QVERIFY(engine.sequencer().running());
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_counter = 0;
};

QTEST_GUILESS_MAIN(TestPattern)
#include "tst_pattern.moc"
