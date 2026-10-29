#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/EventSource.h"
#include "core/SpscQueue.h"
#include "song/Song.h"

// Plays a Song into the rack on the audio thread (docs/SONG_PLAYER.md#playback):
// each track's events go to the instrument it is assigned to, on the exact
// sample, following the rack's transport; the song's tempo map sets the
// transport's tempo (times the speed). Stopping, jumping, reassigning, muting
// or loading another song ends every note it started (with pitch bend and
// sustain reset), so nothing hangs.
//
// Threads: the GUI thread loads songs and sets assignments, levels, mutes,
// solos, speed and the cue; the audio thread calls beforeTransport() and
// render() (IEventSource). Songs are handed over through a lock-free queue and
// handed back for deletion by collectGarbage() (GUI thread).
namespace krate::song {

struct Assignment
{
    int device = -1;      // rack device index, -1 = silent
    int channel = 0;      // the channel the device hears it on (Krimson: a slot's)
    bool drumMap = false; // map drum notes to a basic 11-voice kit (mapToBasicKit)
    bool operator==(const Assignment &) const = default;
};

class SongPlayer final : public IEventSource
{
public:
    static constexpr int kMaxTracks = 64;

    SongPlayer();
    ~SongPlayer() override;
    SongPlayer(const SongPlayer &) = delete;
    SongPlayer &operator=(const SongPlayer &) = delete;

    // --- GUI thread -------------------------------------------------------------
    // Hands a song to the player (nullptr unloads). Assignments, levels, mutes
    // and solos are per track index and stay as they are; set them for the new
    // song. The cue goes back to the start.
    void load(std::unique_ptr<Song> song);
    // Deletes songs the audio thread has let go of. Call now and then.
    void collectGarbage();

    void setAssignment(int track, Assignment a) noexcept;
    Assignment assignment(int track) const noexcept;
    void setLevel(int track, float level) noexcept;  // 0..1, scales velocities
    float level(int track) const noexcept;
    void setMute(int track, bool mute) noexcept;
    bool mute(int track) const noexcept;
    void setSolo(int track, bool solo) noexcept;
    bool solo(int track) const noexcept;
    void setSpeed(double speed) noexcept;          // 0.25..2, times the song's tempo
    double speed() const noexcept { return m_speed.load(std::memory_order_relaxed); }
    void setFollowTempo(bool follow) noexcept { m_followTempo.store(follow, std::memory_order_relaxed); }
    // Where play starts (and, while playing, jump there now).
    void cue(Tick tick) noexcept;

    // Where the song is (the tick at the start of the last block) and whether it ran past its end.
    Tick position() const noexcept { return m_position.load(std::memory_order_relaxed); }
    bool ended() const noexcept { return m_ended.load(std::memory_order_relaxed); }

    // --- Audio thread (IEventSource) --------------------------------------------
    void beforeTransport(Transport &transport) noexcept override;
    std::uint32_t render(const TransportState &transport, std::uint32_t frames, double sampleRate, DeviceEvent *out,
                         std::uint32_t capacity) noexcept override;

private:
    struct Sounding
    {
        std::int16_t device;
        std::uint8_t channel, key;
    };
    struct TrackState
    {
        std::size_t cursor = 0;
        Assignment applied;        // where its notes went
        bool audible = false;
        std::array<Sounding, 128> sounding{};
        int soundingCount = 0;
        bool touched = false;      // sent bends or controllers: reset them on a flush
    };
    struct Command
    {
        enum class Kind : std::uint8_t { Load, Cue } kind = Kind::Cue;
        Song *song = nullptr;
        Tick tick = 0;
    };

    void apply(const Command &command, const TransportState &transport) noexcept;
    void flush(int track, std::uint32_t frame) noexcept;    // end its notes
    void flushAll(std::uint32_t frame) noexcept;
    void seek(Tick tick) noexcept;                          // cursors to the first event at or after tick
    void send(int device, std::uint32_t frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) noexcept;

    // Shared settings.
    std::array<std::atomic<std::int32_t>, kMaxTracks> m_assignment; // packed
    std::array<std::atomic<float>, kMaxTracks> m_level;
    std::array<std::atomic<bool>, kMaxTracks> m_mute;
    std::array<std::atomic<bool>, kMaxTracks> m_solo;
    std::atomic<double> m_speed{1.0};
    std::atomic<bool> m_followTempo{true};
    std::atomic<Tick> m_position{0};
    std::atomic<Tick> m_cueRequest{0}; // the latest cue asked for (for the tempo before render() takes it)
    std::atomic<bool> m_ended{false};
    SpscQueue<Command, 64> m_commands;   // GUI -> audio
    SpscQueue<Song *, 64> m_retired;     // audio -> GUI, for deletion
    Song *m_pending = nullptr;           // GUI side: loads the queue couldn't take yet (it's full)

    // Audio thread only.
    Song *m_song = nullptr;
    std::array<TrackState, kMaxTracks> m_tracks{};
    Tick m_cue = 0;
    double m_beatOffset = 0.0; // song beat = transport beat + this
    bool m_playing = false;
    Tick m_next = 0;           // the tick the next block starts at (for the tempo)
    DeviceEvent *m_out = nullptr;
    std::uint32_t m_outCount = 0, m_outCapacity = 0;
};

} // namespace krate::song
