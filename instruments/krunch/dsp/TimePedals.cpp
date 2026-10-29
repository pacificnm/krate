#include "dsp/TimePedals.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace krunch {

namespace {

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string percent(float v) { return std::format("{}%", std::lround(double(v) * 100.0)); }

double chorusHz(float v) { return 0.1 * std::pow(80.0, double(v)); }            // 0.1 .. 8 Hz
std::string chorusRate(float v) { return std::format("{:.2f} Hz", chorusHz(v)); }

double delaySeconds(float v) { return 0.04 * std::pow(Delay::kMaxSeconds / 0.04, double(v)); } // 40 ms .. 1.5 s
std::string delayTime(float v) { return std::format("{} ms", std::lround(delaySeconds(v) * 1000.0)); }
double delayFeedback(float v) { return 0.95 * double(v); }
double toneHz(float v) { return 1200.0 * std::pow(10.0, double(v)); }               // 1.2 .. 12 kHz
std::string toneText(float v)
{
    const double hz = toneHz(v);
    return hz < 10000.0 ? std::format("{:.1f} kHz", hz / 1000.0) : std::format("{:.0f} kHz", hz / 1000.0);
}

std::string decayText(float v) { return std::format("{:.1f} s", krate::Reverb::decaySeconds(double(v))); }

} // namespace

// --- Chorus -----------------------------------------------------------------------

const PedalInfo Chorus::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"rate", "RATE", 0.45f, &chorusRate},
        {"depth", "DEPTH", 0.5f, &tenth},
        {"mix", "MIX", 0.7f, &percent},
    };
    return PedalInfo{"chorus", "Chorus", "#2F7FA8", knobs, true};
}();

void Chorus::prepareEffect(double sampleRate) noexcept { m_rate = sampleRate; }

void Chorus::resetEffect() noexcept
{
    m_line.fill(0.0f);
    m_write = 0;
}

void Chorus::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const double increment = chorusHz(knob(Rate)) / m_rate;
    const double centre = 0.007 * m_rate;                       // 7 ms
    const double swing = 0.0045 * m_rate * double(knob(Depth)); // up to +-4.5 ms
    const float mix = knob(Mix);
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float x = buffer[n];
        m_line[m_write] = x;
        const double delay = centre + swing * std::sin(2.0 * std::numbers::pi * m_phase);
        m_phase += increment;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
        // Read `delay` samples back, linearly between the two nearest.
        const double back = double(m_write) - delay + double(kSize);
        const auto i = std::size_t(back);
        const double frac = back - double(i);
        const float a = m_line[i & (kSize - 1)], b = m_line[(i + 1) & (kSize - 1)];
        const float wet = float(a + (b - a) * frac);
        m_write = (m_write + 1) & (kSize - 1);
        // Mix: at full, half dry and half wet (where the shimmer is strongest).
        buffer[n] = x + 0.5f * mix * (wet - x);
    }
}

// --- Delay ------------------------------------------------------------------------

const PedalInfo Delay::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"time", "TIME", 0.62f, &delayTime}, // about 380 ms
        {"feedback", "REPEATS", 0.35f, &percent},
        {"tone", "TONE", 0.45f, &toneText},
        {"mix", "MIX", 0.35f, &percent},
    };
    return PedalInfo{"delay", "Delay", "#7A6A55", knobs, true};
}();

Delay::Delay()
    : Pedal(kInfo)
    , m_line(std::size_t(kMaxSeconds * 192000.0) + 4, 0.0f)
{
}

void Delay::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_glide = 1.0 - std::exp(-1.0 / (0.15 * sampleRate)); // the time glides over ~150 ms
    m_lowCut.setCutoff(sampleRate, 90.0);
    m_tone = -1.0f;
}

void Delay::resetEffect() noexcept
{
    std::fill(m_line.begin(), m_line.end(), 0.0f);
    m_write = 0;
    m_delay = -1.0;
    m_toneLowpass.reset();
    m_lowCut.reset();
}

void Delay::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float tone = knob(Tone); tone != m_tone) {
        m_tone = tone;
        m_toneLowpass.setCutoff(m_rate, toneHz(tone));
    }
    const std::size_t size = m_line.size();
    const double target = std::min(delaySeconds(knob(Time)) * m_rate, double(size - 2));
    if (m_delay < 0.0)
        m_delay = target;
    const double feedback = delayFeedback(knob(Feedback));
    const float mix = knob(Mix);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_delay += (target - m_delay) * m_glide;
        const double back = double(m_write) - m_delay + double(size);
        const auto i = std::size_t(back) % size;
        const double frac = back - std::floor(back);
        const float a = m_line[i], b = m_line[(i + 1) % size];
        const float echo = float(a + (b - a) * frac);
        // What goes round again: darker, without the boom, gently saturated.
        const float repeat = m_lowCut.highpass(m_toneLowpass.lowpass(echo));
        const float x = buffer[n];
        m_line[m_write] = float(std::tanh(x + feedback * repeat));
        m_write = (m_write + 1) % size;
        buffer[n] = x + mix * echo;
    }
}

// --- Reverb -----------------------------------------------------------------------

const PedalInfo Reverb::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"decay", "DECAY", 0.35f, &decayText},
        {"tone", "TONE", 0.55f, &toneText},
        {"mix", "MIX", 0.3f, &percent},
    };
    return PedalInfo{"reverb", "Reverb", "#5B4B8A", knobs, true};
}();

void Reverb::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_reverb.prepare(sampleRate);
    m_tone = -1.0f;
}

void Reverb::resetEffect() noexcept
{
    m_reverb.clear();
    m_toneLowpass.reset();
}

void Reverb::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float tone = knob(Tone); tone != m_tone) {
        m_tone = tone;
        m_toneLowpass.setCutoff(m_rate, toneHz(tone));
    }
    // The network adds its wet to whatever it's given: give it the guitar on
    // both sides and keep only what it added.
    std::copy_n(buffer, frames, m_left.data());
    std::copy_n(buffer, frames, m_right.data());
    m_reverb.process(m_left.data(), m_right.data(), frames, double(knob(Decay)), 1.0);
    const float mix = knob(Mix);
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float wet = 0.5f * ((m_left[n] - buffer[n]) + (m_right[n] - buffer[n]));
        buffer[n] += mix * m_toneLowpass.lowpass(wet);
    }
}

} // namespace krunch
