#include "krimson/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"

namespace krimson {

namespace {
constexpr std::uint64_t kSlotParamBits = (std::uint64_t(1) << SlotParamCount) - 1;
constexpr double kFenvOctaves = 8.0;
constexpr double kMaxVibratoSemis = 0.7; // the wheel's vibrato at full
constexpr double kVibratoHz = 5.5;
constexpr double kOutputTrim = 0.7071; // -3 dB, as Kalliope and K909
constexpr double kVolumeSmoothingSeconds = 0.01;
constexpr int kCcModWheel = 1;
} // namespace

Engine::Engine() = default;

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_voice.prepare(2.0 * sampleRate);
    m_decimator.reset();
    m_lfo2.reset();
    m_vibrato.reset();
    m_heldCount = 0;
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

SlotSettings Engine::settings(int slot) const noexcept
{
    const Store &p = m_slots[std::size_t(std::clamp(slot, 0, SlotCount - 1))];
    const auto v = [&p](ParamId id) { return p.get(int(id)); };
    const auto pick = [&](ParamId id, int count) { return units::index(v(id), count); };
    namespace q = positions;
    static constexpr Wave kOsc1[] = {Wave::Sine, Wave::Triangle, Wave::Saw, Wave::Pulse};
    static constexpr Wave kOsc2[] = {Wave::Triangle, Wave::Saw, Wave::Pulse, Wave::Noise};
    static constexpr Lfo::Wave kLfo1[] = {Lfo::Wave::Triangle, Lfo::Wave::UpSaw, Lfo::Wave::Square, Lfo::Wave::Random,
                                          Lfo::Wave::SoftRandom};
    static constexpr double kTrack[] = {0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0};
    const auto times = [&](ParamId a, ParamId d, ParamId s, ParamId r) {
        return Envelope::Times{units::envSeconds(v(a)), units::envSeconds(v(d)), double(v(s)), units::envSeconds(v(r))};
    };

    SlotSettings s;
    s.osc1Wave = kOsc1[pick(ParamId::Osc1Wave, q::kOsc1Wave)];
    s.osc2Wave = kOsc2[pick(ParamId::Osc2Wave, q::kOsc2Wave)];
    s.osc2Semis = units::semitones(v(ParamId::Osc2Semi)) + units::fineCents(v(ParamId::Osc2Fine)) / 100.0;
    s.osc2Kbd = pick(ParamId::Osc2Kbd, q::kSwitch) == 1;
    s.pw = v(ParamId::Pw);
    s.mix = v(ParamId::OscMix);
    s.fm = v(ParamId::Fm);
    s.sync = pick(ParamId::Sync, q::kSwitch) == 1;
    s.ring = pick(ParamId::Ring, q::kSwitch) == 1;
    s.menv = {units::envSeconds(v(ParamId::MenvA)), units::envSeconds(v(ParamId::MenvD)), 0.0, 0.0};
    s.menvAmount = units::bipolar(v(ParamId::MenvAmt));
    s.menvDest = pick(ParamId::MenvDest, q::kMenvDest);
    s.lfo1Hz = units::lfoRateHz(v(ParamId::Lfo1Rate));
    s.lfo1Wave = kLfo1[pick(ParamId::Lfo1Wave, q::kLfo1Wave)];
    s.lfo1Amount = v(ParamId::Lfo1Amt);
    s.lfo1Dest = pick(ParamId::Lfo1Dest, q::kLfo1Dest);
    s.lfo2Amount = v(ParamId::Lfo2Amt);
    s.lfo2Dest = pick(ParamId::Lfo2Dest, q::kLfo2Dest);
    s.filterType = MultiFilter::Type(pick(ParamId::FilterType, q::kFilterType));
    s.cutoffOctaves = std::log2(units::cutoffHz(v(ParamId::Cutoff)));
    s.res = v(ParamId::Res);
    s.drive = v(ParamId::Drive);
    s.fenvOctaves = kFenvOctaves * v(ParamId::FenvAmt);
    s.fenvVelocity = v(ParamId::FenvVel);
    s.kbdTrack = kTrack[pick(ParamId::KbdTrack, q::kKbdTrack)];
    s.fenv = times(ParamId::FenvA, ParamId::FenvD, ParamId::FenvS, ParamId::FenvR);
    s.aenv = times(ParamId::AenvA, ParamId::AenvD, ParamId::AenvS, ParamId::AenvR);
    s.ampVelocity = v(ParamId::AmpVel);
    s.gain = units::gain(v(ParamId::Gain));
    s.bendSemis = m_bend * units::bendSemitones(v(ParamId::BendRange));
    // The wheel: vibrato when Wheel is set to Vibrato (morph arrives in C5).
    s.vibratoSemis = pick(ParamId::WheelDest, q::kWheelDest) == 1 ? kMaxVibratoSemis * m_wheel : 0.0;
    return s;
}

void Engine::noteOn(int note, int velocity) noexcept
{
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
    if (m_heldCount > 0)
        m_voice.noteOn(m_held[std::size_t(m_heldCount - 1)], m_lastVelocity, false); // back, legato
    else
        m_voice.noteOff();
}

void Engine::render(float *out, std::uint32_t frames, const SlotSettings &s) noexcept
{
    const int slot = editedSlot();
    const double lfo2Hz = units::lfoRateHz(m_slots[std::size_t(slot)].get(int(ParamId::Lfo2Rate)));
    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        for (std::uint32_t i = 0; i < n; ++i) {
            m_lfo2Values[i] = float(m_lfo2.next(lfo2Hz, Lfo::Wave::Triangle, m_sampleRate));
            m_vibratoValues[i] = float(m_vibrato.next(kVibratoHz, Lfo::Wave::Sine, m_sampleRate));
        }
        std::fill_n(m_buffer2x.begin(), 2 * n, 0.0f);
        m_voice.render(m_buffer2x.data(), n, m_lfo2Values.data(), m_vibratoValues.data(), s);
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = m_decimator.process(m_buffer2x[2 * i], m_buffer2x[2 * i + 1]);
        out += n;
        frames -= n;
    }
}

void Engine::setEditedSlot(int slot) noexcept
{
    slot = std::clamp(slot, 0, SlotCount - 1);
    if (m_edited.exchange(slot, std::memory_order_relaxed) != slot)
        m_forcedChanges.fetch_or(kSlotParamBits, std::memory_order_relaxed);
}

std::uint64_t Engine::takeParamsChangedByAudio() noexcept
{
    std::uint64_t changed = m_global.takeChanged() | m_forcedChanges.exchange(0, std::memory_order_relaxed);
    for (Store &slot : m_slots)
        changed |= slot.takeChanged() & kSlotParamBits; // any slot: the edited one is what the panel shows
    return changed;
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event);
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            store(r.param).setFromAudio(r.param, r.value); // learned knobs edit the edited slot
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
        break;
    case krate::MidiMessage::Type::Other:
        if (event.size == 3 && (event.data[0] & 0xF0) == 0xE0) { // pitch bend
            const int value = (event.data[2] << 7) | event.data[1];
            m_bend = std::clamp((value - 8192) / 8191.0, -1.0, 1.0);
        }
        break;
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    krate::ScopedFlushDenormals noDenormals;
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    const int slot = editedSlot();

    // Render up to each MIDI event, then apply it on its exact frame.
    std::uint32_t done = 0;
    for (std::uint32_t e = 0; e <= block.midiCount; ++e) {
        const std::uint32_t until = e < block.midiCount ? std::min(block.midi[e].frame, block.frames) : block.frames;
        if (until > done) {
            render(left + done, until - done, settings(slot));
            done = until;
        }
        if (e < block.midiCount)
            handleMidi(block.midi[e]);
    }

    const double target = kOutputTrim * units::gain(m_global.get(int(ParamId::Volume)));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = left[i]; // mono until the slots' pan and effects (C3, C6)
    }
}

} // namespace krimson
