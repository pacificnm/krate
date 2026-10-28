#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace krate {

// Writes interleaved stereo float samples as a 16-bit PCM WAV file (clipped to
// +-1). For offline renderers and tests; not realtime.
inline bool writeStereoWav(const std::string &path, const std::vector<float> &interleaved, int rate)
{
    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const auto writeLe = [f](std::uint32_t v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            std::fputc(int((v >> (8 * i)) & 0xff), f);
    };
    const std::uint32_t dataBytes = std::uint32_t(interleaved.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    writeLe(36 + dataBytes, 4);
    std::fwrite("WAVEfmt ", 1, 8, f);
    writeLe(16, 4);                          // fmt chunk size
    writeLe(1, 2);                           // PCM
    writeLe(2, 2);                           // channels
    writeLe(std::uint32_t(rate), 4);
    writeLe(std::uint32_t(rate) * 4, 4);     // byte rate
    writeLe(4, 2);                           // block align
    writeLe(16, 2);                          // bits per sample
    std::fwrite("data", 1, 4, f);
    writeLe(dataBytes, 4);
    for (float s : interleaved) {
        const float c = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
        writeLe(std::uint32_t(std::uint16_t(std::int16_t(c * 32767.0f))), 2);
    }
    return std::fclose(f) == 0;
}

} // namespace krate
