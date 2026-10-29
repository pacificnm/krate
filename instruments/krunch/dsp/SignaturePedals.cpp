#include "dsp/SignaturePedals.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

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

// --- Madman -----------------------------------------------------------------------

namespace {
std::string tenthText(float v) { return std::format("{:.1f}", double(v) * 10.0); }
double midsDb(float v) { return 12.0 * double(v); }
std::string midsText(float v) { return std::format("+{:.1f} dB", midsDb(v)); }
std::string doubleText(float v) { return v < kOff ? "Off" : std::format("{}%", std::lround(double(v) * 100.0)); }

// Germanium diodes to ground: a soft knee to about +-0.35.
double germanium(double v) { return 0.35 * v / std::sqrt(1.0 + (v / 0.35) * (v / 0.35)); }
} // namespace

const PedalInfo Madman::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"gain", "GAIN", 0.75f, &tenthText},
        {"amp", "AMP", 0.65f, &tenthText},
        {"bass", "BASS", 0.45f, &tenthText},
        {"mids", "MIDS", 0.6f, &midsText},
        {"treble", "TREBLE", 0.6f, &tenthText},
        {"double", "DOUBLE", 0.3f, &doubleText},
        {"level", "LEVEL", 0.5f, &levelText},
    };
    PedalInfo info{"madman", "Madman", "#0E0E0E", knobs, false, 1.8f};
    info.pattern = "dots:#F2F2F2";
    return info;
}();

double Madman::opAmpGain(float gain)
{
    // 1 + 1M / (4.7k + the 1M distortion pot, audio taper: all of it at 0, none at 10).
    const double t = std::clamp(double(gain), 0.0, 1.0);
    const double pot = 1e6 * (std::exp(4.0 * (1.0 - t)) - 1.0) / (std::exp(4.0) - 1.0);
    return 1.0 + 1e6 / (4.7e3 + pot);
}

void Madman::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    const double up = sampleRate * Oversampler4x::kFactor;
    m_input.setCutoff(sampleRate, 30.0);
    m_gainLeg.setCutoff(up, 720.0);     // 4.7 k with 47 nF: the bass gets less of the gain
    m_diodeLowpass.setCutoff(up, 6000.0); // the output's capacitor takes the fizz off
    m_lowCut.setLowShelf(sampleRate, 160.0, -5.0);
    m_amp.prepare(sampleRate);
    m_doubler.prepare(sampleRate);
    m_mids = -1.0f;
}

void Madman::resetEffect() noexcept
{
    m_input.reset();
    m_gainLeg.reset();
    m_diodeLowpass.reset();
    m_oversampler.reset();
    m_lowCut.reset();
    m_midPush.reset();
    m_amp.reset();
    m_doubler.reset();
    m_level = -1.0f;
}

void Madman::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    if (const float mids = knob(Mids); mids != m_mids) {
        m_mids = mids;
        m_midPush.setPeak(m_rate, 1000.0, 0.8, midsDb(mids));
    }
    Preamp::Settings amp;
    amp.voice = Voice::Crunch; // the British stack, two stages
    amp.gain = 0.45f + 0.55f * knob(Amp);
    amp.bass = knob(Bass);
    amp.mid = 0.75f; // a cranked amp's mids up
    amp.treble = knob(Treble);
    amp.presence = 0.6f;
    m_amp.set(amp);
    const double gain = opAmpGain(knob(Gain));

    // The distortion pedal, at 4x: op-amp gain (less below 720 Hz), the op-amp's
    // own ceiling, then the germanium diodes to ground and the output capacitor.
    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_oversampler.up(m_input.highpass(buffer[n]), up);
        for (float &u : up) {
            const double opAmp = u + (gain - 1.0) * m_gainLeg.highpass(u);
            const double railed = 4.0 * std::tanh(opAmp / 4.0);
            u = m_diodeLowpass.lowpass(float(germanium(railed)));
        }
        // The pedal's level up: it slams the amp's input, as on the record.
        constexpr float kIntoTheAmp = 3.0f;
        buffer[n] = kIntoTheAmp * m_midPush.process(m_lowCut.process(m_oversampler.down(up)));
    }
    // The cranked amp.
    m_amp.process(buffer, frames);

    // The doubler and the level.
    m_doubler.process(buffer, frames, knob(Double) >= kOff ? knob(Double) : 0.0f);
    // Trimmed so the default comes in about 4 dB over bypass (a saturated
    // signal, so it barely changes with Gain and Amp): a healthy kick into a
    // clean channel without overdriving its input.
    const float target = float(std::pow(10.0, (-12.0 + 24.0 * double(knob(Level))) / 20.0) * 0.16);
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        buffer[n] *= m_level;
    }
    m_level = target;
}

// --- Doubler ----------------------------------------------------------------------

void Doubler::process(float *buffer, std::uint32_t frames, float amount) noexcept
{
    const double wanderStep = 1.0 / m_rate;
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float x = buffer[n];
        m_line[m_write] = x;
        if (amount > 0.0f) {
            m_wander += wanderStep;
            if (m_wander >= 1000.0)
                m_wander -= 1000.0;
            const auto take = [&](double ms, double hz) {
                const double delay = (ms + 0.6 * std::sin(2.0 * std::numbers::pi * hz * m_wander)) * 0.001 * m_rate;
                const double back = double(m_write) - delay + double(kLine);
                const auto i = std::size_t(back);
                const float f = float(back - double(i));
                const float a = m_line[i & (kLine - 1)], b = m_line[(i + 1) & (kLine - 1)];
                return a + (b - a) * f;
            };
            const float y = x + amount * 0.7f * (take(17.0, 0.3) + take(26.0, 0.47));
            // About as loud as one take: the takes are from different moments, so
            // they add like separate performances (power), not like one copy.
            buffer[n] = y / std::sqrt(1.0f + 0.98f * amount * amount);
        }
        m_write = (m_write + 1) & (kLine - 1);
    }
}

// --- Pyramid ----------------------------------------------------------------------

namespace {
std::string echoText(float v) { return v < kOff ? "Off" : std::format("{}%", std::lround(double(v) * 100.0)); }
} // namespace

const PedalInfo Pyramid::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"gain", "GAIN", 0.55f, &tenthText},
        {"body", "BODY", 0.5f, &tenthText},
        {"mids", "MIDS", 0.65f, &tenthText},
        {"bite", "BITE", 0.65f, &tenthText},
        {"twin", "TWIN", 0.45f, &doubleText},
        {"echo", "ECHO", 0.2f, &echoText},
        {"level", "LEVEL", 0.5f, &levelText},
    };
    PedalInfo info{"pyramid", "Pyramid", "#E8C440", knobs, false, 1.8f};
    info.pattern = "pyramid:#8A6A10";
    return info;
}();

void Pyramid::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tighten.setHighpass(sampleRate, 110.0, 0.7); // the boost: the lows out, so the crunch stays tight
    m_push.setPeak(sampleRate, 900.0, 0.9, 5.0);    // ...and the mids up into the amp
    m_amp.prepare(sampleRate);
    m_doubler.prepare(sampleRate);
    m_delay.setKnob(Delay::Time, 0.62f);    // about 380 ms
    m_delay.setKnob(Delay::Feedback, 0.3f);
    m_delay.setKnob(Delay::Tone, 0.4f);
    m_delay.prepare(sampleRate);
}

void Pyramid::resetEffect() noexcept
{
    m_tighten.reset();
    m_push.reset();
    m_amp.reset();
    m_doubler.reset();
    m_delay.prepare(m_rate);
    m_level = -1.0f;
}

void Pyramid::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    // The boost: shaped, a little gain, softly clipped (it tightens more than it distorts).
    for (std::uint32_t n = 0; n < frames; ++n)
        buffer[n] = float(std::tanh(1.2 * double(m_push.process(m_tighten.process(buffer[n])))));

    // The amp: British stages at a medium crunch.
    Preamp::Settings amp;
    amp.voice = Voice::Crunch;
    amp.gain = 0.15f + 0.6f * knob(Gain);
    amp.bass = knob(Body);
    amp.mid = knob(Mids);
    amp.treble = knob(Bite);
    amp.presence = 0.4f + 0.4f * knob(Bite);
    m_amp.set(amp);
    m_amp.process(buffer, frames);

    m_doubler.process(buffer, frames, knob(Twin) >= kOff ? knob(Twin) : 0.0f);

    const float echo = knob(Echo);
    m_delay.setOn(echo >= kOff);
    m_delay.setKnob(Delay::Mix, 0.5f * echo);
    m_delay.process(buffer, frames);

    const float target = float(std::pow(10.0, (-12.0 + 24.0 * double(knob(Level))) / 20.0) * 0.17); // about 4 dB over bypass
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        buffer[n] *= m_level;
    }
    m_level = target;
}

} // namespace krunch
