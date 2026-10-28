#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace k909 {

namespace {
constexpr int kAccentNoteVelocity = 100; // MIDI velocity at and above this plays accented
constexpr double kVolumeSmoothingSeconds = 0.01;
constexpr float kStepVelocity = 0.8f;   // a sequenced step (docs/DSP.md#accent-and-velocity)
constexpr float kAccentVelocity = 1.0f; // an accented step, plus Total Accent

// Each voice's knobs: tune, level, decay, then the extras (docs/BRIEF.md#parameters).
// ParamId::Count = the voice has no such knob.
struct VoiceKnobs
{
    ParamId tune, level, decay, extra1, extra2;
};
constexpr ParamId kNone = ParamId::Count;
constexpr std::array<VoiceKnobs, DrumCount> kVoiceKnobs{{
    {ParamId::BdTune, ParamId::BdLevel, ParamId::BdDecay, ParamId::BdAttack, kNone},
    {ParamId::SdTune, ParamId::SdLevel, kNone, ParamId::SdTone, ParamId::SdSnappy},
    {ParamId::LtTune, ParamId::LtLevel, ParamId::LtDecay, kNone, kNone},
    {ParamId::MtTune, ParamId::MtLevel, ParamId::MtDecay, kNone, kNone},
    {ParamId::HtTune, ParamId::HtLevel, ParamId::HtDecay, kNone, kNone},
    {kNone, ParamId::RsLevel, kNone, kNone, kNone},
    {kNone, ParamId::CpLevel, kNone, kNone, kNone},
    {kNone, ParamId::ChLevel, ParamId::ChDecay, kNone, kNone},
    {kNone, ParamId::OhLevel, ParamId::OhDecay, kNone, kNone},
    {ParamId::CrTune, ParamId::CrLevel, kNone, kNone, kNone},
    {ParamId::RdTune, ParamId::RdLevel, kNone, kNone, kNone},
}};
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_voices.prepare(sampleRate);
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

DrumId Engine::drumForNote(int note) noexcept
{
    switch (note) {
    case 35: case 36: return DrumId::Bd;
    case 37: return DrumId::Rs;
    case 38: case 40: return DrumId::Sd;
    case 39: return DrumId::Cp;
    case 41: case 43: return DrumId::Lt;
    case 42: case 44: return DrumId::Ch;
    case 45: case 47: return DrumId::Mt;
    case 46: return DrumId::Oh;
    case 48: case 50: return DrumId::Ht;
    case 49: case 57: return DrumId::Cr;
    case 51: case 59: return DrumId::Rd;
    default: return DrumId::Count;
    }
}

void Engine::trigger(DrumId drum, float velocity, bool accent) noexcept
{
    if (drum == DrumId::Count)
        return;
    const VoiceKnobs &knobs = kVoiceKnobs[std::size_t(drum)];
    const auto knob = [this](ParamId id) { return id == kNone ? 0.5f : m_params.get(int(id)); };
    Hit hit;
    hit.accent = accent;
    const double accentGain = accent ? 1.0 + double(m_params.get(int(ParamId::Accent))) : 1.0; // up to +6 dB
    hit.gain = units::gain(knob(knobs.level)) * std::clamp(double(velocity), 0.0, 1.0) * accentGain;
    hit.tune = knob(knobs.tune);
    hit.decay = knob(knobs.decay);
    hit.extra1 = knob(knobs.extra1);
    hit.extra2 = knob(knobs.extra2);
    m_voices.trigger(drum, hit);
}

void Engine::apply(const Command &command) noexcept
{
    switch (command.kind) {
    case Command::Kind::Trigger:
        trigger(DrumId(command.drum), command.velocity, command.accent);
        break;
    case Command::Kind::SetStep:
        m_sequencer.setStep(command.drum, command.step, command.state);
        break;
    case Command::Kind::SetLength:
        m_sequencer.setLength(command.value);
        break;
    case Command::Kind::SetStepsPerBeat:
        m_sequencer.setStepsPerBeat(command.value);
        break;
    }
}

void Engine::play(const Hits &hits) noexcept
{
    for (int d = 0; d < DrumCount; ++d) {
        if (!(hits.on & (1u << d)))
            continue;
        const bool accent = (hits.accent & (1u << d)) != 0;
        trigger(DrumId(d), accent ? kAccentVelocity : kStepVelocity, accent);
    }
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event); // all channels (omni)
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            m_params.setFromAudio(r.param, r.value);
        break;
    case krate::MidiMessage::Type::NoteOn:
        // Velocity 1..127 -> 0.5..1.0, so soft pads still speak.
        trigger(drumForNote(m.data1), 0.5f + 0.5f * float(m.data2) / 127.0f, m.data2 >= kAccentNoteVelocity);
        break;
    case krate::MidiMessage::Type::NoteOff:
    case krate::MidiMessage::Type::ProgramChange:
    case krate::MidiMessage::Type::Other:
        break; // drums ring out; note-offs are ignored
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    krate::ScopedFlushDenormals noDenormals;
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    std::fill_n(left, block.frames, 0.0f);
    std::fill_n(right, block.frames, 0.0f);

    while (const auto command = m_commands.pop())
        apply(*command);

    m_sequencer.setShuffle(units::shuffleAmount(m_params.get(int(ParamId::Shuffle))));

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    if (transport.playing && (transport.restarted || !m_sequencer.running()))
        m_sequencer.start();
    else if (!transport.playing && m_sequencer.running())
        m_sequencer.stop(); // voices ring out
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;

    // Voices render in runs between the frames where something triggers:
    // MIDI notes and sequencer steps land on their exact sample.
    std::uint32_t done = 0;
    const auto renderTo = [&](std::uint32_t frame) {
        if (frame > done) {
            m_voices.render(left + done, right + done, frame - done);
            done = frame;
        }
    };
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame <= i) {
            renderTo(i);
            handleMidi(block.midi[nextEvent++]);
        }
        if (transport.playing) {
            const Hits hits = m_sequencer.tick(transport.beatAtBlockStart + double(i) * beatsPerSample);
            if (hits.on) {
                renderTo(i);
                play(hits);
            }
        }
    }
    while (nextEvent < block.midiCount) // events past the block's end (malformed): apply at the end
        handleMidi(block.midi[nextEvent++]);
    renderTo(block.frames);
    m_currentStep.store(m_sequencer.currentStep(), std::memory_order_relaxed);

    const double target = units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target; // first block: no fade-in
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
}

} // namespace k909
