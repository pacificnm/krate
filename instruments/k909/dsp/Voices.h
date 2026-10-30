#pragma once

#include <array>
#include <cstdint>

#include "k909/dsp/Sample.h"

namespace k909 {

// The voices in panel and track order (docs/DSP.md#voices): the classic
// eleven, the industrial four (Metal, Clank, Noise, Boom), then the sample.
enum class DrumId : int { Bd, Sd, Lt, Mt, Ht, Rs, Cp, Ch, Oh, Cr, Rd, Me, Ck, Nz, Bm, Sm, Count };
static_assert(int(DrumId::Count) <= 16, "Hits are 16-bit masks");
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
    // BD: attack, sweep, bend time, tone. SD: tone, snappy, crack. CP: spread. Sample: start.
    std::array<float, 4> extra{0.5f, 0.5f, 0.5f, 0.5f};
};

// A voice's place in the mix (docs/DSP.md#mixer-and-effects), set every block.
struct Mix
{
    double pan = 0.0;    // -1 .. 1
    double drive = 0.0;  // 0..1
    double reverb = 0.0; // send, 0..1
    double delay = 0.0;  // send, 0..1
};

// The effect sends' buses for one render (frames samples each); written by add.
struct Sends
{
    float *reverbL, *reverbR, *delayL, *delayR;
};

class Voices
{
public:
    Voices() noexcept;

    void prepare(double sampleRate) noexcept;
    void trigger(DrumId drum, const Hit &hit) noexcept;
    void setMix(DrumId drum, const Mix &mix) noexcept;
    // The SAMPLE voice's sample (nullptr: none). A playing hit stops at once
    // (declicked): it must never read a sample the caller may then free.
    void setSample(const Sample *sample) noexcept;
    // Add `frames` samples of every sounding voice to left/right and its sends
    // to `sends` (none of them cleared).
    void render(float *left, float *right, const Sends &sends, std::uint32_t frames) noexcept;
    bool sounding(DrumId drum) const noexcept { return m_active[std::size_t(drum)]; }
    bool anySounding() const noexcept;

private:
    float tick(DrumId drum) noexcept;
    double drive(std::size_t d, double x) noexcept; // anti-aliased tanh drive, peak kept

    double m_rate = 48000.0;
    double m_declick = 0.0;  // per-sample residual decay (1 ms)
    std::array<bool, DrumCount> m_active{};
    std::array<float, DrumCount> m_last{};      // last output sample (after gain), for the residual
    std::array<double, DrumCount> m_residual{};
    std::array<double, DrumCount> m_gain{};
    std::array<float, DrumCount> m_panL{}, m_panR{};
    std::array<double, DrumCount> m_reverbSend{}, m_delaySend{};
    std::array<double, DrumCount> m_driveGain{};   // 0 = clean
    std::array<double, DrumCount> m_driveNorm{};   // 1 / tanh(gain)
    std::array<double, DrumCount> m_drivePrev{};   // last input, scaled by the gain
    std::array<double, DrumCount> m_driveIntegral{}; // its antiderivative

    // Bass drum
    Sine m_bdOsc;
    Env m_bdAmp, m_bdPitch, m_bdClick;
    Svf m_bdClickFilter;
    Noise m_bdNoise{0x1A2B3C4Du};
    double m_bdBase = 55.0, m_bdAttack = 0.5, m_bdSweep = 1.5, m_bdShape = 1.5, m_bdShapeNorm = 1.0; // tanh(m_bdShape)
    int m_bdPulse = 0;

    // Snare
    Sine m_sdOsc1, m_sdOsc2;
    Env m_sdBody, m_sdPitch, m_sdNoiseEnv;
    Svf m_sdHp, m_sdLp;
    Noise m_sdNoise{0x2B3C4D5Eu};
    double m_sdBase = 200.0, m_sdBodyMix = 1.0, m_sdNoiseMix = 0.5;
    Env m_sdCrackEnv;  // the crack: a short, bright noise snap
    Svf m_sdCrackHp;
    Noise m_sdCrackNoise{0x92A3B4C5u};
    double m_sdCrack = 0.0;

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
    int m_cpBurstLength = 528; // samples between the bursts (spread)
    double m_cpTailSeconds = 0.2;

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

    // Metal: struck modes (anvil, pipe, sheet), plus the strike's noise.
    static constexpr int kModes = 6;
    std::array<Sine, kModes> m_meModes{};
    std::array<Env, kModes> m_meEnv{};
    std::array<double, kModes> m_meHz{}, m_meAmp{};
    Env m_meStrike;
    Svf m_meStrikeBand;
    Noise m_meNoise{0xA3B4C5D6u};

    // Clank: FM, two modulators at inharmonic ratios, the index falling fast.
    double m_ckCarrier = 0.0, m_ckMod1 = 0.0, m_ckMod2 = 0.0; // phases 0..1
    double m_ckHz = 300.0, m_ckIndex = 4.0;
    Env m_ckAmp, m_ckIndexEnv, m_ckClick;
    Noise m_ckNoise{0xB4C5D6E7u};

    // Noise Stab: a resonant filter sweeping down through noise, with grit.
    Svf m_nzFilter;
    Env m_nzAmp, m_nzSweepEnv;
    Noise m_nzNoise{0xC5D6E7F8u};
    double m_nzHz = 1500.0, m_nzSweepOctaves = 2.0;

    // Sample: played from Start at its pitch (Tune), faded by Decay.
    const Sample *m_sample = nullptr;
    double m_smPosition = 0.0, m_smStep = 1.0; // source frames
    Env m_smAmp;
    bool m_smFade = false;     // Decay below the top: the envelope fades it
    int m_smFadeIn = 0;        // samples of fade-in left (a start inside the sample)
    int m_smFadeInLength = 96;

    // Boom: a long sub with a slow pitch drop, saturated.
    Sine m_bmOsc;
    Env m_bmAmp, m_bmPitch;
    double m_bmBase = 40.0;
};

} // namespace k909
