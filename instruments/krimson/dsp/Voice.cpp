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
// Analog at 100 % (docs/DSP.md#analog): each oscillator wanders up to +-12
// cents (new targets every 0.2 .. 0.6 s, reached over ~0.4 s), sits up to
// +-5 cents off for the whole note, starts anywhere in its cycle; the cutoff
// sits up to +-1/4 octave off per note.
constexpr double kDriftSemis = 0.12;
constexpr double kNoteOffsetSemis = 0.05;
constexpr double kCutoffOffsetOctaves = 0.25;
constexpr double kDriftSeconds = 0.4;

double noteHz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
} // namespace

SlotSettings blend(const SlotSettings &a, const SlotSettings &b, double t) noexcept
{
    if (t <= 0.0)
        return a;
    t = std::min(t, 1.0);
    const auto lin = [t](double x, double y) { return x + (y - x) * t; };
    const auto log = [t](double x, double y) { // times and rates: as their knobs move (exponentially)
        return x > 0.0 && y > 0.0 ? x * std::pow(y / x, t) : x + (y - x) * t;
    };
    const auto times = [&](const Envelope::Times &x, const Envelope::Times &y) {
        return Envelope::Times{log(x.attack, y.attack), log(x.decay, y.decay), lin(x.sustain, y.sustain),
                               log(x.release, y.release)};
    };
    SlotSettings s = a;
    s.osc2Semis = lin(a.osc2Semis, b.osc2Semis);
    s.pw = lin(a.pw, b.pw);
    s.mix = lin(a.mix, b.mix);
    s.fm = lin(a.fm, b.fm);
    s.menv = times(a.menv, b.menv);
    s.menvAmount = lin(a.menvAmount, b.menvAmount);
    s.lfo1Hz = log(a.lfo1Hz, b.lfo1Hz);
    s.lfo1Amount = lin(a.lfo1Amount, b.lfo1Amount);
    s.lfo2Amount = lin(a.lfo2Amount, b.lfo2Amount);
    s.cutoffOctaves = lin(a.cutoffOctaves, b.cutoffOctaves);
    s.res = lin(a.res, b.res);
    s.drive = lin(a.drive, b.drive);
    s.fenvOctaves = lin(a.fenvOctaves, b.fenvOctaves);
    s.fenvVelocity = lin(a.fenvVelocity, b.fenvVelocity);
    s.fenv = times(a.fenv, b.fenv);
    s.aenv = times(a.aenv, b.aenv);
    s.ampVelocity = lin(a.ampVelocity, b.ampVelocity);
    s.gain = lin(a.gain, b.gain);
    s.glideSeconds = lin(a.glideSeconds, b.glideSeconds);
    s.slotGain = lin(a.slotGain, b.slotGain);
    s.slotPan = lin(a.slotPan, b.slotPan);
    s.analog = lin(a.analog, b.analog);
    return s;
}

void Voice::prepare(double sampleRate2x) noexcept
{
    m_rate = sampleRate2x;
    m_fenv.setSampleRate(sampleRate2x);
    m_aenv.setSampleRate(sampleRate2x);
    m_menv.setSampleRate(sampleRate2x);
    m_fadeStep = 1.0 / (kStealFadeSeconds * sampleRate2x);
    kill();
}

void Voice::setSpread(double detuneSemis, double pan, double gain) noexcept
{
    m_detune = detuneSemis;
    m_pan = std::clamp(pan, -1.0, 1.0);
    m_spreadGain = gain;
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
    // Analog: where this note's oscillators start, and how far off it sits.
    m_freshNote = true;
    m_startPhase1 = 0.5 + 0.5 * random();
    m_startPhase2 = 0.5 + 0.5 * random();
    m_offset1 = random();
    m_offset2 = random();
    m_cutoffOffset = random();
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

    const double driftSmoothing = 1.0 - std::exp(-1.0 / (kDriftSeconds * hostRate));
    for (std::uint32_t i = 0; i < frames; ++i) {
        if (m_freshNote) { // a new note's oscillators start anywhere in their cycle (by Analog)
            m_freshNote = false;
            m_osc1.reset(m_startPhase1 * s.analog);
            m_osc2.reset(m_startPhase2 * s.analog);
        }
        if (--m_driftCountdown <= 0) { // new wander targets now and then
            m_driftTarget1 = random();
            m_driftTarget2 = random();
            m_driftCountdown = int(hostRate * (0.4 + 0.2 * random()));
        }
        m_drift1 += (m_driftTarget1 - m_drift1) * driftSmoothing;
        m_drift2 += (m_driftTarget2 - m_drift2) * driftSmoothing;
        const double analog1 = s.analog * (kDriftSemis * m_drift1 + kNoteOffsetSemis * m_offset1);
        const double analog2 = s.analog * (kDriftSemis * m_drift2 + kNoteOffsetSemis * m_offset2);
        // Control rate: once per host frame (a stolen voice may start its note mid-block).
        const double velocityGain = 1.0 - s.ampVelocity * (1.0 - m_velocity);
        const double envScale = 1.0 - s.fenvVelocity * (1.0 - m_velocity);
        const double l1 = m_lfo1.next(s.lfo1Hz, s.lfo1Wave, hostRate) * s.lfo1Amount;
        const double l2 = double(lfo2[i]) * s.lfo2Amount;
        const double menvNow = m_menv.level() * s.menvAmount;

        m_pitch += (m_note - m_pitch) * glide;
        double pitch1 = m_pitch + m_detune + s.bendSemis + s.vibratoSemis * double(vibrato[i]) + analog1;
        const double lfoPitch = l1 * std::abs(s.lfo1Amount) * kLfoPitchSemis; // amount squared
        if (s.lfo1Dest == 0)
            pitch1 += lfoPitch;
        double pitch2 = (s.osc2Kbd ? pitch1 - analog1 : 60.0 + (s.lfo1Dest == 0 ? lfoPitch : 0.0)) + s.osc2Semis + analog2;
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
                        + s.fenvOctaves * envScale * m_fenv.level() + s.analog * kCutoffOffsetOctaves * m_cutoffOffset;
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
            const double out = double(filtered) * amp * s.gain * velocityGain * tremolo * fade * m_spreadGain;
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
