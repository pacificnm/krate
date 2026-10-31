#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>

#include "konnekt/app/KonnektController.h"

using namespace konnekt;

// The app's controller: modules, cables, knobs, voices, the patch as JSON.
class TestKonnekt : public QObject
{
    Q_OBJECT

    static int find(const QVariantList &list, const QString &key, const QVariant &value)
    {
        for (qsizetype i = 0; i < list.size(); ++i)
            if (list[i].toMap().value(key) == value)
                return int(i);
        return -1;
    }
    // Runs the engine a few blocks so it takes the commands.
    static void pump(Engine &engine)
    {
        std::array<float, 64> l{}, r{};
        for (int i = 0; i < 4; ++i)
            krate::processStereo(engine, l.data(), r.data(), 64);
    }

private slots:
    // First run: MIDI In -> Oscillator -> VCA (opened by an Envelope) -> Output, and it plays.
    void firstPatchPlays()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        QCOMPARE(c.modules().size(), 5);
        QCOMPARE(c.cables().size(), 5);
        pump(engine);
        std::vector<float> l(4800), r(4800);
        const krate::MidiEvent on{0, 3, {0x90, 60, 100}};
        krate::processStereo(engine, l.data(), r.data(), 4800, &on, 1);
        float peak = 0.0f;
        for (float s : l)
            peak = std::max(peak, std::fabs(s));
        QVERIFY(peak > 0.05f);
    }

    void modulesAndCables()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        c.clearPatch();
        pump(engine);
        QCOMPARE(c.modules().size(), 0);
        const int osc = c.addModule(QStringLiteral("oscillator"), 10, 20);
        const int lfo = c.addModule(QStringLiteral("lfo"), 300, 20);
        const int out = c.addModule(QStringLiteral("output"), 600, 20);
        QVERIFY(osc >= 0 && lfo >= 0 && out >= 0);
        QCOMPARE(c.addModule(QStringLiteral("nonsense"), 0, 0), -1);

        QVERIFY(c.plug(osc, 0, out, 0));     // saw -> left
        QVERIFY(c.plug(lfo, 0, osc, 1));     // LFO -> FM
        QVERIFY(c.plug(osc, 3, out, 0));     // sine -> left: replaces the saw's cable
        QCOMPARE(c.cables().size(), 2);
        QCOMPARE(c.cableInto(out, 0).value(QStringLiteral("fromJack")).toInt(), 3);
        QVERIFY(!c.plug(osc, 9, out, 0));    // no such jack
        c.unplug(osc, 1);
        QCOMPARE(c.cables().size(), 1);

        c.moveModule(lfo, 400, 50);
        const QVariantList mods = c.modules();
        QCOMPARE(mods[find(mods, QStringLiteral("slot"), lfo)].toMap().value(QStringLiteral("x")).toDouble(), 400.0);

        c.removeModule(osc); // and its cables
        QCOMPARE(c.cables().size(), 0);
        QCOMPARE(c.modules().size(), 2);
        pump(engine);        // the engine gives it back; it's freed on the next poll
    }

    void knobsAndText()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        c.clearPatch();
        const int f = c.addModule(QStringLiteral("ladder"), 0, 0);
        const int lfo = c.addModule(QStringLiteral("lfo"), 0, 0);
        const int env = c.addModule(QStringLiteral("envelope"), 0, 0);
        QCOMPARE(c.knobText(f, 0, c.knob(f, 0)), QStringLiteral("1.00 kHz")); // cutoff's default
        QCOMPARE(c.knobText(f, 4, 0.0), QStringLiteral("-12 dB"));            // a selector: its label
        QCOMPARE(c.knobText(lfo, 1, 1.0), QStringLiteral("Smooth"));
        QCOMPARE(c.knobText(env, 0, 0.0), QStringLiteral("1.0 ms"));
        c.setKnob(f, 0, 1.0);
        QCOMPARE(c.knob(f, 0), 1.0);
        QCOMPARE(c.knobText(f, 0, 1.0), QStringLiteral("20.00 kHz"));
        c.setVoices(12);
        QCOMPARE(c.voices(), kVoices);
    }

    // Saved and loaded: modules where they were, knobs, cables, voices, MIDI mappings.
    void patchRoundTrip()
    {
        QTemporaryDir dir;
        QJsonObject saved;
        {
            Engine engine;
            engine.prepare(48000.0, 64);
            KonnektController c(engine, dir.path(), nullptr);
            c.clearPatch();
            const int osc = c.addModule(QStringLiteral("oscillator"), 16, 16);
            const int out = c.addModule(QStringLiteral("output"), 300, 16);
            c.plug(osc, 1, out, 0);
            c.setKnob(osc, 0, 1.0); // octave +3
            c.setVoices(2);
            engine.setCcMapping(21, osc, 1);
            saved = c.toJson();
            c.save();
        }
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("current.json"))));
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr); // loads current.json
        QCOMPARE(c.modules().size(), 2);
        QCOMPARE(c.cables().size(), 1);
        QCOMPARE(c.voices(), 2);
        const int osc = c.modules()[0].toMap().value(QStringLiteral("slot")).toInt();
        QCOMPARE(c.knobText(osc, 0, c.knob(osc, 0)), QStringLiteral("+3")); // octave: its label
        QCOMPARE(c.learnedCc(osc, 1), 21);
        QCOMPARE(c.toJson(), saved);
        QVERIFY(!c.fromJson(QJsonObject{{QStringLiteral("hello"), 1}})); // not a patch: nothing changes
        QCOMPARE(c.modules().size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestKonnekt)
#include "tst_konnekt.moc"
