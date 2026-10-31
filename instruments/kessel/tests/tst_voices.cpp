#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "kessel/dsp/Engine.h"

using namespace kessel;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;
constexpr int kNotes[DrumCount] = {36, 38, 41, 45, 48, 37, 39, 42, 46, 49, 51, 56, 54, 52, 34, 60};

struct Render
{
    std::vector<float> left, right;
    float peak() const
    {
        float p = 0.0f;
        for (std::size_t i = 0; i < left.size(); ++i)
            p = std::max({p, std::fabs(left[i]), std::fabs(right[i])});
        return p;
    }
};

// Plays `notes` (frame, note, velocity) and renders `frames` frames.
Render play(Engine &engine, std::vector<std::array<int, 3>> notes, std::uint64_t frames)
{
    Render out;
    std::vector<float> l(kBlock), r(kBlock);
    std::size_t next = 0;
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        std::vector<krate::MidiEvent> midi;
        while (next < notes.size() && std::uint64_t(notes[next][0]) < start + kBlock) {
            const auto &n = notes[next++];
            midi.push_back({std::uint32_t(std::uint64_t(n[0]) - start), 3,
                            {0x99, std::uint8_t(n[1]), std::uint8_t(n[2])}});
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        out.left.insert(out.left.end(), l.begin(), l.end());
        out.right.insert(out.right.end(), r.begin(), r.end());
    }
    return out;
}

// A test sample for the SAMPLE voice: a decaying 200 Hz tone, 0.5 s (no third-party audio).
const Sample &testSample()
{
    static const Sample s = [] {
        std::vector<float> x(24000);
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] = float(std::sin(2.0 * 3.141592653589793 * 200.0 * double(i) / 48000.0) * std::exp(-double(i) / 4800.0));
        return Sample::fromChannels(x, {}, 48000.0, "test");
    }();
    return s;
}

Render hit(int note, int velocity, double seconds)
{
    Engine engine;
    engine.prepare(kRate, kBlock);
    engine.setSample(&testSample());
    return play(engine, {{0, note, velocity}}, std::uint64_t(seconds * kRate));
}

// Fraction of a signal's energy above `hz`, by a plain DFT of the first n samples.
double energyAbove(const std::vector<float> &x, double hz, std::size_t n)
{
    double total = 0.0, above = 0.0;
    for (std::size_t k = 1; k < n / 2; ++k) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * double(k) / double(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double hann = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(n));
            re += hann * x[i] * std::cos(w * double(i));
            im -= hann * x[i] * std::sin(w * double(i));
        }
        const double e = re * re + im * im;
        total += e;
        if (double(k) * kRate / double(n) > hz)
            above += e;
    }
    return total > 0.0 ? above / total : 0.0;
}

} // namespace

class TestVoices : public QObject
{
    Q_OBJECT

private slots:
    void notesMapToVoices()
    {
        QCOMPARE(Engine::drumForNote(36), DrumId::Bd);
        QCOMPARE(Engine::drumForNote(35), DrumId::Bd);
        QCOMPARE(Engine::drumForNote(38), DrumId::Sd);
        QCOMPARE(Engine::drumForNote(42), DrumId::Ch);
        QCOMPARE(Engine::drumForNote(44), DrumId::Ch);
        QCOMPARE(Engine::drumForNote(46), DrumId::Oh);
        QCOMPARE(Engine::drumForNote(51), DrumId::Rd);
        QCOMPARE(Engine::drumForNote(64), DrumId::Count);
        for (int v = 0; v < DrumCount; ++v)
            QCOMPARE(Engine::drumForNote(kNotes[v]), DrumId(v));
    }

    // Every voice sounds, stays bounded even accented, and decays to exact silence.
    void everyVoiceSoundsAndEnds()
    {
        for (int v = 0; v < DrumCount; ++v) {
            const double seconds = v >= int(DrumId::Oh) ? 6.0 : 2.0;
            const Render r = hit(kNotes[v], 127, seconds);
            const float peak = r.peak();
            QVERIFY2(peak > 0.05f, qPrintable(QStringLiteral("voice %1 is too quiet: %2").arg(v).arg(peak)));
            QVERIFY2(peak < 1.0f, qPrintable(QStringLiteral("voice %1 is too loud: %2").arg(v).arg(peak)));
            // The last 100 ms are silent.
            for (std::size_t i = r.left.size() - 4800; i < r.left.size(); ++i)
                QCOMPARE(r.left[i], 0.0f);
        }
    }

    void accentIsLouder()
    {
        for (int v = 0; v < DrumCount; ++v)
            QVERIFY(hit(kNotes[v], 127, 0.3).peak() > 1.3f * hit(kNotes[v], 90, 0.3).peak());
    }

    void deterministic()
    {
        for (int v = 0; v < DrumCount; ++v) {
            const Render a = hit(kNotes[v], 110, 0.5);
            const Render b = hit(kNotes[v], 110, 0.5);
            QVERIFY(a.left == b.left);
            QVERIFY(a.right == b.right);
        }
    }

    // A note lands on its exact frame, even mid-block.
    void sampleAccurate()
    {
        Engine engine;
        engine.prepare(kRate, kBlock);
        const Render r = play(engine, {{101, 36, 127}}, 512);
        for (int i = 0; i < 101; ++i)
            QCOMPARE(r.left[std::size_t(i)], 0.0f);
        QVERIFY(r.left[101] != 0.0f);
    }

    // Metallic voices: PolyBLEP keeps aliasing low; little energy above 20 kHz.
    void littleEnergyAbove20k()
    {
        for (DrumId d : {DrumId::Ch, DrumId::Oh, DrumId::Cr, DrumId::Rd}) {
            const Render r = hit(kNotes[int(d)], 127, 0.1);
            const double fraction = energyAbove(r.left, 20000.0, 2048);
            qInfo("voice %d: %.3f%% of the energy above 20 kHz", int(d), fraction * 100.0);
            QVERIFY2(fraction < 0.02, qPrintable(QStringLiteral("voice %1: %2 of the energy above 20 kHz")
                                                    .arg(int(d)).arg(fraction)));
        }
    }

    // A closed hat cuts a ringing open hat short.
    void closedHatChokesOpenHat()
    {
        Engine engine;
        engine.prepare(kRate, kBlock);
        const auto choke = std::uint64_t(0.1 * kRate);
        play(engine, {{0, 46, 127}, {int(choke), 42, 127}}, choke + std::uint64_t(0.25 * kRate));
        QVERIFY(!engine.sounding(DrumId::Oh));

        Engine open;
        open.prepare(kRate, kBlock);
        play(open, {{0, 46, 127}}, choke + std::uint64_t(0.25 * kRate));
        QVERIFY(open.sounding(DrumId::Oh)); // without the choke it still rings
    }

    // Retriggering a ringing voice fades the old tail instead of jumping.
    void retriggerDoesNotClick()
    {
        for (int v = 0; v < DrumCount; ++v) {
            Engine engine;
            engine.prepare(kRate, kBlock);
            const int at = 2000;
            const Render r = play(engine, {{0, kNotes[v], 127}, {at, kNotes[v], 127}}, 4096);
            // The step across the retrigger is no bigger than the voice's own steps.
            float biggest = 0.0f;
            for (int i = 1; i < at; ++i)
                biggest = std::max(biggest, std::fabs(r.left[std::size_t(i)] - r.left[std::size_t(i - 1)]));
            const float jump = std::fabs(r.left[std::size_t(at)] - r.left[std::size_t(at - 1)]);
            QVERIFY2(jump <= biggest * 1.5f + 1e-3f,
                     qPrintable(QStringLiteral("voice %1: jump %2, largest step %3").arg(v).arg(jump).arg(biggest)));
        }
    }

    // The panel's audition goes through the command queue to the next block.
    void postedTriggerPlays()
    {
        Engine engine;
        engine.prepare(kRate, kBlock);
        Command command;
        command.drum = std::uint8_t(DrumId::Cp);
        QVERIFY(engine.post(command));
        QVERIFY(!engine.sounding(DrumId::Cp));
        const Render r = play(engine, {}, kBlock);
        QVERIFY(engine.sounding(DrumId::Cp));
        QVERIFY(r.peak() > 0.0f);
    }

    void volumeScalesOutput()
    {
        Engine loud, quiet;
        loud.prepare(kRate, kBlock);
        quiet.prepare(kRate, kBlock);
        quiet.setParam(int(ParamId::Volume), 0.4f); // a quarter of the default's gain
        const float a = play(loud, {{0, 36, 127}}, 4800).peak();
        const float b = play(quiet, {{0, 36, 127}}, 4800).peak();
        QVERIFY(std::fabs(b / a - 0.25f) < 0.01f);
    }
};

QTEST_GUILESS_MAIN(TestVoices)
#include "tst_voices.moc"
