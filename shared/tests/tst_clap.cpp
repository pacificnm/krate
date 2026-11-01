#include <QtTest>

#include <cmath>
#include <vector>

#include "plugin/Clap.h"

using namespace krate;
using namespace krate::clap;

// Hosting CLAP plugins (docs/DAW.md, D7), with Krate's test plugin
// (krate-test.clap: a sine synth, a gain): found, made, played through a rack
// slot, knobs, state, a restart asked for, stopped without an audio thread.
class TestClap : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 256;

    struct Services final : HostServices
    {
        int callbacks = 0, restarts = 0;
        void requestCallback() override { ++callbacks; }
        void requestRestart() override { ++restarts; }
        bool registerTimer(std::uint32_t, clap_id *id) override { *id = 1; return true; }
        bool unregisterTimer(clap_id) override { return true; }
        bool registerFd(int, clap_posix_fd_flags_t) override { return true; }
        bool modifyFd(int, clap_posix_fd_flags_t) override { return true; }
        bool unregisterFd(int) override { return true; }
        bool guiRequestResize(std::uint32_t, std::uint32_t) override { return true; }
        bool guiRequestShow() override { return true; }
        bool guiRequestHide() override { return true; }
        void guiClosed(bool) override {}
    };

    static std::shared_ptr<Library> library()
    {
        std::string error;
        auto lib = Library::open(KRATE_TEST_CLAP_DIR "/krate-test.clap", &error);
        if (!lib)
            qWarning("%s", error.c_str());
        return lib;
    }

    // `blocks` blocks through `slot`, a note (key) held from the first; the left channel.
    static std::vector<float> play(IInstrument &slot, int blocks, int key = 69, const float *const *inputs = nullptr)
    {
        std::vector<float> out, l(kBlock), r(kBlock);
        for (int b = 0; b < blocks; ++b) {
            const MidiEvent on{0, 3, {0x90, std::uint8_t(key), 100}};
            TransportState t;
            t.playing = true;
            t.bpm = 120.0;
            float *outputs[2] = {l.data(), r.data()};
            slot.process(AudioBlock{outputs, kBlock, b == 0 && key >= 0 ? &on : nullptr, b == 0 && key >= 0 ? 1u : 0u, &t, inputs});
            out.insert(out.end(), l.begin(), l.end());
        }
        return out;
    }
    // Rising zero crossings a second.
    static double hz(const std::vector<float> &v)
    {
        int crossings = 0;
        for (std::size_t i = 1; i < v.size(); ++i)
            crossings += v[i - 1] < 0.0f && v[i] >= 0.0f;
        return crossings * kRate / double(v.size());
    }

private slots:
    void scans()
    {
        const auto found = scan({KRATE_TEST_CLAP_DIR, "/nowhere/at/all"});
        QCOMPARE(found.size(), std::size_t(2));
        QCOMPARE(found[0].name, std::string("Krate Test Gain")); // by name
        QVERIFY(found[0].effect && !found[0].instrument);
        QCOMPARE(found[1].id, std::string("com.krate.test.synth"));
        QVERIFY(found[1].instrument);
        QVERIFY(!defaultSearchPaths().empty());
    }

    void playsThroughASlot()
    {
        Services services;
        auto synth = Instance::create(library(), "com.krate.test.synth", &services);
        QVERIFY(synth);
        QCOMPARE(synth->info().name, std::string("Krate Test Synth"));
        QCOMPARE(int(synth->parameters().size()), 2);
        QCOMPARE(synth->parameters()[1].key, std::string_view("p2"));
        QCOMPARE(synth->parameters()[1].label, std::string_view("Detune"));
        QCOMPARE(synth->parameters()[1].detents, 25); // -12..12, stepped
        QCOMPARE(synth->param(0), 0.5f);              // Level's default, 0.5 of 0..1
        QCOMPARE(synth->inputChannels(), 0);
        QVERIFY(synth->gui()); // its editor (X11, made only when opened)

        PluginSlot slot;
        slot.prepare(kRate, kBlock);
        QVERIFY(play(slot, 4)[100] == 0.0f); // empty: silence
        QVERIFY(synth->activate(kRate, kBlock));
        QVERIFY(slot.setInstance(synth.get()) == nullptr);
        QCOMPARE(int(slot.parameters().size()), 2);
        std::vector<float> a = play(slot, 375); // 2 s of A4
        QVERIFY(std::fabs(hz(a) - 440.0) < 2.0);
        float peak = 0.0f;
        for (float x : a)
            peak = std::max(peak, std::fabs(x));
        QVERIFY(std::fabs(peak - 0.5f) < 0.01f);

        // Detune up an octave (knob to the top): 880 Hz; Level to 0.25.
        slot.setParam(1, 1.0f);
        slot.setParam(0, 0.25f);
        a = play(slot, 375);
        QVERIFY(std::fabs(hz(a) - 880.0) < 2.0);
        peak = 0.0f;
        for (float x : a)
            peak = std::max(peak, std::fabs(x));
        QVERIFY(std::fabs(peak - 0.25f) < 0.01f);

        // Its state: back in another instance, knobs and all.
        std::vector<std::uint8_t> state;
        QVERIFY(synth->saveState(state));
        auto again = Instance::create(library(), "com.krate.test.synth", &services);
        QVERIFY(again->loadState(state));
        QCOMPARE(again->param(0), 0.25f);
        QCOMPARE(again->param(1), 1.0f);

        // Another rate asks for a restart, and it's silent meanwhile.
        slot.prepare(44100.0, kBlock);
        QVERIFY(synth->wantsRestart());
        QCOMPARE(synth->restartRate(), 44100.0);
        QVERIFY(services.restarts > 0);
        QVERIFY(play(slot, 2)[100] == 0.0f);
        // Out of the slot; stopped and deactivated with no audio thread to do it.
        QVERIFY(slot.setInstance(nullptr) == synth.get());
        synth->deactivate(5);
        QVERIFY(!synth->active());
        QVERIFY(synth->activate(44100.0, kBlock));
        QVERIFY(!synth->wantsRestart());
    }

    void effectTakesItsInput()
    {
        Services services;
        auto gain = Instance::create(library(), "com.krate.test.gain", &services);
        QVERIFY(gain);
        QCOMPARE(gain->inputChannels(), 2);
        QVERIFY(!gain->gui()); // no editor
        QVERIFY(gain->activate(kRate, kBlock));
        PluginSlot slot;
        slot.setInstance(gain.get());
        QCOMPARE(slot.inputChannels(), 2);
        std::vector<float> inL(kBlock, 0.4f), inR(kBlock, -0.2f);
        const float *inputs[2] = {inL.data(), inR.data()};
        QCOMPARE(play(slot, 1, -1, inputs)[10], 0.4f); // Gain 1
        slot.setParam(0, 0.25f);                       // 0.5 of 0..2
        QCOMPARE(play(slot, 1, -1, inputs)[10], 0.2f);
        slot.setInstance(nullptr);
    }
};

QTEST_GUILESS_MAIN(TestClap)
#include "tst_clap.moc"
