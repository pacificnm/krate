// krunch-render: play a guitar DI through Krunch offline, one WAV per channel.
// The DI is a WAV file (--in; its first channel) or a synthetic riff; --ir and
// --ir-b load cabinet IRs (A and B) into every channel.
//
//   krunch-render [--in di.wav] [--ir a.wav] [--ir-b b.wav] [--out prefix] [--channel 1..4]
//                 [--set key=value]... [--bench]
//
// Writes <prefix>-ch<N>.wav (stereo, 16-bit) for each channel rendered, and
// prints each one's peak and RMS; --bench times the amp instead.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SyntheticGuitar.h"
#include "dsp/Krunch.h"
#include "dsp/Wav.h"
#include "tools/WavFile.h"

namespace {

int usage()
{
    std::fprintf(stderr, "usage: krunch-render [--in di.wav] [--ir a.wav] [--ir-b b.wav] [--out prefix] [--channel 1..4]\n"
                         "                     [--set key=value]... [--bench]\n");
    return 2;
}

} // namespace

int main(int argc, char **argv)
{
    std::string in, out = "krunch", irA, irB;
    int only = 0;
    bool bench = false;
    std::vector<std::pair<std::string, float>> settings;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--in" && i + 1 < argc)
            in = argv[++i];
        else if (a == "--ir" && i + 1 < argc)
            irA = argv[++i];
        else if (a == "--ir-b" && i + 1 < argc)
            irB = argv[++i];
        else if (a == "--out" && i + 1 < argc)
            out = argv[++i];
        else if (a == "--channel" && i + 1 < argc)
            only = std::atoi(argv[++i]);
        else if (a == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const auto eq = kv.find('=');
            if (eq == std::string::npos)
                return usage();
            settings.emplace_back(kv.substr(0, eq), std::strtof(kv.c_str() + eq + 1, nullptr));
        } else if (a == "--bench")
            bench = true;
        else
            return usage();
    }

    int rate = 48000;
    std::vector<float> di;
    if (!in.empty()) {
        krate::WavData wav;
        std::string error;
        if (!krate::readWavFile(in, &wav, &error)) {
            std::fprintf(stderr, "could not read %s: %s\n", in.c_str(), error.c_str());
            return 1;
        }
        rate = wav.rate;
        di = std::move(wav.left);
    } else {
        di = krunch::tools::SyntheticGuitar(rate).riff();
    }
    float diPeak = 0.0f;
    for (float s : di)
        diPeak = std::max(diPeak, std::fabs(s));
    std::printf("DI: %.1f s, peak %.1f dBFS\n", double(di.size()) / rate, 20.0 * std::log10(std::max(diPeak, 1e-9f)));

    std::unique_ptr<krunch::IrKernel> kernels[2];
    for (int k = 0; k < 2; ++k) {
        const std::string &path = k == 0 ? irA : irB;
        if (path.empty())
            continue;
        krate::WavData wav;
        std::string error;
        if (!krate::readWavFile(path, &wav, &error) || !(kernels[k] = krunch::IrKernel::build(wav, rate, &error))) {
            std::fprintf(stderr, "could not load the IR %s: %s\n", path.c_str(), error.c_str());
            return 1;
        }
        std::printf("IR %c: %d Hz, %d channel(s), %zu taps here\n", 'A' + k, wav.rate, wav.channels, kernels[k]->taps());
    }

    for (int channel = 1; channel <= krunch::kChannels; ++channel) {
        if (only && channel != only)
            continue;
        krunch::Krunch amp;
        amp.prepare(rate, krunch::Krunch::kBlock);
        for (int c = 0; c < krunch::kChannels; ++c)
            for (int k = 0; k < 2; ++k)
                amp.cabinet().setKernel(krunch::Krunch::cabinetSlot(c, k == 1), kernels[k].get());
        amp.setParam(int(krunch::ParamId::Channel), float(channel - 1) / float(krunch::kChannels - 1));
        for (const auto &[key, value] : settings)
            for (std::size_t p = 0; p < krunch::kParams.size(); ++p)
                if (krunch::kParams[p].key == key)
                    amp.setParam(int(p), value);

        constexpr std::uint32_t kChunk = 128; // as at 128 frames per block
        std::vector<float> left(kChunk), right(kChunk), wav;
        wav.reserve(di.size() * 2);
        double sum = 0.0;
        float peak = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t at = 0; at < di.size(); at += kChunk) {
            const auto frames = std::uint32_t(std::min<std::size_t>(kChunk, di.size() - at));
            float *const outputs[2] = {left.data(), right.data()};
            const float *const inputs[2] = {di.data() + at, di.data() + at};
            krate::AudioBlock block{outputs, frames};
            block.inputs = inputs;
            amp.process(block);
            for (std::uint32_t n = 0; n < frames; ++n) {
                wav.push_back(left[n]);
                wav.push_back(right[n]);
                peak = std::max(peak, std::fabs(right[n]));
                sum += double(left[n]) * left[n];
                peak = std::max(peak, std::fabs(left[n]));
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double rms = std::sqrt(sum / double(di.size()));
        std::printf("channel %d (%s): peak %.1f dBFS, RMS %.1f dBFS", channel,
                    krunch::format::voice(amp.param(krunch::channelParam(channel - 1, krunch::ChannelParam::Voice))).c_str(),
                    20.0 * std::log10(std::max(double(peak), 1e-9)), 20.0 * std::log10(std::max(rms, 1e-9)));
        if (bench)
            std::printf(", %.2f %% of real time", 100.0 * seconds / (double(di.size()) / rate));
        std::printf("\n");
        if (!bench) {
            const std::string path = out + "-ch" + std::to_string(channel) + ".wav";
            if (!krate::writeStereoWav(path, wav, rate)) {
                std::fprintf(stderr, "could not write %s\n", path.c_str());
                return 1;
            }
        }
    }
    return 0;
}
