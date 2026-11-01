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

// A recorded take: its audio (right empty for mono), and where it belongs.
struct Take
{
    std::vector<float> left, right;
    int rate = 48000;
    double startBeat = 0.0;   // the transport's beat where recording began
    std::int64_t offset = 0;  // frames to skip at its start: what the interface's round trip delayed
    std::size_t frames() const { return left.size(); }
};

// An audio track in the Krate rack (docs/DAW.md, D2): records its input
// (the interface's inputs, or another device's output in stereo) while it's
// armed, REC is on and the transport plays, and plays its take back in time.
//
// Recording: the audio thread copies the input into a ring buffer; the disk
// thread (Recorder) writes it to a WAV file. The audio thread never waits on
// the disk. A new take can't begin until the last one is written.
//
// Playback: the take the GUI hands over (setTake), at the transport's beat:
// (beat - startBeat) seconds at the tempo, plus its offset. Monitoring: the
// input to the output (On, or Auto while armed); an armed track recording
// doesn't play its take.
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
    // The take to play (owned by the caller), returning the one before: free
    // it once blocks() has moved on by two (the audio thread may be using it).
    const Take *setTake(const Take *take) noexcept { return m_take.exchange(take, std::memory_order_acq_rel); }
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
    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_writerBusy{false};
    std::atomic<std::uint32_t> m_takeId{0};
    std::atomic<double> m_takeStartBeat{0.0};
    std::atomic<int> m_takeChannels{2};
    std::atomic<const Take *> m_take{nullptr};
    std::atomic<std::uint64_t> m_blocks{0};
    std::atomic<float> m_inputPeak{0.0f};
};

} // namespace krate
