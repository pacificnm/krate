#include "dsp/Krunch.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace units {
int input(float v) { return v >= 0.5f ? 1 : 0; }
int channel(float v) { return std::clamp(int(std::lround(double(v) * (kChannels - 1))), 0, kChannels - 1); }
Voice voice(float v) { return Voice(std::clamp(int(std::lround(double(v) * (int(Voice::Count) - 1))), 0, int(Voice::Count) - 1)); }
double levelDb(float v) { return kLevelMinDb + double(v) * (kLevelMaxDb - kLevelMinDb); }
double levelGain(float v) { return v <= 0.0f ? 0.0 : std::pow(10.0, levelDb(v) / 20.0); }
double volumeDb(float v) { return -40.0 + 46.0 * double(v); }
double volumeGain(float v) { return v <= 0.0f ? 0.0 : std::pow(10.0, volumeDb(v) / 20.0); }
double gateDb(float v) { return -80.0 + 60.0 * double(v); }
} // namespace units

namespace format {
namespace {
std::string db(double value) { return std::format("{:+.1f} dB", std::round(value * 10.0) / 10.0 + 0.0); } // + 0.0: never "-0.0"
} // namespace
std::string input(float v) { return std::format("In {}", units::input(v) + 1); }
std::string channel(float v) { return std::format("Channel {}", units::channel(v) + 1); }
std::string dial(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string level(float v) { return v <= 0.0f ? "-inf dB" : db(units::levelDb(v)); }
std::string voice(float v)
{
    static constexpr const char *kNames[] = {"Clean", "Crunch", "Heavy", "Lead"};
    return kNames[int(units::voice(v))];
}
std::string volume(float v) { return v <= 0.0f ? "-inf dB" : db(units::volumeDb(v)); }
std::string gate(float v) { return v <= 0.0f ? "Off" : std::format("{:.0f} dB", units::gateDb(v)); }
std::string onOff(float v) { return v >= 0.5f ? "On" : "Off"; }
} // namespace format

namespace {
void holdPeak(std::atomic<float> &held, float peak) noexcept
{
    if (peak > held.load(std::memory_order_relaxed))
        held.store(peak, std::memory_order_relaxed);
}
} // namespace

Krunch::Krunch()
{
    m_volume.fill(-1.0f);
}

void Krunch::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_front.prepare(sampleRate);
    m_loop.prepare(sampleRate);
    m_gate.prepare(sampleRate);
    for (Preamp &p : m_preamps)
        p.prepare(sampleRate);
    m_powerAmp.prepare(sampleRate);
    m_speaker.prepare(sampleRate);
    m_fadeStep = 1.0 / (kSwitchSeconds * sampleRate);
    m_active = -1;
    m_previous = -1;
}

Preamp::Settings Krunch::preampSettings(int channel) const noexcept
{
    Preamp::Settings s;
    s.voice = units::voice(channelValue(channel, ChannelParam::Voice));
    s.gain = channelValue(channel, ChannelParam::Gain);
    s.bass = channelValue(channel, ChannelParam::Bass);
    s.mid = channelValue(channel, ChannelParam::Mid);
    s.treble = channelValue(channel, ChannelParam::Treble);
    s.presence = channelValue(channel, ChannelParam::Presence);
    return s;
}

void Krunch::runChannel(int channel, float *out, std::uint32_t frames) noexcept
{
    std::copy_n(m_in.data(), frames, out);
    Preamp &preamp = m_preamps[std::size_t(channel)];
    preamp.set(preampSettings(channel));
    preamp.process(out, frames);

    // Volume, ramped across the block.
    const float target = float(units::volumeGain(channelValue(channel, ChannelParam::Volume)));
    float &volume = m_volume[std::size_t(channel)];
    if (volume < 0.0f)
        volume = target;
    const float step = (target - volume) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        volume += step;
        out[n] *= volume;
    }
    volume = target;
}

void Krunch::process(const krate::AudioBlock &block) noexcept
{
    const float *in = block.inputs ? block.inputs[units::input(m_params.get(int(ParamId::Input)))] : nullptr;
    for (std::uint32_t offset = 0; offset < block.frames; offset += kBlock) {
        const std::uint32_t frames = std::min(kBlock, block.frames - offset);
        processBlock(in ? in + offset : nullptr, block.outputs[0] + offset, block.outputs[1] + offset, frames);
    }
}

void Krunch::processBlock(const float *in, float *left, float *right, std::uint32_t frames) noexcept
{
    float inPeak = 0.0f;
    if (in) {
        std::copy_n(in, frames, m_in.data());
        for (std::uint32_t n = 0; n < frames; ++n)
            inPeak = std::max(inPeak, std::fabs(in[n]));
    } else {
        std::fill_n(m_in.data(), frames, 0.0f);
    }
    holdPeak(m_inputPeak, inPeak);

    // A new channel: start it fresh and fade the old one out.
    const int channel = units::channel(m_params.get(int(ParamId::Channel)));
    if (channel != m_active) {
        if (m_active >= 0) {
            m_previous = m_active;
            m_fade = 0.0;
        }
        m_active = channel;
        m_preamps[std::size_t(channel)].reset();
    }

    m_front.process(m_in.data(), frames);
    m_gate.process(m_in.data(), frames, channelValue(m_active, ChannelParam::Gate));
    m_gateClosed.store(m_gate.closed(), std::memory_order_relaxed);

    runChannel(m_active, m_a.data(), frames);
    if (m_previous >= 0) {
        runChannel(m_previous, m_b.data(), frames);
        for (std::uint32_t n = 0; n < frames; ++n) {
            m_fade = std::min(1.0, m_fade + m_fadeStep);
            m_a[n] = float(m_a[n] * m_fade + m_b[n] * (1.0 - m_fade));
        }
        if (m_fade >= 1.0)
            m_previous = -1;
    }

    if (channelValue(m_active, ChannelParam::Loop) >= 0.5f)
        m_loop.process(m_a.data(), frames);

    m_powerAmp.set(m_params.get(int(ParamId::Master)), m_params.get(int(ParamId::Sag)),
                   m_params.get(int(ParamId::Resonance)));
    m_powerAmp.process(m_a.data(), frames);

    // Speaker and level (ramped), mono on both sides. The trim keeps a cranked
    // master (the output stage at full swing, plus the speaker's peaks) under full scale.
    constexpr double kOutputTrimDb = -4.0;
    const float target = float(units::levelGain(m_params.get(int(ParamId::Level))) * std::pow(10.0, kOutputTrimDb / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    float outPeak = 0.0f;
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        const float y = m_speaker.process(m_a[n]) * m_level;
        left[n] = y;
        right[n] = y;
        outPeak = std::max(outPeak, std::fabs(y));
    }
    m_level = target;
    holdPeak(m_outputPeak, outPeak);
}

} // namespace krunch
