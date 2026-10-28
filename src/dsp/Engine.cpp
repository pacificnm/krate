#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>

namespace k303 {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.010; // docs/DSP.md#parameters
} // namespace

Engine::Engine()
{
    m_volume = static_cast<float>(units::volumeGain(m_params.get(ParamId::Volume)));
    setSampleRate(m_sampleRate);
}

void Engine::setSampleRate(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    m_sequencer.setSampleRate(sampleRate);
    m_voice.setSampleRate(sampleRate);
    m_volumeCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate)));
}

void Engine::handle(const SequencerEvent &event) noexcept
{
    switch (event.kind) {
    case SequencerEvent::Kind::NoteOn:
        m_voice.noteOn(event.note, event.accent);
        break;
    case SequencerEvent::Kind::SlideTo:
        m_voice.slideTo(event.note, event.accent);
        break;
    case SequencerEvent::Kind::GateOff:
        m_voice.gateOff();
        break;
    case SequencerEvent::Kind::None:
        break;
    }
}

void Engine::process(float *outL, float *outR, std::uint32_t frames) noexcept
{
    const ParamStore &p = m_params;

    VoiceParams vp;
    vp.cutoffHz = units::cutoffHz(p.get(ParamId::Cutoff));
    vp.resonance = p.get(ParamId::Resonance);
    vp.envModOctaves = units::envModOctaves(p.get(ParamId::EnvMod));
    vp.decaySeconds = units::decaySeconds(p.get(ParamId::Decay));
    vp.accent = p.get(ParamId::Accent);
    vp.square = units::waveformIsSquare(p.get(ParamId::Waveform));
    vp.tuningSemitones = units::tuningSemitones(p.get(ParamId::Tuning));
    m_voice.setParams(vp);

    m_sequencer.setTempo(units::tempoBpm(p.get(ParamId::Tempo)));
    const float volumeTarget = static_cast<float>(units::volumeGain(p.get(ParamId::Volume)));

    const bool run = m_running.load(std::memory_order_relaxed);
    if (run && !m_sequencer.running())
        m_sequencer.start();
    else if (!run && m_sequencer.running())
        handle(m_sequencer.stop());

    float peak = 0.0f;
    for (std::uint32_t i = 0; i < frames; ++i) {
        handle(m_sequencer.tick());
        m_volume += (volumeTarget - m_volume) * m_volumeCoeff;
        const float s = std::tanh(m_voice.next() * m_volume); // soft clip after volume
        outL[i] = s;
        outR[i] = s;
        peak = std::max(peak, std::fabs(s));
    }

    m_currentStep.store(m_sequencer.currentStep(), std::memory_order_relaxed);
    // Single writer (this thread); the GUI's exchange may drop one block's peak, which is harmless.
    if (peak > m_peak.load(std::memory_order_relaxed))
        m_peak.store(peak, std::memory_order_relaxed);
}

} // namespace k303
