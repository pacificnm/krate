#pragma once

#include <cstdint>

#include "dsp/Envelope.h"
#include "dsp/BlepOscillator.h"
#include "kalliope/dsp/Filters.h"

namespace kalliope {

using krate::Envelope;
using krate::Oscillator;
using krate::Wave;

// Everything a voice needs from the panel, in DSP units, worked out once per
// block by the engine (docs/BRIEF.md#parameters, docs/DSP.md).
struct VoiceSettings
{
    Wave vco1Wave = Wave::Saw;
    Wave vco2Wave = Wave::Saw;
    double vco1Octave = 0.0;     // range: -1 (16') .. +2 (2')
    double vco2Octave = 0.0;
    bool vco2Low = false;        // sub-audio range
    double vco2FineSemis = 0.0;
    bool sync = false;
    double crossMod = 0.0;       // 0..1

    double modLfoOctaves = 0.0;  // LFO on pitch
    double modEnvOctaves = 0.0;  // ENV-1 on pitch
    bool modVco1 = true, modVco2 = true;
    double pwm = 0.0;            // 0..1
    int pwmSource = 1;           // 0 LFO, 1 manual, 2 ENV-1

    double mix = 0.5;            // 0 = VCO-1 .. 1 = VCO-2
    double hpfHz = 0.0;
    double cutoffOctaves = 10.0; // log2(Hz)
    double resonance = 0.0;      // ladder feedback k
    bool slope24 = true;
    double vcfEnvOctaves = 0.0;
    bool vcfEnv2 = false;        // ENV-2 instead of ENV-1
    double vcfLfoOctaves = 0.0;
    double keyFollow = 0.0;      // 0..1 of the note's pitch

    double level = 0.64;         // VCA gain
    double vcaLfo = 0.0;         // tremolo depth 0..1
    Envelope::Times env1, env2;
    bool envKeyFollow = false;
    double velocityAmount = 0.5;
    double lfoDelay = 0.0;       // seconds

    double bendSemis = 0.0;      // pitch bend now
    double vibratoSemis = 0.0;   // mod wheel: LFO on pitch, semitones at full LFO
};

// One voice, running at twice the host rate (docs/DSP.md#oversampling).
class Voice
{
public:
    void prepare(double sampleRate2x) noexcept;

    // retrigger false: legato, the envelopes keep going (Solo, Unison).
    void noteOn(int note, float velocity, bool retrigger) noexcept;
    // Take this sounding voice for a new note: fade it out over 2 ms, then start the note.
    void steal(int note, float velocity) noexcept;
    void noteOff() noexcept;
    void kill() noexcept;           // silence now (tests, reset)
    bool active() const noexcept { return m_env2.active() || m_stealing; }
    bool gate() const noexcept { return m_gate; }
    int note() const noexcept { return m_stealing ? m_nextNote : m_note; }
    double level() const noexcept { return m_env2.level(); }

    // Unison: this voice's detune, output gain and oscillator start phase
    // (0, 1, 0 for one voice per note).
    void setSpread(double detuneSemis, double gain, double phase) noexcept;

    // Adds 2 * frames samples (2x rate) to out2x. lfo: one value per host-rate frame.
    void render(float *out2x, std::uint32_t frames, const float *lfo, const VoiceSettings &s) noexcept;

private:
    void start(int note, float velocity) noexcept; // a fresh note on a silent voice

    double m_rate = 96000.0;
    Oscillator m_vco1, m_vco2;
    HighPass m_hpf;
    Ladder m_vcf;
    Envelope m_env1, m_env2;
    int m_note = 60;
    double m_velocity = 1.0;
    bool m_gate = false;
    double m_sinceOn = 0.0;  // seconds since note-on (LFO delay)
    double m_vco2Value = 0.0; // last VCO-2 output (cross mod)
    double m_cutoff = -1.0;  // smoothed cutoff, octaves; < 0 = take the next value as is
    double m_gain = -1.0;    // smoothed VCA level
    double m_detune = 0.0;   // semitones (unison)
    double m_spreadGain = 1.0;
    double m_startPhase = 0.0;
    bool m_stealing = false; // fading out before m_nextNote starts
    double m_fade = 1.0;
    double m_fadeStep = 0.0;
    int m_nextNote = 60;
    float m_nextVelocity = 1.0f;
};

} // namespace kalliope
