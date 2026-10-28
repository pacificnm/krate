#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>

namespace k303 {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.010; // docs/DSP.md#parameters
constexpr int kAccentVelocity = 100;              // MIDI notes at or above play accented
} // namespace

Engine::Engine()
{
    for (auto &m : m_ccMap)
        m.store(-1, std::memory_order_relaxed);
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

void Engine::setCcMapping(int cc, int param) noexcept
{
    if (cc >= 0 && cc < 128)
        m_ccMap[std::size_t(cc)].store(std::int16_t(param), std::memory_order_relaxed);
}

int Engine::ccMapping(int cc) const noexcept
{
    return (cc >= 0 && cc < 128) ? m_ccMap[std::size_t(cc)].load(std::memory_order_relaxed) : -1;
}

void Engine::handleMidi(const MidiEvent &event) noexcept
{
    if (event.size < 1)
        return;
    const int status = event.data[0] & 0xF0; // all channels (omni)
    const int d1 = event.size > 1 ? event.data[1] & 0x7F : 0;
    const int d2 = event.size > 2 ? event.data[2] & 0x7F : 0;

    switch (status) {
    case 0xB0: { // Control Change
        int param = m_learnTarget.load(std::memory_order_relaxed);
        if (param >= 0 && param < ParamCount) {
            // Bind here, so the rest of this same knob turn already moves the parameter.
            m_ccMap[std::size_t(d1)].store(std::int16_t(param), std::memory_order_relaxed);
            m_learnTarget.store(-1, std::memory_order_relaxed);
            m_events.push({EngineEvent::Kind::CcLearned, std::uint8_t(d1), std::uint8_t(param)});
        } else {
            param = m_ccMap[std::size_t(d1)].load(std::memory_order_relaxed);
        }
        if (param >= 0 && param < ParamCount) {
            const auto id = ParamId(param);
            // Switches flip at the halfway point; knobs follow the value.
            const float v = id == ParamId::Waveform ? (d2 >= 64 ? 1.0f : 0.0f) : float(d2) / 127.0f;
            m_params.setFromAudio(id, v);
        }
        break;
    }
    case 0x90:
        if (d2 > 0) {
            midiNoteOn(d1, d2);
            break;
        }
        [[fallthrough]]; // note on with velocity 0 is a note off
    case 0x80:
        midiNoteOff(d1);
        break;
    default:
        break; // clock and everything else: later milestones
    }
}

void Engine::midiNoteOn(int note, int velocity) noexcept
{
    m_events.push({EngineEvent::Kind::NoteOn, std::uint8_t(note), std::uint8_t(velocity)});
    if (m_sequencer.running())
        return;

    const bool legato = m_heldCount > 0;
    // Re-pressing a held key moves it to the top.
    for (int i = 0; i < m_heldCount; ++i) {
        if (m_held[std::size_t(i)] == note) {
            std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
            --m_heldCount;
            break;
        }
    }
    if (m_heldCount == int(m_held.size())) { // full: forget the oldest
        std::copy(m_held.begin() + 1, m_held.end(), m_held.begin());
        --m_heldCount;
    }
    m_held[std::size_t(m_heldCount++)] = std::uint8_t(note);

    const bool accent = velocity >= kAccentVelocity;
    if (legato)
        m_voice.slideTo(note, accent);
    else
        m_voice.noteOn(note, accent);
}

void Engine::midiNoteOff(int note) noexcept
{
    if (m_sequencer.running() || m_heldCount == 0)
        return;
    const bool wasSounding = m_held[std::size_t(m_heldCount - 1)] == note;
    for (int i = 0; i < m_heldCount; ++i) {
        if (m_held[std::size_t(i)] == note) {
            std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
            --m_heldCount;
            break;
        }
    }
    if (!wasSounding)
        return;
    if (m_heldCount > 0)
        m_voice.slideTo(m_held[std::size_t(m_heldCount - 1)], false); // back to the previous key
    else
        m_voice.gateOff();
}

void Engine::applyCommands() noexcept
{
    while (const auto command = m_commands.pop()) {
        switch (command->kind) {
        case Command::Kind::SetStep:
            m_sequencer.setStep(command->index, command->step);
            break;
        case Command::Kind::SetLength:
            m_sequencer.setLength(command->index);
            break;
        case Command::Kind::LoadPattern:
            m_sequencer.setPattern(command->pattern);
            break;
        case Command::Kind::LiveNoteOn:
            if (!m_sequencer.running())
                m_voice.noteOn(command->index, false);
            break;
        case Command::Kind::LiveNoteOff:
            if (!m_sequencer.running())
                m_voice.gateOff();
            break;
        }
    }
}

void Engine::process(float *outL, float *outR, std::uint32_t frames,
                     const MidiEvent *events, std::uint32_t eventCount) noexcept
{
    applyCommands();

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
    if (run && !m_sequencer.running()) {
        m_sequencer.start();
        m_heldCount = 0; // the sequencer owns the voice now
    }
    else if (!run && m_sequencer.running())
        handle(m_sequencer.stop());

    float peak = 0.0f;
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < frames; ++i) {
        while (nextEvent < eventCount && events[nextEvent].frame <= i)
            handleMidi(events[nextEvent++]);
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
