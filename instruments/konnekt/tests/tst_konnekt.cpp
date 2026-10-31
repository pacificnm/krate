#include <QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "konnekt/app/KonnektController.h"
#include "konnekt/dsp/Modules.h"
#include "konnekt/dsp/PatchSpec.h"

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

    // Every lesson loads whole (no unknown module, jack or knob) and sounds.
    void lessons()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        engine.setParam(0, 1.0f);
        KonnektController c(engine, dir.path(), nullptr);
        c.setLessonsDir(QStringLiteral(KONNEKT_LESSONS));
        QCOMPARE(c.lessons().size(), 10);
        for (int i = 0; i < 10; ++i) {
            QFile f(QStringLiteral(KONNEKT_LESSONS "/%1.json").arg(i + 1, 2, 10, QChar(u'0')));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
            QVERIFY(c.loadLesson(i));
            QCOMPARE(c.lesson(), i);
            QVERIFY(c.sheetText().size() > 100);
            QCOMPARE(c.modules().size(), json.value(u"modules").toArray().size());
            QCOMPARE(c.cables().size(), json.value(u"cables").toArray().size());
            for (const QJsonValue m : json.value(u"modules").toArray()) { // every knob key is a knob
                const QJsonObject o = m.toObject();
                const ModuleInfo *info = findModule(o.value(u"type").toString().toStdString());
                QVERIFY(info);
                for (const QString &key : o.value(u"knobs").toObject().keys())
                    QVERIFY2(knobIndex(*info, key.toStdString()) >= 0, qPrintable(key));
            }
            // It sounds: a chord held for a second (the clocked ones play by themselves).
            pump(engine);
            std::vector<float> l(48000), r(48000);
            const krate::MidiEvent notes[] = {{0, 3, {0x90, 48, 110}}, {0, 3, {0x90, 55, 110}}};
            krate::processStereo(engine, l.data(), r.data(), 48000, notes, 2);
            float peak = 0.0f;
            for (float s : l) {
                QVERIFY(std::isfinite(s));
                peak = std::max(peak, std::fabs(s));
            }
            QVERIFY2(peak > 0.02f, qPrintable(c.sheetTitle() + u' ' + QString::number(peak)));
            QVERIFY2(peak < 2.0f, qPrintable(c.sheetTitle() + u' ' + QString::number(peak)));
            const krate::MidiEvent offs[] = {{0, 3, {0x80, 48, 0}}, {0, 3, {0x80, 55, 0}}};
            krate::processStereo(engine, l.data(), r.data(), 48000, offs, 2);
            // Keys up: the ones that aren't clocked go quiet (after their release and echoes).
            bool clocked = false;
            for (const QJsonValue m : json.value(u"modules").toArray())
                clocked = clocked || m.toObject().value(u"type").toString() == u"clock";
            if (clocked)
                continue;
            for (int s = 0; s < 3; ++s)
                krate::processStereo(engine, l.data(), r.data(), 48000);
            float tail = 0.0f;
            for (float s : l)
                tail = std::max(tail, std::fabs(s));
            QVERIFY2(tail < 0.01f, qPrintable(c.sheetTitle() + QStringLiteral(" still sounds ") + QString::number(tail)));
        }
    }

    // Every preset loads whole, sounds without clipping, and (unless it's
    // clocked) goes quiet when the keys are let go.
    void presets()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        c.setPresetsDir(QStringLiteral(KONNEKT_PRESETS));
        const QVariantList presets = c.presets();
        QCOMPARE(presets.size(), 84);
        for (int i = 0; i < int(presets.size()); ++i) {
            QFile f(QStringLiteral(KONNEKT_PRESETS "/%1.json").arg(i + 1, 2, 10, QChar(u'0')));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
            QVERIFY(!presets[i].toMap().value(QStringLiteral("category")).toString().isEmpty());
            QVERIFY(c.loadPreset(i));
            QCOMPARE(c.preset(), i);
            QCOMPARE(c.lesson(), -1);
            QVERIFY(c.sheetText().size() > 200);
            QCOMPARE(c.modules().size(), json.value(u"modules").toArray().size());
            QCOMPARE(c.cables().size(), json.value(u"cables").toArray().size());
            bool clocked = false;
            for (const QJsonValue m : json.value(u"modules").toArray()) {
                const QJsonObject o = m.toObject();
                const ModuleInfo *info = findModule(o.value(u"type").toString().toStdString());
                QVERIFY(info);
                for (const QString &key : o.value(u"knobs").toObject().keys())
                    QVERIFY2(knobIndex(*info, key.toStdString()) >= 0, qPrintable(key));
                clocked = clocked || o.value(u"type").toString() == u"clock";
            }
            // Two notes held for 2 s (a fifth; the one-voice presets play the newer).
            pump(engine);
            std::vector<float> l(96000), r(96000);
            const krate::MidiEvent notes[] = {{0, 3, {0x90, 43, 110}}, {0, 3, {0x90, 50, 110}}};
            krate::processStereo(engine, l.data(), r.data(), 96000, notes, 2);
            float peak = 0.0f;
            for (std::size_t s = 0; s < l.size(); ++s) {
                QVERIFY(std::isfinite(l[s]) && std::isfinite(r[s]));
                peak = std::max({peak, std::fabs(l[s]), std::fabs(r[s])});
            }
            QVERIFY2(peak > 0.05f && peak < 1.0f, qPrintable(c.sheetTitle() + u' ' + QString::number(peak)));
            const krate::MidiEvent offs[] = {{0, 3, {0x80, 43, 0}}, {0, 3, {0x80, 50, 0}}};
            krate::processStereo(engine, l.data(), r.data(), 48000, offs, 2);
            if (clocked)
                continue;
            for (int s = 0; s < 6; ++s) // the longest release and reverb tails
                krate::processStereo(engine, l.data(), r.data(), 48000);
            float tail = 0.0f;
            for (std::size_t s = 0; s < 48000; ++s) // the last second (the buffer holds two)
                tail = std::max({tail, std::fabs(l[s]), std::fabs(r[s])});
            QVERIFY2(tail < 0.01f, qPrintable(c.sheetTitle() + QStringLiteral(" still sounds ") + QString::number(tail)));
        }
    }

    // Loading stays cheap for the screen: the lesson and preset lists are read
    // once (loading one doesn't announce them again: the window rebuilt all 84
    // preset buttons on every load), and clearing a rack updates it once, not
    // once per module.
    void loadingDoesNotRebuildTheScreen()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        c.setPresetsDir(QStringLiteral(KONNEKT_PRESETS));
        c.setLessonsDir(QStringLiteral(KONNEKT_LESSONS));
        QSignalSpy sheets(&c, &KonnektController::sheetsChanged);
        QSignalSpy modules(&c, &KonnektController::modulesChanged);
        QVERIFY(c.loadPreset(int(c.presets().size()) - 1));
        QVERIFY(c.loadLesson(0));
        QVERIFY(c.loadPreset(0));
        QCOMPARE(sheets.count(), 0);
        QCOMPARE(c.presets().size(), 84);
        QVERIFY(c.modules().size() > 5);
        modules.clear();
        c.clearPatch();
        QCOMPARE(modules.count(), 1);
        QCOMPARE(c.modules().size(), 0);
        c.clearPatch(); // already empty: nothing to say
        QCOMPARE(modules.count(), 1);
    }

    // Every module, knob and jack has its explanation; every link leads somewhere.
    void explanationsComplete()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        c.setExplainDir(QStringLiteral(KONNEKT_EXPLAIN));
        const QStringList problems = c.explainer().problems();
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral(" | "))));

        // Every card opens, and so does everything it links to.
        QStringList paths{QStringLiteral("signal/audio"), QStringLiteral("signal/pitch"), QStringLiteral("signal/gate"),
                          QStringLiteral("signal/mod"), QStringLiteral("app/voices"), QStringLiteral("app/explain")};
        for (const ModuleInfo *info : moduleTypes()) {
            const QString t = QStringLiteral("t/") + QString::fromUtf8(info->type);
            paths << t;
            for (const KnobInfo &k : info->knobs)
                paths << t + QStringLiteral("/knob/") + QString::fromUtf8(k.key);
            for (const JackInfo &j : info->inputs)
                paths << t + QStringLiteral("/in/") + QString::fromUtf8(j.key);
            for (const JackInfo &j : info->outputs)
                paths << t + QStringLiteral("/out/") + QString::fromUtf8(j.key);
        }
        for (const QString &path : paths) {
            const QVariantMap card = c.explain(path);
            QVERIFY2(!card.value(QStringLiteral("title")).toString().isEmpty(), qPrintable(path));
            QVERIFY2(!card.value(QStringLiteral("sections")).toList().isEmpty(), qPrintable(path));
            for (const QVariant &g : card.value(QStringLiteral("groups")).toList())
                for (const QVariant &l : g.toMap().value(QStringLiteral("links")).toList()) {
                    const QString to = l.toMap().value(QStringLiteral("path")).toString();
                    QVERIFY2(!c.explain(to).isEmpty(), qPrintable(path + QStringLiteral(" -> ") + to));
                    QVERIFY2(!l.toMap().value(QStringLiteral("label")).toString().isEmpty(), qPrintable(to));
                }
            for (const QVariant &f : card.value(QStringLiteral("facts")).toList())
                if (const QString to = f.toMap().value(QStringLiteral("link")).toString(); !to.isEmpty())
                    QVERIFY2(!c.explain(to).isEmpty(), qPrintable(path + QStringLiteral(" -> ") + to));
        }
        // Numbered parts get their number; the filters share the Ladder's knobs.
        QVERIFY(c.explain(QStringLiteral("t/mixer/knob/level3")).value(QStringLiteral("sections")).toList()[0].toMap()
                    .value(QStringLiteral("text")).toString().contains(u'3'));
        QVERIFY(!c.explain(QStringLiteral("t/ota_filter/knob/res")).value(QStringLiteral("sections")).toList().isEmpty());
        QVERIFY(c.explain(QStringLiteral("t/nonsense")).isEmpty());
        QVERIFY(c.explain(QStringLiteral("t/vca/knob/nonsense")).isEmpty());
        QVERIFY(c.explain(QStringLiteral("m/63")).isEmpty());
    }

    // On the rack, a card says what the part is doing: its value, its cables.
    void explanationsShowTheRack()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr); // MIDI In -> Oscillator -> VCA (Envelope) -> Output
        c.setExplainDir(QStringLiteral(KONNEKT_EXPLAIN));
        const QVariantList mods = c.modules();
        const auto slotOf = [&](const char *type) {
            return mods[find(mods, QStringLiteral("type"), QString::fromLatin1(type))].toMap().value(QStringLiteral("slot")).toInt();
        };
        const int osc = slotOf("oscillator"), env = slotOf("envelope"), vca = slotOf("vca");
        const auto factsOf = [&](const QString &path) {
            QStringList out;
            for (const QVariant &f : c.explain(path).value(QStringLiteral("facts")).toList())
                out << f.toMap().value(QStringLiteral("label")).toString() + u'=' + f.toMap().value(QStringLiteral("text")).toString()
                           + u'>' + f.toMap().value(QStringLiteral("link")).toString();
            return out;
        };
        const QString m = QStringLiteral("m/%1");

        QVERIFY2(factsOf(m.arg(vca) + QStringLiteral("/in/cv")).contains(QStringLiteral("Plugged in=Envelope · Env>m/%1/out/env").arg(env)),
                 qPrintable(factsOf(m.arg(vca) + QStringLiteral("/in/cv")).join(u'|')));
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/in/fm")).contains(QStringLiteral("Plugged in=Nothing: it reads 0>")));
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/out/saw")).contains(QStringLiteral("Goes to=VCA · In>m/%1/in/in").arg(vca)));
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/out/sine")).contains(QStringLiteral("Goes to=Nothing yet: click it, then click an input>")));
        QCOMPARE(c.explain(m.arg(osc) + QStringLiteral("/out/saw")).value(QStringLiteral("scopeSlot")).toInt(), osc);
        QCOMPARE(c.explain(QStringLiteral("t/oscillator/out/saw")).value(QStringLiteral("scopeSlot")).toInt(), -1);

        c.setKnob(osc, 0, 1.0); // octave +3: the card follows
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/knob/octave")).contains(QStringLiteral("Now=+3>")));
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/knob/fm_amount")).contains(QStringLiteral("Works with=Sets how much the FM input does>m/%1/in/fm").arg(osc)));
        QVERIFY(factsOf(m.arg(vca) + QStringLiteral("/knob/level")).contains(
            QStringLiteral("Works with=What the CV input reads with nothing plugged in; added to what is>m/%1/in/cv").arg(vca)));
        engine.setCcMapping(21, osc, 1);
        QVERIFY(factsOf(m.arg(osc) + QStringLiteral("/knob/tune")).contains(QStringLiteral("MIDI=Linked to CC 21>")));
        QCOMPARE(c.explain(m.arg(vca)).value(QStringLiteral("facts")).toList()[1].toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("2 in, 1 out"));

        // A link into its own type stays on the module; others go to the type.
        QStringList links;
        for (const QVariant &g : c.explain(m.arg(osc) + QStringLiteral("/knob/width")).value(QStringLiteral("groups")).toList())
            for (const QVariant &l : g.toMap().value(QStringLiteral("links")).toList())
                links << l.toMap().value(QStringLiteral("path")).toString();
        QCOMPARE(links, QStringList{m.arg(osc) + QStringLiteral("/knob/pwm_amount")});
        QVERIFY(c.explain(m.arg(vca)).value(QStringLiteral("groups")).toList().last().toMap().value(QStringLiteral("links")).toList()[0]
                    .toMap().value(QStringLiteral("path")).toString().startsWith(u"t/envelope"));
    }

    // Patches saved by name: listed, loaded, deleted; names made safe.
    void savedPatches()
    {
        QTemporaryDir dir;
        Engine engine;
        engine.prepare(48000.0, 64);
        KonnektController c(engine, dir.path(), nullptr);
        QVERIFY(c.savePatch(QStringLiteral("  My/Bass ")));
        QCOMPARE(c.patchNames(), QStringList{QStringLiteral("My-Bass")});
        QCOMPARE(c.patchName(), QStringLiteral("My-Bass"));
        QVERIFY(!c.savePatch(QStringLiteral("   ")));
        c.clearPatch();
        QVERIFY(c.loadPatch(QStringLiteral("My-Bass")));
        QCOMPARE(c.modules().size(), 5);
        QVERIFY(!c.loadPatch(QStringLiteral("nothing")));
        c.deletePatch(QStringLiteral("My-Bass"));
        QVERIFY(c.patchNames().isEmpty());
        QCOMPARE(c.patchName(), QString());
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
