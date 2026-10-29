// krunch-render: play a guitar DI through Krunch offline, one WAV per channel.
// The DI is a WAV file (--in; its first channel) or a synthetic riff.
//
//   krunch-render [--in di.wav] [--out prefix] [--channel 1..4] [--set key=value]... [--bench]
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SyntheticGuitar.h"
#include "dsp/Krunch.h"
#include "tools/WavFile.h"

namespace {

// Reads a PCM 16 / 24 / 32-bit or float WAV's first channel. Empty on failure.
std::vector<float> readWav(const char *path, int *rate)
{
    std::vector<float> samples;
    std::FILE *f = std::fopen(path, "rb");
    if (!f)
        return samples;
    std::vector<unsigned char> data;
    unsigned char chunk[4096];
    for (std::size_t n; (n = std::fread(chunk, 1, sizeof chunk, f)) > 0;)
        data.insert(data.end(), chunk, chunk + n);
    std::fclose(f);
    const auto u32 = [&data](std::size_t at) {
        return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8 | std::uint32_t(data[at + 2]) << 16
               | std::uint32_t(data[at + 3]) << 24;
    };
    const auto u16 = [&data](std::size_t at) { return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8; };
    if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
        return samples;
    std::uint32_t format = 0, channels = 0, bits = 0;
    for (std::size_t at = 12; at + 8 <= data.size();) {
        const std::uint32_t size = u32(at + 4);
        const std::size_t body = at + 8;
        if (std::memcmp(data.data() + at, "fmt ", 4) == 0 && body + 16 <= data.size()) {
            format = u16(body);
            channels = u16(body + 2);
            *rate = int(u32(body + 4));
            bits = u16(body + 14);
            if (format == 0xFFFE && body + 26 <= data.size()) // extensible: the sub-format's first two bytes
                format = u16(body + 24);
        } else if (std::memcmp(data.data() + at, "data", 4) == 0 && channels > 0) {
            const std::size_t frameBytes = channels * bits / 8;
            const std::size_t end = std::min(data.size(), body + size);
            for (std::size_t p = body; p + frameBytes <= end; p += frameBytes) {
                float s = 0.0f;
                if (format == 3 && bits == 32) {
                    const std::uint32_t v = u32(p);
                    std::memcpy(&s, &v, 4);
                } else if (format == 1 && bits == 16) {
                    s = float(std::int16_t(u16(p))) / 32768.0f;
                } else if (format == 1 && bits == 24) {
                    s = float(std::int32_t((u16(p) << 8 | std::uint32_t(data[p + 2]) << 24)) >> 8) / 8388608.0f;
                } else if (format == 1 && bits == 32) {
                    s = float(std::int32_t(u32(p))) / 2147483648.0f;
                } else {
                    return {};
                }
                samples.push_back(s);
            }
            break;
        }
        at = body + size + (size & 1);
    }
    return samples;
}

int usage()
{
    std::fprintf(stderr, "usage: krunch-render [--in di.wav] [--out prefix] [--channel 1..4] [--set key=value]... [--bench]\n");
    return 2;
}

} // namespace

int main(int argc, char **argv)
{
    std::string in, out = "krunch";
    int only = 0;
    bool bench = false;
    std::vector<std::pair<std::string, float>> settings;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--in" && i + 1 < argc)
            in = argv[++i];
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
        di = readWav(in.c_str(), &rate);
        if (di.empty()) {
            std::fprintf(stderr, "could not read %s (PCM 16/24/32-bit or float WAV)\n", in.c_str());
            return 1;
        }
    } else {
        di = krunch::tools::SyntheticGuitar(rate).riff();
    }
    float diPeak = 0.0f;
    for (float s : di)
        diPeak = std::max(diPeak, std::fabs(s));
    std::printf("DI: %.1f s, peak %.1f dBFS\n", double(di.size()) / rate, 20.0 * std::log10(std::max(diPeak, 1e-9f)));

    for (int channel = 1; channel <= krunch::kChannels; ++channel) {
        if (only && channel != only)
            continue;
        krunch::Krunch amp;
        amp.prepare(rate, krunch::Krunch::kBlock);
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
