#include "song/BackingTrack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO_WRITE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#include "dr_mp3.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "song/SongPlayer.h"

namespace krate::song {

bool decodeMp3(const std::string &path, AudioClip *clip, std::string *error)
{
    drmp3_config config{};
    drmp3_uint64 frames = 0;
    drmp3_int16 *pcm = drmp3_open_file_and_read_pcm_frames_s16(path.c_str(), &config, &frames, nullptr);
    if (!pcm || frames == 0 || config.channels == 0 || config.sampleRate == 0) {
        if (pcm)
            drmp3_free(pcm, nullptr);
        if (error)
            *error = "not an MP3 file we can read";
        return false;
    }
    clip->rate = int(config.sampleRate);
    clip->samples.resize(std::size_t(frames) * 2);
    for (std::size_t f = 0; f < std::size_t(frames); ++f) {
        const std::size_t at = f * config.channels;
        clip->samples[2 * f] = pcm[at];
        clip->samples[2 * f + 1] = config.channels > 1 ? pcm[at + 1] : pcm[at];
    }
    drmp3_free(pcm, nullptr);
    return true;
}

namespace {

// A 4-point (Hermite) read between samples.
float hermite(float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((a * t - b) * t + c) * t + x0;
}

} // namespace

void BackingTrack::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_rate = sampleRate;
    m_fadeStep = 1.0 / (0.005 * sampleRate);
    m_expected = -1e9;
    m_fade = 0.0;
}

void BackingTrack::process(const AudioBlock &block) noexcept
{
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    std::fill_n(left, block.frames, 0.0f);
    std::fill_n(right, block.frames, 0.0f);
    const AudioClip *clip = m_clip.load(std::memory_order_acquire);
    if (!clip || clip->frames() < 4 || !m_player.rolling()) {
        m_expected = -1e9;
        m_fade = 0.0;
        m_blocks.fetch_add(1, std::memory_order_release);
        return;
    }
    // Recording seconds per output sample: the song's speed (its time runs
    // slower or faster than the clock, and the recording with it).
    const double speed = m_player.speed();
    const double step = speed / m_rate;
    const double start = m_player.blockSeconds() + offset();
    if (std::fabs(start - m_expected) > 0.002) // started, or jumped: fade in
        m_fade = 0.0;
    m_expected = start + step * block.frames;

    const auto *samples = clip->samples.data();
    const auto frames = std::int64_t(clip->frames());
    const auto at = [samples, frames](std::int64_t i, int side) -> float {
        if (i < 0 || i >= frames)
            return 0.0f;
        return float(samples[std::size_t(i) * 2 + std::size_t(side)]) * (1.0f / 32768.0f);
    };
    for (std::uint32_t n = 0; n < block.frames; ++n) {
        const double position = (start + step * n) * clip->rate; // in the clip's samples
        if (position < -2.0 || position >= double(frames + 2))
            continue;
        const auto i = std::int64_t(std::floor(position));
        const auto t = float(position - double(i));
        m_fade = std::min(1.0, m_fade + m_fadeStep);
        const auto gain = float(m_fade);
        left[n] = gain * hermite(at(i - 1, 0), at(i, 0), at(i + 1, 0), at(i + 2, 0), t);
        right[n] = gain * hermite(at(i - 1, 1), at(i, 1), at(i + 1, 1), at(i + 2, 1), t);
    }
    m_blocks.fetch_add(1, std::memory_order_release);
}

} // namespace krate::song
