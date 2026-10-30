#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "klang/app/KlangInstrument.h"
#include "tools/WavFile.h"

using namespace klang;
using Tool = KlangPatternController::Tool;

namespace {

// A stereo WAV of `seconds` at 44.1 kHz: a quiet hum with loud clicks at `hits` (seconds).
QString writeWav(const QString &path, double seconds, std::vector<double> hits = {})
{
    constexpr int rate = 44100;
    std::vector<float> interleaved;
    for (int i = 0; i < int(seconds * rate); ++i) {
        float x = float(0.001 * std::sin(2.0 * std::numbers::pi * 100.0 * i / rate));
        for (double h : hits)
            if (i >= int(h * rate) && i < int(h * rate) + int(0.03 * rate))
                x = float(0.8 * std::sin(2.0 * std::numbers::pi * 400.0 * (i - h * rate) / rate));
        interleaved.push_back(x);
        interleaved.push_back(x);
    }
    krate::writeStereoWav(path.toStdString(), interleaved, rate);
    return path;
}

QUrl url(const QString &path) { return QUrl::fromLocalFile(path); }

} // namespace

class TestKlang : public QObject
{
    Q_OBJECT

private slots:
    void entryDescribesKlang()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("klang"));
        QCOMPARE(entry.name, QStringLiteral("Klang"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Klang"));
        QCOMPARE(entry.defaultMidiChannel, 10); // channel 11
        QVERIFY(entry.playsNotes);
        QCOMPARE(entry.actions.size(), 36);
        for (const char *a : {"pad1", "pad16", "pattern16", "start", "slice", "next_pad"})
            QVERIFY2(entry.actions.contains(QString::fromLatin1(a)), a);
    }

    // The edited pad: the panel shows its settings, each pad keeps its own.
    void editedPad()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.controller.setEditedPad(5);
        k.controller.setValue(QStringLiteral("cutoff"), 0.25);
        QCOMPARE(k.engine.padParam(5, ParamId::Cutoff), 0.25f);
        k.controller.setEditedPad(6);
        QCOMPARE(k.controller.value(QStringLiteral("cutoff")), double(paramDescriptor(ParamId::Cutoff).defaultValue));
        instance->triggerAction(QStringLiteral("previous_pad"));
        QCOMPARE(k.controller.editedPad(), 5);
        QCOMPARE(k.controller.value(QStringLiteral("cutoff")), 0.25);
    }

    // Loading copies the file into the library; pads.json keeps every pad's
    // sample and settings; a file that isn't audio is refused.
    void loadAndKeep()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("break.wav"), 1.0), b = writeWav(dir.filePath("hit.wav"), 0.25);
        QFile junk(dir.filePath("notes.txt"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("not a sound");
        junk.close();
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            k.pads.loadUrls(2, {url(a), url(b)});
            QCOMPARE(k.pads.name(2), QStringLiteral("break.wav"));
            QCOMPARE(k.pads.name(3), QStringLiteral("hit.wav"));
            QVERIFY(std::abs(k.pads.seconds(2) - 1.0) < 0.001);
            QVERIFY(QFile::exists(dir.filePath("data/samples/break.wav")));
            QVERIFY(k.pads.library().contains(QStringLiteral("break.wav")) && k.pads.library().contains(QStringLiteral("hit.wav")));
            const auto librarySize = k.pads.library().size();
            QCOMPARE(k.pads.waveform(2, 50).size(), 50);
            k.engine.setPadParam(3, ParamId::Tune, position(36, positions::kTune)); // +12
            k.pads.loadUrls(4, {url(dir.filePath("notes.txt"))});
            QVERIFY(k.pads.message().startsWith(QStringLiteral("Could not load notes.txt")));
            QCOMPARE(k.pads.name(4), QStringLiteral("05 clank.wav")); // a failed load leaves the pad as it was
            k.pads.loadUrls(5, {url(a)}); // the same file again: the library's copy, not a second one
            QCOMPARE(k.pads.library().size(), librarySize);
            k.pads.step(5, 1); // the next in the library (sorted: the starter kit's "01 kick.wav" ... come first)
            QCOMPARE(k.pads.name(5), k.pads.library().at(k.pads.library().indexOf(QStringLiteral("break.wav")) + 1));
            k.pads.save();
        }
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.pads.name(2), QStringLiteral("break.wav"));
        QCOMPARE(k.pads.name(3), QStringLiteral("hit.wav"));
        QCOMPARE(units::tuneSemitones(k.engine.padParam(3, ParamId::Tune)), 12);
    }

    // An MP3 of your own (KLANG_TEST_MP3=/path/to/file.mp3): decoded, copied into the library, played.
    void loadsAnMp3()
    {
        const QString mp3 = qEnvironmentVariable("KLANG_TEST_MP3");
        if (mp3.isEmpty())
            QSKIP("set KLANG_TEST_MP3 to an MP3 file to check MP3 loading");
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.pads.loadUrls(0, {url(mp3)});
        QCOMPARE(k.pads.name(0), QFileInfo(mp3).fileName());
        QVERIFY(k.pads.seconds(0) > 1.0);
        qInfo("%s: %.1f s", qPrintable(k.pads.name(0)), k.pads.seconds(0));
        k.engine.prepare(48000.0, 256);
        std::vector<float> l(256), r(256);
        k.engine.setPadParam(0, ParamId::Start, 0.3f); // into the music
        k.engine.postHit(0, 127); // as the screen does: taken at the next block, with the sample
        float peak = 0.0f;
        for (int b = 0; b < 100; ++b) {
            krate::processStereo(k.engine, l.data(), r.data(), 256);
            for (float x : l)
                peak = std::max(peak, std::fabs(x));
        }
        QVERIFY(peak > 0.01f);
    }

    // First run: the starter kit's 16 sounds written into the library and onto the pads.
    void starterKit()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.pads.library().size(), 16);
        QCOMPARE(k.pads.name(0), QStringLiteral("01 kick.wav"));
        QCOMPARE(k.pads.name(8), QStringLiteral("09 break 120.wav"));
        QVERIFY(std::abs(k.pads.seconds(8) - 2.0) < 0.001); // one bar at 120 BPM
        QCOMPARE(units::syncBeats(k.engine.padParam(8, ParamId::Sync)), 4);
        QCOMPARE(k.engine.padParam(9, ParamId::Gate), 1.0f); // the drone, gated
        // The break slices at its hits: kick, snare, hats...
        QVERIFY(k.pads.hits(8).size() >= 8);
        // Each sound is something, and nothing clips.
        for (int pad = 0; pad < 16; ++pad) {
            const auto peaks = k.pads.waveform(pad, 100);
            double peak = 0.0;
            for (const QVariant &v : peaks)
                peak = std::max(peak, v.toDouble());
            QVERIFY2(peak > 0.1 && peak <= 1.0, qPrintable(k.pads.name(pad) + u' ' + QString::number(peak)));
        }
    }

    // Equal slices onto the pad and the ones after it; slices at the hits.
    void slicing()
    {
        QTemporaryDir dir;
        const QString loop = writeWav(dir.filePath("loop.wav"), 2.0, {0.0, 0.5, 0.75, 1.5});
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.pads.loadUrls(0, {url(loop)});
        k.engine.setPadParam(0, ParamId::Mode, 1.0f); // a loop: slices become one shots
        k.pads.slice(0, 4);
        for (int pad = 0; pad < 4; ++pad) {
            QCOMPARE(k.pads.name(pad), QStringLiteral("loop.wav"));
            QVERIFY(std::abs(k.engine.padParam(pad, ParamId::Start) - pad * 0.25f) < 1e-6f);
            QVERIFY(std::abs(k.engine.padParam(pad, ParamId::End) - (pad + 1) * 0.25f) < 1e-6f);
            QCOMPARE(k.engine.padParam(pad, ParamId::Mode), 0.0f);
        }
        QCOMPARE(k.pads.name(4), QStringLiteral("05 clank.wav")); // the starter kit's, untouched

        k.engine.setPadParam(0, ParamId::Start, 0.0f);
        k.engine.setPadParam(0, ParamId::End, 1.0f);
        const auto hits = k.pads.hits(0);
        QCOMPARE(int(hits.size()), 4);
        const double expected[] = {0.0, 0.25, 0.375, 0.75}; // 0, 0.5, 0.75, 1.5 s of 2 s
        for (int i = 0; i < 4; ++i)
            QVERIFY2(std::abs(hits[std::size_t(i)] - expected[i]) < 0.006, qPrintable(QString::number(hits[std::size_t(i)])));
        k.pads.sliceAtHits(0);
        QVERIFY(std::abs(k.engine.padParam(3, ParamId::Start) - float(hits[3])) < 1e-6f);
        QCOMPARE(k.engine.padParam(3, ParamId::End), 1.0f);
    }

    // Steps by tool; choosing a pattern cues it; copy and paste; the gate; kept in patterns.json.
    void patterns()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            KlangPatternController &p = k.patterns;
            QVERIFY(p.used(0) && p.used(1)); // the starter patterns
            p.clear();
            p.pressStep(0, 0);
            QVERIFY(p.stepOn(0, 0));
            p.setTool(Tool::Roll);
            p.pressStep(0, 0);
            p.pressStep(0, 0);
            QCOMPARE(p.stepRepeat(0, 0), 3);
            p.setTool(Tool::PitchDown);
            p.pressStep(1, 4); // a new step, pitched
            QVERIFY(p.stepOn(1, 4));
            QCOMPARE(p.stepPitch(1, 4), -1);
            p.setTool(Tool::Reverse);
            p.pressStep(1, 4);
            QVERIFY(p.stepReverse(1, 4));
            p.setTool(Tool::Note);
            p.pressStep(1, 4); // off, and plain again
            QVERIFY(!p.stepOn(1, 4));
            QCOMPARE(p.stepPitch(1, 4), 0);
            p.toggleGateStep(2); // open by default (on, on, on, off ...)
            QVERIFY(!p.gateStep(2));
            p.setLength(40);
            p.setPage(2);
            QCOMPARE(p.page(), 2);
            QVERIFY(p.used(0));
            QVERIFY(p.padUsed(0));

            p.copy();
            instance->triggerAction(QStringLiteral("pattern3"));
            QCOMPARE(p.current(), 2);
            QCOMPARE(k.engine.cuedPattern(), 2);
            QVERIFY(!p.used(2));
            p.paste();
            QCOMPARE(p.stepRepeat(0, 0), 3);
            QCOMPARE(p.length(), 40);
        }
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.patterns.current(), 2);
        QCOMPARE(k.patterns.stepRepeat(0, 0), 3);
        QVERIFY(!k.patterns.gateStep(2));
        QCOMPARE(k.patterns.pattern(0).length, 40);
        QCOMPARE(k.engine.cuedPattern(), 2);
    }
};

QTEST_GUILESS_MAIN(TestKlang)
#include "tst_klang.moc"
