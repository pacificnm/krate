#include <QtTest>

#include <cmath>
#include <numbers>
#include <set>
#include <vector>

#include "konnekt/dsp/Engine.h"
#include "konnekt/dsp/Modules.h"
#include "konnekt/dsp/PatchSpec.h"

using namespace konnekt;

namespace {

constexpr double kRate = 48000.0;

// One voice; reads a module's output through the engine's probe.
struct Rig
{
    Engine engine;
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<float> left;

    explicit Rig(const PatchSpec &spec)
    {
        engine.prepare(kRate, 64);
        engine.setParam(0, 1.0f);
        modules = build(engine, spec);
    }
    void run(std::uint32_t frames, std::vector<std::array<std::uint8_t, 3>> midi = {})
    {
        std::vector<float> l(frames), r(frames);
        std::vector<krate::MidiEvent> events;
        for (const auto &m : midi)
            events.push_back({0, 3, {m[0], m[1], m[2]}});
        krate::processStereo(engine, l.data(), r.data(), frames, events.data(), std::uint32_t(events.size()));
        left.insert(left.end(), l.begin(), l.end());
    }
    // The last `n` samples of a module's output (n <= kProbeSamples), after running that long with it probed.
    std::vector<float> watch(int slot, int jack, std::uint32_t n, std::vector<std::array<std::uint8_t, 3>> midi = {})
    {
        engine.setProbe(slot, jack);
        run(n, std::move(midi));
        std::vector<float> got(n);
        engine.probe(got);
        return got;
    }
};

PatchSpec spec(std::vector<PatchSpec::ModuleSpec> modules, std::vector<PatchSpec::CableSpec> cables)
{
    PatchSpec p;
    p.voices = 1;
    p.modules = std::move(modules);
    p.cables = std::move(cables);
    return p;
}

double rms(const std::vector<float> &x)
{
    double s = 0.0;
    for (float v : x)
        s += double(v) * v;
    return std::sqrt(s / double(x.size()));
}

std::vector<int> risingEdges(const std::vector<float> &x)
{
    std::vector<int> at;
    for (std::size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.5f && x[i] >= 0.5f)
            at.push_back(int(i));
    return at;
}

} // namespace

class TestModules : public QObject
{
    Q_OBJECT

private slots:
    // A 2093 Hz sine (C7) through each filter at 200 Hz: the low-passes cut it, the high-pass keeps it.
    void filters_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("output");
        QTest::addColumn<bool>("passes");
        QTest::newRow("ladder") << "ladder" << "out" << false;
        QTest::newRow("ota") << "ota_filter" << "out" << false;
        QTest::newRow("svf low") << "svf" << "low" << false;
        QTest::newRow("svf high") << "svf" << "high" << true;
    }
    void filters()
    {
        QFETCH(QString, type);
        QFETCH(QString, output);
        QFETCH(bool, passes);
        Rig rig(spec({{1, "oscillator", {{"octave", 3.0f}}}, {2, type.toStdString(), {{"cutoff", 200.0f}}}},
                     {{1, "sine", 2, "in"}}));
        rig.run(4800);
        const double in = rms(rig.watch(1, 3, 4096));
        const int out = outputIndex(*findModule(type.toStdString()), output.toStdString());
        const double level = rms(rig.watch(2, out, 4096));
        if (passes)
            QVERIFY2(level > 0.8 * in, qPrintable(QString::number(level / in)));
        else
            QVERIFY2(level < 0.05 * in, qPrintable(QString::number(level / in)));
    }

    // Sample & hold: holds still between triggers (a sixteenth at 120 BPM is 6000 samples), moves on at each.
    void sampleHold()
    {
        Rig rig(spec({{0, "clock", {{"tempo", 120.0f}}}, {1, "sample_hold"}}, {{0, "sixteenth", 1, "trigger"}}));
        const auto first = rig.watch(1, 0, 4096);
        const std::set<float> held(first.begin(), first.end());
        QVERIFY(held.size() <= 2);
        const auto later = rig.watch(1, 0, 4096);
        std::set<float> all(later.begin(), later.end());
        all.insert(held.begin(), held.end());
        QVERIFY(all.size() >= 2);
    }

    // Slew: a jump becomes a glide (rise 0.1 s: most of the way after 0.1 s, not all at once).
    void slew()
    {
        Rig rig(spec({{0, "midi_in"}, {1, "slew", {{"rise", 0.1f}}}}, {{0, "pitch", 1, "in"}}));
        rig.run(4800, {{0x90, 48, 100}});
        rig.run(48000);
        auto x = rig.watch(1, 0, 480, {{0x90, 72, 100}}); // 10 ms after a two-octave jump up
        QVERIFY2(x.back() > -1.0f && x.back() < 0.0f, qPrintable(QString::number(x.back())));
        rig.run(4800);
        x = rig.watch(1, 0, 4096); // 0.2 s after the jump
        QVERIFY2(x.back() > 0.99f && x.back() <= 1.0f, qPrintable(QString::number(x.back())));
    }

    // Crusher at 2 bits: a handful of levels.
    void crusher()
    {
        Rig rig(spec({{0, "oscillator"}, {1, "crusher", {{"bits", 15.0f}}}}, {{0, "sine", 1, "in"}}));
        rig.run(4800);
        const auto x = rig.watch(1, 0, 4096);
        std::set<float> levels(x.begin(), x.end());
        QVERIFY2(levels.size() <= 4, qPrintable(QString::number(levels.size())));
    }

    // Ring mod: the product (two steady 0.5s give 0.25).
    void ringMod()
    {
        Rig rig(spec({{0, "attenuverter", {{"offset_a", 0.5f}, {"offset_b", 0.5f}}}, {1, "ring_mod"}},
                     {{0, "out_a", 1, "a"}, {0, "out_b", 1, "b"}}));
        QCOMPARE(rig.watch(1, 0, 64).back(), 0.25f);
    }

    // Clock at 120 BPM: a sixteenth every 6000 samples.
    void clock()
    {
        Rig rig(spec({{0, "clock", {{"tempo", 120.0f}}}}, {}));
        rig.engine.setProbe(0, 3);
        std::vector<float> all;
        for (int k = 0; k < 6; ++k) {
            rig.run(4096);
            std::vector<float> got(4096);
            rig.engine.probe(got);
            all.insert(all.end(), got.begin(), got.end());
        }
        const auto e = risingEdges(all);
        QVERIFY(e.size() >= 3);
        for (std::size_t i = 1; i < e.size(); ++i)
            QVERIFY2(std::abs(e[i] - e[i - 1] - 6000) <= 1, qPrintable(QString::number(e[i] - e[i - 1])));
    }

    // Sequencer: steps through its notes on each tick, rests where off, wraps at its length.
    void sequencer()
    {
        // A sixteenth every 3000 samples; steps 1 (0 st), 2 (+12, off), 3 (-12); 3 long.
        Rig rig(spec({{0, "clock", {{"tempo", 240.0f}}},
                      {1, "sequencer", {{"pitch1", 0.0f}, {"pitch2", 12.0f}, {"pitch3", -12.0f}, {"gate2", 0.0f}, {"length", 3.0f}}}},
                     {{0, "sixteenth", 1, "clock"}}));
        std::vector<float> pitch;
        rig.engine.setProbe(1, 0);
        for (int k = 0; k < 8; ++k) { // every half step
            rig.run(1500);
            std::array<float, 1> last{};
            rig.engine.probe(last);
            pitch.push_back(last[0]);
        }
        QCOMPARE(pitch, (std::vector<float>{0.0f, 0.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f})); // and round again
        // The gate: open for half of step 1 (12000..13500), shut through step 2 (it's off).
        // The gate (steps start every 3000: round again at 9000): step 1 open 9000..10500,
        // step 2 (12000..15000) a rest, step 3 open again from 15000.
        rig.engine.setProbe(1, 1);
        rig.run(3750); // to 15750
        std::vector<float> gate(4096); // 11654..15750
        rig.engine.probe(gate);
        const auto at = [&](int t) { return gate[std::size_t(t - 11654)]; };
        QCOMPARE(*std::max_element(gate.begin() + (12000 - 11654), gate.begin() + (15000 - 11654)), 0.0f); // the rest
        QCOMPARE(at(15100), 1.0f);
        QCOMPARE(at(11700), 0.0f); // step 1's gate shut at half the step
    }

    // Delay: an echo of a short burst arrives TIME later.
    void delay()
    {
        Rig rig(spec({{0, "midi_in"}, {1, "oscillator"}, {2, "envelope", {{"attack", 0.001f}, {"decay", 0.001f}, {"sustain", 0.0f}, {"release", 0.001f}}},
                      {3, "vca"}, {4, "delay", {{"time", 0.05f}, {"feedback", 0.0f}, {"mix", 1.0f}}}, {5, "output", {{"volume", 1.0f}}}},
                     {{1, "sine", 3, "in"}, {0, "gate", 2, "gate"}, {2, "env", 3, "cv"}, {3, "out", 4, "left"}, {4, "left", 5, "left"}}));
        rig.run(64);
        rig.run(4800, {{0x90, 69, 127}});
        const auto &x = rig.left;
        const auto window = [&](double from, double to) {
            double s = 0.0;
            for (auto i = std::size_t(from * kRate); i < std::size_t(to * kRate); ++i)
                s += double(x[i]) * x[i];
            return s;
        };
        QVERIFY(window(0.03, 0.045) < 1e-6);    // after the burst, before the echo
        QVERIFY(window(0.050, 0.06) > 1e-3);    // the echo
    }

    // The stereo effects make two different sides from one; nothing blows up.
    void stereoEffects()
    {
        for (const char *type : {"chorus", "reverb", "delay"}) {
            Rig rig(spec({{0, "noise"}, {1, type, {{"mix", 1.0f}}}}, {{0, "white", 1, std::string(type) == "chorus" ? "in" : "left"}}));
            rig.run(9600);
            const auto l = rig.watch(1, 0, 4096), r = rig.watch(1, 1, 4096);
            QVERIFY2(rms(l) > 0.01, type);
            bool differ = false;
            for (std::size_t i = 0; i < l.size(); ++i) {
                QVERIFY(std::isfinite(l[i]) && std::isfinite(r[i]));
                differ = differ || l[i] != r[i];
            }
            QVERIFY2(differ, type);
        }
    }

    // Pink noise has less treble than white: its sample-to-sample changes are smaller.
    void noise()
    {
        Rig rig(spec({{0, "noise"}}, {}));
        const auto white = rig.watch(0, 0, 4096), pink = rig.watch(0, 1, 4096);
        const auto roughness = [](const std::vector<float> &x) {
            double d = 0.0;
            for (std::size_t i = 1; i < x.size(); ++i)
                d += std::fabs(x[i] - x[i - 1]);
            return d / rms(x);
        };
        QVERIFY(rms(white) > 0.1 && rms(pink) > 0.02);
        QVERIFY(roughness(pink) < 0.5 * roughness(white));
    }
};

QTEST_GUILESS_MAIN(TestModules)
#include "tst_modules.moc"
