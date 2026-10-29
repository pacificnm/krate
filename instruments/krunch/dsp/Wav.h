#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace krunch {

// A decoded WAV file: its channels (up to two kept), at its own rate.
struct WavData
{
    int rate = 0;
    int channels = 0; // in the file
    std::vector<float> left;
    std::vector<float> right; // empty for mono files
};

// PCM 16 / 24 / 32-bit or 32-bit float, any channel count (the first two kept).
// false (with *error) if it isn't one we read. Not realtime.
bool readWav(std::span<const std::uint8_t> bytes, WavData *out, std::string *error = nullptr);
bool readWavFile(const std::string &path, WavData *out, std::string *error = nullptr);

} // namespace krunch
