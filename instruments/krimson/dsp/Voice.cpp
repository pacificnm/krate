#include "krimson/dsp/Voice.h"

#include <algorithm>
#include <cmath>

namespace krimson {

namespace {
constexpr double kFmDepth = 3.0;          // OSC 2 at full scales OSC 1's frequency by 1 +/- 3 (FM 100 %)
constexpr double kMenvPitchSemis = 48.0;  // mod envelope on OSC 2 pitch, at +/-100 %
constexpr double kLfoPitchSemis = 12.0;   // LFO 1 on pitch at 100 % (amount squared: fine at low settings)
constexpr double kLfoFilterOctaves = 4.0;
constexpr double kLfo2FilterOctaves = 2.0;
constexpr double kLfo2PitchSemis = 12.0;
constexpr double kMaxNarrowing = 0.45;    // pulse width 50 % .. 5 %
constexpr double kMixGain = 0.5;
constexpr double kSmoothingSeconds = 0.003;
constexpr double kStealFadeSeconds = 0.002;

double noteHz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
} // namespace

void Voice::prepare(double sampleRate2x) noexcept
{
    m_rate = sampleRate2x;
    m_fenv.setSampleRate(sampleRate2x);
    m_aenv.setSampleRate(sampleRate2x);
    m_menv.setSampleRate(sampleRate2x);
    m_fadeStep = 1.0 / (kStealFadeSeconds * sampleRate2x);
    kill();
}

void Voice::setSpread(double detuneSemis, double pan) noexcept
{
    m_detune = detuneSemis;
    m_pan = std::clamp(pan, -1.0, 1.0);
}

void Voice::kill() noexcept
{
    m_fenv.kill();
    m_aenv.kill();
    m_menv.kill();
    m_gate = false;
    m_filter.reset();
    m_cutoff = -1.0;
    m_stealing = false;
    m_fade = 1.0;
}

void Voice::start(int note, float velocity, double glideFrom) noexcept
{
    m_note = note;
    m_pitch = glideFrom >= 0.0 ? glideFrom : double(note);
    m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    m_gate = true;
    m_osc1.reset();
    m_osc2.reset();
    m_filter.reset();
    m_lfo1.reset();
    m_cutoff = -1.0;
    m_fenv.kill();
    m_aenv.kill();
    m_menv.kill();
    m_fenv.gateOn();
    m_aenv.gateOn();
    m_menv.gateOn();
}

void Voice::noteOn(int note, float velocity, bool retrigger, double glideFrom) noexcept
{
    if (m_stealing) { // still fading out: the new note replaces the one waiting
        m_nextNote = note;
        m_nextVelocity = velocity;
        m_nextGlideFrom = glideFrom;
        return;
    }
    if (!m_aenv.active()) {
        start(note, velocity, glideFrom);
        return;
    }
    // A sounding voice: its pitch glides on from where it is (when glide is set).
    if (glideFrom < 0.0)
        m_pitch = note;
    m_note = note;
    m_velocity = std::clamp(double(velocity), 0.0, 1.0);
    m_gate = true;
    if (retrigger) {
        m_fenv.gateOn();
        m_aenv.gateOn();
        m_menv.kill(); // the mod envelope always starts from zero
        m_menv.gateOn();
    }
}

void Voice::steal(int note, float velocity, double glideFrom) noexcept
{
    if (!m_aenv.active()) {
        start(note, velocity, glideFrom);
        return;
    }
    m_stealing = true;
    m_nextNote = note;
    m_nextVelocity = velocity;
    m_nextGlideFrom = glideFrom;
    m_gate = true;
}

void Voice::noteOff() noexcept
{
    if (m_stealing)
        m_nextNote = -1; // released before it started: fade out and stay silent
    m_gate = false;
    m_fenv.gateOff();
    m_aenv.gateOff();
}

void Voice::render(float *outL2x, float *outR2x, std::uint32_t frames, const float *lfo2, const float *vibrato,
                   const SlotSettings &s) noexcept
{
    if (!active())
        return;
    m_osc1.setWave(s.osc1Wave);
    m_osc2.setWave(s.osc2Wave);
    m_fenv.setTimes(s.fenv);
    m_aenv.setTimes(s.aenv);
    Envelope::Times menv = s.menv;
    menv.sustain = 0.0;
    menv.release = menv.decay;
    m_menv.setTimes(menv);

    const double hostRate = m_rate * 0.5;
    const double smoothing = 1.0 - std::exp(-1.0 / (kSmoothingSeconds * hostRate));
    const double nyquist = std::log2(0.42 * m_rate);
    const double g1 = std::min(1.0, 2.0 * (1.0 - s.mix));
    const double g2 = std::min(1.0, 2.0 * s.mix);
    const double glide = s.glideSeconds > 0.0 ? 1.0 - std::exp(-1.0 / (s.glideSeconds * hostRate)) : 1.0;
    const double pan = std::clamp(m_pan + s.slotPan, -1.0, 1.0);
    const double panL = std::min(1.0, 1.0 - pan) * s.slotGain;
    const double panR = std::min(1.0, 1.0 + pan) * s.slotGain;

    for (std::uint32_t i = 0; i < frames; ++i) {
        // Control rate: once per host frame (a stolen voice may start its note mid-block).
        const double velocityGain = 1.0 - s.ampVelocity * (1.0 - m_velocity);
        const double envScale = 1.0 - s.fenvVelocity * (1.0 - m_velocity);
        const double l1 = m_lfo1.next(s.lfo1Hz, s.lfo1Wave, hostRate) * s.lfo1Amount;
        const double l2 = double(lfo2[i]) * s.lfo2Amount;
        const double menvNow = m_menv.level() * s.menvAmount;

        m_pitch += (m_note - m_pitch) * glide;
        double pitch1 = m_pitch + m_detune + s.bendSemis + s.vibratoSemis * double(vibrato[i]);
        const double lfoPitch = l1 * std::abs(s.lfo1Amount) * kLfoPitchSemis; // amount squared
        if (s.lfo1Dest == 0)
            pitch1 += lfoPitch;
        double pitch2 = (s.osc2Kbd ? pitch1 : 60.0 + (s.lfo1Dest == 0 ? lfoPitch : 0.0)) + s.osc2Semis;
        if (s.lfo1Dest == 1)
            pitch2 += lfoPitch;
        if (s.menvDest == 0)
            pitch2 += kMenvPitchSemis * menvNow;
        if (s.lfo2Dest == 2)
            pitch2 += kLfo2PitchSemis * l2;

        double fm = s.fm + (s.menvDest == 1 ? menvNow : 0.0) + (s.lfo1Dest == 4 ? l1 : 0.0);
        fm = std::clamp(fm, 0.0, 1.0);
        double pw = s.pw + (s.menvDest == 2 ? menvNow : 0.0) + (s.lfo1Dest == 3 ? 0.5 * l1 : 0.0);
        const double width = 0.5 - kMaxNarrowing * std::clamp(pw, 0.0, 1.0);

        const double inc1 = noteHz(pitch1) / m_rate;
        const double inc2 = noteHz(pitch2) / m_rate;

        double cutoff = s.cutoffOctaves + s.kbdTrack * (m_note - 60) / 12.0
                        + s.fenvOctaves * envScale * m_fenv.level();
        if (s.lfo1Dest == 2)
            cutoff += kLfoFilterOctaves * l1;
        if (s.lfo2Dest == 1)
            cutoff += kLfo2FilterOctaves * l2;
        cutoff = std::min(cutoff, nyquist);
        if (m_cutoff < 0.0)
            m_cutoff = cutoff;
        m_cutoff += (cutoff - m_cutoff) * smoothing;
        m_filter.set(std::exp2(m_cutoff), s.res, s.drive, m_rate);
        const double tremolo = s.lfo2Dest == 0 ? 1.0 - 0.5 * s.lfo2Amount * (1.0 - double(lfo2[i])) : 1.0;

        for (int k = 0; k < 2; ++k) {
            // OSC 1 runs first, frequency-modulated by OSC 2's last sample; then
            // OSC 2, reset (sync) at the instant OSC 1 wrapped in this sample.
            const double fmFactor = 1.0 + kFmDepth * fm * m_osc2Value;
            const float o1 = m_osc1.next(std::max(0.0, inc1 * fmFactor), width);
            const float o2 = m_osc2.next(inc2, width, s.sync && m_osc1.wrapped() ? m_osc1.wrapT() : -1.0);
            m_osc2Value = o2;
            const double second = s.ring ? double(o1) * double(o2) : double(o2);
            const float mixed = float(kMixGain * (g1 * o1 + g2 * second));
            const float filtered = m_filter.process(mixed, s.filterType);
            m_fenv.next();
            m_menv.next();
            const double amp = m_aenv.next();
            double fade = 1.0;
            if (m_stealing) {
                m_fade -= m_fadeStep;
                fade = std::max(m_fade, 0.0);
            }
            const double out = double(filtered) * amp * s.gain * velocityGain * tremolo * fade;
            outL2x[2 * i + std::uint32_t(k)] += float(out * panL);
            outR2x[2 * i + std::uint32_t(k)] += float(out * panR);
        }
        if (m_stealing && m_fade <= 0.0) {
            // Faded out: the new note starts on the next frame.
            m_stealing = false;
            m_fade = 1.0;
            if (m_nextNote >= 0) {
                start(m_nextNote, m_nextVelocity, m_nextGlideFrom);
            } else {
                m_aenv.kill();
                m_fenv.kill();
                m_gate = false;
            }
        }
        if (!active())
            break;
    }
}

} // namespace krimson
