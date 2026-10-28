// kalliope-render: render Kalliope offline to WAV, or benchmark it.
//
//   kalliope-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]
//   kalliope-render --bench [--rate HZ]
//
// Writes DIR/kalliope-sounds.wav: a few hard-coded sounds in turn (brass, PWM
// pad, sync lead, cross-mod bell, resonant sweep), each playing a short phrase
// through MIDI notes on the IInstrument interface, as a host would. <param>
// overrides apply on top of every sound.

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
#include "kalliope/dsp/Engine.h"
#include "kalliope/dsp/Params.h"
#include "tools/WavFile.h"

namespace {

using kalliope::ParamId;
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

float pos(int index, int count) { return kalliope::position(index, count); }

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

std::vector<NoteEvent> chords(std::initializer_list<std::initializer_list<int>> list, double step, double gate)
{
    std::vector<NoteEvent> e;
    double t = 0.0;
    for (const auto &chord : list) {
        for (int n : chord) {
            e.push_back({t, n, 100});
            e.push_back({t + gate, -n, 0});
        }
        t += step;
    }
    return e;
}

std::vector<Sound> sounds()
{
    namespace p = kalliope::positions;
    return {
        {"brass", {{ParamId::Vco1Wave, pos(1, p::kVco1Wave)}, {ParamId::Vco2Wave, pos(1, p::kVco2Wave)},
                   {ParamId::Vco2Fine, 0.56f}, {ParamId::Cutoff, 0.45f}, {ParamId::Res, 0.15f},
                   {ParamId::VcfEnv, 0.35f}, {ParamId::Env1A, 0.35f}, {ParamId::Env1D, 0.5f},
                   {ParamId::Env1S, 0.4f}, {ParamId::Env2A, 0.25f}, {ParamId::Env2R, 0.4f}},
         line({48, 52, 55, 60, 55, 52}, 0.35, 0.3)},
        {"pwm pad", {{ParamId::Vco1Wave, pos(2, p::kVco1Wave)}, {ParamId::Vco2Wave, pos(2, p::kVco2Wave)},
                     {ParamId::Vco2Fine, 0.53f}, {ParamId::Pwm, 0.7f}, {ParamId::PwmSrc, pos(0, p::kPwmSrc)},
                     {ParamId::LfoRate, 0.35f}, {ParamId::Cutoff, 0.55f}, {ParamId::VcfEnv, 0.1f},
                     {ParamId::Env2A, 0.6f}, {ParamId::Env2R, 0.65f}},
         chords({{57, 60, 64}, {55, 59, 62}, {53, 57, 60, 64}}, 1.2, 1.1)},
        {"sync lead", {{ParamId::Vco1Wave, pos(1, p::kVco1Wave)}, {ParamId::Vco2Wave, pos(1, p::kVco2Wave)},
                       {ParamId::Sync, 1.0f}, {ParamId::Mix, 1.0f}, {ParamId::ModEnv, 0.6f},
                       {ParamId::ModDest, pos(2, p::kModDest)}, {ParamId::Env1D, 0.55f}, {ParamId::Env1S, 0.0f},
                       {ParamId::Cutoff, 0.8f}, {ParamId::VcfEnv, 0.0f}},
         line({60, 63, 67, 72, 70, 67}, 0.3, 0.25)},
        {"cross-mod bell", {{ParamId::Vco1Wave, pos(0, p::kVco1Wave)}, {ParamId::Vco2Wave, pos(0, p::kVco2Wave)},
                            {ParamId::Vco2Range, pos(4, p::kVco2Range)}, {ParamId::Vco2Fine, 0.62f},
                            {ParamId::CrossMod, 0.45f}, {ParamId::Mix, 0.0f}, {ParamId::Cutoff, 0.85f},
                            {ParamId::VcfEnv, 0.0f}, {ParamId::Env2D, 0.6f}, {ParamId::Env2S, 0.0f},
                            {ParamId::Env2R, 0.6f}},
         line({72, 79, 76, 84}, 0.6, 0.1)},
        {"resonant sweep", {{ParamId::Vco1Wave, pos(1, p::kVco1Wave)}, {ParamId::Vco1Range, pos(0, p::kVco1Range)},
                            {ParamId::Mix, 0.0f}, {ParamId::Cutoff, 0.2f}, {ParamId::Res, 0.85f},
                            {ParamId::VcfEnv, 0.7f}, {ParamId::Env1A, 0.55f}, {ParamId::Env1D, 0.6f},
                            {ParamId::Env1S, 0.2f}, {ParamId::Slope, 1.0f}},
         line({36, 36, 43, 36}, 0.7, 0.6, 110)},
    };
}

// Plays `phrase` and appends `seconds` of output to `out`. Returns the peak before the master stage.
float play(kalliope::Engine &engine, krate::MasterStage &master, const std::vector<NoteEvent> &phrase,
           double seconds, int rate, std::vector<float> &out)
{
    std::vector<NoteEvent> events = phrase;
    std::sort(events.begin(), events.end(), [](const NoteEvent &a, const NoteEvent &b) { return a.time < b.time; });
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
    for (int i = 0; i < kalliope::ParamCount; ++i)
        if (kalliope::kParams[std::size_t(i)].key == key)
            return i;
    return -1;
}

void usage()
{
    std::fprintf(stderr, "usage: kalliope-render [--out-dir DIR] [--rate HZ] [--<param> 0..1 ...]\n"
                         "       kalliope-render --bench [--rate HZ]\n");
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
        // The heaviest case: all eight voices held with sync, cross mod and full resonance, 60 s.
        kalliope::Engine engine;
        engine.prepare(rate, kBlock);
        engine.setParam(ParamId::Sync, 1.0f);
        engine.setParam(ParamId::CrossMod, 0.5f);
        engine.setParam(ParamId::Res, 1.0f);
        std::vector<float> out;
        out.reserve(std::size_t(rate) * 60 * 2 + 2 * kBlock);
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<NoteEvent> chord;
        for (int note : {36, 43, 48, 52, 55, 60, 64, 67})
            chord.push_back({0.0, note, 100});
        play(engine, master, chord, 60.0, rate, out);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of 8 voices at %d Hz (2x oversampled) in %.3f s: %.2f%% of one core\n", rate,
                    seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    std::vector<float> out;
    std::printf("peak per sound (before the master stage):\n");
    for (const Sound &sound : sounds()) {
        kalliope::Engine engine; // each sound from the default panel
        engine.prepare(rate, kBlock);
        for (const auto &[id, value] : sound.params)
            engine.setParam(id, value);
        for (const auto &[index, value] : overrides)
            engine.setParam(index, value);
        double length = 0.0;
        for (const NoteEvent &e : sound.phrase)
            length = std::max(length, e.time);
        const float peak = play(engine, master, sound.phrase, length + 1.5, rate, out);
        std::printf("  %-15s %6.1f dB\n", sound.name, 20.0 * std::log10(std::max(peak, 1e-9f)));
    }
    const std::string path = outDir + "/kalliope-sounds.wav";
    if (!krate::writeStereoWav(path, out, rate)) {
        std::fprintf(stderr, "kalliope-render: could not write %s\n", path.c_str());
        return 1;
    }
    std::printf("wrote %s: %.1f s\n", path.c_str(), double(out.size() / 2) / rate);
    return 0;
}
