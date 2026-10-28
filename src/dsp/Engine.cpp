#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>

namespace k303 {

namespace {
constexpr double kTestToneHz = 220.0;
constexpr float kTestToneLevel = 0.125f; // -18 dBFS before the volume knob
constexpr double kRunSmoothingSeconds = 0.005;
constexpr double kParamSmoothingSeconds = 0.010; // docs/DSP.md#parameters

float onePoleCoeff(double seconds, double sampleRate)
{
    return static_cast<float>(1.0 - std::exp(-1.0 / (seconds * sampleRate)));
}
} // namespace

Engine::Engine()
{
    // Start smoothed values at their targets so nothing glides at startup.
    m_semitones = units::tuningSemitones(m_params.get(ParamId::Tuning));
    m_volume = static_cast<float>(units::volumeGain(m_params.get(ParamId::Volume)));
    setSampleRate(m_sampleRate);
}

void Engine::setSampleRate(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    m_osc.setSampleRate(sampleRate);
    m_runCoeff = onePoleCoeff(kRunSmoothingSeconds, sampleRate);
    m_paramCoeff = onePoleCoeff(kParamSmoothingSeconds, sampleRate);
}

void Engine::process(float *outL, float *outR, std::uint32_t frames) noexcept
{
    const float runTarget = m_running.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    const double semitoneTarget = units::tuningSemitones(m_params.get(ParamId::Tuning));
    const float volumeTarget = static_cast<float>(units::volumeGain(m_params.get(ParamId::Volume)));
    m_osc.setShape(units::waveformIsSquare(m_params.get(ParamId::Waveform)) ? Oscillator::Shape::Square
                                                                             : Oscillator::Shape::Saw);
    float peak = 0.0f;

    for (std::uint32_t i = 0; i < frames; ++i) {
        m_runGain += (runTarget - m_runGain) * m_runCoeff;
        m_volume += (volumeTarget - m_volume) * m_paramCoeff;
        m_semitones += (semitoneTarget - m_semitones) * m_paramCoeff;
        m_osc.setFrequency(kTestToneHz * std::exp2(m_semitones / 12.0));

        const float s = m_osc.next() * kTestToneLevel * m_volume * m_runGain;
        outL[i] = s;
        outR[i] = s;
        peak = std::max(peak, std::fabs(s));
    }

    // Single writer (this thread); the GUI's exchange may drop one block's peak, which is harmless.
    if (peak > m_peak.load(std::memory_order_relaxed))
        m_peak.store(peak, std::memory_order_relaxed);
}

} // namespace k303
