// k303-render: render the engine offline to a WAV file, or benchmark it.
//
//   k303-render [--out file.wav] [--bars N] [--rate HZ]
//               [--tempo BPM] [--<param> 0..1 ...] [--square]
//   k303-render --bench [--rate HZ]
//
// <param> is any key from docs/DSP.md#parameters (cutoff, resonance, envmod,
// decay, accent, tuning, volume). Plays the sequencer's current pattern.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/StandaloneTransport.h"
#include "dsp/Engine.h"
#include "dsp/Params.h"

namespace {

void writeLe(std::FILE *f, std::uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        std::fputc(int((v >> (8 * i)) & 0xff), f);
}

bool writeWav(const std::string &path, const std::vector<float> &interleaved, int rate)
{
    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const std::uint32_t dataBytes = std::uint32_t(interleaved.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    writeLe(f, 36 + dataBytes, 4);
    std::fwrite("WAVEfmt ", 1, 8, f);
    writeLe(f, 16, 4);            // fmt chunk size
    writeLe(f, 1, 2);             // PCM
    writeLe(f, 2, 2);             // channels
    writeLe(f, std::uint32_t(rate), 4);
    writeLe(f, std::uint32_t(rate) * 4, 4); // byte rate
    writeLe(f, 4, 2);             // block align
    writeLe(f, 16, 2);            // bits per sample
    std::fwrite("data", 1, 4, f);
    writeLe(f, dataBytes, 4);
    for (float s : interleaved) {
        const float c = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
        writeLe(f, std::uint32_t(std::uint16_t(std::int16_t(c * 32767.0f))), 2);
    }
    return std::fclose(f) == 0;
}

void usage()
{
    std::fprintf(stderr,
                 "usage: k303-render [--out file.wav] [--bars N] [--rate HZ] [--tempo BPM]\n"
                 "                   [--cutoff|--resonance|--envmod|--decay|--accent|--tuning|--volume 0..1]\n"
                 "                   [--square]\n"
                 "       k303-render --bench [--rate HZ]\n");
}

} // namespace

int main(int argc, char **argv)
{
    std::string out = "k303.wav";
    int bars = 4;
    int rate = 48000;
    bool bench = false;
    k303::Engine engine;
    auto &params = engine.params();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--bench") {
            bench = true;
        } else if (arg == "--square") {
            params.set(k303::ParamId::Waveform, 1.0f);
        } else if (arg == "--out" && hasValue) {
            out = argv[++i];
        } else if (arg == "--bars" && hasValue) {
            bars = std::atoi(argv[++i]);
        } else if (arg == "--rate" && hasValue) {
            rate = std::atoi(argv[++i]);
        } else if (arg == "--tempo" && hasValue) {
            const double bpm = std::atof(argv[++i]);
            params.set(k303::ParamId::Tempo,
                       float((bpm - k303::units::kTempoMinBpm) / (k303::units::kTempoMaxBpm - k303::units::kTempoMinBpm)));
        } else if (arg.rfind("--", 0) == 0 && hasValue && k303::paramFromKey(arg.substr(2))) {
            params.set(*k303::paramFromKey(arg.substr(2)), float(std::atof(argv[++i])));
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
    krate::StandaloneTransport transport;
    transport.setBpm(k303::units::tempoBpm(params.get(k303::ParamId::Tempo)));
    transport.setPlaying(true);
    const auto render = [&](float *left, float *right) {
        const krate::TransportState state = transport.advance(kBlock, rate);
        krate::processStereo(instrument, left, right, kBlock, nullptr, 0, &state);
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

    const double secondsPerStep = 60.0 / (k303::units::tempoBpm(params.get(k303::ParamId::Tempo)) * 4.0);
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

    if (!writeWav(out, interleaved, rate)) {
        std::fprintf(stderr, "k303-render: could not write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s: %d bars, %.1f s\n", out.c_str(), bars, double(frames) / rate);
    return 0;
}
