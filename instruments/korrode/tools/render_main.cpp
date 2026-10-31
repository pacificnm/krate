// korrode-render: render the engine offline to a WAV file, or benchmark it.
//
//   korrode-render [--out file.wav] [--bars N] [--rate HZ]
//               [--tempo BPM] [--<param> 0..1 ...] [--square]
//   korrode-render --bench [--rate HZ] [--<param> 0..1 ...]
//
// <param> is any key from docs/DSP.md#parameters (cutoff, resonance, envmod,
// decay, accent, sweep, drive, tone, delay_level, reverb_level, ...). Plays the
// sequencer's current pattern; --bench with parameters measures that setting.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/MasterStage.h"
#include "core/Transport.h"
#include "tools/WavFile.h"
#include "dsp/Engine.h"
#include "dsp/Params.h"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: korrode-render [--out file.wav] [--bars N] [--rate HZ] [--tempo BPM]\n"
                 "                   [--<param> 0..1 ...]  (any key: cutoff, resonance, drive, delay_level, ...)\n"
                 "                   [--square]\n"
                 "       korrode-render --bench [--rate HZ] [--<param> 0..1 ...]\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::string out = "korrode.wav";
    int bars = 4;
    int rate = 48000;
    bool bench = false;
    korrode::Engine engine;
    auto &params = engine.params();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--bench") {
            bench = true;
        } else if (arg == "--square") {
            params.set(korrode::ParamId::Waveform, 1.0f);
        } else if (arg == "--out" && hasValue) {
            out = argv[++i];
        } else if (arg == "--bars" && hasValue) {
            bars = std::atoi(argv[++i]);
        } else if (arg == "--rate" && hasValue) {
            rate = std::atoi(argv[++i]);
        } else if (arg == "--tempo" && hasValue) {
            const double bpm = std::atof(argv[++i]);
            params.set(korrode::ParamId::Tempo,
                       float((bpm - korrode::units::kTempoMinBpm) / (korrode::units::kTempoMaxBpm - korrode::units::kTempoMinBpm)));
        } else if (arg.rfind("--", 0) == 0 && hasValue && korrode::paramFromKey(arg.substr(2))) {
            params.set(*korrode::paramFromKey(arg.substr(2)), float(std::atof(argv[++i])));
        } else {
            usage();
            return 2;
        }
    }
    if (bars < 1 || rate < 8000) {
        usage();
        return 2;
    }

    constexpr std::uint32_t kBlock = 64;
    krate::IInstrument &instrument = engine; // everything below goes through the interface
    instrument.prepare(rate, kBlock);
    krate::Transport transport;
    krate::MasterStage master;
    transport.setBpm(korrode::units::tempoBpm(params.get(korrode::ParamId::Tempo)));
    transport.setPlaying(true);
    const auto render = [&](float *left, float *right) {
        const krate::TransportState state = transport.advance(kBlock, rate);
        krate::processStereo(instrument, left, right, kBlock, nullptr, 0, &state);
        float *const outputs[2] = {left, right};
        master.process(outputs, 2, kBlock); // the same master stage as the JACK host
    };
    std::vector<float> l(kBlock), r(kBlock);

    if (bench) {
        const std::uint64_t frames = std::uint64_t(rate) * 60; // one minute of audio
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t done = 0; done < frames; done += kBlock)
            render(l.data(), r.data());
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("rendered 60 s at %d Hz in %.3f s: %.2f%% of one core\n", rate, seconds, seconds / 60.0 * 100.0);
        return 0;
    }

    const double secondsPerStep = 60.0 / (korrode::units::tempoBpm(params.get(korrode::ParamId::Tempo)) * 4.0);
    const std::uint64_t frames = std::uint64_t((bars * 16 * secondsPerStep + 0.5) * rate);
    std::vector<float> interleaved;
    interleaved.reserve(frames * 2 + 2 * kBlock);
    for (std::uint64_t done = 0; done < frames; done += kBlock) {
        if (done + kBlock >= frames - std::uint64_t(rate / 2))
            transport.setPlaying(false); // last half second: let the release ring out
        render(l.data(), r.data());
        for (std::uint32_t i = 0; i < kBlock; ++i) {
            interleaved.push_back(l[i]);
            interleaved.push_back(r[i]);
        }
    }

    if (!krate::writeStereoWav(out, interleaved, rate)) {
        std::fprintf(stderr, "korrode-render: could not write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s: %d bars, %.1f s\n", out.c_str(), bars, double(frames) / rate);
    return 0;
}
