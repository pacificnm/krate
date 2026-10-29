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
constexpr double kOutputTrim = 1.0;      // with Slot Level's default (0.8, gain 0.64): about -4 dB
constexpr double kVolumeSmoothingSeconds = 0.01;
constexpr int kCcModWheel = 1;
constexpr int kCcSustain = 64;
constexpr int kCcAllSoundOff = 120;
constexpr int kCcAllNotesOff = 123;
constexpr double kMaxUnisonDetuneSemis = 0.35; // Unison Detune at 100 %: the outer voices +/- 35 cents
constexpr double kUnisonSpread = 0.7;          // stereo position of the outer unison voices
constexpr int kDefaultFirstChannel = 2;        // slots A..D on MIDI channels 3..6
// Delay Time positions in beats: 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2.
constexpr double kDelayBeats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
constexpr double kMaxDelayFeedback = 0.9;
constexpr std::int8_t kKeyUp = -128;
} // namespace

Engine::Engine()
{
    for (auto &slot : m_morph)
        for (auto &target : slot)
            target.store(-1.0f, std::memory_order_relaxed);
    for (int slot = 0; slot < SlotCount; ++slot) {
        m_channel[std::size_t(slot)].store(kDefaultFirstChannel + slot, std::memory_order_relaxed);
        m_on[std::size_t(slot)].store(true, std::memory_order_relaxed);
        m_zone[std::size_t(slot)].store(Zone::Whole, std::memory_order_relaxed);
    }
}

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    for (Voice &voice : m_voices)
        voice.prepare(2.0 * sampleRate);
    for (SlotBus &bus : m_bus) {
        bus.decimateL.reset();
        bus.decimateR.reset();
        bus.chorus.prepare(sampleRate);
        bus.delay.prepare(sampleRate);
        bus.reverb.prepare(sampleRate);
        bus.delayOn = bus.reverbOn = false;
    }
    for (Lfo &lfo : m_lfo2)
        lfo.reset();
    m_vibrato.reset();
    m_sustained.fill(false);
    for (auto &channel : m_keyShift)
        channel.fill(kKeyUp);
    for (int slot = 0; slot < SlotCount; ++slot) {
        m_state[std::size_t(slot)] = SlotState{};
        m_state[std::size_t(slot)].mode = voiceMode(slot);
    }
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

void Engine::setEditedSlot(int slot) noexcept
{
    slot = std::clamp(slot, 0, SlotCount - 1);
    if (m_edited.exchange(slot, std::memory_order_relaxed) != slot)
        m_forcedChanges.fetch_or(kSlotParamBits, std::memory_order_relaxed);
}

void Engine::setSlotChannel(int slot, int channel) noexcept
{
    m_channel[std::size_t(slot)].store(channel >= 0 && channel < 16 ? channel : -1, std::memory_order_relaxed);
}

std::uint64_t Engine::takeParamsChangedByAudio() noexcept
{
    std::uint64_t changed = m_global.takeChanged() | m_forcedChanges.exchange(0, std::memory_order_relaxed);
    for (Store &slot : m_slots)
        changed |= slot.takeChanged() & kSlotParamBits; // any slot: the edited one is what the panel shows
    return changed;
}

void Engine::setMorph(int slot, ParamId id, float target) noexcept
{
    if (slot < 0 || slot >= SlotCount || !morphable(id))
        return;
    m_morph[std::size_t(slot)][std::size_t(id)].store(target < 0.0f ? -1.0f : std::min(target, 1.0f),
                                                     std::memory_order_relaxed);
}

void Engine::clearMorph(int slot) noexcept
{
    for (auto &target : m_morph[std::size_t(slot)])
        target.store(-1.0f, std::memory_order_relaxed);
}

bool Engine::hasMorph(int slot) const noexcept
{
    for (const auto &target : m_morph[std::size_t(slot)])
        if (target.load(std::memory_order_relaxed) >= 0.0f)
            return true;
    return false;
}

SlotSettings Engine::settings(int slot, bool atMorph) const noexcept
{
    slot = std::clamp(slot, 0, SlotCount - 1);
    const Store &p = m_slots[std::size_t(slot)];
    const auto &targets = m_morph[std::size_t(slot)];
    const auto v = [&](ParamId id) {
        if (atMorph && isSlotParam(int(id)))
            if (const float t = targets[std::size_t(id)].load(std::memory_order_relaxed); t >= 0.0f)
                return t;
        return p.get(int(id));
    };
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
    const SlotState &state = m_state[std::size_t(slot)];
    s.bendSemis = state.bend * units::bendSemitones(v(ParamId::BendRange));
    // The wheel: vibrato when Wheel is set to Vibrato (morph arrives in C5).
    s.vibratoSemis = pick(ParamId::WheelDest, q::kWheelDest) == 1 ? kMaxVibratoSemis * state.wheel : 0.0;
    s.slotGain = units::gain(v(ParamId::SlotLevel));
    s.slotPan = units::pan(v(ParamId::SlotPan));
    return s;
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

unsigned Engine::slotsFor(int channel, int note) const noexcept
{
    unsigned own = 0;
    for (int slot = 0; slot < SlotCount; ++slot)
        if (slotOn(slot) && slotChannel(slot) == channel)
            own |= 1u << slot;
    if (own)
        return own; // a slot's own channel
    // The keyboard: the lit slots, each in its part of a split keyboard.
    const int split = splitNote();
    unsigned slots = 0;
    for (int slot = 0; slot < SlotCount; ++slot) {
        if (!slotOn(slot) || !(litSlots() & (1u << slot)))
            continue;
        const Zone zone = slotZone(slot);
        if (split >= 0 && note >= 0 && ((zone == Zone::Lower && note >= split) || (zone == Zone::Upper && note < split)))
            continue;
        slots |= 1u << slot;
    }
    return slots;
}

int Engine::pickVoice(int slot, int note) noexcept
{
    // The same note again in this slot: its own voice.
    for (int v = 0; v < kVoices; ++v)
        if (m_voices[std::size_t(v)].active() && m_voiceSlot[std::size_t(v)] == slot && m_voices[std::size_t(v)].note() == note)
            return v;
    for (int v = 0; v < kVoices; ++v)
        if (!m_voices[std::size_t(v)].active())
            return v;
    // The oldest released (or pedal-held) voice of any slot, else the oldest held one.
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

void Engine::startVoice(int slot, int v, int note, double glideFrom) noexcept
{
    Voice &voice = m_voices[std::size_t(v)];
    const bool sameSlot = m_voiceSlot[std::size_t(v)] == slot;
    m_voiceSlot[std::size_t(v)] = slot;
    m_started[std::size_t(v)] = ++m_counter;
    m_sustained[std::size_t(v)] = false;
    voice.setSpread(0.0, 0.0);
    if (voice.active() && (voice.note() != note || !sameSlot))
        voice.steal(note, m_state[std::size_t(slot)].velocity, glideFrom); // fade its old note out first
    else
        voice.noteOn(note, m_state[std::size_t(slot)].velocity, true, glideFrom);
}

void Engine::playMono(int slot, bool retrigger) noexcept
{
    SlotState &state = m_state[std::size_t(slot)];
    const int note = state.held[std::size_t(state.heldCount - 1)];
    const double glideFrom = state.lastNote >= 0 ? double(state.lastNote) : -1.0;
    const bool unison = state.mode == VoiceMode::Unison;
    const int count = unison ? kUnisonVoices : 1;
    const double detune = kMaxUnisonDetuneSemis * m_slots[std::size_t(slot)].get(int(ParamId::UnisonDetune));
    for (int i = 0; i < count; ++i) {
        int &v = state.monoVoices[std::size_t(i)];
        // Keep this slot's voice if it still has it; else take one.
        const bool kept = v >= 0 && m_voiceSlot[std::size_t(v)] == slot && m_voices[std::size_t(v)].active();
        if (!kept) {
            v = pickVoice(slot, -1);
            m_voiceSlot[std::size_t(v)] = slot;
        }
        Voice &voice = m_voices[std::size_t(v)];
        const double position = count > 1 ? 2.0 * i / (count - 1) - 1.0 : 0.0; // -1, 0, +1
        voice.setSpread(detune * position, unison ? kUnisonSpread * position : 0.0);
        m_started[std::size_t(v)] = ++m_counter;
        m_sustained[std::size_t(v)] = false;
        if (!kept && voice.active())
            voice.steal(note, state.velocity, glideFrom);
        else
            voice.noteOn(note, state.velocity, retrigger || !kept, glideFrom);
    }
    state.lastNote = note;
}

void Engine::noteOn(int slot, int note, int velocity) noexcept
{
    SlotState &state = m_state[std::size_t(slot)];
    state.velocity = float(velocity) / 127.0f;
    for (int i = 0; i < state.heldCount; ++i)
        if (state.held[std::size_t(i)] == note) {
            std::copy(state.held.begin() + i + 1, state.held.begin() + state.heldCount, state.held.begin() + i);
            --state.heldCount;
            break;
        }
    if (state.heldCount == int(state.held.size())) {
        std::copy(state.held.begin() + 1, state.held.end(), state.held.begin());
        --state.heldCount;
    }
    const bool othersHeld = state.heldCount > 0;
    state.held[std::size_t(state.heldCount++)] = std::uint8_t(note);

    switch (state.mode) {
    case VoiceMode::Poly: {
        const double glideFrom = state.lastNote >= 0 ? double(state.lastNote) : -1.0;
        startVoice(slot, pickVoice(slot, note), note, glideFrom);
        state.lastNote = note;
        break;
    }
    case VoiceMode::Mono:
    case VoiceMode::Unison:
        playMono(slot, true);
        break;
    case VoiceMode::Legato:
        if (!othersHeld) // a fresh phrase: no glide into it
            state.lastNote = -1;
        playMono(slot, !othersHeld);
        break;
    }
}

void Engine::releaseVoice(int slot, int v) noexcept
{
    if (m_state[std::size_t(slot)].sustain)
        m_sustained[std::size_t(v)] = true;
    else
        m_voices[std::size_t(v)].noteOff();
}

void Engine::noteOff(int slot, int note) noexcept
{
    SlotState &state = m_state[std::size_t(slot)];
    const bool wasTop = state.heldCount > 0 && state.held[std::size_t(state.heldCount - 1)] == note;
    for (int i = 0; i < state.heldCount; ++i)
        if (state.held[std::size_t(i)] == note) {
            std::copy(state.held.begin() + i + 1, state.held.begin() + state.heldCount, state.held.begin() + i);
            --state.heldCount;
            break;
        }
    if (state.mode == VoiceMode::Poly) {
        for (int v = 0; v < kVoices; ++v) {
            const Voice &voice = m_voices[std::size_t(v)];
            if (m_voiceSlot[std::size_t(v)] == slot && voice.gate() && voice.note() == note && !m_sustained[std::size_t(v)])
                releaseVoice(slot, v);
        }
        return;
    }
    if (!wasTop)
        return;
    if (state.heldCount > 0) {
        playMono(slot, state.mode != VoiceMode::Legato); // back to the previous key (Legato: without retrigger)
        return;
    }
    for (int v : state.monoVoices)
        if (v >= 0 && m_voiceSlot[std::size_t(v)] == slot)
            releaseVoice(slot, v);
}

void Engine::setSustain(int slot, bool down) noexcept
{
    m_state[std::size_t(slot)].sustain = down;
    if (down)
        return;
    for (int v = 0; v < kVoices; ++v)
        if (m_sustained[std::size_t(v)] && m_voiceSlot[std::size_t(v)] == slot) {
            m_sustained[std::size_t(v)] = false;
            m_voices[std::size_t(v)].noteOff();
        }
}

void Engine::allNotesOff(int slot) noexcept
{
    m_state[std::size_t(slot)].heldCount = 0;
    for (int v = 0; v < kVoices; ++v)
        if (m_voiceSlot[std::size_t(v)] == slot) {
            m_sustained[std::size_t(v)] = false;
            m_voices[std::size_t(v)].noteOff();
        }
}

void Engine::publishHeld() noexcept
{
    std::uint64_t mask[2] = {0, 0};
    for (const SlotState &state : m_state)
        for (int i = 0; i < state.heldCount; ++i) {
            const int n = state.held[std::size_t(i)];
            mask[n / 64] |= std::uint64_t(1) << (n % 64);
        }
    m_heldMask[0].store(mask[0], std::memory_order_relaxed);
    m_heldMask[1].store(mask[1], std::memory_order_relaxed);
}

void Engine::handleMidi(const krate::MidiEvent &event, bool keyboard) noexcept
{
    krate::MidiMessage m = krate::decode(event);
    const int midiChannel = event.data[0] & 0x0F;
    const int channel = keyboard ? -1 : midiChannel;
    // Octave shifts MIDI keys (the on-screen keyboard already shows the shifted range).
    if (!keyboard && m.type == krate::MidiMessage::Type::NoteOn) {
        const int shift = 12 * units::octaveShift(m_global.get(int(ParamId::Octave)));
        const int note = m.data1 + shift;
        if (note < 0 || note > 127)
            return;
        m_keyShift[std::size_t(midiChannel)][m.data1] = std::int8_t(shift);
        m.data1 = note;
    } else if (!keyboard && m.type == krate::MidiMessage::Type::NoteOff) {
        const int shift = m_keyShift[std::size_t(midiChannel)][m.data1];
        if (shift == kKeyUp)
            return;
        m_keyShift[std::size_t(midiChannel)][m.data1] = kKeyUp;
        m.data1 = m.data1 + shift;
    }
    const auto forSlots = [&](int note, auto &&action) {
        const unsigned slots = slotsFor(channel, note);
        for (int slot = 0; slot < SlotCount; ++slot)
            if (slots & (1u << slot))
                action(slot);
    };
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            store(r.param).setFromAudio(r.param, r.value); // learned knobs edit the edited slot
        else if (m.data1 == kCcModWheel)
            forSlots(-1, [&](int slot) { m_state[std::size_t(slot)].wheel = m.data2 / 127.0; });
        else if (m.data1 == kCcSustain)
            forSlots(-1, [&](int slot) { setSustain(slot, m.data2 >= 64); });
        else if (m.data1 == kCcAllNotesOff || m.data1 == kCcAllSoundOff)
            forSlots(-1, [&](int slot) { allNotesOff(slot); });
        break;
    case krate::MidiMessage::Type::NoteOn:
        forSlots(m.data1, [&](int slot) { noteOn(slot, m.data1, m.data2); });
        break;
    case krate::MidiMessage::Type::NoteOff:
        // To every slot holding that key: the lit slots or the split may have
        // changed since the note-on, and a stuck note is worse than a stray release.
        for (int slot = 0; slot < SlotCount; ++slot) {
            const SlotState &state = m_state[std::size_t(slot)];
            if (std::find(state.held.begin(), state.held.begin() + state.heldCount, m.data1)
                != state.held.begin() + state.heldCount)
                noteOff(slot, m.data1);
        }
        break;
    case krate::MidiMessage::Type::ProgramChange:
        // The programs controller loads it into the slots this channel reaches.
        if (const unsigned slots = slotsFor(channel, -1))
            m_programChanges.push({std::uint8_t(slots), std::uint8_t(m.data1)});
        break;
    case krate::MidiMessage::Type::Other:
        if (event.size == 3 && (event.data[0] & 0xF0) == 0xE0) { // pitch bend
            const int value = (event.data[2] << 7) | event.data[1];
            const double bend = std::clamp((value - 8192) / 8191.0, -1.0, 1.0);
            forSlots(-1, [&](int slot) { m_state[std::size_t(slot)].bend = bend; });
        }
        break;
    }
}

void Engine::render(float *left, float *right, std::uint32_t frames, double bpm) noexcept
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
        for (SlotBus &bus : m_bus) {
            std::fill_n(bus.left2x.begin(), 2 * n, 0.0f);
            std::fill_n(bus.right2x.begin(), 2 * n, 0.0f);
        }
        // Each voice into its slot's bus.
        for (int v = 0; v < kVoices; ++v) {
            Voice &voice = m_voices[std::size_t(v)];
            if (!voice.active())
                continue;
            const auto slot = std::size_t(m_voiceSlot[std::size_t(v)]);
            SlotBus &bus = m_bus[slot];
            if (!m_morphing[slot]) {
                voice.render(bus.left2x.data(), bus.right2x.data(), n, m_lfo2Values[slot].data(), m_vibratoValues.data(),
                             m_settings[slot]);
                continue;
            }
            // Morph: the slot's wheel, or this voice's velocity, between panel and targets.
            const double amount = m_wheelMorph[slot] ? m_state[slot].wheel : voice.velocity();
            const SlotSettings morphed = blend(m_settings[slot], m_morphSettings[slot], amount);
            voice.render(bus.left2x.data(), bus.right2x.data(), n, m_lfo2Values[slot].data(), m_vibratoValues.data(),
                         morphed);
        }
        // Each slot: decimate, its effects, into the mix.
        std::fill_n(left, n, 0.0f);
        std::fill_n(right, n, 0.0f);
        for (int slot = 0; slot < SlotCount; ++slot) {
            SlotBus &bus = m_bus[std::size_t(slot)];
            const Store &p = m_slots[std::size_t(slot)];
            for (std::uint32_t i = 0; i < n; ++i) {
                bus.left[i] = bus.decimateL.process(bus.left2x[2 * i], bus.left2x[2 * i + 1]);
                bus.right[i] = bus.decimateR.process(bus.right2x[2 * i], bus.right2x[2 * i + 1]);
            }
            static constexpr int kChorusModes[] = {krate::Chorus::Off, krate::Chorus::I, krate::Chorus::Ensemble};
            bus.chorus.processStereo(bus.left.data(), bus.right.data(), n,
                                     kChorusModes[units::index(p.get(int(ParamId::Chorus)), positions::kChorus)]);
            const double delayLevel = p.get(int(ParamId::DelayLevel));
            if (delayLevel > 0.0) {
                if (!bus.delayOn) // turned up: no stale echoes from before
                    bus.delay.clear();
                const double beats = kDelayBeats[units::index(p.get(int(ParamId::DelayTime)), positions::kDelayTime)];
                bus.delay.process(bus.left.data(), bus.right.data(), n, beats * 60.0 / std::max(bpm, 1.0), delayLevel,
                                  kMaxDelayFeedback * p.get(int(ParamId::DelayFb)));
            }
            bus.delayOn = delayLevel > 0.0;
            const double reverbLevel = p.get(int(ParamId::ReverbLevel));
            if (reverbLevel > 0.0) {
                if (!bus.reverbOn)
                    bus.reverb.clear();
                bus.reverb.process(bus.left.data(), bus.right.data(), n, p.get(int(ParamId::ReverbSize)), reverbLevel);
            }
            bus.reverbOn = reverbLevel > 0.0;
            for (std::uint32_t i = 0; i < n; ++i) {
                left[i] += bus.left[i];
                right[i] += bus.right[i];
            }
        }
        left += n;
        right += n;
        frames -= n;
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    krate::ScopedFlushDenormals noDenormals;
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    for (int slot = 0; slot < SlotCount; ++slot)
        if (const VoiceMode mode = voiceMode(slot); mode != m_state[std::size_t(slot)].mode) {
            allNotesOff(slot); // a new voice mode starts from silence (release tails ring out)
            m_state[std::size_t(slot)].mode = mode;
            m_state[std::size_t(slot)].lastNote = -1;
        }

    const double bpm = block.transport ? block.transport->bpm : units::tempoBpm(m_global.get(int(ParamId::Tempo)));

    // The on-screen keyboard and wheels, at the start of the block.
    while (const auto event = m_gui.pop())
        handleMidi(*event, true);

    // Render up to each MIDI event, then apply it on its exact frame.
    std::uint32_t done = 0;
    for (std::uint32_t e = 0; e <= block.midiCount; ++e) {
        const std::uint32_t until = e < block.midiCount ? std::min(block.midi[e].frame, block.frames) : block.frames;
        if (until > done) {
            for (int slot = 0; slot < SlotCount; ++slot) { // bend and wheel can change between events
                const auto i = std::size_t(slot);
                m_settings[i] = settings(slot);
                m_morphing[i] = hasMorph(slot);
                if (m_morphing[i]) {
                    m_morphSettings[i] = settings(slot, true);
                    m_wheelMorph[i] = units::index(m_slots[i].get(int(ParamId::WheelDest)), positions::kWheelDest) == 0;
                }
            }
            render(left + done, right + done, until - done, bpm);
            done = until;
        }
        if (e < block.midiCount)
            handleMidi(block.midi[e]);
    }
    publishHeld();

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
