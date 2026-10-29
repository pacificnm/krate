#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/Instrument.h"

namespace krate::song {

class SongPlayer;

// A decoded recording: 16-bit stereo at its own rate (docs/SONG_PLAYER.md#backing-track).
struct AudioClip
{
    int rate = 0;
    std::vector<std::int16_t> samples; // interleaved left, right (mono files on both)
    std::size_t frames() const noexcept { return samples.size() / 2; }
    double seconds() const noexcept { return rate > 0 ? double(frames()) / rate : 0.0; }
};

// Decodes an MP3 file (dr_mp3). false with *error if it can't. Not realtime.
bool decodeMp3(const std::string &path, AudioClip *clip, std::string *error = nullptr);

// A song's backing track, as a rack device: it plays its clip at the song's
// time (SongPlayer::blockSeconds()) plus the offset, so it starts, stops and
// jumps with the song; silent while the song isn't playing. Between samples it
// reads with a 4-point curve, which also takes a recording at another rate.
// A start or a jump fades in over 5 ms.
//
// The clip is handed over from any thread; the caller keeps it alive until the
// audio thread has moved two blocks on (blocksProcessed()).
class BackingTrack final : public IInstrument
{
public:
    explicit BackingTrack(const SongPlayer &player) : m_player(player) {}

    void setClip(const AudioClip *clip) noexcept { m_clip.store(clip, std::memory_order_release); }
    // Where bar 1 of the song is in the recording, in seconds (negative: the
    // recording starts after bar 1).
    void setOffset(double seconds) noexcept { m_offset.store(seconds, std::memory_order_relaxed); }
    double offset() const noexcept { return m_offset.load(std::memory_order_relaxed); }
    std::uint64_t blocksProcessed() const noexcept { return m_blocks.load(std::memory_order_acquire); }

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const ParamDescriptor> parameters() const noexcept override { return {}; }
    float param(int) const noexcept override { return 0.0f; }
    void setParam(int, float) noexcept override {}
    void setParamFromHost(int, float) noexcept override {}
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void process(const AudioBlock &block) noexcept override;

private:
    const SongPlayer &m_player;
    std::atomic<const AudioClip *> m_clip{nullptr};
    std::atomic<double> m_offset{0.0};
    std::atomic<std::uint64_t> m_blocks{0};
    double m_rate = 48000.0;
    double m_expected = -1e9; // where this block should start if nothing jumped
    double m_fade = 0.0;      // 0..1, the fade-in after a start or a jump
    double m_fadeStep = 0.0;
};

} // namespace krate::song
