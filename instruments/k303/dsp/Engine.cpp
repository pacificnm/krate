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
    m_volume = static_cast<float>(units::volumeGain(m_params.get(ParamId::Volume)));
    prepare(m_sampleRate, 0);
}

float Engine::param(int index) const noexcept
{
    return (index >= 0 && index < ParamCount) ? m_params.get(ParamId(index)) : 0.0f;
}

void Engine::setParam(int index, float normalized) noexcept
{
    if (index >= 0 && index < ParamCount)
        m_params.set(ParamId(index), normalized);
}

void Engine::setParamFromHost(int index, float normalized) noexcept
{
    if (index >= 0 && index < ParamCount)
        m_params.setFromAudio(ParamId(index), normalized);
}

void Engine::prepare(double sampleRate, std::uint32_t /*maxFrames: k303 renders per sample*/) noexcept
{
    m_sampleRate = sampleRate;
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

void Engine::handleMidi(const MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event); // all channels (omni)
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            m_params.setFromAudio(ParamId(r.param), r.value);
        break;
    case krate::MidiMessage::Type::NoteOn:
        midiNoteOn(m.data1, m.data2);
        break;
    case krate::MidiMessage::Type::NoteOff:
        midiNoteOff(m.data1);
        break;
    case krate::MidiMessage::Type::ProgramChange:
    case krate::MidiMessage::Type::Other:
        break; // program changes, clock and everything else: not used by k303
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

void Engine::process(const krate::AudioBlock &block) noexcept
{
    float *outL = block.outputs[0];
    float *outR = block.outputs[1];
    const std::uint32_t frames = block.frames;
    const MidiEvent *events = block.midi;
    const std::uint32_t eventCount = block.midiCount;

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

    m_sequencer.setStepsPerBeat(units::scaleStepsPerBeat(p.get(ParamId::Scale)));
    m_sequencer.setPlayMode(Sequencer::PlayMode(units::playMode(p.get(ParamId::PlayMode))));
    const float volumeTarget = static_cast<float>(units::volumeGain(p.get(ParamId::Volume)));

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    if (transport.playing && (transport.restarted || !m_sequencer.running())) {
        if (m_sequencer.running())
            handle(m_sequencer.stop());
        m_sequencer.start();
        m_heldCount = 0; // the sequencer owns the voice now
    } else if (!transport.playing && m_sequencer.running()) {
        handle(m_sequencer.stop());
    }
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;

    float peak = 0.0f;
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < frames; ++i) {
        while (nextEvent < eventCount && events[nextEvent].frame <= i)
            handleMidi(events[nextEvent++]);
        if (transport.playing)
            handle(m_sequencer.tick(transport.beatAtBlockStart + double(i) * beatsPerSample));
        m_volume += (volumeTarget - m_volume) * m_volumeCoeff;
        const float s = m_voice.next() * m_volume; // the host's master stage soft-clips
        outL[i] = s;
        outR[i] = s;
        peak = std::max(peak, std::fabs(s));
    }

    m_currentStep.store(m_sequencer.currentStep(), std::memory_order_relaxed);
    // Lit for the first step of each beat: the beat's fractional part is within one step.
    const double lastBeat = transport.beatAtBlockStart + double(frames ? frames - 1 : 0) * beatsPerSample;
    m_onBeat.store(m_sequencer.running()
                       && lastBeat - std::floor(lastBeat) < 1.0 / m_sequencer.stepsPerBeat(),
                   std::memory_order_relaxed);
    // Single writer (this thread); the GUI's exchange may drop one block's peak, which is harmless.
    if (peak > m_peak.load(std::memory_order_relaxed))
        m_peak.store(peak, std::memory_order_relaxed);
}

} // namespace k303
