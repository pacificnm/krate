#include "kalliope/dsp/Voice.h"

#include <algorithm>
#include <cmath>

namespace kalliope {

namespace {
constexpr double kCrossModDepth = 4.0;      // VCO-2 at full scales VCO-1's frequency by 1 +/- 4
constexpr double kLowRangeHzAtC2 = 0.3;     // VCO-2 Low
constexpr double kSmoothingSeconds = 0.004; // cutoff and level changes
constexpr double kMaxPulseNarrowing = 0.45; // width 0.5 down to 0.05
constexpr double kMixGain = 0.5;
constexpr double kStealFadeSeconds = 0.002;

double noteHz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
} // namespace

void Voice::prepare(double sampleRate2x) noexcept
{
    m_rate = sampleRate2x;
    m_env1.setSampleRate(sampleRate2x);
    m_env2.setSampleRate(sampleRate2x);
    m_fadeStep = 1.0 / (kStealFadeSeconds * sampleRate2x);
    kill();
}

void Voice::kill() noexcept
{
    m_env1.kill();
    m_env2.kill();
    m_gate = false;
    m_hpf.reset();
    m_vcf.reset();
    m_cutoff = -1.0;
    m_gain = -1.0;
    m_stealing = false;
    m_fade = 1.0;
}

void Voice::setSpread(double detuneSemis, double gain, double phase) noexcept
{
    m_detune = detuneSemis;
    m_spreadGain = gain;
    m_startPhase = phase;
}

void Voice::start(int note, float velocity) noexcept
{
    m_note = note;
    m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    m_gate = true;
    m_vco1.reset(m_startPhase);
    m_vco2.reset(m_startPhase);
    m_hpf.reset();
    m_vcf.reset();
    m_cutoff = -1.0;
    m_gain = -1.0;
    m_sinceOn = 0.0;
    m_env1.kill();
    m_env2.kill();
    m_env1.gateOn();
    m_env2.gateOn();
}

void Voice::steal(int note, float velocity) noexcept
{
    if (!m_env2.active()) {
        start(note, velocity);
        return;
    }
    m_stealing = true;
    m_nextNote = note;
    m_nextVelocity = velocity;
    m_gate = true;
}

void Voice::noteOn(int note, float velocity, bool retrigger) noexcept
{
    if (m_stealing) { // still fading out: the new note replaces the one waiting
        m_nextNote = note;
        m_nextVelocity = velocity;
        return;
    }
    if (!m_env2.active()) { // a fresh voice: start clean
        start(note, velocity);
        return;
    }
    // A sounding voice: same pitch path, envelopes from where they are.
    m_note = note;
    m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    m_gate = true;
    if (retrigger) {
        m_env1.gateOn();
        m_env2.gateOn();
    }
}

void Voice::noteOff() noexcept
{
    if (m_stealing) { // released before it started: let the fade finish, then stay silent
        m_nextNote = -1;
    }
    m_gate = false;
    m_env1.gateOff();
    m_env2.gateOff();
}

void Voice::render(float *out2x, std::uint32_t frames, const float *lfo, const VoiceSettings &s) noexcept
{
    if (!active())
        return;

    m_vco1.setWave(s.vco1Wave);
    m_vco2.setWave(s.vco2Wave);
    // Env key follow: up to half the times at the top of the keyboard.
    const double keyScale = s.envKeyFollow ? std::clamp(1.0 - (m_note - 36) / 96.0, 0.5, 1.0) : 1.0;
    m_env1.setTimes(s.env1, keyScale);
    m_env2.setTimes(s.env2, keyScale);
    m_hpf.set(s.hpfHz, m_rate);

    const double smoothing = 1.0 - std::exp(-1.0 / (kSmoothingSeconds * m_rate));
    const double nyquistOctaves = std::log2(0.42 * m_rate);
    const double dt = 1.0 / (m_rate * 0.5); // seconds per host frame

    for (std::uint32_t i = 0; i < frames; ++i) {
        // Per frame: a stolen voice can start its new note mid-block.
        const double velocityGain = 1.0 - s.velocityAmount * (1.0 - m_velocity);
        const double targetGain = s.level * velocityGain;
        const double baseCutoff = s.cutoffOctaves + s.keyFollow * (m_note - 60) / 12.0;
        if (m_cutoff < 0.0)
            m_cutoff = baseCutoff;
        if (m_gain < 0.0)
            m_gain = targetGain;

        // LFO, faded in after note-on by the LFO delay.
        const double fade = s.lfoDelay > 0.0 ? std::min(1.0, m_sinceOn / s.lfoDelay) : 1.0;
        const double l = double(lfo[i]) * fade;
        m_sinceOn += dt;

        // Pitch for this host frame (shared by its two 2x samples).
        const double env1Now = m_env1.level();
        const double modOct = s.modLfoOctaves * l + s.modEnvOctaves * env1Now;
        const double common = m_note + m_detune + s.bendSemis + s.vibratoSemis * double(lfo[i]);
        const double f1 = noteHz(common + 12.0 * (s.vco1Octave + (s.modVco1 ? modOct : 0.0)));
        const double f2 = s.vco2Low
                              ? kLowRangeHzAtC2 * std::exp2((common - 36.0 + s.vco2FineSemis) / 12.0
                                                            + (s.modVco2 ? modOct : 0.0))
                              : noteHz(common + s.vco2FineSemis + 12.0 * (s.vco2Octave + (s.modVco2 ? modOct : 0.0)));
        const double inc1 = f1 / m_rate;
        const double inc2 = f2 / m_rate;

        double pwmAmount = s.pwm;
        if (s.pwmSource == 0)
            pwmAmount *= 0.5 + 0.5 * l;
        else if (s.pwmSource == 2)
            pwmAmount *= env1Now;
        const double width = 0.5 - kMaxPulseNarrowing * pwmAmount;

        // Filter cutoff for this frame.
        const double envForVcf = s.vcfEnv2 ? m_env2.level() : env1Now;
        const double target = baseCutoff + s.vcfEnvOctaves * envForVcf * velocityGain + s.vcfLfoOctaves * l;
        m_cutoff += (std::min(target, nyquistOctaves) - m_cutoff) * smoothing * 2.0;
        m_gain += (targetGain - m_gain) * smoothing * 2.0;
        m_vcf.set(std::exp2(m_cutoff), s.resonance, m_rate);
        const double tremolo = 1.0 - s.vcaLfo * (0.5 - 0.5 * l);

        const double g1 = std::min(1.0, 2.0 * (1.0 - s.mix));
        const double g2 = std::min(1.0, 2.0 * s.mix);

        for (int k = 0; k < 2; ++k) {
            // Cross mod: VCO-2's last output bends VCO-1's frequency.
            const double fm = 1.0 + s.crossMod * kCrossModDepth * m_vco2Value;
            const float o1 = m_vco1.next(std::max(0.0, inc1 * fm), width);
            const float o2 = m_vco2.next(inc2, width, s.sync && m_vco1.wrapped() ? m_vco1.wrapT() : -1.0);
            m_vco2Value = o2;
            const float mixed = float(kMixGain * (g1 * o1 + g2 * o2));
            const float filtered = m_vcf.process(m_hpf.process(mixed), s.slope24);
            m_env1.next();
            const double amp = m_env2.next();
            double fade = 1.0;
            if (m_stealing) {
                m_fade -= m_fadeStep;
                fade = std::max(m_fade, 0.0);
            }
            out2x[2 * i + std::uint32_t(k)] += float(double(filtered) * amp * m_gain * tremolo * m_spreadGain * fade);
        }
        if (m_stealing && m_fade <= 0.0) {
            // Faded out: the new note starts on the next frame.
            m_stealing = false;
            m_fade = 1.0;
            if (m_nextNote >= 0) {
                start(m_nextNote, m_nextVelocity);
            } else {
                m_env1.kill();
                m_env2.kill();
                m_gate = false;
            }
        }
        if (!active())
            break;
    }
}

} // namespace kalliope
