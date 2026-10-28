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
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    for (Voice &voice : m_voices)
        voice.prepare(2.0 * sampleRate);
    m_heldCount = 0;
    m_sustained.fill(false);
    m_sustain = false;
    m_mode = keyMode();
    applyKeyMode(m_mode);
    m_decimator.reset();
    m_lfo.reset();
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

void Engine::noteOn(int note, int velocity) noexcept
{
    m_lastVelocity = float(velocity) / 127.0f;
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

    if (m_mode != KeyMode::Poly) {
        playMono(!othersHeld); // legato while another key is down
        return;
    }
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
    for (int v = 0; v < kVoices; ++v) {
        const Voice &voice = m_voices[std::size_t(v)];
        if (voice.gate() && voice.note() == note && !m_sustained[std::size_t(v)])
            releaseVoice(v);
    }
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
    case krate::MidiMessage::Type::NoteOn:
        noteOn(m.data1, m.data2);
        break;
    case krate::MidiMessage::Type::NoteOff:
        noteOff(m.data1);
        break;
    case krate::MidiMessage::Type::ProgramChange:
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

    // Render up to each MIDI event, then apply it on its exact frame.
    std::uint32_t done = 0;
    for (std::uint32_t e = 0; e <= block.midiCount; ++e) {
        const std::uint32_t until = e < block.midiCount ? std::min(block.midi[e].frame, block.frames) : block.frames;
        if (until > done) {
            VoiceSettings now = s;
            now.bendSemis = m_bend * units::bendSemitones(m_params.get(int(ParamId::BendRange)));
            now.vibratoSemis = kWheelVibratoSemis * m_wheel;
            render(left + done, until - done, now);
            done = until;
        }
        if (e < block.midiCount)
            handleMidi(block.midi[e]);
    }

    publishHeld();

    const double target = kOutputTrim * units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = left[i]; // mono until the chorus (P4)
    }
}

} // namespace kalliope
