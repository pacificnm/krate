#include <QtTest>

#include <cmath>
#include <numbers>
#include <random>
#include <set>
#include <vector>

#include "dsp/Krunch.h"
#include "dsp/Tuner.h"

using namespace krunch;

namespace {

// A plucked string: harmonics at exact multiples of hz, the higher ones
// louder at first and dying faster, `age` seconds after the pluck.
std::vector<float> string(double rate, double hz, std::size_t n, double age = 0.2, float amplitude = 0.3f)
{
    std::vector<float> s(n);
    for (int h = 1; h <= 12 && h * hz < 0.45 * rate; ++h) {
        const double a = (h == 1 ? 0.6 : 1.0 / h) * std::exp(-age * 1.5 * h);
        const double phase = 0.37 * h * h;
        for (std::size_t i = 0; i < n; ++i)
            s[i] += float(a * std::sin(2.0 * std::numbers::pi * h * hz * double(i) / rate + phase));
    }
    float peak = 0.0f;
    for (float v : s)
        peak = std::max(peak, std::fabs(v));
    for (float &v : s)
        v *= amplitude / peak;
    return s;
}

double centsOff(double hz, double expected) { return 1200.0 * std::log2(hz / expected); }

} // namespace

class TestTuner : public QObject
{
    Q_OBJECT

private slots:
    void notesAndNames()
    {
        QCOMPARE(tuner::noteHz(69, 440.0), 440.0);
        QVERIFY(std::fabs(tuner::noteHz(40, 440.0) - 82.4069) < 1e-3); // low E
        QVERIFY(std::fabs(tuner::noteHz(69, 432.0) - 432.0) < 1e-9);
        QVERIFY(std::fabs(tuner::noteOf(110.0, 440.0) - 45.0) < 1e-9);
        QCOMPARE(tuner::noteName(40, false), std::string("E"));
        QCOMPARE(tuner::noteName(39, true), std::string("E♭"));
        QCOMPARE(tuner::noteName(39, false), std::string("D♯"));
        QCOMPARE(tuner::octave(40), 2);
        QCOMPARE(tuner::octave(60), 4);
    }

    void tuningsAreWellFormed()
    {
        QCOMPARE(std::string(tuner::tunings()[0].id), std::string("chromatic"));
        QCOMPARE(std::string(tuner::tuning("nonsense").id), std::string("standard"));
        QCOMPARE(std::string(tuner::tuning(tuner::kDefaultTuning).id), std::string("standard"));
        std::set<std::string> ids;
        for (const tuner::Tuning &t : tuner::tunings()) {
            QVERIFY2(ids.insert(t.id).second, t.id);
            QVERIFY(t.count >= 0 && t.count <= tuner::kMaxStrings);
            for (int s = 1; s < t.count; ++s) // lowest first (an open tuning may repeat none)
                QVERIFY2(t.notes[std::size_t(s)] > t.notes[std::size_t(s - 1)], t.id);
            for (int s = 0; s < t.count; ++s) // every string within the detector's range, at A 400..480
                QVERIFY2(tuner::noteHz(t.notes[std::size_t(s)], tuner::kMinReferenceHz) > PitchDetector::kMinHz, t.id);
        }
        const tuner::Tuning &eb = tuner::tuning("eb");
        QCOMPARE(eb.count, 6);
        QCOMPARE(eb.notes[0], 39);
        QVERIFY(eb.flats);
    }

    void readsAgainstTheTuning()
    {
        const tuner::Tuning &standard = tuner::tuning("standard");
        // A little sharp of the low E.
        auto r = tuner::read(standard, tuner::noteHz(40.1, 440.0), 440.0);
        QCOMPARE(r.string, 0);
        QCOMPARE(r.note, 40);
        QVERIFY(std::fabs(r.cents - 10.0) < 1e-6);
        // Way flat of A (tuning a string up from slack): still the A string, nearer than E.
        r = tuner::read(standard, tuner::noteHz(43.0, 440.0), 440.0);
        QCOMPARE(r.string, 1);
        QVERIFY(std::fabs(r.cents + 200.0) < 1e-6);
        // Drop D: the low string is D.
        r = tuner::read(tuner::tuning("dropd"), tuner::noteHz(38.0, 440.0), 440.0);
        QCOMPARE(r.string, 0);
        QCOMPARE(r.note, 38);
        // Chromatic: the nearest note, no string.
        r = tuner::read(tuner::tuning("chromatic"), tuner::noteHz(61.3, 440.0), 440.0);
        QCOMPARE(r.string, -1);
        QCOMPARE(r.note, 61);
        QVERIFY(std::fabs(r.cents - 30.0) < 1e-6);
        // The reference: 440 Hz is 31.8 cents sharp of A at 432.
        r = tuner::read(tuner::tuning("chromatic"), 440.0, 432.0);
        QCOMPARE(r.note, 69);
        QVERIFY(std::fabs(r.cents - 1200.0 * std::log2(440.0 / 432.0)) < 1e-6);
        QCOMPARE(tuner::read(standard, 0.0, 440.0).note, -1);
    }

    void detectsStrings_data()
    {
        QTest::addColumn<double>("rate");
        QTest::addColumn<double>("hz");
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (int note : {23, 28, 33, 35, 40, 45, 50, 55, 59, 64, 76, 88}) { // bass B0 .. E6 (24th fret)
                const double hz = tuner::noteHz(note + 0.13, 440.0); // a little off the note
                QTest::addRow("%g Hz, note %d", rate, note) << rate << hz;
            }
    }
    void detectsStrings()
    {
        QFETCH(double, rate);
        QFETCH(double, hz);
        PitchDetector detector;
        detector.prepare(rate);
        QVERIFY(detector.windowSize() <= TunerTap::kSize / 2);
        QVERIFY(detector.windowSize() < std::size_t(0.2 * rate)); // quick to follow the string
        for (double age : {0.05, 0.5, 2.0}) {
            const auto s = string(rate, hz, detector.windowSize(), age);
            const auto r = detector.detect(s);
            QVERIFY2(r.hz > 0.0, qPrintable(QStringLiteral("no pitch, age %1").arg(age)));
            const double off = centsOff(r.hz, hz);
            QVERIFY2(std::fabs(off) < 0.5, qPrintable(QStringLiteral("%1 Hz read as %2 (%3 cents), age %4").arg(hz).arg(r.hz).arg(off).arg(age)));
            QVERIFY(r.clarity > 0.8);
        }
    }

    void quietOrNoisyIsNoPitch()
    {
        PitchDetector detector;
        detector.prepare(48000.0);
        std::vector<float> silence(detector.windowSize(), 0.0f);
        QCOMPARE(detector.detect(silence).hz, 0.0);
        // Below -60 dBFS: nothing playing.
        QCOMPARE(detector.detect(string(48000.0, 110.0, detector.windowSize(), 0.2, 0.0005f)).hz, 0.0);
        std::mt19937 random(7);
        std::uniform_real_distribution<float> noise(-0.3f, 0.3f);
        std::vector<float> hiss(detector.windowSize());
        for (float &v : hiss)
            v = noise(random);
        QCOMPARE(detector.detect(hiss).hz, 0.0);
        // Too few samples.
        QCOMPARE(detector.detect(std::span<const float>(hiss).first(100)).hz, 0.0);
    }

    void tapKeepsTheLatestInput()
    {
        TunerTap tap;
        std::vector<float> in(256);
        std::vector<float> out(1000);
        tap.write(in.data(), 256); // closed: nothing kept
        QVERIFY(!tap.read(out));
        tap.setOpen(true);
        float v = 0.0f;
        for (int block = 0; block < 300; ++block) { // wraps the ring
            for (float &x : in)
                x = v++;
            tap.write(in.data(), 256);
        }
        QVERIFY(tap.read(out));
        QCOMPARE(out.back(), v - 1.0f);
        QCOMPARE(out.front(), v - 1000.0f);
        // Closed and opened again: starts over.
        tap.setOpen(false);
        tap.write(in.data(), 256);
        tap.setOpen(true);
        tap.write(nullptr, 256);
        QVERIFY(!tap.read(out));
        std::vector<float> few(256);
        QVERIFY(tap.read(few));
        QCOMPARE(few.back(), 0.0f);
    }

    void engineFeedsTheTapAndMutes()
    {
        Krunch amp;
        amp.prepare(48000.0, 512);
        // The built-in speaker, no capture: silent anyway; so check the tap and the level ramp only.
        amp.tunerTap().setOpen(true);
        std::vector<float> in = string(48000.0, 196.0, 512), left(512), right(512);
        const float *inputs[2] = {in.data(), in.data()};
        float *outputs[2] = {left.data(), right.data()};
        krate::AudioBlock block{};
        block.frames = 512;
        block.inputs = inputs;
        block.outputs = outputs;
        amp.process(block);
        std::vector<float> got(512);
        QVERIFY(amp.tunerTap().read(got));
        QCOMPARE(got, in);
        amp.setMuted(true);
        QVERIFY(amp.muted());
    }
};

QTEST_GUILESS_MAIN(TestTuner)
#include "tst_tuner.moc"
