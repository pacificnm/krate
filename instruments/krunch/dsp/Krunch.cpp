#include "dsp/Krunch.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace units {
int input(float v) { return v >= 0.5f ? 1 : 0; }
int channel(float v) { return std::clamp(int(std::lround(double(v) * (kChannels - 1))), 0, kChannels - 1); }
namespace {
double around(float v, double range) { return (std::clamp(double(v), 0.0, 1.0) - 0.5) * 2.0 * range; }
} // namespace
double gainDb(float v) { return around(v, 12.0); }
double bassDb(float v) { return around(v, 12.0); }
double midDb(float v) { return around(v, 10.0); }
double trebleDb(float v) { return around(v, 12.0); }
double presenceDb(float v) { return around(v, 8.0); }
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
std::string gain(float v) { return db(units::gainDb(v)); }
std::string bass(float v) { return db(units::bassDb(v)); }
std::string mid(float v) { return db(units::midDb(v)); }
std::string treble(float v) { return db(units::trebleDb(v)); }
std::string presence(float v) { return db(units::presenceDb(v)); }
std::string volume(float v) { return v <= 0.0f ? "-inf dB" : db(units::volumeDb(v)); }
std::string gate(float v) { return v <= 0.0f ? "Off" : std::format("{:.0f} dB", units::gateDb(v)); }
std::string onOff(float v) { return v >= 0.5f ? "On" : "Off"; }
std::string cabMix(float v)
{
    const long b = std::lround(double(v) * 100.0);
    return std::format("A {} / B {}", 100 - b, b);
}
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
    m_dualHead.prepare(sampleRate);
    m_speaker.prepare(sampleRate);
    m_cabinet.prepare();
    m_rateForKernels.store(sampleRate, std::memory_order_relaxed);
    m_fadeStep = 1.0 / (kSwitchSeconds * sampleRate);
    m_active = -1;
    m_previous = -1;
}

void Krunch::addCabinet(int channel, const float *weight, std::uint32_t frames) noexcept
{
    const double rate = m_sampleRate;
    const auto usable = [this, rate](int slot) {
        const IrKernel *k = m_cabinet.kernel(slot);
        return k && k->sampleRate() == rate;
    };
    const int slotA = cabinetSlot(channel, false), slotB = cabinetSlot(channel, true);
    if (channelValue(channel, ChannelParam::Cabinet) < 0.5f) { // CABINET off: no IR, no speaker (a capture with its own)
        for (std::uint32_t n = 0; n < frames; ++n)
            m_directWeight[n] += weight[n];
        m_slotPans[std::size_t(slotA)] = m_slotPans[std::size_t(slotB)] = 0.0f;
        return;
    }
    const bool a = usable(slotA), b = usable(slotB);
    if (!a && !b) {
        for (std::uint32_t n = 0; n < frames; ++n)
            m_speakerWeight[n] += weight[n];
        return;
    }
    const float mix = a && b ? channelValue(channel, ChannelParam::CabMix) : (b ? 1.0f : 0.0f);
    const auto assign = [&](int slot, float share) {
        auto &w = m_slotWeights[std::size_t(slot)];
        for (std::uint32_t n = 0; n < frames; ++n)
            w[n] = weight[n] * share;
        m_slotWeightPtrs[std::size_t(slot)] = w.data();
    };
    if (a && mix < 1.0f)
        assign(slotA, 1.0f - mix);
    if (b && mix > 0.0f)
        assign(slotB, mix);
    // SPACE: with both IRs, A moves left and B right (at 10: hard left and right).
    const float space = a && b ? std::clamp(channelValue(channel, ChannelParam::Space), 0.0f, 1.0f) : 0.0f;
    m_slotPans[std::size_t(slotA)] = -space;
    m_slotPans[std::size_t(slotB)] = space;
}

bool Krunch::playsCapture(int channel) const noexcept
{
    const CaptureModel *model = m_captures[std::size_t(channel)].load(std::memory_order_acquire);
    return model && model->sampleRate() == m_sampleRate;
}

void Krunch::runChannel(int channel, float *out, std::uint32_t frames) noexcept
{
    std::copy_n(m_in.data(), frames, out);
    CaptureModel *capture = m_captures[std::size_t(channel)].load(std::memory_order_acquire);
    if (!capture || capture->sampleRate() != m_sampleRate) { // no amp: silence
        std::fill_n(out, frames, 0.0f);
        return;
    }
    // The capture: GAIN drives it (noon as captured), then the EQ.
    const float drive = float(std::pow(10.0, units::gainDb(channelValue(channel, ChannelParam::Gain)) / 20.0));
    for (std::uint32_t n = 0; n < frames; ++n)
        out[n] *= drive;
    capture->process(out, frames);
    CaptureEq &eq = m_captureEq[std::size_t(channel)];
    const std::array<float, 4> knobs{channelValue(channel, ChannelParam::Bass), channelValue(channel, ChannelParam::Mid),
                                     channelValue(channel, ChannelParam::Treble), channelValue(channel, ChannelParam::Presence)};
    if (knobs != eq.knobs) {
        eq.knobs = knobs;
        eq.bass.setLowShelf(m_sampleRate, 120.0, units::bassDb(knobs[0]));
        eq.mid.setPeak(m_sampleRate, 700.0, 0.7, units::midDb(knobs[1]));
        eq.treble.setHighShelf(m_sampleRate, 2500.0, units::trebleDb(knobs[2]));
        eq.presence.setHighShelf(m_sampleRate, 5000.0, units::presenceDb(knobs[3]));
    }
    for (std::uint32_t n = 0; n < frames; ++n)
        out[n] = eq.presence.process(eq.treble.process(eq.mid.process(eq.bass.process(out[n]))));

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
    m_blocks.fetch_add(1, std::memory_order_release);
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
    m_tunerTap.write(in, frames);

    // A new channel: start it fresh and fade the old one out.
    const int channel = units::channel(m_params.get(int(ParamId::Channel)));
    if (channel != m_active) {
        if (m_active >= 0) {
            m_previous = m_active;
            m_fade = 0.0;
        }
        m_active = channel;
    }

    m_front.process(m_in.data(), frames);
    m_gate.process(m_in.data(), frames, channelValue(m_active, ChannelParam::Gate));
    m_gateClosed.store(m_gate.closed(), std::memory_order_relaxed);

    runChannel(m_active, m_a.data(), frames);
    const int previous = m_previous; // fading out this block (its cabinet too)
    if (previous >= 0) {
        runChannel(previous, m_b.data(), frames);
        for (std::uint32_t n = 0; n < frames; ++n) {
            m_fade = std::min(1.0, m_fade + m_fadeStep);
            m_fadeIn[n] = float(m_fade);
            m_fadeOut[n] = float(1.0 - m_fade);
            m_a[n] = m_a[n] * m_fadeIn[n] + m_b[n] * m_fadeOut[n];
        }
        if (m_fade >= 1.0)
            m_previous = -1;
    } else {
        std::fill_n(m_fadeIn.data(), frames, 1.0f);
    }

    if (channelValue(m_active, ChannelParam::Loop) >= 0.5f)
        m_loop.process(m_a.data(), frames);

    // The cabinets: the playing channel's (and the previous one's while it fades).
    std::fill_n(left, frames, 0.0f);
    std::fill_n(right, frames, 0.0f);
    m_slotWeightPtrs.fill(nullptr);
    std::fill_n(m_speakerWeight.data(), frames, 0.0f);
    std::fill_n(m_directWeight.data(), frames, 0.0f);
    addCabinet(m_active, m_fadeIn.data(), frames);
    if (previous >= 0)
        addCabinet(previous, m_fadeOut.data(), frames);
    m_cabinet.process(m_a.data(), frames, m_slotWeightPtrs.data(), left, right, m_slotPans.data());
    for (std::uint32_t n = 0; n < frames; ++n) { // the built-in speaker keeps running, so it's ready
        const float y = m_speaker.process(m_a[n]) * m_speakerWeight[n];
        left[n] += y;
        right[n] += y;
    }
    // The cabinet off: the amp's output as it is (at the speaker's level, so switching doesn't jump much).
    constexpr float kDirectGain = 1.0f; // (measured: within 1.5 dB of a cabinet IR, through a capture)
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float y = m_a[n] * m_directWeight[n] * kDirectGain;
        left[n] += y;
        right[n] += y;
    }

    // Dual Head (a pedal in front or in the loop, on): the second amp, on the right.
    // (In the loop, only with the playing channel's loop on, as the loop's other pedals.)
    const DualHead *dual = nullptr;
    const bool loopOn = channelValue(m_active, ChannelParam::Loop) >= 0.5f;
    for (const PedalChain *chain : {&m_front, &m_loop})
        if (const PedalList *list = chain->list(); list && !dual && (chain == &m_front || loopOn))
            for (int i = 0; i < list->count; ++i)
                if (list->pedals[std::size_t(i)]->info().type == DualHead::kInfo.type && list->pedals[std::size_t(i)]->on()) {
                    dual = static_cast<const DualHead *>(list->pedals[std::size_t(i)]);
                    break;
                }
    m_dualHead.process(dual, left, right, frames);

    // Level (ramped), a little under unity (the speaker's peaks).
    constexpr double kOutputTrimDb = -0.5;
    const float target = m_muted.load(std::memory_order_relaxed) // the tuner's MUTE: ramps down and back
                             ? 0.0f
                             : float(units::levelGain(m_params.get(int(ParamId::Level))) * std::pow(10.0, kOutputTrimDb / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    float outPeak = 0.0f;
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        left[n] *= m_level;
        right[n] *= m_level;
        outPeak = std::max(outPeak, std::max(std::fabs(left[n]), std::fabs(right[n])));
    }
    m_level = target;
    holdPeak(m_outputPeak, outPeak);
}

} // namespace krunch
