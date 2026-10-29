#pragma once

#include <array>

#include "dsp/Filters.h"
#include "dsp/Pedal.h"

namespace krunch {

// Compressor: a pedal compressor for sustain and evening out (docs/PEDALS.md).
// A peak detector with Attack and a 200 ms release drives a soft-kneed gain
// computer; Sustain lowers the threshold and raises the ratio together, with
// make-up gain following; Blend mixes the dry signal back in (keeping the
// pick's attack); Level sets the output.
class Compressor final : public Pedal
{
public:
    enum Knob : int { Sustain, Attack, Level, Blend };
    static const PedalInfo kInfo;

    Compressor() : Pedal(kInfo) {}

    // The gain change in dB for a level in dBFS at a Sustain setting (tests).
    static double gainDb(double levelDb, float sustain);

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    double m_rate = 48000.0;
    double m_envelope = 0.0; // the detected peak level, linear
    double m_release = 0.0;
};

// Graphic EQ: Bands peaking filters at fixed frequencies, each +-rangeDb, and
// an output Level (+-12 dB); all sliders, flat in the middle.
template<int Bands>
class GraphicEq final : public Pedal
{
public:
    static const PedalInfo kInfo;
    static const std::array<double, Bands> kFrequencies;
    static const double kRangeDb;
    static const double kQ;

    GraphicEq() : Pedal(kInfo) {}

    static double bandDb(float v) { return (double(v) - 0.5) * 2.0 * kRangeDb; }

protected:
    void prepareEffect(double sampleRate) noexcept override
    {
        m_rate = sampleRate;
        m_set.fill(-1.0f);
    }
    void resetEffect() noexcept override
    {
        for (Biquad &b : m_bands)
            b.reset();
    }
    void processEffect(float *buffer, std::uint32_t frames) noexcept override
    {
        for (int b = 0; b < Bands; ++b)
            if (const float v = knob(b); v != m_set[std::size_t(b)]) {
                if (m_set[std::size_t(b)] == 0.5f) // it was skipped while flat: start it clean
                    m_bands[std::size_t(b)].reset();
                m_set[std::size_t(b)] = v;
                m_bands[std::size_t(b)].setPeak(m_rate, kFrequencies[std::size_t(b)], kQ, bandDb(v));
            }
        const float level = float(std::pow(10.0, bandDb(knob(Bands)) * (12.0 / kRangeDb) / 20.0));
        for (std::uint32_t n = 0; n < frames; ++n) {
            float y = buffer[n];
            for (int b = 0; b < Bands; ++b)
                if (m_set[std::size_t(b)] != 0.5f) // a flat band changes nothing: skip it
                    y = m_bands[std::size_t(b)].process(y);
            buffer[n] = y * level;
        }
    }

private:
    double m_rate = 48000.0;
    std::array<Biquad, Bands> m_bands;
    std::array<float, Bands> m_set{};
};

using Eq5 = GraphicEq<5>;
using Eq10 = GraphicEq<10>;
template<> const PedalInfo Eq5::kInfo;
template<> const PedalInfo Eq10::kInfo;
template<> const std::array<double, 5> Eq5::kFrequencies;
template<> const std::array<double, 10> Eq10::kFrequencies;
template<> const double Eq5::kRangeDb;
template<> const double Eq10::kRangeDb;
template<> const double Eq5::kQ;
template<> const double Eq10::kQ;

} // namespace krunch
