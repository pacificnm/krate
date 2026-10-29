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
constexpr int kCcSustain = 64;
constexpr int kCcAllSoundOff = 120;
constexpr int kCcAllNotesOff = 123;
constexpr double kMaxUnisonDetuneSemis = 0.35; // Unison Detune at 100 %: the outer voices +/- 35 cents
constexpr double kUnisonSpread = 0.7;          // stereo position of the outer unison voices
} // namespace

Engine::Engine() = default;

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    for (Voice &voice : m_voices)
        voice.prepare(2.0 * sampleRate);
    m_decimatorL.reset();
    m_decimatorR.reset();
    for (Lfo &lfo : m_lfo2)
        lfo.reset();
    m_vibrato.reset();
    m_heldCount = 0;
    m_sustained.fill(false);
    m_sustain = false;
    m_lastNote = -1;
    m_mode = voiceMode(editedSlot());
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
    s.glideSeconds = units::glideSeconds(v(ParamId::Glide)) / 3.0; // the knob shows the time to get (almost) there
    s.gain = units::gain(v(ParamId::Gain));
    s.bendSemis = m_bend * units::bendSemitones(v(ParamId::BendRange));
    // The wheel: vibrato when Wheel is set to Vibrato (morph arrives in C5).
    s.vibratoSemis = pick(ParamId::WheelDest, q::kWheelDest) == 1 ? kMaxVibratoSemis * m_wheel : 0.0;
    return s;
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

Engine::VoiceMode Engine::voiceMode(int slot) const noexcept
{
    return VoiceMode(units::index(m_slots[std::size_t(slot)].get(int(ParamId::VoiceMode)), positions::kVoiceMode));
}

int Engine::activeVoices() const noexcept
{
    int n = 0;
    for (const Voice &voice : m_voices)
        n += voice.active() ? 1 : 0;
    return n;
}

int Engine::pickVoice(int note) noexcept
{
    // The same note again: its own voice.
    for (int v = 0; v < kVoices; ++v)
        if (m_voices[std::size_t(v)].active() && m_voices[std::size_t(v)].note() == note)
            return v;
    for (int v = 0; v < kVoices; ++v)
        if (!m_voices[std::size_t(v)].active())
            return v;
    // The oldest released (or pedal-held) voice, else the oldest held one.
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

void Engine::startVoice(int v, int note, double glideFrom) noexcept
{
    Voice &voice = m_voices[std::size_t(v)];
    m_voiceSlot[std::size_t(v)] = editedSlot();
    m_started[std::size_t(v)] = ++m_counter;
    m_sustained[std::size_t(v)] = false;
    voice.setSpread(0.0, 0.0);
    if (voice.active() && voice.note() != note)
        voice.steal(note, m_lastVelocity, glideFrom); // fade its old note out first
    else
        voice.noteOn(note, m_lastVelocity, true, glideFrom);
}

void Engine::playMono(bool retrigger) noexcept
{
    const int note = m_held[std::size_t(m_heldCount - 1)];
    const double glideFrom = m_lastNote >= 0 ? double(m_lastNote) : -1.0;
    const int slot = editedSlot();
    const bool unison = m_mode == VoiceMode::Unison;
    const int count = unison ? kUnisonVoices : 1;
    const double detune = kMaxUnisonDetuneSemis * m_slots[std::size_t(slot)].get(int(ParamId::UnisonDetune));
    for (int v = 0; v < count; ++v) {
        Voice &voice = m_voices[std::size_t(v)];
        const double position = count > 1 ? 2.0 * v / (count - 1) - 1.0 : 0.0; // -1, 0, +1
        voice.setSpread(detune * position, unison ? kUnisonSpread * position : 0.0);
        m_voiceSlot[std::size_t(v)] = slot;
        m_sustained[std::size_t(v)] = false;
        voice.noteOn(note, m_lastVelocity, retrigger, glideFrom);
    }
    m_lastNote = note;
}

void Engine::noteOn(int note, int velocity) noexcept
{
    m_lastVelocity = float(velocity) / 127.0f;
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

    switch (m_mode) {
    case VoiceMode::Poly: {
        const double glideFrom = m_lastNote >= 0 ? double(m_lastNote) : -1.0;
        startVoice(pickVoice(note), note, glideFrom);
        m_lastNote = note;
        break;
    }
    case VoiceMode::Mono:
    case VoiceMode::Unison:
        playMono(true);
        break;
    case VoiceMode::Legato:
        if (!othersHeld) // a fresh phrase: no glide into it
            m_lastNote = -1;
        playMono(!othersHeld);
        break;
    }
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
    if (m_mode == VoiceMode::Poly) {
        for (int v = 0; v < kVoices; ++v) {
            const Voice &voice = m_voices[std::size_t(v)];
            if (voice.gate() && voice.note() == note && !m_sustained[std::size_t(v)])
                releaseVoice(v);
        }
        return;
    }
    if (!wasTop)
        return;
    if (m_heldCount > 0) {
        playMono(m_mode != VoiceMode::Legato); // back to the previous key (Legato: without retrigger)
        return;
    }
    const int count = m_mode == VoiceMode::Unison ? kUnisonVoices : 1;
    for (int v = 0; v < count; ++v)
        releaseVoice(v);
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

void Engine::allNotesOff() noexcept
{
    m_heldCount = 0;
    m_sustained.fill(false);
    for (Voice &voice : m_voices)
        voice.noteOff();
}

void Engine::render(float *left, float *right, std::uint32_t frames) noexcept
{
    std::array<double, SlotCount> lfo2Hz{};
    for (int slot = 0; slot < SlotCount; ++slot)
        lfo2Hz[std::size_t(slot)] = units::lfoRateHz(m_slots[std::size_t(slot)].get(int(ParamId::Lfo2Rate)));
    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        for (std::uint32_t i = 0; i < n; ++i) {
            for (int slot = 0; slot < SlotCount; ++slot)
                m_lfo2Values[std::size_t(slot)][i] =
                    float(m_lfo2[std::size_t(slot)].next(lfo2Hz[std::size_t(slot)], Lfo::Wave::Triangle, m_sampleRate));
            m_vibratoValues[i] = float(m_vibrato.next(kVibratoHz, Lfo::Wave::Sine, m_sampleRate));
        }
        std::fill_n(m_bufferL.begin(), 2 * n, 0.0f);
        std::fill_n(m_bufferR.begin(), 2 * n, 0.0f);
        for (int v = 0; v < kVoices; ++v) {
            const auto slot = std::size_t(m_voiceSlot[std::size_t(v)]);
            m_voices[std::size_t(v)].render(m_bufferL.data(), m_bufferR.data(), n, m_lfo2Values[slot].data(),
                                            m_vibratoValues.data(), m_settings[slot]);
        }
        for (std::uint32_t i = 0; i < n; ++i) {
            left[i] = m_decimatorL.process(m_bufferL[2 * i], m_bufferL[2 * i + 1]);
            right[i] = m_decimatorR.process(m_bufferR[2 * i], m_bufferR[2 * i + 1]);
        }
        left += n;
        right += n;
        frames -= n;
    }
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
    if (const VoiceMode mode = voiceMode(editedSlot()); mode != m_mode) {
        allNotesOff(); // a new voice mode starts from silence (release tails ring out)
        m_mode = mode;
        m_lastNote = -1;
    }

    // Render up to each MIDI event, then apply it on its exact frame.
    std::uint32_t done = 0;
    for (std::uint32_t e = 0; e <= block.midiCount; ++e) {
        const std::uint32_t until = e < block.midiCount ? std::min(block.midi[e].frame, block.frames) : block.frames;
        if (until > done) {
            for (int slot = 0; slot < SlotCount; ++slot)
                m_settings[std::size_t(slot)] = settings(slot); // bend and wheel can change between events
            render(left + done, right + done, until - done);
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
        right[i] = float(right[i] * m_volume);
    }
}

} // namespace krimson
