#include "dsp/Wav.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace krate {

namespace {
bool fail(std::string *error, const char *why)
{
    if (error)
        *error = why;
    return false;
}
} // namespace

bool readWav(std::span<const std::uint8_t> data, WavData *out, std::string *error)
{
    const auto u32 = [&data](std::size_t at) {
        return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8 | std::uint32_t(data[at + 2]) << 16
               | std::uint32_t(data[at + 3]) << 24;
    };
    const auto u16 = [&data](std::size_t at) { return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8; };
    if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
        return fail(error, "not a WAV file");

    std::uint32_t format = 0, channels = 0, bits = 0, rate = 0;
    for (std::size_t at = 12; at + 8 <= data.size();) {
        const std::uint32_t size = u32(at + 4);
        const std::size_t body = at + 8;
        if (std::memcmp(data.data() + at, "fmt ", 4) == 0) {
            if (body + 16 > data.size())
                return fail(error, "damaged WAV header");
            format = u16(body);
            channels = u16(body + 2);
            rate = u32(body + 4);
            bits = u16(body + 14);
            if (format == 0xFFFE && size >= 26 && body + 26 <= data.size()) // extensible: the sub-format
                format = u16(body + 24);
        } else if (std::memcmp(data.data() + at, "data", 4) == 0) {
            if (channels == 0 || rate == 0)
                return fail(error, "WAV data before its format");
            const bool pcm = format == 1 && (bits == 16 || bits == 24 || bits == 32);
            const bool flt = format == 3 && bits == 32;
            if (!pcm && !flt)
                return fail(error, "unsupported WAV encoding (PCM 16/24/32-bit or 32-bit float)");
            const std::size_t bytes = bits / 8, frameBytes = channels * bytes;
            const std::size_t end = std::min<std::size_t>(data.size(), body + std::size_t(size));
            const auto sample = [&](std::size_t p) -> float {
                if (flt) {
                    const std::uint32_t v = u32(p);
                    float f;
                    std::memcpy(&f, &v, 4);
                    return f;
                }
                if (bits == 16)
                    return float(std::int16_t(u16(p))) / 32768.0f;
                if (bits == 24)
                    return float(std::int32_t(u16(p) << 8 | std::uint32_t(data[p + 2]) << 24) >> 8) / 8388608.0f;
                return float(std::int32_t(u32(p))) / 2147483648.0f;
            };
            out->rate = int(rate);
            out->channels = int(channels);
            out->left.clear();
            out->right.clear();
            for (std::size_t p = body; p + frameBytes <= end; p += frameBytes) {
                out->left.push_back(sample(p));
                if (channels > 1)
                    out->right.push_back(sample(p + bytes));
            }
            if (out->left.empty())
                return fail(error, "empty WAV file");
            return true;
        }
        at = body + std::size_t(size) + (size & 1u);
    }
    return fail(error, "no audio in the WAV file");
}

bool readWavFile(const std::string &path, WavData *out, std::string *error)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return fail(error, "could not open the file");
    std::vector<std::uint8_t> bytes;
    std::uint8_t chunk[65536];
    for (std::size_t n; (n = std::fread(chunk, 1, sizeof chunk, f)) > 0;)
        bytes.insert(bytes.end(), chunk, chunk + n);
    std::fclose(f);
    return readWav(bytes, out, error);
}

} // namespace krate
