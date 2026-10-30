#include "kobalt/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace kobalt {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.01;
// One voice: no headroom kept for chords (single notes peak around -10 .. -20 dBFS).
constexpr double kOutputTrim = 1.0;
constexpr double kMaxResonance = 4.4;   // filter feedback at Resonance 100%: self-oscillation
constexpr double kVcfEnvOctaves = 8.0;
constexpr double kVcfModOctaves = 4.0;
constexpr double kBendVcfOctaves = 4.0;
constexpr int kCcModWheel = 1;
constexpr int kCcSustain = 64;
constexpr int kCcAllSoundOff = 120;
constexpr int kCcAllNotesOff = 123;
constexpr std::int8_t kKeyUp = -128; // m_keyShift: the key is not down
// Arp Rate positions as steps per beat: 1/8, 1/16, 1/8T, 1/16T.
constexpr int kStepsPerBeat[] = {2, 4, 3, 6};
constexpr double kGate = 0.5; // arpeggio and sequencer notes last half a step (unless tied or legato)
constexpr float kSeqVelocity = 100.0f / 127.0f;
// Delay Time positions in beats: 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2.
constexpr double kDelayBeats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_voice.prepare(2.0 * sampleRate);
    m_heldCount = 0;
    m_keyShift.fill(kKeyUp);
    m_sustain = false;
    m_sustained = false;
    m_latchedCount = 0;
    m_arpNote = -1;
    m_seqNote = -1;
    m_seqLegato = false;
    m_clock.stop();
    m_decimator.reset();
    m_lfo.reset();
    m_delay.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_reverbOn = false;
    m_settings = settings();
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

VoiceSettings Engine::settings() const noexcept
{
    VoiceSettings s = baseSettings();
    applyLive(s);
    return s;
}

VoiceSettings Engine::baseSettings() const noexcept
{
    const auto v = [this](ParamId id) { return m_params.get(int(id)); };
    const auto pick = [&](ParamId id, int count) { return units::index(v(id), count); };
    using namespace positions;

    VoiceSettings s;
    s.rangeOctaves = pick(ParamId::VcoRange, kVcoRange) - 1; // 16' 8' 4' 2'
    s.vcoModOctaves = units::vcoModOctaves(v(ParamId::VcoMod));
    s.pw = v(ParamId::Pw);
    s.pwmSource = pick(ParamId::PwmSrc, kPwmSrc);
    s.pulse = v(ParamId::PulseLevel);
    s.saw = v(ParamId::SawLevel);
    s.sub = v(ParamId::SubLevel);
    s.noise = v(ParamId::NoiseLevel);
    s.subType = SubType(pick(ParamId::SubType, kSubType));
    s.cutoffOctaves = std::log2(units::cutoffHz(v(ParamId::Cutoff)));
    s.resonance = kMaxResonance * v(ParamId::Res);
    s.vcfEnvOctaves = kVcfEnvOctaves * v(ParamId::VcfEnv);
    s.vcfModOctaves = kVcfModOctaves * v(ParamId::VcfMod);
    s.keyFollow = v(ParamId::KeyFollow);
    s.vcaGate = pick(ParamId::VcaMode, kVcaMode) == 1;
    s.env = {units::envSeconds(v(ParamId::EnvA)), units::envSeconds(v(ParamId::EnvD)), v(ParamId::EnvS),
             units::envSeconds(v(ParamId::EnvR))};
    s.envTrigger = EnvTrigger(pick(ParamId::EnvTrig, kEnvTrig));
    s.portaSeconds = units::portaSeconds(v(ParamId::PortaTime));
    s.portaMode = PortaMode(pick(ParamId::PortaMode, kPortaMode));
    s.velocityAmount = v(ParamId::Velocity);
    s.analog = v(ParamId::Analog);
    return s;
}

void Engine::applyLive(VoiceSettings &s) const noexcept
{
    s.bendSemis = m_bend * units::bendSemitones(m_params.get(int(ParamId::BendVco)));
    s.bendOctaves = m_bend * kBendVcfOctaves * m_params.get(int(ParamId::BendVcf));
    s.vibratoSemis = m_wheel * units::leverSemitones(m_params.get(int(ParamId::LeverMod)));
}

void Engine::allNotesOff() noexcept
{
    m_heldCount = 0;
    m_keyShift.fill(kKeyUp);
    m_sustained = false;
    m_latchedCount = 0;
    m_arpNote = -1;
    m_seqNote = -1;
    m_seqLegato = false;
    m_voice.noteOff();
}

void Engine::noteOn(int note, int velocity) noexcept
{
    m_lastVelocity = float(velocity) / 127.0f;
    // The held keys, oldest first; the same key again moves to the top.
    for (int i = 0; i < m_heldCount; ++i)
        if (m_held[std::size_t(i)] == note) {
            std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
            --m_heldCount;
            break;
        }
    if (m_heldCount == int(m_held.size())) {
        std::copy(m_held.begin() + 1, m_held.end(), m_held.begin());
        --m_heldCount;
    }
    // Hold: the first key of a new chord replaces what was latched.
    if (m_hold) {
        if (m_heldCount == 0)
            m_latchedCount = 0;
        if (std::find(m_latched.begin(), m_latched.begin() + m_latchedCount, note) == m_latched.begin() + m_latchedCount
            && m_latchedCount < int(m_latched.size()))
            m_latched[std::size_t(m_latchedCount++)] = std::uint8_t(note);
    }
    const bool legato = m_heldCount > 0; // another key is still down
    m_held[std::size_t(m_heldCount++)] = std::uint8_t(note);

    if (m_playMode == PlayMode::Arpeggio)
        return; // the arpeggiator plays it
    if (m_playMode == PlayMode::Sequencer && m_clock.running()) {
        m_transpose = note - kSeqTransposeCentre; // keys transpose the running pattern
        return;
    }
    m_sustained = false;
    m_voice.noteOn(note, m_lastVelocity, legato, m_settings);
}

void Engine::noteOff(int note) noexcept
{
    const bool wasTop = m_heldCount > 0 && m_held[std::size_t(m_heldCount - 1)] == note;
    for (int i = 0; i < m_heldCount; ++i)
        if (m_held[std::size_t(i)] == note) {
            std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
            --m_heldCount;
            break;
        }
    if (!wasTop || m_playMode == PlayMode::Arpeggio || (m_playMode == PlayMode::Sequencer && m_clock.running()))
        return; // the arpeggiator follows the keys; keys transposing the sequencer don't sound
    if (m_heldCount > 0) { // back to the newest key still held, legato
        m_voice.noteOn(m_held[std::size_t(m_heldCount - 1)], m_lastVelocity, true, m_settings);
        return;
    }
    if (m_sustain)
        m_sustained = true;
    else
        m_voice.noteOff();
}

void Engine::setSustain(bool down) noexcept
{
    m_sustain = down;
    if (!down && m_sustained) {
        m_sustained = false;
        m_voice.noteOff();
    }
}

void Engine::playKeys() noexcept
{
    m_voice.noteOff();
    if (m_heldCount > 0)
        m_voice.noteOn(m_held[std::size_t(m_heldCount - 1)], m_lastVelocity, false, m_settings);
}

void Engine::playUpdate(const krate::TransportState &transport) noexcept
{
    const auto mode = PlayMode(units::index(m_params.get(int(ParamId::PlayMode)), positions::kPlayMode));
    const bool hold = units::index(m_params.get(int(ParamId::ArpHold)), positions::kSwitch) == 1;
    if (hold != m_hold) {
        m_hold = hold;
        // Turning Hold on latches what is held now; off, the arpeggio follows the keys again.
        m_latchedCount = 0;
        if (hold)
            for (int i = 0; i < m_heldCount; ++i)
                m_latched[std::size_t(m_latchedCount++)] = m_held[std::size_t(i)];
    }
    while (const auto pattern = m_sequences.pop())
        m_sequence = *pattern;

    if (mode != m_playMode) {
        arpRelease();
        seqRelease();
        m_clock.stop();
        m_playMode = mode;
        if (mode == PlayMode::Arpeggio)
            m_voice.noteOff(); // the arpeggio takes over from the held keys
        else
            playKeys();        // keys still down sound again
    }

    m_clock.setStepsPerBeat(kStepsPerBeat[units::index(m_params.get(int(ParamId::ArpRate)), positions::kArpRate)]);
    m_clock.setGateLength(kGate);
    m_clock.setLength(m_playMode == PlayMode::Sequencer ? m_sequence.length : 16);
    const bool run = m_playMode != PlayMode::Note && transport.playing
                     && (m_playMode != PlayMode::Sequencer
                         || (m_sequencerEnabled.load(std::memory_order_relaxed)
                             && !m_sequencerSuspended.load(std::memory_order_relaxed)));
    if (run && (transport.restarted || !m_clock.running())) {
        arpRelease();
        seqRelease();
        if (m_playMode == PlayMode::Sequencer)
            m_voice.noteOff(); // keys stop sounding: from now on they transpose
        if (m_playMode == PlayMode::Sequencer && !transport.restarted && transport.beatAtBlockStart > 0.0)
            m_clock.startAt(krate::StepClock::nextBar(transport.beatAtBlockStart)); // joining: from the next bar
        else
            m_clock.start();
        m_arpPosition = 0;
    } else if (!run && m_clock.running()) {
        m_clock.stop();
        arpRelease();
        seqRelease();
        if (m_playMode == PlayMode::Sequencer)
            playKeys();
    }
}

void Engine::arpRelease() noexcept
{
    if (m_arpNote < 0)
        return;
    m_voice.noteOff();
    m_arpNote = -1;
}

void Engine::arpStep() noexcept
{
    arpRelease();
    // The keys to arpeggiate, low to high, without repeats.
    std::array<int, 16> keys{};
    int count = 0;
    const std::uint8_t *source = m_hold ? m_latched.data() : m_held.data();
    const int sourceCount = m_hold ? m_latchedCount : m_heldCount;
    for (int i = 0; i < sourceCount; ++i) {
        const int n = source[i];
        if (std::find(keys.begin(), keys.begin() + count, n) == keys.begin() + count)
            keys[std::size_t(count++)] = n;
    }
    if (count == 0)
        return;
    std::sort(keys.begin(), keys.begin() + count);

    const int octaves = 1 + units::index(m_params.get(int(ParamId::ArpRange)), positions::kArpRange);
    const auto length = std::uint32_t(count * octaves);
    std::uint32_t index = 0;
    switch (units::index(m_params.get(int(ParamId::ArpMode)), positions::kArpMode)) {
    case 0: // Up
        index = m_arpPosition % length;
        break;
    case 1: { // Up & Down, without repeating the top and bottom notes
        if (length == 1)
            break;
        const std::uint32_t k = m_arpPosition % (2 * length - 2);
        index = k < length ? k : 2 * length - 2 - k;
        break;
    }
    default: // Down
        index = length - 1 - m_arpPosition % length;
        break;
    }
    ++m_arpPosition;
    const int note = std::min(127, keys[index % std::uint32_t(count)] + 12 * int(index / std::uint32_t(count)));
    m_voice.noteOn(note, m_lastVelocity, false, m_settings);
    m_arpNote = note;
}

void Engine::seqRelease() noexcept
{
    if (m_seqNote < 0)
        return;
    m_voice.noteOff();
    m_seqNote = -1;
    m_seqLegato = false;
}

void Engine::seqStep(int step) noexcept
{
    if (step < 0 || step >= kSeqMaxSteps)
        return;
    const SeqStep &s = m_sequence.steps[std::size_t(step)];
    switch (s.kind) {
    case SeqStep::Kind::Rest:
        seqRelease();
        break;
    case SeqStep::Kind::Tie: // the note goes on
        if (m_seqNote >= 0)
            m_seqLegato = s.legato;
        break;
    case SeqStep::Kind::Note: {
        const int note = std::clamp(int(s.note) + m_transpose, 0, 127);
        const bool legato = m_seqNote >= 0 && m_seqLegato; // the step before runs into this one
        if (!legato)
            seqRelease();
        m_voice.noteOn(note, kSeqVelocity, legato, m_settings);
        m_seqNote = note;
        m_seqLegato = s.legato;
        break;
    }
    }
}

void Engine::publishHeld() noexcept
{
    std::uint64_t mask[2] = {0, 0};
    for (int i = 0; i < m_heldCount; ++i) {
        const int n = m_held[std::size_t(i)];
        mask[n / 64] |= std::uint64_t(1) << (n % 64);
    }
    m_heldMask[0].store(mask[0], std::memory_order_relaxed);
    m_heldMask[1].store(mask[1], std::memory_order_relaxed);
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event); // all channels (omni)
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            m_params.setFromAudio(r.param, r.value);
        else if (m.data1 == kCcModWheel)
            m_wheel = m.data2 / 127.0;
        else if (m.data1 == kCcSustain)
            setSustain(m.data2 >= 64);
        else if (m.data1 == kCcAllNotesOff || m.data1 == kCcAllSoundOff)
            allNotesOff();
        break;
    case krate::MidiMessage::Type::NoteOn: {
        // Octave shifts the player's keys; a song's notes play as written.
        const int shift = event.sequenced ? 0 : 12 * units::octaveShift(m_params.get(int(ParamId::Octave)));
        const int note = m.data1 + shift;
        if (note < 0 || note > 127)
            break;
        m_keyShift[m.data1] = std::int8_t(shift);
        noteOn(note, m.data2);
        break;
    }
    case krate::MidiMessage::Type::NoteOff: {
        const int shift = m_keyShift[m.data1];
        if (shift == kKeyUp)
            break;
        m_keyShift[m.data1] = kKeyUp;
        noteOff(m.data1 + shift);
        break;
    }
    case krate::MidiMessage::Type::ProgramChange:
        m_programs.push(std::uint8_t(m.data1)); // the patch controller selects the patch
        break;
    case krate::MidiMessage::Type::Other:
        if (event.size == 3 && (event.data[0] & 0xF0) == 0xE0) { // pitch bend: 14 bits, centre 8192
            const int value = (event.data[2] << 7) | event.data[1];
            m_bend = std::clamp((value - 8192) / 8191.0, -1.0, 1.0);
        }
        break;
    }
}

void Engine::render(float *out, std::uint32_t frames, const VoiceSettings &s) noexcept
{
    const auto lfoWave = Lfo::Wave(units::index(m_params.get(int(ParamId::LfoWave)), positions::kLfoWave));
    const double lfoHz = units::lfoRateHz(m_params.get(int(ParamId::LfoRate)));
    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        for (std::uint32_t i = 0; i < n; ++i) {
            m_lfoValues[i] = float(m_lfo.next(lfoHz, lfoWave, m_sampleRate));
            m_lfoWrapped[i] = m_lfo.wrapped();
        }
        std::fill_n(m_buffer2x.begin(), 2 * n, 0.0f);
        m_voice.render(m_buffer2x.data(), n, m_lfoValues.data(), m_lfoWrapped.data(), s);
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = m_decimator.process(m_buffer2x[2 * i], m_buffer2x[2 * i + 1]);
        out += n;
        frames -= n;
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    krate::ScopedFlushDenormals noDenormals;
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    const VoiceSettings base = baseSettings();
    m_settings = base;
    applyLive(m_settings);
    while (const auto n = m_guiNotes.pop()) {
        if (n->velocity > 0)
            noteOn(n->note, n->velocity);
        else
            noteOff(n->note);
    }

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    playUpdate(transport);
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;

    // The voice renders in runs between the frames where something happens:
    // MIDI events and arpeggio or sequencer steps land on their exact sample.
    std::uint32_t done = 0;
    const auto renderTo = [&](std::uint32_t frame) {
        if (frame <= done)
            return;
        render(left + done, frame - done, m_settings);
        done = frame;
    };
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame <= i) {
            renderTo(i);
            handleMidi(block.midi[nextEvent++]);
            m_settings = base;
            applyLive(m_settings); // the bender or the lever may have moved
        }
        if (!m_clock.running())
            continue;
        const auto tick = m_clock.tick(transport.beatAtBlockStart + double(i) * beatsPerSample);
        const bool sequencer = m_playMode == PlayMode::Sequencer;
        if (tick.stepStarted) {
            renderTo(i);
            if (sequencer)
                seqStep(m_clock.step());
            else
                arpStep();
        } else if (tick.gatePoint) {
            renderTo(i);
            if (!sequencer)
                arpRelease();
            else if (!m_seqLegato
                     && m_sequence.steps[std::size_t(m_clock.nextStep())].kind != SeqStep::Kind::Tie)
                seqRelease(); // a tie or legato holds the note through to the next step
        }
    }
    while (nextEvent < block.midiCount) { // events past the block's end (malformed): apply at the end
        handleMidi(block.midi[nextEvent++]);
        m_settings = base;
        applyLive(m_settings);
    }
    renderTo(block.frames);
    publishHeld();
    m_seqPlayingStep.store(m_playMode == PlayMode::Sequencer && m_clock.running() ? m_clock.step() : -1,
                           std::memory_order_relaxed);

    // The voice is mono: the delay makes it stereo, then the reverb, then Volume.
    std::copy_n(left, block.frames, right);
    const double bpm = block.transport ? block.transport->bpm : units::tempoBpm(m_params.get(int(ParamId::Tempo)));
    const double beats = kDelayBeats[units::index(m_params.get(int(ParamId::DelayTime)), positions::kDelayTime)];
    m_delay.process(left, right, block.frames, beats * 60.0 / std::max(bpm, 1.0),
                    m_params.get(int(ParamId::DelayLevel)), units::delayFeedback(m_params.get(int(ParamId::DelayFb))));
    // The reverb runs only while it is up (cleared when it comes back: no stale tail).
    if (const double reverb = m_params.get(int(ParamId::ReverbLevel)); reverb > 0.0) {
        if (!m_reverbOn)
            m_reverb.clear();
        m_reverb.process(left, right, block.frames, m_params.get(int(ParamId::ReverbSize)), reverb);
        m_reverbOn = true;
    } else {
        m_reverbOn = false;
    }

    const double target = kOutputTrim * units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
}

} // namespace kobalt
