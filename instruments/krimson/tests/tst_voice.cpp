#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "krimson/dsp/Engine.h"

using namespace krimson;
namespace p = krimson::positions;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;
constexpr float kSemi = 1.0f / 48.0f; // one step of OSC 2 Semitones

struct Event
{
    double time;
    std::uint8_t status, d1, d2;
};

std::vector<float> render(Engine &engine, std::vector<Event> events, double seconds)
{
    std::vector<float> out, l(kBlock), r(kBlock);
    std::size_t next = 0;
    for (std::uint64_t start = 0; start < std::uint64_t(seconds * kRate); start += kBlock) {
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

std::vector<Event> hold(int note, double length = 10.0)
{
    return {{0.0, 0x90, std::uint8_t(note), 100}, {length, 0x80, std::uint8_t(note), 0}};
}

// One oscillator, wide-open LP 12, organ envelope, no keyboard tracking on the filter.
Engine &plain(Engine &e, bool osc2 = false)
{
    e.prepare(kRate, kBlock);
    e.setParam(ParamId::OscMix, osc2 ? 1.0f : 0.0f);
    e.setParam(ParamId::Cutoff, 1.0f);
    e.setParam(ParamId::Res, 0.0f);
    e.setParam(ParamId::FenvAmt, 0.0f);
    e.setParam(ParamId::KbdTrack, 0.0f);
    e.setParam(ParamId::AenvA, 0.0f);
    e.setParam(ParamId::AenvS, 1.0f);
    e.setParam(ParamId::AenvR, 0.0f);
    return e;
}

double frequency(const std::vector<float> &x, double from, double to)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (auto i = std::size_t(from * kRate); i < std::size_t(to * kRate); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
            if (first < 0.0)
                first = t;
            else
                ++count;
            last = t;
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

// Amplitude of one frequency (Goertzel) over a window.
double amplitudeAt(const std::vector<float> &x, double hz, double from, double to)
{
    const auto a = std::size_t(from * kRate), b = std::size_t(to * kRate);
    const double w = 2.0 * std::numbers::pi * hz / kRate;
    double re = 0.0, im = 0.0;
    for (std::size_t i = a; i < b; ++i) {
        re += x[i] * std::cos(w * double(i));
        im -= x[i] * std::sin(w * double(i));
    }
    return 2.0 * std::sqrt(re * re + im * im) / double(b - a);
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

} // namespace

class TestVoice : public QObject
{
    Q_OBJECT

private slots:
    void osc1Waves_data()
    {
        QTest::addColumn<int>("wave");
        QTest::newRow("sine") << 0;
        QTest::newRow("triangle") << 1;
        QTest::newRow("saw") << 2;
        QTest::newRow("pulse") << 3;
    }
    void osc1Waves()
    {
        QFETCH(int, wave);
        Engine e;
        plain(e);
        e.setParam(ParamId::Osc1Wave, position(wave, p::kOsc1Wave));
        const auto out = render(e, hold(69), 0.6);
        QVERIFY2(std::abs(frequency(out, 0.1, 0.6) - 440.0) < 0.9, qPrintable(QString::number(frequency(out, 0.1, 0.6))));
    }

    void osc2SemitonesFineAndKeyboard()
    {
        Engine e;
        plain(e, true);
        e.setParam(ParamId::Osc2Wave, position(1, p::kOsc2Wave)); // saw
        e.setParam(ParamId::Osc2Semi, 0.5f + 12 * kSemi);
        e.setParam(ParamId::Osc2Fine, 1.0f); // +50 cents
        auto out = render(e, hold(69), 0.6);
        const double expected = 880.0 * std::exp2(0.5 / 12.0);
        QVERIFY(std::abs(frequency(out, 0.1, 0.6) - expected) < expected * 0.002);

        // Keyboard tracking off: OSC 2 stays at middle C (+ its semitones), whatever the key.
        Engine fixed;
        plain(fixed, true);
        fixed.setParam(ParamId::Osc2Kbd, 0.0f);
        out = render(fixed, hold(81), 0.6);
        QVERIFY(std::abs(frequency(out, 0.1, 0.6) - 261.63) < 0.6);
    }

    void pitchBend()
    {
        Engine e;
        plain(e);
        const auto out = render(e, {{0.0, 0xE0, 0x7F, 0x7F}, {0.0, 0x90, 69, 100}}, 0.6);
        QVERIFY(std::abs(frequency(out, 0.1, 0.6) - 440.0 * std::exp2(2.0 / 12.0)) < 1.0);
    }

    void filterTypes()
    {
        const auto level = [](int type, int note, float cutoff) {
            Engine e;
            plain(e);
            e.setParam(ParamId::FilterType, position(type, p::kFilterType));
            e.setParam(ParamId::Cutoff, cutoff);
            return rms(render(e, hold(note), 0.5), 0.1, 0.5);
        };
        const float c440 = float(std::log(440.0 / 20.0) / std::log(1000.0)); // cutoff at 440 Hz
        // LP 24 cuts more than LP 12 above the cutoff; HP cuts a low note; BP and notch around it.
        QVERIFY(level(1, 84, 0.4f) < 0.6 * level(0, 84, 0.4f));
        QVERIFY(level(3, 36, 0.75f) < 0.3 * level(0, 36, 0.75f));
        QVERIFY(level(2, 36, 0.75f) < 0.5 * level(0, 36, 0.75f));
        // A sine at the cutoff: the notch takes it out.
        const auto sineAt = [&](int type) {
            Engine e;
            plain(e);
            e.setParam(ParamId::Osc1Wave, 0.0f);
            e.setParam(ParamId::FilterType, position(type, p::kFilterType));
            e.setParam(ParamId::Cutoff, c440);
            return rms(render(e, hold(69), 0.5), 0.1, 0.5);
        };
        QVERIFY(sineAt(4) < 0.2 * sineAt(0));
    }

    void boundedAtFullResonanceAndDrive()
    {
        for (int type = 0; type < 5; ++type) {
            Engine e;
            plain(e);
            e.setParam(ParamId::FilterType, position(type, p::kFilterType));
            e.setParam(ParamId::Res, 1.0f);
            e.setParam(ParamId::Drive, 1.0f);
            e.setParam(ParamId::Cutoff, 0.3f);
            e.setParam(ParamId::FenvAmt, 1.0f);
            e.setParam(ParamId::FenvD, 0.6f);
            e.setParam(ParamId::FenvS, 0.0f);
            e.setParam(ParamId::Gain, 1.0f);
            e.setParam(ParamId::Volume, 1.0f);
            const auto out = render(e, hold(36), 1.5);
            float peak = 0.0f;
            for (float s : out) {
                QVERIFY(std::isfinite(s));
                peak = std::max(peak, std::fabs(s));
            }
            QVERIFY2(peak < 1.5f && peak > 0.02f, qPrintable(QStringLiteral("type %1: %2").arg(type).arg(peak)));
        }
    }

    void syncAndFmAliasLittle()
    {
        for (int which = 0; which < 2; ++which) {
            Engine e;
            plain(e, which == 0);
            if (which == 0) {
                e.setParam(ParamId::Sync, 1.0f);
                e.setParam(ParamId::Osc2Semi, 0.5f + 9 * kSemi);
            } else {
                e.setParam(ParamId::Fm, 0.7f);
                e.setParam(ParamId::Osc2Semi, 0.5f + 7 * kSemi);
            }
            const auto out = render(e, hold(84), 0.4);
            const double fraction = energyAbove(out, std::size_t(0.2 * kRate), 20000.0, 2048);
            qInfo("%s: %.3f%% of the energy above 20 kHz", which == 0 ? "sync" : "FM", fraction * 100.0);
            QVERIFY(fraction < 0.01);
        }
    }

    // Ring mod: OSC 1 x OSC 2 replaces OSC 2; OSC 1's own fundamental mostly disappears.
    void ringMod()
    {
        Engine e;
        plain(e, true);
        e.setParam(ParamId::Osc1Wave, 0.0f);                     // sine 440
        e.setParam(ParamId::Osc2Wave, position(0, p::kOsc2Wave)); // triangle
        e.setParam(ParamId::Osc2Semi, 0.5f + 7 * kSemi);
        e.setParam(ParamId::Ring, 1.0f);
        const auto out = render(e, hold(69), 0.6);
        QVERIFY(amplitudeAt(out, 440.0, 0.1, 0.6) < 0.1 * rms(out, 0.1, 0.6));
    }

    // The mod envelope on OSC 2 pitch: high at the start, settling to the plain pitch.
    void modEnvelopeSweepsOsc2()
    {
        Engine e;
        plain(e, true);
        e.setParam(ParamId::MenvAmt, 0.75f); // +50 %: 24 semitones at the peak
        e.setParam(ParamId::MenvD, 0.6f); // about 0.19 s
        const auto out = render(e, hold(57), 1.2);
        QVERIFY(frequency(out, 0.005, 0.03) > 1.5 * 220.0);
        QVERIFY(std::abs(frequency(out, 0.8, 1.2) - 220.0) < 1.0);
    }

    void releaseEndsInSilence()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::AenvR, 0.3f);
        const auto out = render(e, {{0.0, 0x90, 60, 100}, {0.2, 0x80, 60, 0}}, 1.0);
        QVERIFY(rms(out, 0.05, 0.15) > 0.01);
        for (std::size_t i = std::size_t(0.8 * kRate); i < out.size(); ++i)
            QCOMPARE(out[i], 0.0f);
    }

    void deterministic()
    {
        Engine a, b;
        for (Engine *e : {&a, &b}) {
            plain(*e);
            e->setParam(ParamId::Sync, 1.0f);
            e->setParam(ParamId::Fm, 0.3f);
            e->setParam(ParamId::Lfo1Amt, 0.3f);
            e->setParam(ParamId::Lfo1Wave, position(4, p::kLfo1Wave));
        }
        QVERIFY(render(a, hold(60), 0.3) == render(b, hold(60), 0.3));
    }
};

QTEST_GUILESS_MAIN(TestVoice)
#include "tst_voice.moc"
