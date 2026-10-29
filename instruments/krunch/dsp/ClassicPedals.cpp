#include "dsp/ClassicPedals.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>
#include <numbers>

namespace krunch {

namespace {

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string percent(float v) { return std::format("{}%", std::lround(double(v) * 100.0)); }
std::string vibeSpeed(float v) { return std::format("{:.1f} Hz", Vibe::speedHz(v)); }
std::string vibeMode(float v) { return v >= 0.5f ? "Vibrato" : "Chorus"; }
std::string headTime(float v) { return std::format("{} ms", std::lround(DrumEcho::headSeconds(v) * 1000.0)); }
std::string headsText(float v)
{
    static constexpr const char *kNames[] = {"1", "1+2", "1+3", "All"};
    return kNames[std::clamp(int(std::lround(double(v) * 3.0)), 0, 3)];
}
double toneHz(float v) { return 1500.0 * std::pow(6.0, double(v)); } // 1.5 .. 9 kHz
std::string toneText(float v) { return std::format("{:.1f} kHz", toneHz(v) / 1000.0); }

} // namespace

// --- Vibe ---------------------------------------------------------------------------

const PedalInfo Vibe::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"speed", "SPEED", 0.3f, &vibeSpeed},
        {"intensity", "INTENSITY", 0.7f, &tenth},
        {"mode", "MODE", 0.0f, &vibeMode, 2},
    };
    return PedalInfo{"vibe", "Vibe", "#8A6E3B", knobs, false};
}();

double Vibe::speedHz(float speed) { return 0.5 * std::pow(16.0, std::clamp(double(speed), 0.0, 1.0)); }

void Vibe::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_cellUp = 1.0 - std::exp(-1.0 / (0.010 * sampleRate));   // cells answer brightening in ~10 ms
    m_cellDown = 1.0 - std::exp(-1.0 / (0.060 * sampleRate)); // ...and dimming in ~60 ms
}

void Vibe::resetEffect() noexcept
{
    m_state.fill(0.0);
    m_phase = 0.0;
    m_cell = 0.0;
}

void Vibe::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    // The four stages' capacitors, relative to the second (the largest): their
    // corners land far apart, at about 1x, 15x, 47x and 470x the lowest.
    static constexpr std::array<double, kStages> kCaps{15e-9, 220e-9, 470e-12, 4.7e-9};
    const double increment = speedHz(knob(Speed)) / m_rate;
    const double intensity = knob(Intensity);
    const bool vibrato = knob(Mode) >= 0.5f;
    for (std::uint32_t n = 0; n < frames; ++n) {
        // The lamp: a sine, brighter peaks (it's driven hard); the cells lag it.
        const double lamp = std::pow(0.5 + 0.5 * std::sin(2.0 * std::numbers::pi * m_phase), 1.6);
        m_phase += increment;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
        m_cell += (lamp - m_cell) * (lamp > m_cell ? m_cellUp : m_cellDown);
        // Light lowers the cells' resistance: 150 k dark .. down to 15 k lit (by Intensity).
        const double ohms = 150e3 * std::pow(0.1, intensity * m_cell);

        const double x = buffer[n];
        double y = x;
        for (int s = 0; s < kStages; ++s) {
            const double fc = std::min(1.0 / (2.0 * std::numbers::pi * ohms * kCaps[std::size_t(s)]), 0.45 * m_rate);
            const double k = std::tan(std::numbers::pi * fc / m_rate);
            const double a = (k - 1.0) / (k + 1.0);
            double &z = m_state[std::size_t(s)];
            const double out = a * y + z;
            z = y - a * out;
            y = out;
        }
        buffer[n] = float(vibrato ? y : 0.5 * (x + y));
    }
}

// --- Drum Echo ----------------------------------------------------------------------

const PedalInfo DrumEcho::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"time", "TIME", 0.55f, &headTime},
        {"heads", "HEADS", 1.0f, &headsText, 4},
        {"swell", "SWELL", 0.35f, &percent},
        {"tone", "TONE", 0.4f, &toneText},
        {"mix", "MIX", 0.35f, &percent},
    };
    return PedalInfo{"drum_echo", "Drum Echo", "#4B5A3A", knobs, true, 1.2f};
}();

double DrumEcho::headSeconds(float time) { return 0.06 * std::pow(kMaxHeadSeconds / 0.06, std::clamp(double(time), 0.0, 1.0)); }

unsigned DrumEcho::headMask(float heads)
{
    static constexpr unsigned kMasks[] = {0b0001, 0b0011, 0b0101, 0b1111};
    return kMasks[std::clamp(int(std::lround(double(heads) * 3.0)), 0, 3)];
}

DrumEcho::DrumEcho()
    : Pedal(kInfo)
    , m_line(std::size_t(kHeads * kMaxHeadSeconds * 1.01 * 192000.0) + 8, 0.0f)
{
}

void DrumEcho::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_glide = 1.0 - std::exp(-1.0 / (0.2 * sampleRate)); // the drum's speed changes over ~200 ms
    m_lowCut.setCutoff(sampleRate, 120.0);
    m_tone = -1.0f;
}

void DrumEcho::resetEffect() noexcept
{
    std::fill(m_line.begin(), m_line.end(), 0.0f);
    m_write = 0;
    m_delay = -1.0;
    m_toneLowpass.reset();
    m_lowCut.reset();
}

void DrumEcho::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float tone = knob(Tone); tone != m_tone) {
        m_tone = tone;
        m_toneLowpass.setCutoff(m_rate, toneHz(tone));
    }
    const std::size_t size = m_line.size();
    const double target = headSeconds(knob(Time)) * m_rate;
    if (m_delay < 0.0)
        m_delay = target;
    const unsigned mask = headMask(knob(Heads));
    const int playing = std::popcount(mask);
    const float headGain = 1.0f / std::sqrt(float(playing)); // more heads, each a little quieter
    const double swell = 0.85 * double(knob(Swell)) / std::sqrt(double(playing));
    const float mix = knob(Mix);
    const double wobbleStep = 0.6 / m_rate; // the drum's 0.6 Hz wobble
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_delay += (target - m_delay) * m_glide;
        m_wobble += wobbleStep;
        if (m_wobble >= 1.0)
            m_wobble -= 1.0;
        const double wobble = 1.0 + 0.0015 * std::sin(2.0 * std::numbers::pi * m_wobble);
        float heads = 0.0f;
        for (int h = 0; h < kHeads; ++h) {
            if (!(mask & (1u << h)))
                continue;
            const double back = double(m_write) - m_delay * wobble * double(h + 1) + double(size);
            const auto i = std::size_t(back) % size;
            const double frac = back - std::floor(back);
            const float a = m_line[i], b = m_line[(i + 1) % size];
            heads += float(a + (b - a) * frac) * headGain;
        }
        // What goes round the drum again: darker, thinner, gently saturated.
        const float repeat = m_lowCut.highpass(m_toneLowpass.lowpass(heads));
        const float x = buffer[n];
        m_line[m_write] = float(std::tanh(x + swell * repeat));
        m_write = (m_write + 1) % size;
        buffer[n] = x + mix * heads;
    }
}

} // namespace krunch
