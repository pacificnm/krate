#include "kobalt/dsp/Voice.h"

#include <algorithm>
#include <cmath>

namespace kobalt {

namespace {
constexpr double kSmoothingSeconds = 0.004; // cutoff and level changes
constexpr double kMaxPulseNarrowing = 0.45; // width 0.5 down to 0.05
constexpr double kMixGain = 0.5;
constexpr double kLevel = 0.8;              // the VCA fully open
constexpr double kGateRampSeconds = 0.0015; // the VCA in Gate mode: on and off without a click
constexpr double kSubPulseWidth = 0.25;
// Analog at 100 %: the oscillator wanders up to +-8 cents (new targets every
// 0.3 .. 0.7 s, reached over ~0.5 s); each note sits up to +-3 cents and
// +-1/6 octave of cutoff off.
constexpr double kDriftSemis = 0.08;
constexpr double kNoteTuneSemis = 0.03;
constexpr double kNoteCutoffOctaves = 1.0 / 6.0;
constexpr double kDriftSeconds = 0.5;
constexpr double kLn100 = 4.605170185988091; // a glide's time: 99 % of the way

double noteHz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
} // namespace

// --- Lfo ------------------------------------------------------------------------------

double Lfo::random() noexcept
{
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return double(std::int32_t(m_rng)) / 2147483648.0;
}

double Lfo::next(double hz, Wave wave, double sampleRate) noexcept
{
    double v = 0.0;
    switch (wave) {
    case Wave::Triangle: v = 1.0 - 4.0 * std::fabs(m_phase - 0.5); break;
    case Wave::Square: v = m_phase < 0.5 ? 1.0 : -1.0; break;
    case Wave::Random: v = m_held; break;
    case Wave::Noise: v = random(); break;
    }
    m_wrapped = false;
    m_phase += hz / sampleRate;
    if (m_phase >= 1.0) {
        m_phase -= std::floor(m_phase);
        m_wrapped = true;
        m_held = random();
    }
    return v;
}

// --- Voice ----------------------------------------------------------------------------

void Voice::prepare(double sampleRate2x) noexcept
{
    m_rate = sampleRate2x;
    m_env.setSampleRate(sampleRate2x);
    m_gateStep = 1.0 / (kGateRampSeconds * sampleRate2x);
    m_saw.setWave(krate::Wave::Saw);
    m_pulse.setWave(krate::Wave::Pulse);
    m_sub1.setWave(krate::Wave::Square);
    m_sub2.setWave(krate::Wave::Square);
    // One phase for all four: they start together and advance by the same
    // increment (halved, quartered), so the subs' edges stay on the main wave's.
    m_saw.reset();
    m_pulse.reset();
    m_sub1.reset();
    m_sub2.reset();
    m_played = false;
    m_random = 0x9E3779B9u;
    m_drift = m_driftTarget = 0.0;
    m_driftCountdown = 0;
    kill();
}

double Voice::random() noexcept
{
    m_random ^= m_random << 13; // xorshift32
    m_random ^= m_random >> 17;
    m_random ^= m_random << 5;
    return double(m_random) / 2147483648.0 - 1.0;
}

void Voice::kill() noexcept
{
    m_env.kill();
    m_gate = false;
    m_gateAmp = 0.0;
    m_filter.reset();
    m_cutoff = -1.0;
    m_gain = -1.0;
}

bool Voice::active() const noexcept
{
    return m_gate || m_env.active() || m_gateAmp > 0.0;
}

void Voice::noteOn(int note, float velocity, bool legato, const VoiceSettings &s) noexcept
{
    legato = legato && m_gate; // only into a note still sounding with its key down
    const bool glide = s.portaSeconds > 0.0 && m_played
                       && (s.portaMode == PortaMode::On || (s.portaMode == PortaMode::Auto && legato));
    if (!active()) { // from silence: start clean
        m_filter.reset();
        m_cutoff = -1.0;
        m_gain = -1.0;
    }
    m_target = note;
    m_glide = glide;
    if (!glide)
        m_pitch = note;
    m_played = true;
    m_gate = true;
    if (!legato || s.envTrigger == EnvTrigger::GateTrig) {
        m_env.gateOn(); // from where it is: no click
        m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    }
    if (!legato) { // a new note: its own small differences
        m_noteTune = random();
        m_noteCutoff = random();
    }
}

void Voice::noteOff() noexcept
{
    m_gate = false;
    m_env.gateOff();
}

void Voice::render(float *out2x, std::uint32_t frames, const float *lfo, const bool *lfoWrapped,
                   const VoiceSettings &s) noexcept
{
    if (!active())
        return;

    m_env.setTimes(s.env);
    const double hostRate = 0.5 * m_rate;
    const double smoothing = 1.0 - std::exp(-1.0 / (kSmoothingSeconds * m_rate));
    const double nyquistOctaves = std::log2(0.42 * m_rate);
    const double driftSmoothing = 1.0 - std::exp(-1.0 / (kDriftSeconds * hostRate));
    const double glideCoefficient =
        m_glide && s.portaSeconds > 0.0 ? 1.0 - std::exp(-kLn100 / (s.portaSeconds * hostRate)) : 1.0;
    const double velocityGain = 1.0 - s.velocityAmount * (1.0 - m_velocity);
    const double targetGain = kLevel * velocityGain;
    const auto subWave = s.subType == SubType::TwoOctavesPulse ? krate::Wave::Pulse : krate::Wave::Square;
    m_sub2.setWave(subWave);

    for (std::uint32_t i = 0; i < frames; ++i) {
        const double l = double(lfo[i]);
        if (s.envTrigger == EnvTrigger::Lfo && m_gate && lfoWrapped[i])
            m_env.gateOn(); // the LFO restarts the envelope every cycle

        m_pitch += (double(m_target) - m_pitch) * glideCoefficient;
        if (--m_driftCountdown <= 0) {
            m_driftTarget = random();
            m_driftCountdown = int(hostRate * (0.5 + 0.2 * random()));
        }
        m_drift += (m_driftTarget - m_drift) * driftSmoothing;

        const double semis = m_pitch + 12.0 * s.rangeOctaves + s.bendSemis + s.vibratoSemis * l
                             + 12.0 * s.vcoModOctaves * l
                             + s.analog * (kDriftSemis * m_drift + kNoteTuneSemis * m_noteTune);
        const double increment = noteHz(semis) / m_rate;

        const double envNow = m_env.level();
        double pwAmount = s.pw;
        if (s.pwmSource == 0)
            pwAmount *= 0.5 + 0.5 * l;
        else if (s.pwmSource == 2)
            pwAmount *= envNow;
        const double width = 0.5 - kMaxPulseNarrowing * pwAmount;

        const double target = s.cutoffOctaves + s.keyFollow * (m_pitch - 60.0) / 12.0
                              + s.vcfEnvOctaves * envNow * velocityGain + s.vcfModOctaves * l + s.bendOctaves
                              + s.analog * kNoteCutoffOctaves * m_noteCutoff;
        if (m_cutoff < 0.0)
            m_cutoff = std::min(target, nyquistOctaves);
        if (m_gain < 0.0)
            m_gain = targetGain;
        m_cutoff += (std::min(target, nyquistOctaves) - m_cutoff) * smoothing * 2.0;
        m_gain += (targetGain - m_gain) * smoothing * 2.0;
        m_filter.set(std::exp2(m_cutoff), s.resonance, m_rate);

        for (int k = 0; k < 2; ++k) {
            const double saw = m_saw.next(increment, 0.5);
            const double pulse = m_pulse.next(increment, width);
            const double sub1 = m_sub1.next(increment * 0.5, 0.5);
            const double sub2 = m_sub2.next(increment * 0.25, kSubPulseWidth);
            m_noise ^= m_noise << 13;
            m_noise ^= m_noise >> 17;
            m_noise ^= m_noise << 5;
            const double noise = double(std::int32_t(m_noise)) / 2147483648.0;
            const double sub = s.subType == SubType::OneOctave ? sub1 : sub2;
            const double mixed = kMixGain * (s.pulse * pulse + s.saw * saw + s.sub * sub + s.noise * noise);
            const float filtered = m_filter.process(float(mixed));

            const double env = m_env.next();
            m_gateAmp = m_gate ? std::min(1.0, m_gateAmp + m_gateStep) : std::max(0.0, m_gateAmp - m_gateStep);
            const double amp = s.vcaGate ? m_gateAmp : env;
            out2x[2 * i + std::uint32_t(k)] += float(double(filtered) * amp * m_gain);
        }
        if (s.vcaGate && !m_gate && m_gateAmp <= 0.0)
            m_env.kill(); // the VCA has closed: the envelope's tail is silent
        if (!active())
            break;
    }
}

} // namespace kobalt
