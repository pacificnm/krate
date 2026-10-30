#include "k909/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace k909 {

namespace {
constexpr int kAccentNoteVelocity = 100; // MIDI velocity at and above this plays accented
constexpr double kVolumeSmoothingSeconds = 0.01;
// -3 dB under the voices' trims: an accented kick and clap together at the
// default Volume stay a few dB under the host's soft clip.
constexpr double kOutputTrim = 0.7071;
constexpr float kStepVelocity = 0.8f;   // a sequenced step (docs/DSP.md#accent-and-velocity)
constexpr float kAccentVelocity = 1.0f; // an accented step, plus Total Accent

// Each voice's knobs: tune, level, decay, then the extras (docs/BRIEF.md#parameters).
// ParamId::Count = the voice has no such knob.
constexpr ParamId kNone = ParamId::Count;
struct VoiceKnobs
{
    ParamId tune, level, decay;
    std::array<ParamId, 4> extra{kNone, kNone, kNone, kNone};
};
constexpr std::array<VoiceKnobs, DrumCount> kVoiceKnobs{{
    {ParamId::BdTune, ParamId::BdLevel, ParamId::BdDecay, {ParamId::BdAttack, ParamId::BdSweep, ParamId::BdBend, ParamId::BdTone}},
    {ParamId::SdTune, ParamId::SdLevel, ParamId::SdDecay, {ParamId::SdTone, ParamId::SdSnappy, ParamId::SdCrack, kNone}},
    {ParamId::LtTune, ParamId::LtLevel, ParamId::LtDecay},
    {ParamId::MtTune, ParamId::MtLevel, ParamId::MtDecay},
    {ParamId::HtTune, ParamId::HtLevel, ParamId::HtDecay},
    {ParamId::RsTune, ParamId::RsLevel, ParamId::RsDecay},
    {ParamId::CpTune, ParamId::CpLevel, ParamId::CpDecay, {ParamId::CpSpread, kNone, kNone, kNone}},
    {ParamId::HhTune, ParamId::ChLevel, ParamId::ChDecay},
    {ParamId::HhTune, ParamId::OhLevel, ParamId::OhDecay},
    {ParamId::CrTune, ParamId::CrLevel, kNone},
    {ParamId::RdTune, ParamId::RdLevel, kNone},
    {ParamId::MeTune, ParamId::MeLevel, ParamId::MeDecay, {ParamId::MeMaterial, kNone, kNone, kNone}},
    {ParamId::CkTune, ParamId::CkLevel, ParamId::CkDecay, {ParamId::CkHard, kNone, kNone, kNone}},
    {ParamId::NzTone, ParamId::NzLevel, ParamId::NzDecay, {ParamId::NzSweep, kNone, kNone, kNone}},
    {ParamId::BmTune, ParamId::BmLevel, ParamId::BmDecay},
    {ParamId::SmTune, ParamId::SmLevel, ParamId::SmDecay, {ParamId::SmStart, kNone, kNone, kNone}},
}};
// The mixer's parameters are in DrumId order, one block per kind.
constexpr ParamId mixParam(ParamId first, int drum) { return ParamId(int(first) + drum); }

constexpr double kGateThreshold = 0.003;   // the reverb gate opens on a send above -50 dBFS
constexpr double kGatedDecay = 0.75;       // the reverb's Size while gated (~3.8 s: dense, then cut)
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_voices.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_delay.prepare(sampleRate);
    m_reverbOn = m_delayOn = false;
    m_gateGain = 0.0;
    m_gateHold = 0;
    m_gateAttack = 1.0 - std::exp(-1.0 / (0.001 * sampleRate));
    m_gateRelease = 1.0 - std::exp(-1.0 / (0.015 * sampleRate));
    m_compEnvelope = 0.0;
    m_compGain = 1.0;
    m_compSmoothing = 1.0 - std::exp(-1.0 / (0.005 * sampleRate));
    m_crushPhase = 1.0;
    m_crushL = m_crushR = 0.0f;
    m_flams.fill({});
    m_flamsPending = 0;
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

void Engine::setPattern(const Pattern &pattern, int slot) noexcept
{
    m_sequencer.setPattern(pattern, slot);
    m_playingSlot.store(slot, std::memory_order_relaxed);
    m_queuedSlot.store(-1, std::memory_order_relaxed);
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
    case 56: case 67: return DrumId::Me; // cowbell, high agogo
    case 54: case 69: return DrumId::Ck; // tambourine, cabasa
    case 52: case 70: return DrumId::Nz; // Chinese cymbal, maracas
    case 34: case 33: return DrumId::Bm; // below the GM kit: no clash
    case 60: case 61: return DrumId::Sm; // the bongos: middle C
    default: return DrumId::Count;
    }
}

void Engine::trigger(DrumId drum, float velocity, bool accent, float tuneLock, float decayLock) noexcept
{
    if (drum == DrumId::Count)
        return;
    const VoiceKnobs &knobs = kVoiceKnobs[std::size_t(drum)];
    const auto knob = [this](ParamId id) { return id == kNone ? 0.5f : m_params.get(int(id)); };
    Hit hit;
    hit.accent = accent;
    const double accentGain = accent ? 1.0 + double(m_params.get(int(ParamId::Accent))) : 1.0; // up to +6 dB
    hit.gain = units::gain(knob(knobs.level)) * std::clamp(double(velocity), 0.0, 1.0) * accentGain;
    hit.tune = tuneLock >= 0.0f ? tuneLock : knob(knobs.tune);
    hit.decay = decayLock >= 0.0f ? decayLock : knob(knobs.decay);
    for (std::size_t i = 0; i < hit.extra.size(); ++i)
        hit.extra[i] = knob(knobs.extra[i]);
    m_voices.trigger(drum, hit);
}

void Engine::updateMix() noexcept
{
    for (int d = 0; d < DrumCount; ++d) {
        Mix mix;
        mix.pan = units::pan(m_params.get(int(mixParam(ParamId::BdPan, d))));
        mix.drive = m_params.get(int(mixParam(ParamId::BdDrive, d)));
        mix.reverb = m_params.get(int(mixParam(ParamId::BdRev, d)));
        mix.delay = m_params.get(int(mixParam(ParamId::BdDly, d)));
        m_voices.setMix(DrumId(d), mix);
    }
}

void Engine::renderVoices(float *left, float *right, std::uint32_t frames) noexcept
{
    bool reverbSends = false, delaySends = false;
    for (int d = 0; d < DrumCount; ++d) {
        reverbSends = reverbSends || m_params.get(int(mixParam(ParamId::BdRev, d))) > 0.0f;
        delaySends = delaySends || m_params.get(int(mixParam(ParamId::BdDly, d))) > 0.0f;
    }
    // An effect runs while any voice sends to it; switched on again it starts empty.
    if (reverbSends && !m_reverbOn)
        m_reverb.clear();
    if (delaySends && !m_delayOn)
        m_delay.clear();
    m_reverbOn = reverbSends;
    m_delayOn = delaySends;
    const bool gated = units::index(m_params.get(int(ParamId::RevGate)), 2) == 1;
    const double size = m_params.get(int(ParamId::RevSize));
    // Gated: the reverb rings long and dense, and the gate holds it open 80 .. 500 ms after each hit.
    const auto holdSamples = int(0.08 * std::pow(6.25, size) * m_sampleRate);
    const double feedback = std::min(0.9, 0.9 * double(m_params.get(int(ParamId::DlyFb))));

    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        std::fill_n(m_revL.begin(), n, 0.0f);
        std::fill_n(m_revR.begin(), n, 0.0f);
        std::fill_n(m_dlyL.begin(), n, 0.0f);
        std::fill_n(m_dlyR.begin(), n, 0.0f);
        m_voices.render(left, right, Sends{m_revL.data(), m_revR.data(), m_dlyL.data(), m_dlyR.data()}, n);
        if (m_reverbOn) {
            std::copy_n(m_revL.begin(), n, m_dryL.begin());
            std::copy_n(m_revR.begin(), n, m_dryR.begin());
            m_reverb.process(m_revL.data(), m_revR.data(), n, gated ? kGatedDecay : size, 1.0);
            for (std::uint32_t i = 0; i < n; ++i) {
                double gate = 1.0;
                if (gated) {
                    if (std::max(std::fabs(m_dryL[i]), std::fabs(m_dryR[i])) > kGateThreshold)
                        m_gateHold = holdSamples;
                    const double target = m_gateHold > 0 ? 1.0 : 0.0;
                    if (m_gateHold > 0)
                        --m_gateHold;
                    m_gateGain += (target - m_gateGain) * (target > m_gateGain ? m_gateAttack : m_gateRelease);
                    gate = m_gateGain;
                }
                left[i] = float(double(left[i]) + gate * (double(m_revL[i]) - m_dryL[i]));
                right[i] = float(double(right[i]) + gate * (double(m_revR[i]) - m_dryR[i]));
            }
        }
        if (m_delayOn) {
            std::copy_n(m_dlyL.begin(), n, m_dryL.begin());
            std::copy_n(m_dlyR.begin(), n, m_dryR.begin());
            m_delay.process(m_dlyL.data(), m_dlyR.data(), n, m_delaySeconds, 1.0, feedback);
            for (std::uint32_t i = 0; i < n; ++i) {
                left[i] += m_dlyL[i] - m_dryL[i];
                right[i] += m_dlyR[i] - m_dryR[i];
            }
        }
        left += n;
        right += n;
        frames -= n;
    }
}

void Engine::processBus(float *left, float *right, std::uint32_t frames) noexcept
{
    const double comp = m_params.get(int(ParamId::Comp));
    if (comp > 0.0) {
        // Feed-forward, stereo-linked: a peak detector (instant attack; release
        // 150 ms, gentle glue, down to 40 ms at full, pumping with the beat), the
        // gain computed in dB (threshold -6 .. -30 dB, ratio 1.5 .. 10), three
        // quarters of the reduction made up. Reduction applies at once (no overshoot on the
        // hits), recovery is smoothed.
        const double threshold = -6.0 - 24.0 * comp;
        const double slope = 1.0 - 1.0 / (1.5 + 8.5 * comp);
        const double makeup = 0.75 * -threshold * slope;
        const double release = 1.0 - std::exp(-1.0 / ((0.15 - 0.11 * comp) * m_sampleRate));
        for (std::uint32_t i = 0; i < frames; ++i) {
            const double peak = std::max(std::fabs(left[i]), std::fabs(right[i]));
            m_compEnvelope = peak > m_compEnvelope ? peak : m_compEnvelope + (peak - m_compEnvelope) * release;
            const double over = 20.0 * std::log10(std::max(m_compEnvelope, 1e-6)) - threshold;
            const double gainDb = makeup - (over > 0.0 ? over * slope : 0.0);
            const double target = std::pow(10.0, gainDb / 20.0);
            m_compGain = target < m_compGain ? target : m_compGain + (target - m_compGain) * m_compSmoothing;
            left[i] = float(left[i] * m_compGain);
            right[i] = float(right[i] * m_compGain);
        }
    } else {
        m_compEnvelope = 0.0;
        m_compGain = 1.0;
    }

    const double crush = m_params.get(int(ParamId::Crush));
    if (crush > 0.0) {
        // Bits 16 .. 4 and the rate down to 1/16 (a sample held for 1 .. 16 frames).
        const double steps = std::pow(2.0, 15.0 - 12.0 * crush);
        const double advance = 1.0 / (1.0 + 15.0 * crush * crush);
        for (std::uint32_t i = 0; i < frames; ++i) {
            m_crushPhase += advance;
            if (m_crushPhase >= 1.0) {
                m_crushPhase -= 1.0;
                m_crushL = float(std::round(double(left[i]) * steps) / steps);
                m_crushR = float(std::round(double(right[i]) * steps) / steps);
            }
            left[i] = m_crushL;
            right[i] = m_crushR;
        }
    } else {
        m_crushPhase = 1.0;
    }
}

void Engine::apply(const Command &command) noexcept
{
    switch (command.kind) {
    case Command::Kind::Trigger:
        trigger(DrumId(command.drum), command.velocity, command.accent);
        break;
    case Command::Kind::SetStep:
        m_sequencer.setStep(command.slot, command.drum, command.step, command.state);
        break;
    case Command::Kind::SetLength:
        m_sequencer.setLength(command.slot, command.value);
        break;
    case Command::Kind::SetStepsPerBeat:
        m_sequencer.setStepsPerBeat(command.slot, command.value);
        break;
    case Command::Kind::SetOrder:
        m_sequencer.setOrder(command.slot, command.value);
        break;
    case Command::Kind::SetStepDetail:
        m_sequencer.setStepDetail(command.slot, command.drum, command.step, command.value, command.tune, command.decay);
        break;
    }
}

void Engine::play(const Hits &hits) noexcept
{
    for (int d = 0; d < DrumCount; ++d) {
        if (!(hits.on & (1u << d)))
            continue;
        const auto i = std::size_t(d);
        const bool accent = (hits.accent & (1u << d)) != 0;
        const float velocity = hits.velocity[i] > 0 ? float(hits.velocity[i]) / 127.0f
                                                    : accent ? kAccentVelocity : kStepVelocity;
        const float tune = hits.tune[i] > 0 ? lockValue(hits.tune[i]) : -1.0f;
        const float decay = hits.decay[i] > 0 ? lockValue(hits.decay[i]) : -1.0f;
        if (hits.flam & (1u << d)) {
            // The grace hit now, the main one kFlamSeconds later.
            trigger(DrumId(d), kFlamGrace * velocity, false, tune, decay);
            PendingFlam &flam = m_flams[i];
            if (flam.frames == 0)
                ++m_flamsPending;
            flam = {std::max(1, int(kFlamSeconds * m_sampleRate)), velocity, tune, decay, accent};
            continue;
        }
        trigger(DrumId(d), velocity, accent, tune, decay);
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

    // Whole patterns first, so edits that follow them in time land on them.
    while (const auto load = m_patternLoads.pop()) {
        if (load->replace)
            m_sequencer.replacePattern(load->pattern, load->slot);
        else
            m_sequencer.queuePattern(load->pattern, load->slot);
    }
    while (const auto command = m_commands.pop())
        apply(*command);

    m_sequencer.setShuffle(units::shuffleAmount(m_params.get(int(ParamId::Shuffle))));
    updateMix();
    m_voices.setSample(m_pendingSample.load(std::memory_order_acquire));

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    const bool sequence = transport.playing && m_sequencerEnabled.load(std::memory_order_relaxed)
                          && !m_sequencerSuspended.load(std::memory_order_relaxed);
    if (sequence && (transport.restarted || !m_sequencer.running())) {
        if (transport.restarted || transport.beatAtBlockStart <= 0.0)
            m_sequencer.start();
        else // switched on while the transport plays: in time, from the next bar
            m_sequencer.startAt(krate::StepClock::nextBar(transport.beatAtBlockStart));
    }
    else if (!sequence && m_sequencer.running())
        m_sequencer.stop(); // voices ring out
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;
    const double bpm = block.transport ? transport.bpm : units::tempoBpm(m_params.get(int(ParamId::Tempo)));
    m_delaySeconds = units::delayBeats(m_params.get(int(ParamId::DlyTime))) * 60.0 / std::max(bpm, 1.0);

    // Voices render in runs between the frames where something triggers:
    // MIDI notes and sequencer steps land on their exact sample.
    std::uint32_t done = 0;
    const auto renderTo = [&](std::uint32_t frame) {
        if (frame > done) {
            renderVoices(left + done, right + done, frame - done);
            done = frame;
        }
    };
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame <= i) {
            renderTo(i);
            handleMidi(block.midi[nextEvent++]);
        }
        if (m_flamsPending > 0)
            for (int d = 0; d < DrumCount; ++d) {
                PendingFlam &flam = m_flams[std::size_t(d)];
                if (flam.frames > 0 && --flam.frames == 0) {
                    renderTo(i);
                    trigger(DrumId(d), flam.velocity, flam.accent, flam.tune, flam.decay);
                    --m_flamsPending;
                }
            }
        if (sequence) {
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
    m_playingSlot.store(m_sequencer.slot(), std::memory_order_relaxed);
    m_queuedSlot.store(m_sequencer.queuedSlot(), std::memory_order_relaxed);
    processBus(left, right, block.frames);
    m_blocks.fetch_add(1, std::memory_order_release); // after the last read of the sample

    const double target = kOutputTrim * units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target; // first block: no fade-in
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
}

} // namespace k909
