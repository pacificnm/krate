#include "rec/AudioTrack.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace krate {

namespace {
std::string onOff(float v) { return v > 0.5f ? "On" : "Off"; }
std::string monitorText(float v)
{
    const int i = int(std::lround(v * 2.0f));
    return i == 0 ? "Off" : i == 1 ? "Auto" : "On";
}
std::string inputText(float v)
{
    const int i = int(std::lround(v * 2.0f));
    return i == 0 ? "Stereo" : i == 1 ? "In 1" : "In 2";
}

constexpr std::array<ParamDescriptor, AudioTrack::ParamCount> kParams{{
    {"arm", "Arm", 0.0f, 2, &onOff},
    {"monitor", "Monitor", 0.5f, 3, &monitorText}, // Auto: hear the input while armed
    {"input", "Input", 0.0f, 3, &inputText},
}};
} // namespace

AudioTrack::AudioTrack()
{
    for (std::size_t i = 0; i < kParams.size(); ++i)
        m_params[i].store(kParams[i].defaultValue, std::memory_order_relaxed);
}

void AudioTrack::prepare(double sampleRate, std::uint32_t maxFrames) noexcept
{
    m_rate = sampleRate;
    m_inL.assign(maxFrames, 0.0f); // prepare runs before processing: not realtime
    m_inR.assign(maxFrames, 0.0f);
    m_covered.assign(maxFrames, 0);
}

std::span<const ParamDescriptor> AudioTrack::parameters() const noexcept { return kParams; }

float AudioTrack::param(int index) const noexcept
{
    return index >= 0 && index < ParamCount ? m_params[std::size_t(index)].load(std::memory_order_relaxed) : 0.0f;
}

void AudioTrack::setParam(int index, float normalized) noexcept
{
    if (index >= 0 && index < ParamCount)
        m_params[std::size_t(index)].store(snapToDetent(kParams[std::size_t(index)], std::clamp(normalized, 0.0f, 1.0f)),
                                           std::memory_order_relaxed);
}

void AudioTrack::setParamFromHost(int index, float normalized) noexcept
{
    setParam(index, normalized);
    if (index >= 0 && index < ParamCount)
        m_changedByAudio.fetch_or(std::uint64_t(1) << index, std::memory_order_relaxed);
}

float AudioTrack::input(const float *const *in, int channel, std::uint32_t i) const noexcept
{
    return in ? in[channel][i] : 0.0f;
}

void AudioTrack::process(const AudioBlock &block) noexcept
{
    static constexpr TransportState kStopped{};
    const TransportState &t = block.transport ? *block.transport : kStopped;
    const std::uint32_t n = std::min<std::uint32_t>(block.frames, std::uint32_t(m_inL.size()));
    const bool armed = param(Arm) > 0.5f;
    const int monitor = int(std::lround(param(Monitor) * 2.0f));
    const int inputMode = int(std::lround(param(Input) * 2.0f));

    // This block's input, as the take gets it: the stereo pair, or one input on both sides.
    float peak = 0.0f;
    for (std::uint32_t i = 0; i < n; ++i) {
        const float a = input(block.inputs, 0, i), b = input(block.inputs, 1, i);
        m_inL[i] = inputMode == Input2 ? b : a;
        m_inR[i] = inputMode == Input1 ? a : b;
        peak = std::max({peak, std::fabs(m_inL[i]), std::fabs(m_inR[i])});
    }
    if (peak > m_inputPeak.load(std::memory_order_relaxed))
        m_inputPeak.store(peak, std::memory_order_relaxed);

    // Recording: armed, REC on, the transport playing. A play from the top
    // (restarted) is a new take.
    const bool want = armed && m_recordEnabled.load(std::memory_order_relaxed) && t.playing;
    bool recording = m_recording.load(std::memory_order_relaxed);
    if (recording && (!want || t.restarted))
        recording = false;
    if (want && !recording && !m_writerBusy.load(std::memory_order_acquire)) {
        m_writerBusy.store(true, std::memory_order_relaxed);
        m_takeStartBeat.store(t.beatAtBlockStart, std::memory_order_relaxed);
        m_takeChannels.store(inputMode == Stereo ? 2 : 1, std::memory_order_relaxed);
        m_takeId.fetch_add(1, std::memory_order_release);
        recording = true;
    }
    m_recording.store(recording, std::memory_order_release);
    if (recording)
        m_ring.write(m_inL.data(), m_inR.data(), n);

    // Output: the take (not while this track records over it), and the input if monitored.
    float *outL = block.outputs[0];
    float *outR = block.outputs[1];
    std::fill_n(outL, block.frames, 0.0f);
    std::fill_n(outR, block.frames, 0.0f);
    const Playlist *playlist = m_playlist.load(std::memory_order_acquire);
    if (playlist && !playlist->clips.empty() && t.playing && !(armed && m_recordEnabled.load(std::memory_order_relaxed))
        && t.bpm > 0.0) {
        std::fill_n(m_covered.begin(), n, std::uint8_t(0));
        for (auto it = playlist->clips.rbegin(); it != playlist->clips.rend(); ++it) { // the latest on top
            const AudioClip &clip = *it;
            const Take *take = clip.take;
            if (!take || take->frames() < 2)
                continue;
            const std::int64_t length = std::int64_t(take->frames());
            const std::int64_t end = clip.frames > 0 ? std::min(length, clip.skip + clip.frames) : length;
            const double seconds = (t.beatAtBlockStart - clip.startBeat) * 60.0 / t.bpm;
            const double start = seconds * take->rate + double(clip.skip);
            const double step = double(take->rate) / m_rate;
            if (start + double(n) * step < double(clip.skip) || start >= double(end - 1))
                continue; // not in this block
            const bool mono = take->right.empty();
            for (std::uint32_t i = 0; i < n; ++i) {
                const double pos = start + double(i) * step;
                if (pos < double(clip.skip) || m_covered[i])
                    continue;
                const std::size_t k = std::size_t(pos);
                if (std::int64_t(k) + 1 >= end)
                    break;
                const float f = float(pos - double(k));
                outL[i] = take->left[k] + f * (take->left[k + 1] - take->left[k]);
                outR[i] = mono ? outL[i] : take->right[k] + f * (take->right[k + 1] - take->right[k]);
                m_covered[i] = 1;
            }
        }
    }
    if (monitor == MonitorOn || (monitor == MonitorAuto && armed))
        for (std::uint32_t i = 0; i < n; ++i) {
            outL[i] += m_inL[i];
            outR[i] += m_inR[i];
        }
    m_blocks.fetch_add(1, std::memory_order_release);
}

} // namespace krate
