#include "kalliope/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace kalliope {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.01;
// -3 dB under the voices: a four-note pad at the default levels peaks around
// -4 dBFS, under the host's soft clip; single notes sit around -15 dBFS.
constexpr double kOutputTrim = 0.7071;
constexpr double kMaxResonance = 4.2;     // ladder feedback at Resonance 100%: self-oscillation
constexpr double kModLfoOctaves = 1.0;
constexpr double kModEnvOctaves = 2.0;
constexpr double kVcfEnvOctaves = 8.0;
constexpr double kVcfLfoOctaves = 4.0;
constexpr double kWheelVibratoSemis = 1.0;
constexpr int kCcModWheel = 1;
constexpr int kCcSustain = 64;
constexpr int kCcAllSoundOff = 120;
constexpr int kCcAllNotesOff = 123;
constexpr double kUnisonDetuneCents = 12.0; // voices spread -12 .. +12 cents
constexpr std::int8_t kKeyUp = -128;        // m_keyShift: the key is not down
// Arp Rate positions as steps per beat: 1/8, 1/16, 1/8T, 1/16T.
constexpr int kArpStepsPerBeat[] = {2, 4, 3, 6};
constexpr double kArpGate = 0.5; // each arpeggio note lasts half a step
// Delay Time positions in beats: 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2.
constexpr double kDelayBeats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    for (Voice &voice : m_voices)
        voice.prepare(2.0 * sampleRate);
    m_heldCount = 0;
    m_keyShift.fill(kKeyUp);
    m_sustained.fill(false);
    m_sustain = false;
    m_mode = keyMode();
    applyKeyMode(m_mode);
    m_decimator.reset();
    m_lfo.reset();
    m_chorus.prepare(sampleRate);
    m_delay.prepare(sampleRate);
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

VoiceSettings Engine::settings() const noexcept
{
    const auto v = [this](ParamId id) { return m_params.get(int(id)); };
    const auto pick = [&](ParamId id, int count) { return units::index(v(id), count); };
    using namespace positions;

    VoiceSettings s;
    static constexpr Wave kWave1[] = {Wave::Triangle, Wave::Saw, Wave::Pulse, Wave::Square};
    static constexpr Wave kWave2[] = {Wave::Sine, Wave::Saw, Wave::Pulse, Wave::Noise};
    s.vco1Wave = kWave1[pick(ParamId::Vco1Wave, kVco1Wave)];
    s.vco2Wave = kWave2[pick(ParamId::Vco2Wave, kVco2Wave)];
    s.vco1Octave = pick(ParamId::Vco1Range, kVco1Range) - 1;  // 16' 8' 4' 2'
    const int range2 = pick(ParamId::Vco2Range, kVco2Range);  // Low 16' 8' 4' 2'
    s.vco2Low = range2 == 0;
    s.vco2Octave = range2 - 2;
    s.vco2FineSemis = units::fineCents(v(ParamId::Vco2Fine)) / 100.0;
    s.sync = pick(ParamId::Sync, kSwitch) == 1;
    s.crossMod = v(ParamId::CrossMod);

    s.modLfoOctaves = kModLfoOctaves * v(ParamId::ModLfo);
    s.modEnvOctaves = kModEnvOctaves * v(ParamId::ModEnv);
    const int dest = pick(ParamId::ModDest, kModDest);
    s.modVco1 = dest <= 1;
    s.modVco2 = dest >= 1;
    s.pwm = v(ParamId::Pwm);
    s.pwmSource = pick(ParamId::PwmSrc, kPwmSrc);

    s.mix = v(ParamId::Mix);
    s.hpfHz = units::hpfHz(v(ParamId::Hpf));
    s.cutoffOctaves = std::log2(units::cutoffHz(v(ParamId::Cutoff)));
    s.resonance = kMaxResonance * v(ParamId::Res);
    s.slope24 = pick(ParamId::Slope, kSwitch) == 1;
    s.vcfEnvOctaves = kVcfEnvOctaves * v(ParamId::VcfEnv);
    s.vcfEnv2 = pick(ParamId::VcfEnvSrc, kSwitch) == 1;
    s.vcfLfoOctaves = kVcfLfoOctaves * v(ParamId::VcfLfo);
    s.keyFollow = v(ParamId::KeyFollow);

    s.level = units::gain(v(ParamId::VcaLevel));
    s.vcaLfo = v(ParamId::VcaLfo);
    s.env1 = {units::envSeconds(v(ParamId::Env1A)), units::envSeconds(v(ParamId::Env1D)), v(ParamId::Env1S),
              units::envSeconds(v(ParamId::Env1R))};
    s.env2 = {units::envSeconds(v(ParamId::Env2A)), units::envSeconds(v(ParamId::Env2D)), v(ParamId::Env2S),
              units::envSeconds(v(ParamId::Env2R))};
    s.envKeyFollow = pick(ParamId::EnvKey, kSwitch) == 1;
    s.velocityAmount = v(ParamId::Velocity);
    s.lfoDelay = units::lfoDelaySeconds(v(ParamId::LfoDelay));

    s.bendSemis = m_bend * units::bendSemitones(v(ParamId::BendRange));
    s.vibratoSemis = kWheelVibratoSemis * m_wheel;
    return s;
}

int Engine::activeVoices() const noexcept
{
    int n = 0;
    for (const Voice &voice : m_voices)
        n += voice.active() ? 1 : 0;
    return n;
}

Engine::KeyMode Engine::keyMode() const noexcept
{
    return KeyMode(units::index(m_params.get(int(ParamId::KeyMode)), positions::kKeyMode));
}

void Engine::applyKeyMode(KeyMode mode) noexcept
{
    m_mode = mode;
    for (int v = 0; v < kVoices; ++v) {
        if (mode == KeyMode::Unison) {
            // Detuned across the voices, spread start phases, gain so the stack sums near one voice's level.
            const double detune = kUnisonDetuneCents * (2.0 * v / (kVoices - 1) - 1.0) / 100.0;
            m_voices[std::size_t(v)].setSpread(detune, 1.0 / std::sqrt(double(kVoices)), double(v) / kVoices);
        } else {
            m_voices[std::size_t(v)].setSpread(0.0, 1.0, 0.0);
        }
    }
}

void Engine::allNotesOff() noexcept
{
    m_heldCount = 0;
    m_keyShift.fill(kKeyUp);
    m_latchedCount = 0;
    m_arpNote = -1;
    m_seqCount = 0;
    m_sustained.fill(false);
    for (Voice &voice : m_voices)
        voice.noteOff();
}

int Engine::pickVoice(int note) noexcept
{
    // The same note again: its own voice (sounding or releasing).
    for (int v = 0; v < kVoices; ++v)
        if (m_voices[std::size_t(v)].active() && m_voices[std::size_t(v)].note() == note)
            return v;
    // A silent voice.
    for (int v = 0; v < kVoices; ++v)
        if (!m_voices[std::size_t(v)].active())
            return v;
    // The oldest released voice, else the oldest held one.
    int best = -1;
    for (int pass = 0; pass < 2 && best < 0; ++pass)
        for (int v = 0; v < kVoices; ++v) {
            const bool released = !m_voices[std::size_t(v)].gate() || m_sustained[std::size_t(v)];
            if ((pass == 0) != released)
                continue;
            if (best < 0 || m_started[std::size_t(v)] < m_started[std::size_t(best)])
                best = v;
        }
    return best;
}

void Engine::playMono(bool retrigger) noexcept
{
    const int note = m_held[std::size_t(m_heldCount - 1)];
    const int count = m_mode == KeyMode::Unison ? kVoices : 1;
    for (int v = 0; v < count; ++v) {
        m_sustained[std::size_t(v)] = false;
        m_voices[std::size_t(v)].noteOn(note, m_lastVelocity, retrigger);
    }
}

void Engine::voiceOn(int note) noexcept
{
    const int v = pickVoice(note);
    Voice &voice = m_voices[std::size_t(v)];
    // Free-running oscillators, as on analog machines: each new note starts at
    // its own phase, so a chord's notes don't all start on the same edge.
    m_phaseSeed ^= m_phaseSeed << 13;
    m_phaseSeed ^= m_phaseSeed >> 17;
    m_phaseSeed ^= m_phaseSeed << 5;
    voice.setSpread(0.0, 1.0, double(m_phaseSeed >> 8) / double(1u << 24));
    m_started[std::size_t(v)] = ++m_counter;
    m_sustained[std::size_t(v)] = false;
    if (voice.active() && voice.note() != note)
        voice.steal(note, m_lastVelocity);   // fade out its old note first
    else
        voice.noteOn(note, m_lastVelocity, true);
}

void Engine::voiceOff(int note) noexcept
{
    for (int v = 0; v < kVoices; ++v) {
        const Voice &voice = m_voices[std::size_t(v)];
        if (voice.gate() && voice.note() == note && !m_sustained[std::size_t(v)])
            releaseVoice(v);
    }
}

void Engine::releaseAllVoices() noexcept
{
    m_sustained.fill(false);
    for (Voice &voice : m_voices)
        voice.noteOff();
}

void Engine::noteOn(int note, int velocity) noexcept
{
    m_lastVelocity = float(velocity) / 127.0f;
    // Hold: the first key of a new chord replaces the latched one.
    if (m_hold) {
        if (m_heldCount == 0)
            m_latchedCount = 0;
        if (std::find(m_latched.begin(), m_latched.begin() + m_latchedCount, note) == m_latched.begin() + m_latchedCount
            && m_latchedCount < int(m_latched.size()))
            m_latched[std::size_t(m_latchedCount++)] = std::uint8_t(note);
    }
    // The held keys, oldest first (last-note priority in Unison and Solo).
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
    const bool othersHeld = m_heldCount > 0;
    m_held[std::size_t(m_heldCount++)] = std::uint8_t(note);

    if (m_arpOn)
        return; // the arpeggiator plays it
    if (m_playMode == PlayMode::Sequencer && m_arpClock.running()) {
        m_transpose = note - kSeqTransposeCentre; // keys transpose the running pattern
        return;
    }
    if (m_mode != KeyMode::Poly)
        playMono(!othersHeld); // legato while another key is down
    else
        voiceOn(note);
}

void Engine::releaseVoice(int v) noexcept
{
    if (m_sustain)
        m_sustained[std::size_t(v)] = true;
    else
        m_voices[std::size_t(v)].noteOff();
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
    if (m_arpOn)
        return; // the arpeggiator follows the held (or latched) keys
    if (m_playMode == PlayMode::Sequencer && m_arpClock.running())
        return; // transposing keys don't sound

    if (m_mode != KeyMode::Poly) {
        if (!wasTop)
            return;
        if (m_heldCount > 0) {
            playMono(false); // back to the previous held key, legato
            return;
        }
        const int count = m_mode == KeyMode::Unison ? kVoices : 1;
        for (int v = 0; v < count; ++v)
            releaseVoice(v);
        return;
    }
    voiceOff(note);
}

void Engine::arpUpdate(const krate::TransportState &transport) noexcept
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
        m_arpClock.stop();
        m_playMode = mode;
        m_arpOn = mode == PlayMode::Arpeggio;
        if (mode == PlayMode::Arpeggio) {
            releaseAllVoices(); // held keys stop sounding as chords: the arpeggio takes over
        } else {
            releaseAllVoices();
            // Keys still down play as chords again.
            for (int i = 0; i < m_heldCount; ++i)
                if (m_mode == KeyMode::Poly)
                    voiceOn(m_held[std::size_t(i)]);
            if (m_mode != KeyMode::Poly && m_heldCount > 0)
                playMono(true);
        }
    }

    m_arpClock.setStepsPerBeat(kArpStepsPerBeat[units::index(m_params.get(int(ParamId::ArpRate)), positions::kArpRate)]);
    m_arpClock.setGateLength(kArpGate);
    m_arpClock.setLength(m_playMode == PlayMode::Sequencer ? m_sequence.length : 16);
    const bool run = m_playMode != PlayMode::Note && transport.playing
                     && (m_playMode != PlayMode::Sequencer || m_sequencerEnabled.load(std::memory_order_relaxed));
    if (run && (transport.restarted || !m_arpClock.running())) {
        arpRelease();
        seqRelease();
        if (m_playMode == PlayMode::Sequencer)
            releaseAllVoices(); // keys stop sounding: from now on they transpose
        m_arpClock.start();
        m_arpPosition = 0;
    } else if (!run && m_arpClock.running()) {
        m_arpClock.stop();
        arpRelease();
        seqRelease();
    }
}

void Engine::playNotes(const int *notes, int count) noexcept
{
    if (count <= 0)
        return;
    if (m_mode == KeyMode::Poly) {
        for (int i = 0; i < count; ++i)
            voiceOn(notes[i]);
        return;
    }
    // Solo and Unison: one note at a time, the top one.
    const int top = *std::max_element(notes, notes + count);
    const int voices = m_mode == KeyMode::Unison ? kVoices : 1;
    for (int v = 0; v < voices; ++v) {
        m_sustained[std::size_t(v)] = false;
        m_voices[std::size_t(v)].noteOn(top, m_lastVelocity, true);
    }
}

void Engine::releaseNotes(const int *notes, int count) noexcept
{
    if (count <= 0)
        return;
    if (m_mode == KeyMode::Poly) {
        for (int i = 0; i < count; ++i)
            voiceOff(notes[i]);
        return;
    }
    const int voices = m_mode == KeyMode::Unison ? kVoices : 1;
    for (int v = 0; v < voices; ++v)
        releaseVoice(v);
}

void Engine::seqRelease() noexcept
{
    releaseNotes(m_seqNotes.data(), m_seqCount);
    m_seqCount = 0;
}

void Engine::seqStep(int step) noexcept
{
    if (step < 0 || step >= 16)
        return;
    const SeqStep &s = m_sequence.steps[std::size_t(step)];
    if (s.tie && m_seqCount > 0)
        return; // the notes go on
    seqRelease();
    for (int i = 0; i < s.count && i < 4; ++i)
        m_seqNotes[std::size_t(m_seqCount++)] = std::clamp(int(s.notes[std::size_t(i)]) + m_transpose, 0, 127);
    playNotes(m_seqNotes.data(), m_seqCount);
}

void Engine::arpRelease() noexcept
{
    if (m_arpNote < 0)
        return;
    releaseNotes(&m_arpNote, 1);
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
    case 1: // Down
        index = length - 1 - m_arpPosition % length;
        break;
    case 2: { // Up & Down, without repeating the top and bottom notes
        if (length == 1)
            break;
        const std::uint32_t k = m_arpPosition % (2 * length - 2);
        index = k < length ? k : 2 * length - 2 - k;
        break;
    }
    default: // Random
        m_arpRandom ^= m_arpRandom << 13;
        m_arpRandom ^= m_arpRandom >> 17;
        m_arpRandom ^= m_arpRandom << 5;
        index = m_arpRandom % length;
        break;
    }
    ++m_arpPosition;
    const int note = std::min(127, keys[index % std::uint32_t(count)] + 12 * int(index / std::uint32_t(count)));

    playNotes(&note, 1);
    m_arpNote = note;
}

void Engine::setSustain(bool down) noexcept
{
    m_sustain = down;
    if (down)
        return;
    for (int v = 0; v < kVoices; ++v)
        if (m_sustained[std::size_t(v)]) {
            m_sustained[std::size_t(v)] = false;
            m_voices[std::size_t(v)].noteOff();
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
        // Octave shifts MIDI keys (the on-screen keyboard already shows the shifted range).
        const int shift = 12 * units::octaveShift(m_params.get(int(ParamId::Octave)));
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
        // Pitch bend: 14 bits, centre 8192.
        if (event.size == 3 && (event.data[0] & 0xF0) == 0xE0) {
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
        for (std::uint32_t i = 0; i < n; ++i)
            m_lfoValues[i] = float(m_lfo.next(lfoHz, lfoWave, m_sampleRate));
        std::fill_n(m_buffer2x.begin(), 2 * n, 0.0f);
        for (Voice &voice : m_voices)
            voice.render(m_buffer2x.data(), n, m_lfoValues.data(), s);
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
    const VoiceSettings s = settings();
    if (const KeyMode mode = keyMode(); mode != m_mode) {
        allNotesOff(); // a new key mode starts from silence (release tails ring out)
        applyKeyMode(mode);
    }
    while (const auto n = m_guiNotes.pop()) {
        if (n->velocity > 0)
            noteOn(n->note, n->velocity);
        else
            noteOff(n->note);
    }

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    arpUpdate(transport);
    const double beatsPerSample = transport.bpm / 60.0 / m_sampleRate;

    // Voices render in runs between the frames where something happens: MIDI
    // events and arpeggiator steps land on their exact sample.
    std::uint32_t done = 0;
    const auto renderTo = [&](std::uint32_t frame) {
        if (frame <= done)
            return;
        VoiceSettings now = s;
        now.bendSemis = m_bend * units::bendSemitones(m_params.get(int(ParamId::BendRange)));
        now.vibratoSemis = kWheelVibratoSemis * m_wheel;
        render(left + done, frame - done, now);
        done = frame;
    };
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame <= i) {
            renderTo(i);
            handleMidi(block.midi[nextEvent++]);
        }
        if (m_arpClock.running()) {
            const auto tick = m_arpClock.tick(transport.beatAtBlockStart + double(i) * beatsPerSample);
            const bool sequencer = m_playMode == PlayMode::Sequencer;
            if (tick.stepStarted) {
                renderTo(i);
                if (sequencer)
                    seqStep(m_arpClock.step());
                else
                    arpStep();
            } else if (tick.gatePoint) {
                renderTo(i);
                // A tie on the next step holds the notes through it.
                if (!sequencer)
                    arpRelease();
                else if (!m_sequence.steps[std::size_t(m_arpClock.nextStep())].tie)
                    seqRelease();
            }
        }
    }
    while (nextEvent < block.midiCount) // events past the block's end (malformed): apply at the end
        handleMidi(block.midi[nextEvent++]);
    renderTo(block.frames);

    publishHeld();
    m_seqPlayingStep.store(m_playMode == PlayMode::Sequencer && m_arpClock.running() ? m_arpClock.step() : -1,
                           std::memory_order_relaxed);

    // The voices are mono: the chorus makes them stereo, then the delay, then Volume.
    m_chorus.process(left, left, right, block.frames, units::index(m_params.get(int(ParamId::Chorus)), positions::kChorus));
    const double bpm = block.transport ? block.transport->bpm : units::tempoBpm(m_params.get(int(ParamId::Tempo)));
    const double beats = kDelayBeats[units::index(m_params.get(int(ParamId::DelayTime)), positions::kDelayTime)];
    m_delay.process(left, right, block.frames, beats * 60.0 / std::max(bpm, 1.0),
                    m_params.get(int(ParamId::DelayLevel)), units::delayFeedback(m_params.get(int(ParamId::DelayFb))));

    const double target = kOutputTrim * units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
}

} // namespace kalliope
