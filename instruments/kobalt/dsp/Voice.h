#pragma once

#include <cstdint>

#include "dsp/BlepOscillator.h"
#include "dsp/Envelope.h"
#include "kobalt/dsp/Filter.h"

namespace kobalt {

using krate::Envelope;
using krate::Oscillator;

enum class EnvTrigger : std::uint8_t { GateTrig, Gate, Lfo };
enum class PortaMode : std::uint8_t { Off, Auto, On };
enum class SubType : std::uint8_t { OneOctave, TwoOctaves, TwoOctavesPulse };

// Everything the voice needs from the panel, in DSP units, worked out once per
// block by the engine (docs/BRIEF.md#parameters).
struct VoiceSettings
{
    double rangeOctaves = 0.0;   // 16' -1 .. 2' +2
    double vcoModOctaves = 0.0;  // LFO on pitch
    double pw = 0.0;             // 0..1: how far the width moves from 50 %
    int pwmSource = 1;           // 0 LFO, 1 manual, 2 envelope

    double pulse = 0.0, saw = 0.8, sub = 0.0, noise = 0.0; // source mixer levels
    SubType subType = SubType::OneOctave;

    double cutoffOctaves = 10.0; // log2(Hz)
    double resonance = 0.0;      // feedback k
    double vcfEnvOctaves = 0.0;
    double vcfModOctaves = 0.0;  // LFO on the cutoff
    double keyFollow = 0.0;      // 0..1 of the note's pitch

    bool vcaGate = false;        // the VCA follows the gate, not the envelope
    Envelope::Times env;
    EnvTrigger envTrigger = EnvTrigger::GateTrig;
    double portaSeconds = 0.0;
    PortaMode portaMode = PortaMode::Off;
    double velocityAmount = 0.25;
    double analog = 0.0;         // 0..1: drift and note-to-note spread

    double bendSemis = 0.0;      // pitch bend now
    double bendOctaves = 0.0;    // pitch bend on the cutoff now
    double vibratoSemis = 0.0;   // mod wheel: LFO on pitch, semitones at full LFO
};

// The LFO (docs/BRIEF.md#scope-v1): triangle, square, random (a new value
// each cycle), noise (a new value every frame). Output -1..1; wrapped() is
// true on the frame a cycle starts (the envelope's LFO trigger). Deterministic.
class Lfo
{
public:
    enum class Wave : std::uint8_t { Triangle, Square, Random, Noise };

    void reset() noexcept { m_phase = 0.0; m_held = 0.0; m_wrapped = false; }
    double next(double hz, Wave wave, double sampleRate) noexcept;
    bool wrapped() const noexcept { return m_wrapped; }

private:
    double random() noexcept; // -1..1

    double m_phase = 0.0;
    double m_held = 0.0;
    bool m_wrapped = false;
    std::uint32_t m_rng = 0x6C8E9CF5u;
};

// Kobalt's one voice, running at twice the host rate (docs/BRIEF.md#sound-engine).
class Voice
{
public:
    void prepare(double sampleRate2x) noexcept;

    // legato: another note was still sounding (its key held). It glides there
    // in Auto (and always in On); the envelope restarts unless the trigger is Gate.
    void noteOn(int note, float velocity, bool legato, const VoiceSettings &s) noexcept;
    void noteOff() noexcept;
    void kill() noexcept;    // silence now (tests, reset)
    bool active() const noexcept;
    bool gate() const noexcept { return m_gate; }
    int note() const noexcept { return m_target; }
    double pitch() const noexcept { return m_pitch; }   // semitones now (gliding)
    double envLevel() const noexcept { return m_env.level(); }

    // Adds 2 * frames samples (2x rate) to out2x. lfo: one value per host-rate
    // frame; lfoWrapped: a new LFO cycle starts on that frame.
    void render(float *out2x, std::uint32_t frames, const float *lfo, const bool *lfoWrapped,
                const VoiceSettings &s) noexcept;

private:
    double random() noexcept; // -1..1

    double m_rate = 96000.0;
    Oscillator m_saw, m_pulse, m_sub1, m_sub2; // advanced together: one phase
    Filter m_filter;
    Envelope m_env;
    std::uint32_t m_noise = 0x2545F491u;

    int m_target = 60;         // the note played
    double m_pitch = 60.0;     // where the pitch is (glides to m_target)
    bool m_glide = false;      // this note glides
    bool m_played = false;     // a note has played (portamento has somewhere to glide from)
    double m_velocity = 1.0;
    bool m_gate = false;
    double m_gateAmp = 0.0;    // the VCA in Gate mode
    double m_gateStep = 0.0;
    double m_cutoff = -1.0;    // smoothed cutoff, octaves; < 0 = take the next value as is
    double m_gain = -1.0;      // smoothed VCA level

    // Analog (scaled by VoiceSettings::analog as it plays).
    std::uint32_t m_random = 0x9E3779B9u;
    double m_noteTune = 0.0, m_noteCutoff = 0.0; // this note's parts, -1..1
    double m_drift = 0.0, m_driftTarget = 0.0;
    int m_driftCountdown = 0;
};

} // namespace kobalt
