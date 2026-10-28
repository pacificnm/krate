#include "kalliope/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace kalliope {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.01;
constexpr double kMaxResonance = 4.2;     // ladder feedback at Resonance 100%: self-oscillation
constexpr double kModLfoOctaves = 1.0;
constexpr double kModEnvOctaves = 2.0;
constexpr double kVcfEnvOctaves = 8.0;
constexpr double kVcfLfoOctaves = 4.0;
constexpr double kWheelVibratoSemis = 1.0;
constexpr int kCcModWheel = 1;
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_voice.prepare(2.0 * sampleRate);
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

void Engine::noteOn(int note, int velocity) noexcept
{
    // Last-note priority over the held keys.
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
    m_held[std::size_t(m_heldCount++)] = std::uint8_t(note);
    m_lastVelocity = float(velocity) / 127.0f;
    m_voice.noteOn(note, m_lastVelocity, true);
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
    if (!wasTop)
        return;
    if (m_heldCount > 0) // back to the previous held key, legato
        m_voice.noteOn(m_held[std::size_t(m_heldCount - 1)], m_lastVelocity, false);
    else
        m_voice.noteOff();
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
        m_voice.render(m_buffer2x.data(), n, m_lfoValues.data(), s);
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

    const double target = units::gain(m_params.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = left[i]; // mono until the chorus (P4)
    }
}

} // namespace kalliope
