// klang-render: render Klang offline to WAV, or benchmark it.
//
//   klang-render [--out-dir DIR] [--rate HZ]
//   klang-render --bench [--rate HZ]
//
// Writes DIR/klang-sounds.wav: the starter kit (sounds Klang makes itself, no
// sample files) playing patterns A and B, two bars each, on the transport at
// 120 BPM, through the pads' gate, crusher, ring modulator, filter, delay and
// reverb, as the app's first run sets them up.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/MasterStage.h"
#include "core/Transport.h"
#include "klang/dsp/Engine.h"
#include "klang/dsp/StarterKit.h"
#include "tools/WavFile.h"

namespace {
constexpr std::uint32_t kBlock = 64;
}

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
    const std::vector<klang::StarterSound> kit = klang::starterSounds();
    klang::Engine engine;
    engine.prepare(rate, kBlock);
    for (int pad = 0; pad < klang::Engine::kPads; ++pad) {
        engine.setSample(pad, &kit[std::size_t(pad)].sample);
        klang::applyStarterPad(engine, pad);
    }
    for (int i = 0; i < klang::kPatterns; ++i)
        engine.setPattern(i, klang::starterPattern(i));
    std::vector<float> l(kBlock), r(kBlock);

    if (bench) {
        // The heaviest case: all 16 pads looping the drone through every effect, 60 s.
        using klang::ParamId;
        for (int pad = 0; pad < klang::Engine::kPads; ++pad) {
            engine.setSample(pad, &kit[9].sample);
            for (ParamId id : {ParamId::Mode, ParamId::Filter, ParamId::Gate})
                engine.setPadParam(pad, id, 1.0f);
            engine.setPadParam(pad, ParamId::Drive, 0.8f);
            engine.setPadParam(pad, ParamId::Bits, 0.5f);
            engine.setPadParam(pad, ParamId::Downsample, 0.4f);
            engine.setPadParam(pad, ParamId::Ring, 0.5f);
            engine.setPadParam(pad, ParamId::DelaySend, 0.3f);
            engine.setPadParam(pad, ParamId::ReverbSend, 0.3f);
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock); // take the samples and settings
        for (int pad = 0; pad < klang::Engine::kPads; ++pad)
            engine.hit(pad, 1.0f);
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t f = 0; f < std::uint64_t(60.0 * rate); f += kBlock)
            krate::processStereo(engine, l.data(), r.data(), kBlock);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s of 16 looping pads with every effect at %d Hz in %.3f s: %.2f%% of one core\n", rate,
                    seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    krate::MasterStage master;
    krate::Transport transport;
    transport.setBpm(120.0);
    transport.setPlaying(true);
    std::vector<float> out;
    const double seconds = 8.0 + 1.5; // A for two bars, B for two, the tails
    for (std::uint64_t f = 0; f < std::uint64_t(seconds * rate); f += kBlock) {
        if (f >= std::uint64_t(2.0 * rate) && engine.cuedPattern() == 0)
            engine.cuePattern(1); // B from bar 3
        if (f >= std::uint64_t(8.0 * rate))
            transport.setPlaying(false);
        const krate::TransportState state = transport.advance(kBlock, rate);
        krate::processStereo(engine, l.data(), r.data(), kBlock, nullptr, 0, &state);
        float *const outputs[2] = {l.data(), r.data()};
        master.process(outputs, 2, kBlock);
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            out.push_back(l[i]);
            out.push_back(r[i]);
        }
    }
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
