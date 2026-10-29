#include "dsp/SignaturePedals.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

constexpr float kOff = 0.02f; // a part's knob below this takes it out

std::string partText(float v) { return v < kOff ? "Off" : std::format("{:.1f}", double(v) * 10.0); }
std::string vibeText(float v) { return v < kOff ? "Off" : std::format("{:.1f} Hz", Vibe::speedHz(v)); }
std::string timeText(float v) { return std::format("{} ms", std::lround(DrumEcho::headSeconds(v) * 1000.0)); }
double tiltDb(float v) { return (double(v) - 0.5) * 12.0; } // -6 .. +6 dB
std::string toneText(float v) { return std::format("{:+.1f} dB", std::round(tiltDb(v) * 10.0) / 10.0 + 0.0); }
std::string levelText(float v) { return std::format("{:+.1f} dB", std::round((-12.0 + 24.0 * double(v)) * 10.0) / 10.0 + 0.0); }

} // namespace

const PedalInfo Prism::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"fuzz", "FUZZ", 0.0f, &partText},
        {"vibe", "VIBE", 0.25f, &vibeText},
        {"echo", "ECHO", 0.45f, &partText},
        {"time", "TIME", 0.6f, &timeText},
        {"space", "SPACE", 0.45f, &partText},
        {"tone", "TONE", 0.6f, &toneText},
        {"level", "LEVEL", 0.5f, &levelText},
    };
    return PedalInfo{"prism", "Prism", "#141416", knobs, false, 1.8f};
}();

void Prism::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    setParts();
    for (Pedal *p : std::initializer_list<Pedal *>{&m_fuzz, &m_vibe, &m_echo, &m_reverb})
        p->prepare(sampleRate);
    m_tilt = -1.0f;
}

void Prism::resetEffect() noexcept
{
    setParts();
    for (Pedal *p : std::initializer_list<Pedal *>{&m_fuzz, &m_vibe, &m_echo, &m_reverb})
        p->prepare(m_rate); // fresh, faded in as the switch says
    m_lowShelf.reset();
    m_highShelf.reset();
    m_level = -1.0f;
}

void Prism::setParts() noexcept
{
    // Fuzz: a silicon round fuzz, from a fat, singing lead upward.
    const float fuzz = knob(Fuzz);
    m_fuzz.setOn(fuzz >= kOff);
    m_fuzz.setKnob(RoundFuzz::Fuzz, 0.45f + 0.55f * fuzz);
    m_fuzz.setKnob(RoundFuzz::Volume, 0.62f);
    m_fuzz.setKnob(RoundFuzz::Bias, 0.5f);
    m_fuzz.setKnob(RoundFuzz::Type, 1.0f);

    // Vibe: in chorus, its speed from the knob.
    const float vibe = knob(VibeSpeed);
    m_vibe.setOn(vibe >= kOff);
    m_vibe.setKnob(Vibe::Speed, vibe);
    m_vibe.setKnob(Vibe::Intensity, 0.75f);
    m_vibe.setKnob(Vibe::Mode, 0.0f);

    // Echo: more of it, more swell, and from halfway every head (the rhythmic swirl).
    const float echo = knob(Echo);
    m_echo.setOn(echo >= kOff);
    m_echo.setKnob(DrumEcho::Time, knob(Time));
    m_echo.setKnob(DrumEcho::Heads, echo < 0.5f ? 2.0f / 3.0f : 1.0f); // 1+3, then all
    m_echo.setKnob(DrumEcho::Swell, 0.15f + 0.55f * echo);
    m_echo.setKnob(DrumEcho::Mix, 0.15f + 0.45f * echo);
    m_echo.setKnob(DrumEcho::Tone, 0.35f);

    // Space: a long, dark-ish hall.
    const float space = knob(Space);
    m_reverb.setOn(space >= kOff);
    m_reverb.setKnob(Reverb::Decay, 0.35f + 0.35f * space);
    m_reverb.setKnob(Reverb::Mix, 0.1f + 0.45f * space);
    m_reverb.setKnob(Reverb::Tone, 0.5f);
}

void Prism::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    setParts();
    m_fuzz.process(buffer, frames);
    m_vibe.process(buffer, frames);
    m_echo.process(buffer, frames);
    m_reverb.process(buffer, frames);

    // Tone: a tilt around 800 Hz, the lows down as the highs go up.
    if (const float tilt = knob(Tone); tilt != m_tilt) {
        m_tilt = tilt;
        m_lowShelf.setLowShelf(m_rate, 800.0, -tiltDb(tilt));
        m_highShelf.setHighShelf(m_rate, 800.0, tiltDb(tilt));
    }
    const float target = float(std::pow(10.0, (-12.0 + 24.0 * double(knob(Level))) / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    const bool flat = m_tilt == 0.5f;
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        const float y = flat ? buffer[n] : m_highShelf.process(m_lowShelf.process(buffer[n]));
        buffer[n] = y * m_level;
    }
    m_level = target;
}

} // namespace krunch
