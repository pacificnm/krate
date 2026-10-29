#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "core/Transport.h"
#include "dsp/Effects.h"
#include "dsp/Reverb.h"
#include "krimson/dsp/Engine.h"

using namespace krimson;

// Effects (docs/DSP.md#effects-per-slot-then-summed).
class TestFx : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    static double rms(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += double(x[i]) * x[i];
        return std::sqrt(sum / double(to - from));
    }

    // Plays `events` (frame, status, note) through the engine; left channel.
    static std::vector<float> play(Engine &e, std::vector<std::array<int, 3>> events, double seconds,
                                   std::vector<float> *right = nullptr, double bpm = 120.0)
    {
        std::vector<float> out, l(kBlock), r(kBlock);
        krate::TransportState t;
        t.bpm = bpm;
        std::size_t next = 0;
        for (std::uint64_t start = 0; start < std::uint64_t(seconds * kRate); start += kBlock) {
            std::vector<krate::MidiEvent> midi;
            while (next < events.size() && std::uint64_t(events[next][0]) < start + kBlock) {
                const auto &ev = events[next++];
                midi.push_back({std::uint32_t(std::uint64_t(ev[0]) - start), 3,
                                {std::uint8_t(ev[1]), std::uint8_t(ev[2]), std::uint8_t(ev[1] & 0x10 ? 100 : 0)}});
            }
            krate::processStereo(e, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()), &t);
            out.insert(out.end(), l.begin(), l.end());
            if (right)
                right->insert(right->end(), r.begin(), r.end());
        }
        return out;
    }

private slots:
    void chorusModes()
    {
        const std::size_t n = 48000;
        for (int mode : {krate::Chorus::I, krate::Chorus::Ensemble}) {
            krate::Chorus chorus;
            chorus.prepare(kRate);
            std::vector<float> l(n), r(n);
            for (std::size_t i = 0; i < n; ++i)
                l[i] = r[i] = float(0.5 * std::sin(2.0 * std::numbers::pi * 330.0 * double(i) / kRate));
            const std::vector<float> dry = l;
            chorus.processStereo(l.data(), r.data(), std::uint32_t(n), mode);
            QVERIFY(l != r); // stereo
            const double ratio = rms(l, 4800, n) / rms(dry, 4800, n);
            QVERIFY2(ratio > 0.5 && ratio < 1.5, qPrintable(QString::number(ratio)));
        }
        krate::Chorus off;
        off.prepare(kRate);
        std::vector<float> l(1000, 0.3f), r(1000, -0.2f);
        off.processStereo(l.data(), r.data(), 1000, krate::Chorus::Off);
        QVERIFY(l == std::vector<float>(1000, 0.3f) && r == std::vector<float>(1000, -0.2f));
    }

    // A bigger reverb rings longer; the tail is stereo and bounded.
    void reverbSizeSetsTheDecay()
    {
        double tail[2];
        for (int i = 0; i < 2; ++i) {
            krate::Reverb reverb;
            reverb.prepare(kRate);
            std::vector<float> l(std::size_t(3 * kRate), 0.0f), r(l.size(), 0.0f);
            l[0] = r[0] = 1.0f;
            reverb.process(l.data(), r.data(), std::uint32_t(l.size()), i == 0 ? 0.2 : 0.8, 1.0);
            tail[i] = rms(l, std::size_t(1.0 * kRate), std::size_t(1.5 * kRate));
            float peak = 0.0f;
            for (std::size_t k = 1; k < l.size(); ++k) {
                QVERIFY(std::isfinite(l[k]) && std::isfinite(r[k]));
                peak = std::max({peak, std::fabs(l[k]), std::fabs(r[k])});
            }
            QVERIFY(peak < 1.0f);
            QVERIFY(l != r);
        }
        QVERIFY2(tail[1] > 10.0 * tail[0], qPrintable(QStringLiteral("%1 vs %2").arg(tail[1]).arg(tail[0])));
        QVERIFY(std::abs(krate::Reverb::decaySeconds(0.0) - 0.4) < 1e-9);
        QVERIFY(std::abs(krate::Reverb::decaySeconds(1.0) - 8.0) < 1e-9);
    }

    // Each slot's effects only touch that slot: B's reverb rings after B's note,
    // A (no reverb) ends in silence.
    void effectsArePerSlot()
    {
        Engine withTail, dry;
        for (Engine *e : {&withTail, &dry}) {
            e->prepare(kRate, kBlock);
            e->setSlotParam(1, ParamId::ReverbLevel, 1.0f);
            e->setSlotParam(1, ParamId::ReverbSize, 0.7f);
        }
        // Note on slot B's channel (4), released at 0.2 s; or on slot A's (3).
        const auto tailB = play(withTail, {{0, 0x93, 60}, {9600, 0x83, 60}}, 2.0);
        const auto tailA = play(dry, {{0, 0x92, 60}, {9600, 0x82, 60}}, 2.0);
        QVERIFY(rms(tailB, std::size_t(1.0 * kRate), std::size_t(1.5 * kRate)) > 1e-4);
        for (std::size_t i = std::size_t(1.0 * kRate); i < tailA.size(); ++i)
            QCOMPARE(tailA[i], 0.0f);
    }

    // The delay follows the transport's tempo (1/8 at 120 BPM: 0.25 s).
    void delayFollowsTheTempo()
    {
        for (double bpm : {120.0, 60.0}) {
            Engine e;
            e.prepare(kRate, kBlock);
            e.setParam(ParamId::DelayLevel, 1.0f);
            e.setParam(ParamId::DelayFb, 0.0f);
            e.setParam(ParamId::AenvD, 0.3f);
            e.setParam(ParamId::AenvS, 0.0f);
            e.setParam(ParamId::AenvR, 0.2f);
            const auto out = play(e, {{0, 0x92, 72}, {600, 0x82, 72}}, 1.5, nullptr, bpm);
            const auto echo = std::size_t(0.5 * 60.0 / bpm * kRate);
            QVERIFY(rms(out, echo, echo + 480) > 10.0 * rms(out, echo - 2400, echo - 480));
        }
    }

    void everythingAtMaximumStaysBounded()
    {
        Engine e;
        e.prepare(kRate, kBlock);
        for (int s = 0; s < SlotCount; ++s) {
            e.setSlotParam(s, ParamId::Chorus, 1.0f);
            e.setSlotParam(s, ParamId::DelayLevel, 1.0f);
            e.setSlotParam(s, ParamId::DelayFb, 1.0f);
            e.setSlotParam(s, ParamId::ReverbLevel, 1.0f);
            e.setSlotParam(s, ParamId::ReverbSize, 1.0f);
            e.setSlotParam(s, ParamId::Res, 1.0f);
        }
        std::vector<std::array<int, 3>> events;
        for (int n = 0; n < 16; ++n)
            events.push_back({0, 0x92 + n % 4, 40 + 2 * n});
        std::vector<float> right;
        const auto out = play(e, events, 3.0, &right);
        float peak = 0.0f;
        for (std::size_t i = 0; i < out.size(); ++i) {
            QVERIFY(std::isfinite(out[i]) && std::isfinite(right[i]));
            peak = std::max({peak, std::fabs(out[i]), std::fabs(right[i])});
        }
        QVERIFY2(peak < 20.0f, qPrintable(QString::number(peak)));
    }
};

QTEST_GUILESS_MAIN(TestFx)
#include "tst_fx.moc"
