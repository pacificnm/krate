#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "konnekt/dsp/Engine.h"
#include "konnekt/dsp/Modules.h"
#include "konnekt/dsp/PatchSpec.h"

using namespace konnekt;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kHostBlock = 64;

struct Rig
{
    Engine engine;
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<float> left, right;

    explicit Rig(const PatchSpec &spec)
    {
        engine.prepare(kRate, kHostBlock);
        engine.setParam(0, 1.0f); // Konnekt's volume at unity: outputs exact
        modules = build(engine, spec);
    }
    // Runs `frames`, with MIDI events (status, data1, data2) at the start.
    void run(std::uint32_t frames, std::vector<std::array<std::uint8_t, 3>> midi = {})
    {
        std::vector<float> l(frames), r(frames);
        std::vector<krate::MidiEvent> events;
        for (const auto &m : midi)
            events.push_back({0, 3, {m[0], m[1], m[2]}});
        krate::processStereo(engine, l.data(), r.data(), frames, events.data(), std::uint32_t(events.size()));
        left.insert(left.end(), l.begin(), l.end());
        right.insert(right.end(), r.begin(), r.end());
    }
};

// Energy at `hz` in the last `n` samples (Goertzel), normalized.
double tone(const std::vector<float> &x, double hz, std::size_t n)
{
    const std::size_t start = x.size() - n;
    const double w = 2.0 * std::numbers::pi * hz / kRate;
    double s1 = 0.0, s2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double s = x[start + i] + 2.0 * std::cos(w) * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - 2.0 * std::cos(w) * s1 * s2) / double(n);
}

double frequency(const std::vector<float> &x, std::size_t from)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (std::size_t i = from + 1; i < x.size(); ++i)
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

// MIDI In -> Oscillator (sine) -> Output.
PatchSpec plainTone(int voices, float octave = 0.0f)
{
    PatchSpec p;
    p.voices = voices;
    p.modules = {{0, "midi_in"}, {1, "oscillator", {{"octave", octave}}}, {2, "output", {{"volume", 1.0f}}}};
    p.cables = {{0, "pitch", 1, "pitch"}, {1, "sine", 2, "left"}};
    return p;
}

// ... with an envelope and a VCA, so a voice only sounds while its note does.
PatchSpec playable(int voices)
{
    PatchSpec p;
    p.voices = voices;
    p.modules = {{0, "midi_in"}, {1, "oscillator"}, {2, "envelope", {{"attack", 0.001f}, {"sustain", 1.0f}, {"release", 0.001f}}},
                 {3, "vca"}, {4, "output", {{"volume", 1.0f}}}};
    p.cables = {{0, "pitch", 1, "pitch"}, {0, "gate", 2, "gate"}, {2, "env", 3, "cv"}, {1, "sine", 3, "in"},
                {3, "out", 4, "left"}};
    return p;
}

} // namespace

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void moduleTables()
    {
        for (const ModuleInfo *m : moduleTypes()) {
            QVERIFY(findModule(m->type) == m);
            QVERIFY(std::string_view(m->help).size() > 20);
            QVERIFY(int(m->inputs.size()) <= kMaxJacks && int(m->outputs.size()) <= kMaxJacks);
            QVERIFY(int(m->knobs.size()) <= kMaxKnobs);
            for (const JackInfo &j : m->inputs) {
                QVERIFY2(std::string_view(j.help).size() > 10, j.key);
                QVERIFY(j.knob < int(m->knobs.size()) && j.amount < int(m->knobs.size()));
            }
            for (const JackInfo &j : m->outputs)
                QVERIFY2(std::string_view(j.help).size() > 10, j.key);
            for (const KnobInfo &k : m->knobs) {
                QVERIFY2(std::string_view(k.help).size() > 10, k.key);
                QVERIFY(k.defaultValue >= std::min(k.min, k.max) && k.defaultValue <= std::max(k.min, k.max));
                QVERIFY(std::abs(k.value(k.position(k.defaultValue)) - k.defaultValue) < 1e-3f * std::max(1.0f, std::abs(k.max)));
                QVERIFY(k.labels.empty() || int(k.labels.size()) == k.detents);
            }
            delete m->create();
        }
    }

    // Pitch is 1 per octave from C4: A4 is 440 Hz; Octave +1, 880.
    void pitch_data()
    {
        QTest::addColumn<int>("note");
        QTest::addColumn<float>("octave");
        QTest::addColumn<double>("hz");
        QTest::newRow("A4") << 69 << 0.0f << 440.0;
        QTest::newRow("A4, octave +1") << 69 << 1.0f << 880.0;
        QTest::newRow("C3") << 48 << 0.0f << 130.81;
    }
    void pitch()
    {
        QFETCH(int, note);
        QFETCH(float, octave);
        QFETCH(double, hz);
        Rig rig(plainTone(1, octave));
        rig.run(kHostBlock);
        rig.run(48000, {{0x90, std::uint8_t(note), 100}});
        QVERIFY2(std::abs(frequency(rig.left, 4800) - hz) < hz * 0.002, qPrintable(QString::number(frequency(rig.left, 4800))));
    }

    // An unplugged input reads its knob: a VCA's LEVEL alone opens it.
    void unpluggedInputsReadTheirKnobs()
    {
        for (float level : {0.0f, 1.0f}) {
            PatchSpec p;
            p.voices = 1;
            p.modules = {{0, "oscillator"}, {1, "vca", {{"level", level}}}, {2, "output", {{"volume", 1.0f}}}};
            p.cables = {{0, "sine", 1, "in"}, {1, "out", 2, "left"}};
            Rig rig(p);
            rig.run(4800);
            float peak = 0.0f;
            for (float s : rig.left)
                peak = std::max(peak, std::fabs(s));
            if (level == 0.0f)
                QCOMPARE(peak, 0.0f);
            else
                QVERIFY(peak > 0.9f);
        }
    }

    // A per-voice cable into a one-voice input mixes its voices; the right side takes the left's when empty.
    void voicesMixIntoOneVoiceInputs()
    {
        PatchSpec p;
        p.voices = 3;
        p.modules = {{0, "attenuverter", {{"offset_a", 0.25f}}}, {1, "output", {{"volume", 1.0f}}}};
        p.cables = {{0, "out_a", 1, "left"}};
        Rig rig(p);
        rig.run(kHostBlock);
        QCOMPARE(rig.left.back(), 0.75f);
        QCOMPARE(rig.right.back(), 0.75f);
    }

    // Notes go to voices: two notes, two tones.
    void chords()
    {
        Rig rig(playable(4));
        rig.run(kHostBlock);
        rig.run(24000, {{0x90, 60, 127}, {0x90, 67, 127}});
        const double c = tone(rig.left, 261.63, 19200), g = tone(rig.left, 392.0, 19200), e = tone(rig.left, 329.63, 19200);
        QVERIFY(c > 0.1 && g > 0.1);
        QVERIFY(e < 0.02);
        QCOMPARE(rig.engine.notesHeld(), 2);
    }

    // All voices busy: the oldest is taken (its gate closes a block, then opens on the new note).
    void voiceStealing()
    {
        Rig rig(playable(2));
        rig.run(kHostBlock);
        rig.run(4800, {{0x90, 60, 127}});
        rig.run(4800, {{0x90, 64, 127}});
        rig.run(24000, {{0x90, 67, 127}}); // takes 60's voice
        QCOMPARE(rig.engine.notesHeld(), 2);
        QVERIFY(tone(rig.left, 261.63, 19200) < 0.02);
        QVERIFY(tone(rig.left, 392.0, 19200) > 0.1 && tone(rig.left, 329.63, 19200) > 0.1);
    }

    // One voice: the newest key plays; letting it go returns to the one held.
    void oneVoiceLegato()
    {
        Rig rig(plainTone(1));
        rig.engine.setProbe(0, 0); // MIDI In's PITCH
        rig.run(kHostBlock);
        rig.run(512, {{0x90, 60, 100}});
        rig.run(512, {{0x90, 64, 100}});
        std::array<float, 16> p{};
        rig.engine.probe(p);
        QCOMPARE(p.back(), 4.0f / 12.0f);
        rig.run(512, {{0x80, 64, 0}});
        rig.engine.probe(p);
        QCOMPARE(p.back(), 0.0f);
        QCOMPARE(rig.engine.notesHeld(), 1);
    }

    // A loop cable (an attenuverter into itself) arrives one block (16 samples) late.
    void loopsAreABlockLate()
    {
        PatchSpec p;
        p.voices = 1;
        p.modules = {{0, "attenuverter", {{"scale_a", 0.5f}, {"offset_a", 1.0f}}}};
        p.cables = {{0, "out_a", 0, "a"}};
        Rig rig(p);
        rig.engine.setProbe(0, 0);
        rig.run(3 * kBlock);
        std::array<float, 3 * kBlock> got{};
        rig.engine.probe(got);
        QCOMPARE(got[0], 1.0f);             // block 1: nothing yet round the loop
        QCOMPARE(got[kBlock - 1], 1.0f);
        QCOMPARE(got[kBlock], 1.5f);        // block 2: block 1's 1, halved, plus 1
        QCOMPARE(got[2 * kBlock], 1.75f);
    }

    // Modules run sources first, whatever their slots; a loop keeps slot order.
    void signalOrder()
    {
        const std::vector<int> all{0, 1, 2, 3};
        const std::vector<Engine::Cable> chain{{3, 0, 1, 0}, {1, 0, 0, 0}, {0, 0, 2, 0}}; // 3 -> 1 -> 0 -> 2
        QCOMPARE(Engine::signalOrder(all, chain), (std::vector<int>{3, 1, 0, 2}));
        const std::vector<Engine::Cable> loop{{0, 0, 1, 0}, {1, 0, 0, 0}, {1, 0, 2, 0}};
        QCOMPARE(Engine::signalOrder(all, loop), (std::vector<int>{3, 0, 1, 2}));
    }

    // MIDI Learn: arm a knob, a CC takes it and moves it from then on.
    void midiLearn()
    {
        Rig rig(plainTone(1));
        rig.engine.armLearn(1, 1); // the oscillator's TUNE
        rig.run(kHostBlock, {{0xB0, 21, 127}});
        const auto learned = rig.engine.popLearned();
        QVERIFY(learned.has_value());
        QCOMPARE(learned->cc, 21);
        QCOMPARE(rig.modules[1]->knobPosition(1), 1.0f);
        rig.run(kHostBlock, {{0xB0, 21, 0}});
        QCOMPARE(rig.modules[1]->knobPosition(1), 0.0f);
        QCOMPARE(rig.engine.ccMapping(21), std::make_optional(std::make_pair(1, 1)));
        rig.engine.forgetSlot(1);
        QVERIFY(!rig.engine.ccMapping(21));
    }

    // Removing a module: it comes back to be freed, and the cables from it are gone.
    void removeModule()
    {
        Rig rig(plainTone(1));
        rig.run(kHostBlock);
        rig.engine.removeModule(1);
        rig.run(kHostBlock);
        Module *gone = rig.engine.popRetired();
        QCOMPARE(gone, rig.modules[1].get());
        rig.modules[1].release();
        delete gone;
        rig.run(kHostBlock);
        QCOMPARE(rig.left.back(), 0.0f);
    }

    // Everything at once doesn't blow up: every module, every input fed.
    void everyModuleRuns()
    {
        PatchSpec p;
        p.voices = 8;
        int slot = 0;
        for (const ModuleInfo *m : moduleTypes())
            p.modules.push_back({slot++, m->type});
        for (int to = 0; to < slot; ++to)
            for (const JackInfo &j : findModule(p.modules[std::size_t(to)].type)->inputs)
                p.cables.push_back({1, "saw", to, j.key}); // the oscillator into everything
        Rig rig(p);
        rig.run(48000, {{0x90, 60, 100}, {0x90, 64, 100}});
        for (float s : rig.left)
            QVERIFY(std::isfinite(s));
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "tst_engine.moc"
