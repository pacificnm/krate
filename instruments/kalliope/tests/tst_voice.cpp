#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "kalliope/dsp/Engine.h"

using namespace kalliope;
namespace p = kalliope::positions;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;

struct Event
{
    double time;
    std::uint8_t status, d1, d2;
};

std::vector<float> render(Engine &engine, std::vector<Event> events, double seconds)
{
    std::vector<float> out;
    std::vector<float> l(kBlock), r(kBlock);
    std::size_t next = 0;
    const auto frames = std::uint64_t(seconds * kRate);
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        std::vector<krate::MidiEvent> midi;
        while (next < events.size() && std::uint64_t(events[next].time * kRate) < start + kBlock) {
            const Event &e = events[next++];
            midi.push_back({std::uint32_t(std::uint64_t(e.time * kRate) - start), 3, {e.status, e.d1, e.d2}});
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        out.insert(out.end(), l.begin(), l.end());
    }
    return out;
}

// A plain test sound: one oscillator, wide-open filter, organ envelope.
Engine &plain(Engine &e, bool vco2 = false)
{
    e.prepare(kRate, kBlock);
    e.setParam(ParamId::Mix, vco2 ? 1.0f : 0.0f);
    e.setParam(ParamId::Cutoff, 1.0f);
    e.setParam(ParamId::Res, 0.0f);
    e.setParam(ParamId::VcfEnv, 0.0f);
    e.setParam(ParamId::KeyFollow, 0.0f);
    e.setParam(ParamId::Env2A, 0.0f);
    e.setParam(ParamId::Env2S, 1.0f);
    e.setParam(ParamId::Env2R, 0.0f);
    return e;
}

// Frequency from upward zero crossings between `from` and `to` seconds.
double frequency(const std::vector<float> &x, double from, double to)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (auto i = std::size_t(from * kRate); i < std::size_t(to * kRate); ++i) {
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
            if (first < 0.0)
                first = t;
            else
                ++count;
            last = t;
        }
    }
    return count > 0 ? count * kRate / (last - first) : 0.0;
}

double rms(const std::vector<float> &x, double from, double to)
{
    double sum = 0.0;
    const auto a = std::size_t(from * kRate), b = std::size_t(to * kRate);
    for (std::size_t i = a; i < b; ++i)
        sum += double(x[i]) * x[i];
    return std::sqrt(sum / double(b - a));
}

double energyAbove(const std::vector<float> &x, std::size_t start, double hz, std::size_t n)
{
    double total = 0.0, above = 0.0;
    for (std::size_t k = 1; k < n / 2; ++k) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * double(k) / double(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double hann = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(n));
            re += hann * x[start + i] * std::cos(w * double(i));
            im -= hann * x[start + i] * std::sin(w * double(i));
        }
        total += re * re + im * im;
        if (double(k) * kRate / double(n) > hz)
            above += re * re + im * im;
    }
    return above / total;
}

std::vector<Event> hold(int note, double length = 10.0)
{
    return {{0.0, 0x90, std::uint8_t(note), 100}, {length, 0x80, std::uint8_t(note), 0}};
}

} // namespace

class TestVoice : public QObject
{
    Q_OBJECT

private slots:
    void vco1Ranges_data()
    {
        QTest::addColumn<int>("range");
        QTest::addColumn<int>("wave");
        QTest::addColumn<double>("hz");
        QTest::newRow("16' saw") << 0 << 1 << 220.0;
        QTest::newRow("8' saw") << 1 << 1 << 440.0;
        QTest::newRow("4' square") << 2 << 3 << 880.0;
        QTest::newRow("2' pulse") << 3 << 2 << 1760.0;
        QTest::newRow("8' triangle") << 1 << 0 << 440.0;
    }
    void vco1Ranges()
    {
        QFETCH(int, range);
        QFETCH(int, wave);
        QFETCH(double, hz);
        Engine e;
        plain(e);
        e.setParam(ParamId::Vco1Range, position(range, p::kVco1Range));
        e.setParam(ParamId::Vco1Wave, position(wave, p::kVco1Wave));
        const auto out = render(e, hold(69), 0.6);
        QVERIFY2(std::abs(frequency(out, 0.1, 0.6) - hz) < hz * 0.002,
                 qPrintable(QString::number(frequency(out, 0.1, 0.6))));
    }

    void vco2RangeAndFine()
    {
        Engine e;
        plain(e, true);
        e.setParam(ParamId::Vco2Range, position(3, p::kVco2Range)); // 4'
        e.setParam(ParamId::Vco2Wave, position(0, p::kVco2Wave));   // sine
        e.setParam(ParamId::Vco2Fine, 1.0f);                         // +50 cents
        const auto out = render(e, hold(69), 0.6);
        const double expected = 880.0 * std::exp2(0.5 / 12.0);
        QVERIFY(std::abs(frequency(out, 0.1, 0.6) - expected) < expected * 0.002);
    }

    void pitchBendAndLegato()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::KeyMode, 1.0f); // Solo: legato, last-note priority
        // Bend fully up (range 2 st), then hold 60, press 64, release 64: back to 60.
        const auto out = render(e, {{0.0, 0xE0, 0x7F, 0x7F}, {0.0, 0x90, 69, 100}, {0.5, 0xE0, 0x00, 0x40},
                                    {0.5, 0x80, 69, 0}, {0.6, 0x90, 60, 100}, {1.0, 0x90, 64, 100},
                                    {1.5, 0x80, 64, 0}},
                            2.0);
        QVERIFY(std::abs(frequency(out, 0.1, 0.45) - 440.0 * std::exp2(2.0 / 12.0)) < 1.0);
        QVERIFY(std::abs(frequency(out, 1.1, 1.45) - 329.63) < 0.7);
        QVERIFY(std::abs(frequency(out, 1.6, 1.95) - 261.63) < 0.6);
    }

    void slope24CutsMoreThan12()
    {
        double level[2];
        for (int slope = 0; slope < 2; ++slope) {
            Engine e;
            plain(e);
            e.setParam(ParamId::Cutoff, 0.4f); // ~320 Hz, two octaves under the note
            e.setParam(ParamId::Slope, float(slope));
            level[slope] = rms(render(e, hold(84), 0.5), 0.1, 0.5);
        }
        QVERIFY(level[1] < 0.5 * level[0]);
    }

    void highPassCutsLows()
    {
        Engine open, cut;
        plain(open);
        plain(cut);
        cut.setParam(ParamId::Hpf, 1.0f); // 1 kHz
        cut.setParam(ParamId::Vco1Wave, position(0, p::kVco1Wave)); // triangle: little above the fundamental
        open.setParam(ParamId::Vco1Wave, position(0, p::kVco1Wave));
        QVERIFY(rms(render(cut, hold(36), 0.5), 0.1, 0.5) < 0.2 * rms(render(open, hold(36), 0.5), 0.1, 0.5));
    }

    // Full resonance, a loud low saw and a cutoff sweep: the ladder rings but stays bounded.
    void boundedAtFullResonance()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::Res, 1.0f);
        e.setParam(ParamId::Cutoff, 0.3f);
        e.setParam(ParamId::VcfEnv, 1.0f);
        e.setParam(ParamId::Env1D, 0.6f);
        e.setParam(ParamId::Env1S, 0.0f);
        e.setParam(ParamId::Vco1Range, 0.0f);
        e.setParam(ParamId::VcaLevel, 1.0f);
        e.setParam(ParamId::Volume, 1.0f);
        const auto out = render(e, hold(36), 2.0);
        float peak = 0.0f;
        for (float s : out) {
            QVERIFY(std::isfinite(s));
            peak = std::max(peak, std::fabs(s));
        }
        QVERIFY2(peak < 2.0f, qPrintable(QString::number(peak)));
        QVERIFY(peak > 0.05f);
    }

    // Sync and cross mod create content far above the note; 2x oversampling and
    // band-limited steps keep what folds back small.
    void syncAndCrossModAliasLittle()
    {
        for (int which = 0; which < 2; ++which) {
            Engine e;
            plain(e, which == 0);
            if (which == 0) { // sync: VCO-2 saw a fifth and a bit above VCO-1, high note
                e.setParam(ParamId::Sync, 1.0f);
                e.setParam(ParamId::Vco2Range, position(3, p::kVco2Range));
                e.setParam(ParamId::Vco2Fine, 0.85f);
            } else { // cross mod into a triangle
                e.setParam(ParamId::Vco1Wave, position(0, p::kVco1Wave));
                e.setParam(ParamId::CrossMod, 0.6f);
                e.setParam(ParamId::Vco2Wave, position(1, p::kVco2Wave));
            }
            const auto out = render(e, hold(84), 0.4);
            const double fraction = energyAbove(out, std::size_t(0.2 * kRate), 20000.0, 2048);
            qInfo("%s: %.3f%% of the energy above 20 kHz", which == 0 ? "sync" : "cross mod", fraction * 100.0);
            QVERIFY(fraction < 0.01);
        }
    }

    void envelopeTimes()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::Vco1Wave, position(3, p::kVco1Wave)); // square: steady level
        e.setParam(ParamId::Env2A, 0.5f);                          // 100 ms attack
        e.setParam(ParamId::Env2R, 0.5f);                          // 100 ms release (to -60 dB)
        const auto out = render(e, {{0.0, 0x90, 69, 127}, {0.5, 0x80, 69, 0}}, 1.0);
        const double early = rms(out, 0.02, 0.04);
        const double full = rms(out, 0.2, 0.4);
        QVERIFY(early < 0.6 * full);
        QVERIFY(rms(out, 0.09, 0.11) > 0.85 * full);
        QVERIFY(rms(out, 0.57, 0.6) < 0.05 * full);
        for (std::size_t i = std::size_t(0.8 * kRate); i < out.size(); ++i)
            QCOMPARE(out[i], 0.0f); // the voice finished: exact silence
    }

    void deterministic()
    {
        Engine a, b;
        plain(a);
        plain(b);
        for (Engine *e : {&a, &b}) {
            e->setParam(ParamId::Sync, 1.0f);
            e->setParam(ParamId::CrossMod, 0.3f);
            e->setParam(ParamId::ModLfo, 0.2f);
        }
        QVERIFY(render(a, hold(60), 0.3) == render(b, hold(60), 0.3));
    }
};

QTEST_GUILESS_MAIN(TestVoice)
#include "tst_voice.moc"
