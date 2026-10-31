#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>

namespace korrode {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.010; // docs/DSP.md#parameters
constexpr int kAccentVelocity = 100;              // MIDI notes at or above play accented
constexpr double kMaxDrive = 24.0;                // Drive 100 %: up to +28 dB into the tanh
constexpr double kDriveReference = 0.5;           // a typical voice peak: kept where it is by the drive
constexpr double kDriveTrim = 0.14;               // the rest of the loudness gained, taken back at full drive
constexpr double kLn2 = 0.6931471805599453;

double logCosh(double u) // tanh's antiderivative, without overflow
{
    const double a = std::fabs(u);
    return a + std::log1p(std::exp(-2.0 * a)) - kLn2;
}
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

void Engine::prepare(double sampleRate, std::uint32_t /*maxFrames: korrode renders per sample*/) noexcept
{
    m_sampleRate = sampleRate;
    m_voice.setSampleRate(sampleRate);
    m_delay.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_delayOn = m_reverbOn = false;
    m_drivePrev = m_driveIntegral = m_tone = 0.0;
    m_volumeCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate)));
}

double Engine::drive(double x) noexcept
{
    // First-order antiderivative anti-aliasing: the average of tanh over the
    // step from the last sample, (F(u) - F(u')) / (u - u'), F = ln cosh.
    const double u = m_driveGain * x;
    const double integral = logCosh(u);
    const double du = u - m_drivePrev;
    const double y = std::fabs(du) > 1e-6 ? (integral - m_driveIntegral) / du : std::tanh(0.5 * (u + m_drivePrev));
    m_drivePrev = u;
    m_driveIntegral = integral;
    return y * m_driveNorm;
}

void Engine::effects(float *left, float *right, std::uint32_t frames, double bpm) noexcept
{
    const ParamStore &p = m_params;
    const double delayLevel = p.get(ParamId::DelayLevel);
    const double reverbLevel = p.get(ParamId::ReverbLevel);
    // Each runs only while it is up, and starts empty when it comes back.
    if (delayLevel > 0.0 && !m_delayOn)
        m_delay.clear();
    if (reverbLevel > 0.0 && !m_reverbOn)
        m_reverb.clear();
    m_delayOn = delayLevel > 0.0;
    m_reverbOn = reverbLevel > 0.0;
    if (!m_delayOn && !m_reverbOn)
        return;
    const double seconds = units::delayBeats(p.get(ParamId::DelayTime)) * 60.0 / std::max(bpm, 1.0);
    const double feedback = units::delayFeedback(p.get(ParamId::DelayFeedback));
    const double size = p.get(ParamId::ReverbSize);
    const double side = 2.0 * p.get(ParamId::Width); // 0 = the effects in mono, 1 = as they are, 2 = wider

    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        std::copy_n(left, n, m_dryChunk.begin()); // the dry bass, mono (left = right)
        std::copy_n(left, n, m_delayL.begin());
        std::copy_n(left, n, m_delayR.begin());
        if (m_delayOn) {
            m_delay.process(m_delayL.data(), m_delayR.data(), n, seconds, 1.0, feedback);
            for (std::uint32_t i = 0; i < n; ++i) { // the echoes alone, at the Delay level
                m_delayL[i] = float(delayLevel * (m_delayL[i] - m_dryChunk[i]));
                m_delayR[i] = float(delayLevel * (m_delayR[i] - m_dryChunk[i]));
            }
        } else {
            std::fill_n(m_delayL.begin(), n, 0.0f);
            std::fill_n(m_delayR.begin(), n, 0.0f);
        }
        if (m_reverbOn) {
            // The reverb hears the bass and its echoes.
            for (std::uint32_t i = 0; i < n; ++i) {
                m_reverbL[i] = m_dryChunk[i] + m_delayL[i];
                m_reverbR[i] = m_dryChunk[i] + m_delayR[i];
            }
            m_reverb.process(m_reverbL.data(), m_reverbR.data(), n, size, reverbLevel);
            for (std::uint32_t i = 0; i < n; ++i) { // its wet part only
                m_reverbL[i] -= m_dryChunk[i] + m_delayL[i];
                m_reverbR[i] -= m_dryChunk[i] + m_delayR[i];
            }
        }
        for (std::uint32_t i = 0; i < n; ++i) {
            double wetL = m_delayL[i], wetR = m_delayR[i];
            if (m_reverbOn) {
                wetL += m_reverbL[i];
                wetR += m_reverbR[i];
            }
            const double mid = 0.5 * (wetL + wetR), s = 0.5 * (wetL - wetR) * side;
            left[i] = float(m_dryChunk[i] + mid + s);
            right[i] = float(m_dryChunk[i] + mid - s);
        }
        left += n;
        right += n;
        frames -= n;
    }
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
        break; // program changes, clock and everything else: not used by Korrode
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
    vp.sweepOctaves = units::sweepOctaves(p.get(ParamId::Sweep));
    vp.square = units::waveformIsSquare(p.get(ParamId::Waveform));
    vp.tuningSemitones = units::tuningSemitones(p.get(ParamId::Tuning));
    m_voice.setParams(vp);

    m_sequencer.setStepsPerBeat(units::scaleStepsPerBeat(p.get(ParamId::Scale)));
    m_sequencer.setPlayMode(Sequencer::PlayMode(units::playMode(p.get(ParamId::PlayMode))));
    const float volumeTarget = static_cast<float>(units::volumeGain(p.get(ParamId::Volume)));
    const double driveAmount = p.get(ParamId::Drive);
    const bool driven = driveAmount > 0.0;
    if (driven) {
        m_driveGain = 1.0 + kMaxDrive * driveAmount * driveAmount;
        // Peaks stay about where they were; the body squares off (louder), partly trimmed back.
        m_driveNorm = kDriveReference / std::tanh(kDriveReference * m_driveGain) * (1.0 - kDriveTrim * driveAmount);
        // Tone: a low-pass after the drive, 800 Hz (dark) .. 16 kHz (open).
        const double toneHz = 800.0 * std::pow(20.0, double(p.get(ParamId::Tone)));
        m_toneCoeff = 1.0 - std::exp(-2.0 * 3.141592653589793 * toneHz / m_sampleRate);
    }

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    const bool sequence = transport.playing && m_sequencerEnabled.load(std::memory_order_relaxed)
                          && !m_sequencerSuspended.load(std::memory_order_relaxed);
    if (sequence && (transport.restarted || !m_sequencer.running())) {
        if (m_sequencer.running())
            handle(m_sequencer.stop());
        if (transport.restarted || transport.beatAtBlockStart <= 0.0)
            m_sequencer.start();
        else // switched on while the transport plays: in time, from the next bar
            m_sequencer.startAt(krate::StepClock::nextBar(transport.beatAtBlockStart));
        m_heldCount = 0; // the sequencer owns the voice now
    } else if (!sequence && m_sequencer.running()) {
        handle(m_sequencer.stop());
    }
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;

    float peak = 0.0f;
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < frames; ++i) {
        while (nextEvent < eventCount && events[nextEvent].frame <= i)
            handleMidi(events[nextEvent++]);
        if (sequence)
            handle(m_sequencer.tick(transport.beatAtBlockStart + double(i) * beatsPerSample));
        double s = m_voice.next();
        if (driven) {
            s = drive(s);
            m_tone += (s - m_tone) * m_toneCoeff;
            s = m_tone;
        } else {
            m_drivePrev = m_driveIntegral = 0.0;
            m_tone = s;
        }
        outL[i] = float(s);
        outR[i] = float(s);
    }
    effects(outL, outR, frames, block.transport ? transport.bpm : units::tempoBpm(p.get(ParamId::Tempo)));
    for (std::uint32_t i = 0; i < frames; ++i) {
        m_volume += (volumeTarget - m_volume) * m_volumeCoeff;
        outL[i] *= m_volume; // the host's master stage soft-clips
        outR[i] *= m_volume;
        peak = std::max({peak, std::fabs(outL[i]), std::fabs(outR[i])});
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

} // namespace Korrode
