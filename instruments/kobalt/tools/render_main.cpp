// kobalt-render: render Kobalt offline to WAV, or benchmark it.
//
//   kobalt-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]
//   kobalt-render --bench [--rate HZ]
//
// Writes DIR/kobalt-sounds.wav: a few hard-coded sounds in turn (a sub bass,
// a gliding lead played legato, a PWM lead with delay, the envelope fired by
// the LFO, a resonant sweep), each playing a short phrase through MIDI notes
// on the IInstrument interface, as a host would (tempo from the Tempo
// parameter: there is no transport). <param> overrides apply on top of every sound.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/MasterStage.h"
#include "kobalt/dsp/Engine.h"
#include "kobalt/dsp/Params.h"
#include "tools/WavFile.h"

namespace {

using kobalt::ParamId;
constexpr std::uint32_t kBlock = 64;

struct NoteEvent
{
    double time; // seconds
    int note;    // > 0 on, < 0 off (-note)
    int velocity;
};

struct Sound
{
    const char *name;
    std::vector<std::pair<ParamId, float>> params;
    std::vector<NoteEvent> phrase;
};

float pos(int index, int count) { return kobalt::position(index, count); }

// Notes `step` apart, each held for `gate` (longer than `step`: legato).
std::vector<NoteEvent> line(std::initializer_list<int> notes, double step, double gate, int velocity = 100)
{
    std::vector<NoteEvent> e;
    double t = 0.0;
    for (int n : notes) {
        e.push_back({t, n, velocity});
        e.push_back({t + gate, -n, 0});
        t += step;
    }
    return e;
}

std::vector<Sound> sounds()
{
    namespace p = kobalt::positions;
    return {
        {"sub bass", {{ParamId::VcoRange, pos(0, p::kVcoRange)}, {ParamId::SawLevel, 0.5f}, {ParamId::SubLevel, 0.9f},
                      {ParamId::Cutoff, 0.35f}, {ParamId::Res, 0.2f}, {ParamId::VcfEnv, 0.3f},
                      {ParamId::EnvD, 0.4f}, {ParamId::EnvS, 0.3f}, {ParamId::EnvR, 0.2f}},
         line({36, 36, 43, 36, 39, 41}, 0.3, 0.2, 110)},
        {"glide lead", {{ParamId::SawLevel, 0.7f}, {ParamId::PulseLevel, 0.5f}, {ParamId::Cutoff, 0.6f},
                        {ParamId::Res, 0.35f}, {ParamId::VcfEnv, 0.25f}, {ParamId::EnvS, 0.7f},
                        {ParamId::PortaMode, pos(1, p::kPortaMode)}, {ParamId::PortaTime, 0.45f},
                        {ParamId::EnvTrig, pos(1, p::kEnvTrig)}, {ParamId::DelayLevel, 0.3f}},
         line({60, 67, 72, 70, 67, 63, 60}, 0.35, 0.4)},
        {"pwm lead", {{ParamId::SawLevel, 0.0f}, {ParamId::PulseLevel, 0.9f}, {ParamId::Pw, 0.7f},
                      {ParamId::PwmSrc, pos(0, p::kPwmSrc)}, {ParamId::LfoRate, 0.4f}, {ParamId::Cutoff, 0.65f},
                      {ParamId::VcfEnv, 0.15f}, {ParamId::EnvA, 0.3f}, {ParamId::EnvS, 0.8f},
                      {ParamId::EnvR, 0.45f}, {ParamId::DelayLevel, 0.4f}, {ParamId::DelayTime, pos(3, p::kDelayTime)},
                      {ParamId::DelayFb, 0.45f}, {ParamId::ReverbLevel, 0.2f}},
         line({64, 67, 71, 74}, 0.8, 0.7)},
        {"lfo trigger", {{ParamId::SawLevel, 0.6f}, {ParamId::SubLevel, 0.5f}, {ParamId::Cutoff, 0.3f},
                         {ParamId::Res, 0.5f}, {ParamId::VcfEnv, 0.5f}, {ParamId::EnvD, 0.45f}, {ParamId::EnvS, 0.0f},
                         {ParamId::EnvTrig, pos(2, p::kEnvTrig)}, {ParamId::LfoWave, pos(1, p::kLfoWave)},
                         {ParamId::LfoRate, 0.55f}},
         line({48, 51}, 1.5, 1.4)},
        {"resonant sweep", {{ParamId::VcoRange, pos(0, p::kVcoRange)}, {ParamId::SawLevel, 0.8f},
                            {ParamId::Cutoff, 0.15f}, {ParamId::Res, 0.85f}, {ParamId::VcfEnv, 0.7f},
                            {ParamId::EnvA, 0.55f}, {ParamId::EnvD, 0.6f}, {ParamId::EnvS, 0.2f}},
         line({36, 36, 43, 36}, 0.7, 0.6, 110)},
    };
}

// Plays `phrase` and appends `seconds` of output to `out`. Returns the peak before the master stage.
float play(kobalt::Engine &engine, krate::MasterStage &master, const std::vector<NoteEvent> &phrase, double seconds,
           int rate, std::vector<float> &out)
{
    std::vector<NoteEvent> events = phrase;
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent &a, const NoteEvent &b) { return a.time < b.time; });
    const auto frames = std::uint64_t(seconds * rate);
    std::vector<float> l(kBlock), r(kBlock);
    std::vector<krate::MidiEvent> midi;
    std::size_t next = 0;
    float peak = 0.0f;
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        midi.clear();
        while (next < events.size() && std::uint64_t(events[next].time * rate) < start + kBlock) {
            const NoteEvent &e = events[next++];
            const auto frame = std::uint32_t(std::uint64_t(e.time * rate) - start);
            if (e.note > 0)
                midi.push_back({frame, 3, {0x90, std::uint8_t(e.note), std::uint8_t(e.velocity)}});
            else
                midi.push_back({frame, 3, {0x80, std::uint8_t(-e.note), 0}});
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        for (std::uint32_t i = 0; i < kBlock; ++i)
            peak = std::max(peak, std::fabs(l[i]));
        float *const outputs[2] = {l.data(), r.data()};
        master.process(outputs, 2, kBlock);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            out.push_back(l[i]);
            out.push_back(r[i]);
        }
    }
    return peak;
}

int paramIndex(std::string_view key)
{
    for (int i = 0; i < kobalt::ParamCount; ++i)
        if (kobalt::kParams[std::size_t(i)].key == key)
            return i;
    return -1;
}

void usage()
{
    std::fprintf(stderr, "usage: kobalt-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]\n"
                         "       kobalt-render --bench [--rate HZ]\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::string outDir = ".";
    int rate = 48000;
    bool bench = false;
    std::vector<std::pair<int, float>> overrides;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--bench") {
            bench = true;
        } else if (arg == "--out-dir" && hasValue) {
            outDir = argv[++i];
        } else if (arg == "--rate" && hasValue) {
            rate = std::atoi(argv[++i]);
        } else if (arg.rfind("--", 0) == 0 && hasValue && paramIndex(arg.substr(2)) >= 0) {
            overrides.push_back({paramIndex(arg.substr(2)), float(std::atof(argv[++i]))});
        } else {
            usage();
            return 2;
        }
    }
    if (rate < 8000) {
        usage();
        return 2;
    }

    krate::MasterStage master;
    if (bench) {
        // The heaviest case: every source up, full resonance, Analog, the
        // delay and the reverb, one note held for 60 s.
        kobalt::Engine engine;
        engine.prepare(rate, kBlock);
        engine.setParam(ParamId::PulseLevel, 1.0f);
        engine.setParam(ParamId::SubLevel, 1.0f);
        engine.setParam(ParamId::NoiseLevel, 0.5f);
        engine.setParam(ParamId::Res, 1.0f);
        engine.setParam(ParamId::Analog, 1.0f);
        engine.setParam(ParamId::DelayLevel, 0.5f);
        engine.setParam(ParamId::ReverbLevel, 0.3f);
        std::vector<float> out;
        out.reserve(std::size_t(rate) * 60 * 2 + 2 * kBlock);
        const auto t0 = std::chrono::steady_clock::now();
        play(engine, master, {{0.0, 48, 100}}, 60.0, rate, out);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of one voice with delay and reverb at %d Hz (2x oversampled) in %.3f s: %.2f%% of one core\n",
                    rate, seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    std::vector<float> out;
    std::printf("peak per sound (before the master stage):\n");
    for (const Sound &sound : sounds()) {
        kobalt::Engine engine; // each sound from the default panel
        engine.prepare(rate, kBlock);
        for (const auto &[id, value] : sound.params)
            engine.setParam(id, value);
        for (const auto &[index, value] : overrides)
            engine.setParam(index, value);
        double length = 0.0;
        for (const NoteEvent &e : sound.phrase)
            length = std::max(length, e.time);
        const float peak = play(engine, master, sound.phrase, length + 2.0, rate, out);
        std::printf("  %-15s %6.1f dB\n", sound.name, 20.0 * std::log10(std::max(peak, 1e-9f)));
    }
    const std::string path = outDir + "/kobalt-sounds.wav";
    if (!krate::writeStereoWav(path, out, rate)) {
        std::fprintf(stderr, "kobalt-render: could not write %s\n", path.c_str());
        return 1;
    }
    std::printf("wrote %s: %.1f s\n", path.c_str(), double(out.size() / 2) / rate);
    return 0;
}
