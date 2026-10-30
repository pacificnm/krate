#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/EventSource.h"
#include "core/SpscQueue.h"
#include "seq/Pattern.h"

// The rack sequencer's player (docs/SEQUENCER.md): plays the current pattern
// into the rack on the audio thread, looping on the rack's transport, each
// track's notes going straight to its target instrument (as the song
// player's do). Choosing a pattern while it plays switches at the end of the
// current one. Stopping, muting, retargeting or switching RUN off ends every
// note it started, so nothing hangs.
//
// Threads: the GUI thread hands over whole banks (setBank), sets targets,
// mutes, RUN and the cued pattern; the audio thread calls render()
// (IEventSource). Banks are handed back for deletion by collectGarbage().
namespace krate::seq {

struct Target
{
    int device = -1; // rack device index, -1 = silent
    int channel = 0; // the channel the device hears it on (Krimson: a slot's)
    bool operator==(const Target &) const = default;
};

class Sequencer final : public IEventSource
{
public:
    Sequencer();
    ~Sequencer() override;
    Sequencer(const Sequencer &) = delete;
    Sequencer &operator=(const Sequencer &) = delete;

    // --- GUI thread -------------------------------------------------------------
    // Hands the audio thread a new bank (every pattern). Notes already sounding
    // end at their own time.
    void setBank(std::unique_ptr<Bank> bank);
    // Deletes banks the audio thread has let go of. Call now and then.
    void collectGarbage();

    void setTarget(int track, Target t) noexcept;
    Target target(int track) const noexcept;
    void setMute(int track, bool mute) noexcept;
    bool mute(int track) const noexcept;
    void setRunning(bool on) noexcept { m_running.store(on, std::memory_order_relaxed); }
    bool runningSetting() const noexcept { return m_running.load(std::memory_order_relaxed); }
    // The pattern to play: at once while stopped, at the end of the current one while playing.
    void cue(int pattern) noexcept;
    int cued() const noexcept { return m_cued.load(std::memory_order_relaxed); }

    // For the screen: the pattern playing (or that will play), whether it is
    // playing, and the tick inside it at the start of the last block.
    int playingPattern() const noexcept { return m_playingPattern.load(std::memory_order_relaxed); }
    bool playing() const noexcept { return m_isPlaying.load(std::memory_order_relaxed); }
    int tick() const noexcept { return m_tick.load(std::memory_order_relaxed); }

    // --- Audio thread (IEventSource) --------------------------------------------
    void beforeTransport(Transport &) noexcept override {}
    std::uint32_t render(const TransportState &transport, std::uint32_t frames, double sampleRate, DeviceEvent *out,
                         std::uint32_t capacity) noexcept override;

private:
    struct Sounding
    {
        double offBeat;
        std::int16_t device;
        std::uint8_t channel, pitch;
    };
    struct TrackState
    {
        Target applied;
        std::array<Sounding, 128> sounding{};
        int count = 0;
    };

    void play(int track, const Note &note, double onBeat, double b0, double framesPerBeat) noexcept;
    void endNotes(double b0, double b1, double framesPerBeat) noexcept; // offs due in [b0, b1)
    void flush(int track, std::uint32_t frame) noexcept;
    void flushAll(std::uint32_t frame) noexcept;
    void send(int device, std::uint32_t frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) noexcept;
    std::uint32_t frameAt(double beat, double b0, double framesPerBeat) const noexcept;

    // Shared settings.
    std::array<std::atomic<std::int32_t>, kTracks> m_target; // packed
    std::array<std::atomic<bool>, kTracks> m_mute;
    std::atomic<bool> m_running{true};
    std::atomic<int> m_cued{0};
    std::atomic<int> m_playingPattern{0};
    std::atomic<bool> m_isPlaying{false};
    std::atomic<int> m_tick{0};

    SpscQueue<Bank *, 16> m_banks;   // GUI -> audio
    SpscQueue<Bank *, 64> m_retired; // audio -> GUI, for deletion
    Bank *m_pending = nullptr;       // GUI side: a bank the queue couldn't take yet

    // Audio thread only.
    Bank *m_bank = nullptr;
    Bank *m_backlog = nullptr; // retired, waiting for room in m_retired
    std::array<TrackState, kTracks> m_tracks{};
    bool m_playing = false;
    int m_pattern = 0;
    double m_loopStart = 0.0; // the transport beat the current pattern pass began on
    std::uint32_t m_frames = 0;
    DeviceEvent *m_out = nullptr;
    std::uint32_t m_outCount = 0, m_outCapacity = 0;
};

} // namespace krate::seq
