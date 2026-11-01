#include "rec/WavWriter.h"

#include <algorithm>
#include <cmath>

namespace krate {

namespace {
constexpr int kBytesPerSample = 3; // 24-bit

void put32(std::uint8_t *p, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = std::uint8_t(v >> (8 * i));
}
void put16(std::uint8_t *p, std::uint16_t v)
{
    p[0] = std::uint8_t(v);
    p[1] = std::uint8_t(v >> 8);
}

// The 44-byte header for `frames` frames.
void header(std::uint8_t *h, int rate, int channels, std::uint64_t frames)
{
    const std::uint32_t data = std::uint32_t(std::min<std::uint64_t>(frames * std::uint64_t(channels) * kBytesPerSample, 0xFFFFFFFFu - 36));
    std::copy_n("RIFF", 4, h);
    put32(h + 4, 36 + data);
    std::copy_n("WAVEfmt ", 8, h + 8);
    put32(h + 16, 16);
    put16(h + 20, 1); // PCM
    put16(h + 22, std::uint16_t(channels));
    put32(h + 24, std::uint32_t(rate));
    put32(h + 28, std::uint32_t(rate * channels * kBytesPerSample));
    put16(h + 32, std::uint16_t(channels * kBytesPerSample));
    put16(h + 34, 8 * kBytesPerSample);
    std::copy_n("data", 4, h + 36);
    put32(h + 40, data);
}
} // namespace

bool WavWriter::open(const std::string &path, int sampleRate, int channels)
{
    close();
    m_channels = std::clamp(channels, 1, 2);
    m_frames = 0;
    m_ok = true;
    m_file = std::fopen(path.c_str(), "wb");
    if (!m_file)
        return false;
    std::uint8_t h[44];
    header(h, sampleRate, m_channels, 0);
    m_rate = sampleRate;
    if (std::fwrite(h, 1, sizeof h, m_file) != sizeof h) {
        std::fclose(m_file);
        m_file = nullptr;
        return false;
    }
    return true;
}

bool WavWriter::write(const float *left, const float *right, std::size_t frames)
{
    if (!m_file)
        return false;
    std::uint8_t buffer[4096 * 2 * kBytesPerSample];
    const std::size_t chunk = sizeof buffer / std::size_t(m_channels * kBytesPerSample);
    for (std::size_t done = 0; done < frames;) {
        const std::size_t n = std::min(chunk, frames - done);
        std::uint8_t *p = buffer;
        for (std::size_t i = 0; i < n; ++i)
            for (int c = 0; c < m_channels; ++c) {
                const float x = std::clamp(c == 0 ? left[done + i] : right[done + i], -1.0f, 1.0f);
                const std::int32_t v = std::int32_t(std::lrint(double(x) * 8388607.0));
                p[0] = std::uint8_t(v);
                p[1] = std::uint8_t(v >> 8);
                p[2] = std::uint8_t(v >> 16);
                p += kBytesPerSample;
            }
        const std::size_t bytes = std::size_t(p - buffer);
        if (std::fwrite(buffer, 1, bytes, m_file) != bytes)
            m_ok = false;
        done += n;
    }
    m_frames += frames;
    return m_ok;
}

bool WavWriter::close()
{
    if (!m_file)
        return true;
    std::uint8_t h[44];
    header(h, m_rate, m_channels, m_frames);
    bool ok = m_ok && std::fseek(m_file, 0, SEEK_SET) == 0 && std::fwrite(h, 1, sizeof h, m_file) == sizeof h;
    ok = std::fclose(m_file) == 0 && ok;
    m_file = nullptr;
    return ok;
}

} // namespace krate
