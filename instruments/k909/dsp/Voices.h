#pragma once

#include <array>
#include <cstdint>

namespace k909 {

// The eleven voices in panel and track order (docs/DSP.md#voices).
enum class DrumId : int { Bd, Sd, Lt, Mt, Ht, Rs, Cp, Ch, Oh, Cr, Rd, Count };
inline constexpr int DrumCount = int(DrumId::Count);

// ---- Building blocks (docs/DSP.md#building-blocks). Realtime-safe, no allocation.

// xorshift32 white noise in -1..1; one per voice, so voices are deterministic.
class Noise
{
public:
    explicit Noise(std::uint32_t seed) noexcept : m_state(seed) {}
    float next() noexcept
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return float(std::int32_t(m_state)) * (1.0f / 2147483648.0f);
    }

private:
    std::uint32_t m_state;
};

// Exponential decay: value *= factor each sample.
class Env
{
public:
    // Time for the level to fall 60 dB.
    void start(double level, double seconds, double sampleRate) noexcept;
    void setTime(double seconds, double sampleRate) noexcept; // keep level, change the rate
    float next() noexcept
    {
        const double v = m_level;
        m_level *= m_factor;
        return float(v);
    }
    double level() const noexcept { return m_level; }
    void stop() noexcept { m_level = 0.0; }

private:
    double m_level = 0.0;
    double m_factor = 0.0;
};

// Topology-preserving (trapezoidal) state-variable filter.
class Svf
{
public:
    struct Out
    {
        float lp, bp, hp;
    };
    void set(double hz, double q, double sampleRate) noexcept;
    Out process(float in) noexcept;
    void reset() noexcept { m_ic1 = m_ic2 = 0.0; }

private:
    double m_k = 1.0, m_a1 = 0.0, m_a2 = 0.0, m_a3 = 0.0;
    double m_ic1 = 0.0, m_ic2 = 0.0;
};

// Sine oscillator; frequency may change every sample.
class Sine
{
public:
    void reset() noexcept { m_phase = 0.0; }
    float next(double hz, double sampleRate) noexcept;
    double phase() const noexcept { return m_phase; }

private:
    double m_phase = 0.0;
};

// Six PolyBLEP squares at inharmonic ratios, summed (-1..1): hats and cymbals.
class MetalBank
{
public:
    void set(double baseHz, const std::array<double, 6> &ratios, double sampleRate) noexcept;
    void reset() noexcept;
    float next() noexcept;

private:
    std::array<double, 6> m_phase{};
    std::array<double, 6> m_increment{};
};

// ---- Voices. Each is monophonic; trigger() restarts it, carrying the old
// tail's last value as a 1 ms residual so a retrigger never clicks.

struct Hit
{
    double gain = 1.0;     // level^2 x velocity x accent gain
    bool accent = false;
    float tune = 0.5f;     // the voice's knobs, normalized, read at trigger time
    float decay = 0.5f;
    float extra1 = 0.5f;   // BD attack, SD tone
    float extra2 = 0.5f;   // SD snappy
};

class Voices
{
public:
    Voices() noexcept;

    void prepare(double sampleRate) noexcept;
    void trigger(DrumId drum, const Hit &hit) noexcept;
    // Add `frames` samples of every sounding voice to left/right (not cleared).
    void render(float *left, float *right, std::uint32_t frames) noexcept;
    bool sounding(DrumId drum) const noexcept { return m_active[std::size_t(drum)]; }
    bool anySounding() const noexcept;

private:
    float tick(DrumId drum) noexcept;

    double m_rate = 48000.0;
    double m_declick = 0.0;  // per-sample residual decay (1 ms)
    std::array<bool, DrumCount> m_active{};
    std::array<float, DrumCount> m_last{};      // last output sample (after gain), for the residual
    std::array<double, DrumCount> m_residual{};
    std::array<double, DrumCount> m_gain{};
    std::array<float, DrumCount> m_panL{}, m_panR{};

    // Bass drum
    Sine m_bdOsc;
    Env m_bdAmp, m_bdPitch, m_bdClick;
    Svf m_bdClickFilter;
    Noise m_bdNoise{0x1A2B3C4Du};
    double m_bdBase = 55.0, m_bdAttack = 0.5;
    int m_bdPulse = 0;

    // Snare
    Sine m_sdOsc1, m_sdOsc2;
    Env m_sdBody, m_sdPitch, m_sdNoiseEnv;
    Svf m_sdHp, m_sdLp;
    Noise m_sdNoise{0x2B3C4D5Eu};
    double m_sdBase = 200.0, m_sdBodyMix = 1.0, m_sdNoiseMix = 0.5;

    // Toms: low, mid, hi
    struct Tom
    {
        Sine osc;
        Env amp, pitch, noiseEnv;
        Svf noiseLp;
        Noise noise;
        double base = 100.0;
    };
    std::array<Tom, 3> m_toms{{{{}, {}, {}, {}, {}, Noise(0x3C4D5E6Fu)},
                              {{}, {}, {}, {}, {}, Noise(0x4D5E6F70u)},
                              {{}, {}, {}, {}, {}, Noise(0x5E6F7081u)}}};

    // Rim
    Svf m_rsLow, m_rsHigh, m_rsHp;
    Env m_rsAmp;
    int m_rsImpulse = 0;

    // Clap
    Svf m_cpBand;
    Env m_cpBurst, m_cpTail;
    Noise m_cpNoise{0x6F708192u};
    int m_cpTime = 0;

    // Hats
    MetalBank m_chBank, m_ohBank;
    Svf m_chBand, m_chHp, m_ohBand, m_ohHp;
    Env m_chAmp, m_ohAmp;
    Noise m_hatNoise{0x708192A3u};

    // Cymbals
    MetalBank m_crBank, m_rdBank;
    Svf m_crHp, m_rdHp, m_crLp, m_rdLp;
    Sine m_rdBell;
    Env m_crAmp, m_rdAmp, m_rdBellAmp;
    Noise m_cyNoise{0x8192A3B4u};
    double m_rdBellHz = 800.0;
};

} // namespace k909
