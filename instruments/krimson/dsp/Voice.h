#pragma once

#include <cstdint>

#include "dsp/BlepOscillator.h"
#include "dsp/Envelope.h"
#include "dsp/Lfo.h"
#include "krimson/dsp/Filter.h"

namespace krimson {

using krate::Envelope;
using krate::Lfo;
using krate::Oscillator;
using krate::Wave;

// One slot's sound in DSP units, worked out once per block (docs/BRIEF.md, docs/DSP.md).
struct SlotSettings
{
    Wave osc1Wave = Wave::Saw;
    Wave osc2Wave = Wave::Saw;
    double osc2Semis = 0.0;  // semitones + fine
    bool osc2Kbd = true;     // false: OSC 2 stays at middle C (+ semitones)
    double pw = 0.0;         // 0..1: width 50 % .. 5 %
    double mix = 0.5;        // 0 = OSC 1 .. 1 = OSC 2
    double fm = 0.0;         // 0..1
    bool sync = false;
    bool ring = false;

    Envelope::Times menv;    // attack / decay (sustain 0)
    double menvAmount = 0.0; // -1..1
    int menvDest = 0;        // 0 OSC 2 pitch, 1 FM, 2 pulse width

    double lfo1Hz = 2.0;
    Lfo::Wave lfo1Wave = Lfo::Wave::Triangle;
    double lfo1Amount = 0.0;
    int lfo1Dest = 0;        // 0 OSC 1+2 pitch, 1 OSC 2 pitch, 2 filter, 3 pulse width, 4 FM
    double lfo2Amount = 0.0;
    int lfo2Dest = 0;        // 0 amp, 1 filter, 2 OSC 2 pitch

    MultiFilter::Type filterType = MultiFilter::Type::Lp12;
    double cutoffOctaves = 10.0; // log2(Hz)
    double res = 0.0;
    double drive = 0.0;
    double fenvOctaves = 0.0;
    double fenvVelocity = 0.0; // 0..1: how much velocity scales the filter envelope
    double kbdTrack = 0.0;     // 0, 1/3, 2/3, 1
    Envelope::Times fenv, aenv;
    double ampVelocity = 0.5;
    double gain = 0.64;

    double glideSeconds = 0.0; // time constant of the pitch glide (0 = none)

    double bendSemis = 0.0;
    double vibratoSemis = 0.0; // depth now (wheel to Vibrato)

    double slotGain = 0.64;    // the slot's level
    double slotPan = 0.0;      // -1..1
    double analog = 0.0;       // 0..1: drift and spread between notes and oscillators (docs/DSP.md#analog)
};

// Morph (docs/DSP.md#morph): a slot's sound between its panel settings (t = 0)
// and its morph targets (t = 1). Continuous quantities blend (times and rates
// in the log domain, so they move as their knobs do); the rest come from `a`.
SlotSettings blend(const SlotSettings &a, const SlotSettings &b, double t) noexcept;

// One voice at twice the host rate (docs/DSP.md).
class Voice
{
public:
    void prepare(double sampleRate2x) noexcept;
    // retrigger false: legato (envelopes go on). glideFrom: the note the pitch
    // glides from (< 0: start on the note).
    void noteOn(int note, float velocity, bool retrigger, double glideFrom = -1.0) noexcept;
    // Take this sounding voice for a new note: fade out over 2 ms, then start it.
    void steal(int note, float velocity, double glideFrom = -1.0) noexcept;
    void noteOff() noexcept;
    void kill() noexcept;
    bool active() const noexcept { return m_aenv.active() || m_stealing; }
    bool gate() const noexcept { return m_gate; }
    int note() const noexcept { return m_stealing ? m_nextNote : m_note; }
    double level() const noexcept { return m_aenv.level(); }
    double velocity() const noexcept { return m_velocity; } // 0..1 (morph by velocity)

    // Unison: detune (semitones), stereo position (-1..1) and level of this voice.
    void setSpread(double detuneSemis, double pan, double gain = 1.0) noexcept;
    // Its own random sequence (analog drift): a different seed per voice, so
    // renders are the same every time.
    void setSeed(std::uint32_t seed) noexcept { m_random = seed ? seed : 1u; }

    // Adds 2 * frames samples to outL2x / outR2x. lfo2 and vibrato: one value per host frame.
    void render(float *outL2x, float *outR2x, std::uint32_t frames, const float *lfo2, const float *vibrato,
                const SlotSettings &s) noexcept;

private:
    void start(int note, float velocity, double glideFrom) noexcept; // a fresh note on a silent voice
    double random() noexcept; // -1..1

    double m_rate = 96000.0;
    Oscillator m_osc1, m_osc2;
    MultiFilter m_filter;
    Envelope m_fenv, m_aenv, m_menv;
    Lfo m_lfo1;
    int m_note = 60;
    double m_velocity = 1.0;
    bool m_gate = false;
    double m_osc2Value = 0.0;
    double m_cutoff = -1.0; // smoothed, octaves
    double m_pitch = 60.0;  // the sounding pitch (glides toward m_note)
    double m_detune = 0.0;
    double m_pan = 0.0;     // unison position; the slot's pan adds to it
    double m_spreadGain = 1.0; // unison: each voice's share
    // Analog: drawn per note (-1..1, scaled by the slot's Analog when it plays).
    std::uint32_t m_random = 1;
    bool m_freshNote = false;  // its oscillators take their random start on the next frame
    double m_startPhase1 = 0.0, m_startPhase2 = 0.0;
    double m_offset1 = 0.0, m_offset2 = 0.0; // fixed detune of each oscillator for this note
    double m_cutoffOffset = 0.0;
    double m_drift1 = 0.0, m_drift2 = 0.0;   // slow wander, now
    double m_driftTarget1 = 0.0, m_driftTarget2 = 0.0;
    int m_driftCountdown = 0;                // frames until new wander targets
    bool m_stealing = false;
    double m_fade = 1.0, m_fadeStep = 0.0;
    int m_nextNote = 60;
    float m_nextVelocity = 1.0f;
    double m_nextGlideFrom = -1.0;
};

} // namespace krimson
