// konnekt-render: render a built-in Konnekt patch offline to WAV, or benchmark it.
//
//   konnekt-render [--out-dir DIR] [--rate HZ]
//   konnekt-render --bench [--rate HZ]
//
// Writes DIR/konnekt-patch.wav: two detuned oscillators, an LFO on their pulse
// width, mixed, through a VCA opened by an envelope, played as a chord
// progression on 4 voices. --bench: the same patch with 8 voices held for 60 s.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/MasterStage.h"
#include "konnekt/dsp/PatchSpec.h"
#include "tools/WavFile.h"

namespace {

constexpr std::uint32_t kBlock = 64;

konnekt::PatchSpec demo(int voices)
{
    konnekt::PatchSpec p;
    p.voices = voices;
    p.modules = {
        {0, "midi_in"},
        {1, "oscillator", {{"pwm_amount", 0.3f}}},
        {2, "oscillator", {{"tune", 0.08f}, {"octave", -1.0f}}},
        {3, "lfo", {{"rate", 0.6f}}},
        {4, "mixer"},
        {5, "envelope", {{"attack", 0.02f}, {"decay", 0.4f}, {"sustain", 0.6f}, {"release", 0.5f}}},
        {6, "vca"},
        {7, "output", {{"volume", 0.6f}}},
    };
    p.cables = {
        {0, "pitch", 1, "pitch"}, {0, "pitch", 2, "pitch"}, {3, "out", 1, "pwm"}, {1, "pulse", 4, "in1"},
        {2, "saw", 4, "in2"},     {4, "out", 6, "in"},      {0, "gate", 5, "gate"}, {5, "env", 6, "cv"},
        {6, "out", 7, "left"},
    };
    return p;
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
            std::fprintf(stderr, "usage: konnekt-render [--out-dir DIR] [--rate HZ] | --bench [--rate HZ]\n");
            return 2;
        }
    }
    konnekt::Engine engine;
    engine.prepare(rate, kBlock);
    auto modules = konnekt::build(engine, demo(bench ? 8 : 4));
    std::vector<float> l(kBlock), r(kBlock), out;
    const auto play = [&](double seconds, std::vector<krate::MidiEvent> midi, bool keep) {
        for (std::uint64_t f = 0; f < std::uint64_t(seconds * rate); f += kBlock) {
            krate::processStereo(engine, l.data(), r.data(), kBlock, f == 0 ? midi.data() : nullptr,
                                 f == 0 ? std::uint32_t(midi.size()) : 0);
            if (keep)
                for (std::uint32_t i = 0; i < kBlock; ++i) {
                    out.push_back(l[i]);
                    out.push_back(r[i]);
                }
        }
    };
    const auto chord = [](std::initializer_list<int> notes, bool on) {
        std::vector<krate::MidiEvent> e;
        for (int n : notes)
            e.push_back({0, 3, {std::uint8_t(on ? 0x90 : 0x80), std::uint8_t(n), 100}});
        return e;
    };

    if (bench) {
        play(0.01, chord({36, 43, 48, 52, 55, 60, 64, 67}, true), false);
        const auto t0 = std::chrono::steady_clock::now();
        play(60.0, {}, false);
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of 8 voices (2 oscillators, LFO, mixer, envelope, VCA each) at %d Hz in %.3f s: %.2f%% of one core\n",
                    rate, s, s / 60.0 * 100.0);
        return 0;
    }
    for (const auto &c : {std::initializer_list<int>{57, 60, 64}, {55, 59, 62}, {53, 57, 60, 64}, {52, 56, 59}}) {
        play(1.2, chord(c, true), true);
        play(0.5, chord(c, false), true);
    }
    krate::MasterStage master;
    for (std::size_t i = 0; i + 1 < out.size(); i += 2) {
        float a[1] = {out[i]}, b[1] = {out[i + 1]};
        float *const io[2] = {a, b};
        master.process(io, 2, 1);
        out[i] = a[0];
        out[i + 1] = b[0];
    }
    const std::string path = outDir + "/konnekt-patch.wav";
    if (!krate::writeStereoWav(path, out, rate)) {
        std::fprintf(stderr, "konnekt-render: could not write %s\n", path.c_str());
        return 1;
    }
    float peak = 0.0f;
    for (float s : out)
        peak = std::max(peak, std::fabs(s));
    std::printf("wrote %s: %.1f s, peak %.1f dB\n", path.c_str(), double(out.size() / 2) / rate, 20.0 * std::log10(peak));
    return 0;
}
