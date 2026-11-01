#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

#include "core/Instrument.h"
#include "core/ParamDescriptor.h"
#include "rec/AudioRing.h"

namespace krate {

// A recorded take's audio (right empty for mono).
struct Take
{
    std::vector<float> left, right;
    int rate = 48000;
    std::size_t frames() const { return left.size(); }
};

// A take placed on the track's lane (docs/DAW.md, D3): it plays from
// `startBeat`, `skip` frames into the take (its round trip, and anything
// trimmed off its start), for `frames` frames (0: to its end), fading in and
// out over `fadeIn` / `fadeOut` frames (linear; D4).
struct AudioClip
{
    const Take *take = nullptr;
    double startBeat = 0.0;
    std::int64_t skip = 0;
    std::int64_t frames = 0;
    std::int64_t fadeIn = 0;
    std::int64_t fadeOut = 0;
};

// What a track plays: its clips, a later one on top where they overlap.
struct Playlist
{
    std::vector<AudioClip> clips;
};

// An audio track in the Krate rack (docs/DAW.md, D2): records its input
// (the interface's inputs, or another device's output in stereo) while it's
// armed, REC is on and the transport plays, and plays its take back in time.
//
// Recording: the audio thread copies the input into a ring buffer; the disk
// thread (Recorder) writes it to a WAV file. The audio thread never waits on
// the disk. A new take can't begin until the last one is written.
//
// Playback: the playlist the GUI hands over (setPlaylist), at the
// transport's beat: each clip from its start beat, (beat - startBeat)
// seconds at the tempo into it, a later clip on top. Monitoring: the input to
// the output (On, or Auto while armed); an armed track recording doesn't play.
class AudioTrack final : public IInstrument
{
public:
    enum Param { Arm, Monitor, Input, ParamCount };
    enum MonitorMode { MonitorOff, MonitorAuto, MonitorOn };
    enum InputMode { Stereo, Input1, Input2 };
    static constexpr std::size_t kRingFrames = 48000 * 10; // ten seconds of disk stalls

    AudioTrack();

    // --- IInstrument
    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const ParamDescriptor> parameters() const noexcept override;
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_changedByAudio.exchange(0); }
    int inputChannels() const noexcept override { return 2; }
    bool stereoInput() const noexcept override { return true; }
    void process(const AudioBlock &block) noexcept override;

    // --- GUI thread
    void setRecordEnabled(bool on) noexcept { m_recordEnabled.store(on, std::memory_order_relaxed); }
    // Punch (docs/DAW.md, D3): while on, records only inside bars [fromBar,
    // toBar), starting and stopping on the exact sample.
    void setPunch(bool on, int fromBar, int toBar) noexcept
    {
        const bool valid = on && fromBar >= 0 && toBar > fromBar;
        m_punch.store(valid ? std::uint64_t(std::uint32_t(fromBar)) << 32 | std::uint32_t(toBar) : 0,
                      std::memory_order_relaxed);
    }
    // The playlist to play (owned by the caller, with its takes), returning
    // the one before: free it (and takes only it used) once blocks() has
    // moved on by two (the audio thread may be using it).
    const Playlist *setPlaylist(const Playlist *playlist) noexcept
    {
        return m_playlist.exchange(playlist, std::memory_order_acq_rel);
    }
    std::uint64_t blocks() const noexcept { return m_blocks.load(std::memory_order_acquire); }
    bool recording() const noexcept { return m_recording.load(std::memory_order_acquire); }
    float takeInputPeak() noexcept { return m_inputPeak.exchange(0.0f, std::memory_order_relaxed); }
    double sampleRate() const noexcept { return m_rate; }

    // --- The disk thread (Recorder)
    AudioRing &ring() noexcept { return m_ring; }
    std::uint32_t takeId() const noexcept { return m_takeId.load(std::memory_order_acquire); }
    double takeStartBeat() const noexcept { return m_takeStartBeat.load(std::memory_order_acquire); }
    int takeChannels() const noexcept { return m_takeChannels.load(std::memory_order_acquire); }
    // Written and closed: a new take may begin.
    void writerDone() noexcept { m_writerBusy.store(false, std::memory_order_release); }

private:
    float input(const float *const *in, int channel, std::uint32_t i) const noexcept;

    std::array<std::atomic<float>, ParamCount> m_params{};
    std::atomic<std::uint64_t> m_changedByAudio{0};
    double m_rate = 48000.0;
    AudioRing m_ring{kRingFrames};
    std::vector<float> m_inL, m_inR; // this block's input, as recorded

    std::atomic<bool> m_recordEnabled{false};
    std::atomic<std::uint64_t> m_punch{0}; // fromBar << 32 | toBar, 0 = off
    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_writerBusy{false};
    std::atomic<std::uint32_t> m_takeId{0};
    std::atomic<double> m_takeStartBeat{0.0};
    std::atomic<int> m_takeChannels{2};
    std::atomic<const Playlist *> m_playlist{nullptr};
    std::vector<std::uint8_t> m_covered; // this block's samples a later clip has written
    std::atomic<std::uint64_t> m_blocks{0};
    std::atomic<float> m_inputPeak{0.0f};
};

} // namespace krate
