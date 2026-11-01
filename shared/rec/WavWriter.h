#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace krate {

// A WAV file written as it's recorded: 24-bit PCM, mono or stereo. The header
// is written first with placeholder sizes and corrected by close(), so a file
// cut short (a crash) still has its audio, readable once repaired. Not
// realtime: the disk thread's (docs/DAW.md, D2).
class WavWriter
{
public:
    WavWriter() = default;
    ~WavWriter() { close(); }
    WavWriter(const WavWriter &) = delete;
    WavWriter &operator=(const WavWriter &) = delete;

    bool open(const std::string &path, int sampleRate, int channels);
    bool isOpen() const { return m_file != nullptr; }
    // `right` is ignored for mono. Samples are clipped to -1..1.
    bool write(const float *left, const float *right, std::size_t frames);
    bool close(); // writes the sizes; false if the file couldn't be finished
    std::uint64_t frames() const { return m_frames; }

private:
    std::FILE *m_file = nullptr;
    int m_rate = 48000;
    int m_channels = 2;
    std::uint64_t m_frames = 0;
    bool m_ok = true;
};

} // namespace krate
