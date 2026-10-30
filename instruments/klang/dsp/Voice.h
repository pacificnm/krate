#pragma once

#include <cstdint>

#include "klang/dsp/Sample.h"

namespace klang {

enum class PadMode : std::uint8_t { OneShot, Gate, Loop };
enum class FilterType : std::uint8_t { Off, LowPass, HighPass, BandPass };

// A pad's settings in DSP units, worked out once per block by the engine.
struct PadSettings
{
    double gain = 0.64;
    double panLeft = 0.7071, panRight = 0.7071; // equal power
    double tuneSemis = 0.0;
    double start = 0.0, end = 1.0; // fractions of the sample
    double attack = 0.0;           // seconds
    double decay = -1.0;           // seconds to -60 dB; < 0 = Full (no decay)
    PadMode mode = PadMode::OneShot;
    int syncBeats = 0;             // 0 = off
    bool reverse = false;
    int choke = 0;                 // 0 = none
    FilterType filter = FilterType::Off;
    double cutoffHz = 2000.0;
    double resonance = 0.0;        // 0..1
    double filterEnvOctaves = 0.0;
    double drive = 0.0;            // 0..1
    int bits = 0;                  // 0 = off
    int downsample = 1;
    double ring = 0.0, ringHz = 440.0;
    bool gate = false;
    double delaySend = 0.0, reverbSend = 0.0;
};

// Where a voice adds its sound: the dry mix and the two sends, stereo.
struct PadBuses
{
    float *left, *right, *delayLeft, *delayRight, *reverbLeft, *reverbRight;
};

// One hit of a pad (docs/BRIEF.md#sound-engine): reads the region forwards
// or backwards with cubic interpolation, at the pitch (and the tempo, with
// SYNC), loops with a crossfaded seam, through the envelope, the filter,
// drive, the bitcrusher, the ring modulator and the gate.
class PadVoice
{
public:
    void prepare(double hostRate) noexcept { m_rate = hostRate; kill(); }

    // pitchSemis: on top of the pad's Tune (a step's pitch, a chromatic key);
    // flip: reverse this hit (a step's reverse). note: the key that played it, -1 for a pad hit.
    void start(const Sample *sample, const PadSettings &s, float velocity, double pitchSemis, bool flip,
               int note) noexcept;
    void release() noexcept; // key up: Gate and Loop fade out over 20 ms (One shot plays on)
    void choke() noexcept;   // out over 3 ms (a new hit, the choke group)
    void kill() noexcept { m_sample = nullptr; }
    bool active() const noexcept { return m_sample != nullptr; }
    bool held() const noexcept { return active() && !m_released; }
    int note() const noexcept { return m_note; }
    bool looping() const noexcept { return m_loop; }
    double position() const noexcept { return m_pos; } // frames into the region, in play order (tests)

    void render(const PadBuses &out, const float *gate, std::uint32_t frames, const PadSettings &s,
                double bpm) noexcept;

private:
    float read(const float *channel, double position) const noexcept; // cubic, clamped to the sample

    double m_rate = 48000.0;
    const Sample *m_sample = nullptr;
    double m_regionStart = 0.0, m_regionLength = 0.0; // frames
    double m_pos = 0.0;            // frames into the region, in play order
    bool m_reverse = false;
    bool m_loop = false;
    bool m_oneShot = true;  // plays on when its key is let go
    double m_pitch = 0.0;          // semitones on top of Tune
    double m_velocity = 1.0;
    int m_note = -1;
    double m_attackDone = 0.0;     // 0..1 through the attack
    double m_env = 1.0;            // decay level
    bool m_released = false;
    double m_fade = 1.0, m_fadeStep = 0.0; // release or choke fade
    // Tone
    double m_ic1[2] = {0.0, 0.0}, m_ic2[2] = {0.0, 0.0}; // state-variable filter
    double m_a1 = 1.0, m_a2 = 0.0, m_a3 = 0.0, m_k = 2.0;
    int m_hold = 0;
    float m_held[2] = {0.0f, 0.0f};
    double m_ringPhase = 0.0;
};

} // namespace klang
