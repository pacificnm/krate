// klang-render: render Klang offline to WAV, or benchmark it.
//
//   klang-render [--out-dir DIR] [--rate HZ]
//   klang-render --bench [--rate HZ]
//
// Writes DIR/klang-sounds.wav: four sounds Klang makes for itself (a kick, a
// metal clank, a noise burst, a drone loop) played on the transport at 120 BPM
// through pads with the gate, the crusher, the ring modulator, the filter, the
// delay and the reverb. No sample files are read.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <string>
#include <vector>

#include "core/MasterStage.h"
#include "core/Transport.h"
#include "klang/dsp/Engine.h"
#include "tools/WavFile.h"

namespace {

using klang::ParamId;
using klang::Sample;
constexpr std::uint32_t kBlock = 64;

std::uint32_t g_noise = 0x2545F491u;
double noise()
{
    g_noise ^= g_noise << 13;
    g_noise ^= g_noise >> 17;
    g_noise ^= g_noise << 5;
    return double(std::int32_t(g_noise)) / 2147483648.0;
}

Sample make(double seconds, double rate, double (*f)(double t), const char *name)
{
    std::vector<float> x(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = float(f(double(i) / rate));
    return Sample::fromChannels(std::move(x), {}, rate, name);
}

double kick(double t)
{
    const double phase = 2.0 * std::numbers::pi * (45.0 * t + 120.0 * (1.0 - std::exp(-t * 30.0)) / 30.0);
    return 0.9 * std::sin(phase) * std::exp(-t * 7.0);
}
double clank(double t)
{
    double y = 0.0;
    for (double f : {317.0, 523.0, 811.0, 1297.0, 2083.0})
        y += std::sin(2.0 * std::numbers::pi * f * t);
    return 0.25 * y * std::exp(-t * 9.0);
}
double burst(double t) { return 0.7 * noise() * std::exp(-t * 25.0); }
double drone(double t)
{
    double y = 0.0;
    for (double f : {55.0, 82.41, 110.3})
        y += 2.0 * (f * t - std::floor(f * t + 0.5));
    return 0.25 * y;
}

void play(klang::Engine &engine, krate::MasterStage &master, double seconds, int rate, std::vector<float> &out)
{
    krate::Transport transport;
    transport.setBpm(120.0);
    transport.setPlaying(true);
    std::vector<float> l(kBlock), r(kBlock);
    const double step = 0.125; // 1/16 at 120 BPM
    // Kick on the beats, clank on the off-beats, bursts on two, the drone held.
    std::vector<std::pair<double, int>> hits;
    for (int s = 0; s < int(seconds / step); ++s) {
        if (s % 4 == 0)
            hits.push_back({s * step, 0});
        if (s % 4 == 2)
            hits.push_back({s * step, 1});
        if (s % 8 == 4 || s % 16 == 15)
            hits.push_back({s * step, 2});
    }
    hits.push_back({0.0, 3});
    std::sort(hits.begin(), hits.end());
    std::size_t next = 0;
    for (std::uint64_t f = 0; f < std::uint64_t(seconds * rate); f += kBlock) {
        std::vector<krate::MidiEvent> midi;
        while (next < hits.size() && std::uint64_t(hits[next].first * rate) < f + kBlock) {
            midi.push_back({std::uint32_t(std::uint64_t(hits[next].first * rate) - f), 3,
                            {0x90, std::uint8_t(36 + hits[next].second), 110}});
            ++next;
        }
        const krate::TransportState state = transport.advance(kBlock, rate);
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()), &state);
        float *const outputs[2] = {l.data(), r.data()};
        master.process(outputs, 2, kBlock);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            out.push_back(l[i]);
            out.push_back(r[i]);
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::string outDir = ".";
    int rate = 48000;
    bool bench = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bench")
            bench = true;
        else if (arg == "--out-dir" && i + 1 < argc)
            outDir = argv[++i];
        else if (arg == "--rate" && i + 1 < argc)
            rate = std::atoi(argv[++i]);
        else {
            std::fprintf(stderr, "usage: klang-render [--out-dir DIR] [--rate HZ] | --bench [--rate HZ]\n");
            return 2;
        }
    }
    const Sample sounds[] = {make(0.6, 44100.0, kick, "kick"), make(0.5, 44100.0, clank, "clank"),
                             make(0.3, 44100.0, burst, "burst"), make(2.0, 44100.0, drone, "drone")};
    krate::MasterStage master;
    klang::Engine engine;
    engine.prepare(rate, kBlock);
    for (int pad = 0; pad < 4; ++pad)
        engine.setSample(pad, &sounds[pad]);
    namespace p = klang::positions;
    engine.setPadParam(1, ParamId::Ring, 0.6f);         // clank: ring-modulated, crushed, echoed
    engine.setPadParam(1, ParamId::Bits, klang::position(9, p::kBits));
    engine.setPadParam(1, ParamId::DelaySend, 0.5f);
    engine.setPadParam(2, ParamId::Filter, klang::position(2, p::kFilter)); // burst: high-passed, reverb
    engine.setPadParam(2, ParamId::Cutoff, 0.6f);
    engine.setPadParam(2, ParamId::ReverbSend, 0.6f);
    engine.setPadParam(3, ParamId::Mode, klang::position(2, p::kMode));     // drone: looped, gated, filtered
    engine.setPadParam(3, ParamId::Sync, klang::position(3, p::kSync));     // 4 beats
    engine.setPadParam(3, ParamId::Gate, 1.0f);
    engine.setPadParam(3, ParamId::Filter, klang::position(1, p::kFilter));
    engine.setPadParam(3, ParamId::Cutoff, 0.5f);
    engine.setPadParam(3, ParamId::Res, 0.6f);
    engine.setPadParam(3, ParamId::Drive, 0.4f);
    engine.setPadParam(3, ParamId::PadLevel, 0.6f);

    if (bench) {
        // The heaviest case: all 16 pads looping the drone with every effect, 60 s.
        for (int pad = 0; pad < klang::Engine::kPads; ++pad) {
            engine.setSample(pad, &sounds[3]);
            for (ParamId id : {ParamId::Mode, ParamId::Filter, ParamId::Gate})
                engine.setPadParam(pad, id, 1.0f);
            engine.setPadParam(pad, ParamId::Drive, 0.8f);
            engine.setPadParam(pad, ParamId::Bits, 0.5f);
            engine.setPadParam(pad, ParamId::Downsample, 0.4f);
            engine.setPadParam(pad, ParamId::Ring, 0.5f);
            engine.setPadParam(pad, ParamId::DelaySend, 0.3f);
            engine.setPadParam(pad, ParamId::ReverbSend, 0.3f);
        }
        krate::processStereo(engine, nullptr, nullptr, 0); // take the samples
        for (int pad = 0; pad < klang::Engine::kPads; ++pad)
            engine.hit(pad, 1.0f);
        std::vector<float> l(kBlock), r(kBlock);
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t f = 0; f < std::uint64_t(60.0 * rate); f += kBlock)
            krate::processStereo(engine, l.data(), r.data(), kBlock);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of 16 looping pads with every effect at %d Hz in %.3f s: %.2f%% of one core\n", rate,
                    seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    std::vector<float> out;
    play(engine, master, 8.0, rate, out);
    float peak = 0.0f;
    for (float s : out)
        peak = std::max(peak, std::fabs(s));
    const std::string path = outDir + "/klang-sounds.wav";
    if (!krate::writeStereoWav(path, out, rate)) {
        std::fprintf(stderr, "klang-render: could not write %s\n", path.c_str());
        return 1;
    }
    std::printf("wrote %s: %.1f s, peak %.1f dB\n", path.c_str(), double(out.size() / 2) / rate,
                20.0 * std::log10(std::max(peak, 1e-9f)));
    return 0;
}
