#include "rack/Rack.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "core/Midi.h"

namespace krate {

Rack::Rack(std::uint32_t maxFrames)
    : m_maxFrames(std::max<std::uint32_t>(maxFrames, 1))
    , m_silence(m_maxFrames, 0.0f)
{
    for (auto &bus : m_bus)
        bus.assign(m_maxFrames, 0.0f);
    m_dry.assign(2 * std::size_t(m_maxFrames), 0.0f);
    m_quietFrames.fill(~std::uint64_t(0) / 2);
    for (auto &keys : m_noteDevice)
        keys.fill(-1);
    for (auto &c : m_channelDevice)
        c.store(-1, std::memory_order_relaxed);
    clearMappings();
}

int Rack::addDevice(IInstrument &instrument, int midiChannel)
{
    if (m_count == kMaxDevices)
        return -1;
    Device &d = m_devices[std::size_t(m_count)];
    d.instrument = &instrument;
    for (int b = 0; b < ChannelEq::Count; ++b)
        d.eq[std::size_t(b)].store(ChannelEq::kFlat[std::size_t(b)], std::memory_order_relaxed);
    d.left.assign(m_maxFrames, 0.0f);
    d.right.assign(m_maxFrames, 0.0f);
    d.raw.assign(m_maxFrames, 0.0f);
    if (midiChannel >= 0 && midiChannel < kChannels)
        m_channelDevice[std::size_t(midiChannel)].store(std::int8_t(m_count), std::memory_order_relaxed);
    return m_count++;
}

void Rack::prepare(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    for (int i = 0; i < m_count; ++i) {
        m_devices[std::size_t(i)].instrument->prepare(sampleRate, m_maxFrames);
        m_devices[std::size_t(i)].eqDsp.prepare(sampleRate);
    }
    m_reverb.prepare(sampleRate);
    m_delay.prepare(sampleRate);
}

// --- Any thread ---------------------------------------------------------------

void Rack::setFocus(int device) noexcept
{
    if (device >= 0 && device < m_count)
        m_focus.store(device, std::memory_order_relaxed);
}

void Rack::setInputSource(int device, int source) noexcept
{
    if (device < 0 || device >= m_count)
        return;
    // Not itself, nor another device that takes inputs (no loops).
    if (source == device || source < -1 || source >= m_count || (source >= 0 && takesInput(source)))
        source = -1;
    m_devices[std::size_t(device)].inputSource.store(source, std::memory_order_relaxed);
}

int Rack::inputSource(int device) const noexcept
{
    return device >= 0 && device < m_count ? m_devices[std::size_t(device)].inputSource.load(std::memory_order_relaxed)
                                           : -1;
}

void Rack::setChannelDevice(int channel, int device) noexcept
{
    if (channel >= 0 && channel < kChannels)
        m_channelDevice[std::size_t(channel)].store(std::int8_t(device >= 0 && device < m_count ? device : -1),
                                                    std::memory_order_relaxed);
}

int Rack::channelDevice(int channel) const noexcept
{
    return (channel >= 0 && channel < kChannels) ? m_channelDevice[std::size_t(channel)].load(std::memory_order_relaxed)
                                                 : -1;
}

void Rack::setLevel(int device, float gain) noexcept
{
    if (device >= 0 && device < m_count)
        m_devices[std::size_t(device)].level.store(std::max(gain, 0.0f), std::memory_order_relaxed);
}

void Rack::setPan(int device, float pan) noexcept
{
    if (device >= 0 && device < m_count)
        m_devices[std::size_t(device)].pan.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_relaxed);
}

void Rack::setEq(int device, int band, float value) noexcept
{
    if (device >= 0 && device < m_count && band >= 0 && band < ChannelEq::Count)
        m_devices[std::size_t(device)].eq[std::size_t(band)].store(value, std::memory_order_relaxed);
}

float Rack::eq(int device, int band) const noexcept
{
    if (device < 0 || device >= m_count || band < 0 || band >= ChannelEq::Count)
        return 0.0f;
    return m_devices[std::size_t(device)].eq[std::size_t(band)].load(std::memory_order_relaxed);
}

void Rack::setSend(int device, int effect, float level) noexcept
{
    if (device >= 0 && device < m_count && effect >= 0 && effect < EffectCount)
        m_devices[std::size_t(device)].sends[std::size_t(effect)].store(std::clamp(level, 0.0f, 1.0f),
                                                                        std::memory_order_relaxed);
}

float Rack::send(int device, int effect) const noexcept
{
    if (device < 0 || device >= m_count || effect < 0 || effect >= EffectCount)
        return 0.0f;
    return m_devices[std::size_t(device)].sends[std::size_t(effect)].load(std::memory_order_relaxed);
}

void Rack::setReverb(float level, float size) noexcept
{
    m_reverbLevel.store(std::clamp(level, 0.0f, 1.0f), std::memory_order_relaxed);
    m_reverbSize.store(std::clamp(size, 0.0f, 1.0f), std::memory_order_relaxed);
}

void Rack::setDelay(float level, int division, float feedback) noexcept
{
    m_delayLevel.store(std::clamp(level, 0.0f, 1.0f), std::memory_order_relaxed);
    m_delayDivision.store(std::clamp(division, 0, int(kDelayBeats.size()) - 1), std::memory_order_relaxed);
    m_delayFeedback.store(std::clamp(feedback, 0.0f, 0.9f), std::memory_order_relaxed);
}

void Rack::setMute(int device, bool mute) noexcept
{
    if (device >= 0 && device < m_count)
        m_devices[std::size_t(device)].mute.store(mute, std::memory_order_relaxed);
}

void Rack::setSolo(int device, bool solo) noexcept
{
    if (device >= 0 && device < m_count)
        m_devices[std::size_t(device)].solo.store(solo, std::memory_order_relaxed);
}

float Rack::takeDevicePeak(int device) noexcept
{
    if (device < 0 || device >= m_count)
        return 0.0f;
    return m_devices[std::size_t(device)].peak.exchange(0.0f, std::memory_order_relaxed);
}

// Packed mapping: bits 29-31 kind, 16-23 device, 0-15 target. 0 = none.
namespace {
// Buttons fire on a press, can be learned from notes, and don't fire on the press that teaches them.
bool isButton(const Rack::Mapping &m)
{
    using Kind = Rack::Mapping::Kind;
    return m.kind == Kind::FocusAction || m.kind == Kind::Action
           || (m.kind == Kind::Mixer && Rack::isMixerButton(m.target));
}
} // namespace

std::uint32_t Rack::pack(Mapping m) noexcept
{
    if (m.kind == Mapping::Kind::None)
        return 0;
    return (std::uint32_t(m.kind) << 29) | (std::uint32_t(m.device & 0xFF) << 16) | std::uint32_t(m.target & 0xFFFF);
}

Rack::Mapping Rack::unpack(std::uint32_t packed) noexcept
{
    Mapping m;
    m.kind = Mapping::Kind(packed >> 29);
    if (m.kind == Mapping::Kind::None)
        return m;
    m.device = int((packed >> 16) & 0xFF);
    m.target = int(packed & 0xFFFF);
    return m;
}

void Rack::setCcMapping(int channel, int cc, Mapping mapping) noexcept
{
    if (channel >= 0 && channel <= kAnyChannel && cc >= 0 && cc < 128)
        m_ccMap[std::size_t(channel)][std::size_t(cc)].store(pack(mapping), std::memory_order_relaxed);
}

Rack::Mapping Rack::ccMapping(int channel, int cc) const noexcept
{
    if (channel < 0 || channel > kAnyChannel || cc < 0 || cc >= 128)
        return {};
    return unpack(m_ccMap[std::size_t(channel)][std::size_t(cc)].load(std::memory_order_relaxed));
}

void Rack::setNoteMapping(int channel, int note, Mapping mapping) noexcept
{
    if (!isButton(mapping))
        mapping = {};
    if (channel >= 0 && channel < kChannels && note >= 0 && note < 128)
        m_noteMap[std::size_t(channel)][std::size_t(note)].store(pack(mapping), std::memory_order_relaxed);
}

Rack::Mapping Rack::noteMapping(int channel, int note) const noexcept
{
    if (channel < 0 || channel >= kChannels || note < 0 || note >= 128)
        return {};
    return unpack(m_noteMap[std::size_t(channel)][std::size_t(note)].load(std::memory_order_relaxed));
}

void Rack::clearMappings() noexcept
{
    for (auto &row : m_ccMap)
        for (auto &e : row)
            e.store(0, std::memory_order_relaxed);
    for (auto &row : m_noteMap)
        for (auto &e : row)
            e.store(0, std::memory_order_relaxed);
}

void Rack::arm(Mapping target) noexcept
{
    m_armed.store(pack(target), std::memory_order_relaxed);
}

// --- Audio thread ---------------------------------------------------------------

bool Rack::runAction(int action) noexcept
{
    if (m_count == 0)
        return false;
    const int current = m_focus.load(std::memory_order_relaxed);
    const auto focusable = [this](int d) { return m_devices[std::size_t(d)].focusable.load(std::memory_order_relaxed); };
    int next = current;
    if (action == kFocusNext || action == kFocusPrevious) { // the next one that can take focus
        const int step = action == kFocusNext ? 1 : m_count - 1;
        for (int i = 0; i < m_count; ++i) {
            next = (next + step) % m_count;
            if (focusable(next))
                break;
        }
    } else if (action >= 0 && action < m_count && focusable(action)) {
        next = action;
    } else {
        return false;
    }
    m_focus.store(next, std::memory_order_relaxed);
    return true;
}

void Rack::forward(int device, const MidiEvent &event) noexcept
{
    if (device < 0 || device >= m_count)
        return;
    Device &d = m_devices[std::size_t(device)];
    if (d.midiCount < kMaxEventsPerDevice)
        d.midi[d.midiCount++] = event;
}

int Rack::resolveFollowFocus(const Mapping &m) const noexcept
{
    // The parameter key learned on one device, looked up on the focused one.
    const int focused = m_focus.load(std::memory_order_relaxed);
    if (m.device < 0 || m.device >= m_count || focused < 0 || focused >= m_count)
        return -1;
    const auto source = m_devices[std::size_t(m.device)].instrument->parameters();
    if (m.target < 0 || m.target >= int(source.size()))
        return -1;
    const std::string_view key = source[std::size_t(m.target)].key;
    const auto params = m_devices[std::size_t(focused)].instrument->parameters();
    for (std::size_t i = 0; i < params.size(); ++i)
        if (params[i].key == key)
            return int(i);
    return -1;
}

void Rack::route(MidiEvent event) noexcept
{
    const MidiMessage m = decode(event);
    const int channel = m.channel;
    const int focused = m_focus.load(std::memory_order_relaxed);
    const int assigned = m_channelDevice[std::size_t(channel)].load(std::memory_order_relaxed);
    const int keys = m_keysChannel.load(std::memory_order_relaxed);
    const bool followsFocus = keys == kAnyChannel || keys == channel; // the keyboard: always the focused device
    const int target = assigned >= 0 && !followsFocus ? assigned : focused;

    switch (m.type) {
    case MidiMessage::Type::ControlChange: {
        // Learning takes the CC (with its channel). A learned knob applies its value
        // at once; a learned button does not also fire on the press that taught it.
        if (const Mapping armed = unpack(m_armed.load(std::memory_order_relaxed)); armed.kind != Mapping::Kind::None) {
            m_ccMap[std::size_t(channel)][m.data1].store(pack(armed), std::memory_order_relaxed);
            m_armed.store(0, std::memory_order_relaxed);
            m_learned.push({false, std::uint8_t(channel), m.data1, armed});
            if (isButton(armed))
                break;
        }
        Mapping map = unpack(m_ccMap[std::size_t(channel)][m.data1].load(std::memory_order_relaxed));
        if (map.kind == Mapping::Kind::None)
            map = unpack(m_ccMap[kAnyChannel][m.data1].load(std::memory_order_relaxed));

        switch (map.kind) {
        case Mapping::Kind::None:
            forward(target, event); // the device may have its own use for it
            break;
        case Mapping::Kind::Param:
            if (map.device >= 0 && map.device < m_count) {
                IInstrument &inst = *m_devices[std::size_t(map.device)].instrument;
                const auto params = inst.parameters();
                if (map.target >= 0 && map.target < int(params.size()))
                    inst.setParamFromHost(map.target, snapToDetent(params[std::size_t(map.target)], float(m.data2) / 127.0f));
            }
            break;
        case Mapping::Kind::FollowFocus:
            if (const int index = resolveFollowFocus(map); index >= 0) {
                IInstrument &inst = *m_devices[std::size_t(focused)].instrument;
                inst.setParamFromHost(index, snapToDetent(inst.parameters()[std::size_t(index)], float(m.data2) / 127.0f));
            }
            break;
        case Mapping::Kind::FocusAction:
            if (m.data2 > 0) // a button press, not its release
                runAction(map.target);
            break;
        case Mapping::Kind::Action:
            if (m.data2 > 0)
                m_fired.push({map.device, map.target});
            break;
        case Mapping::Kind::Mixer:
            if (!isMixerButton(map.target) || m.data2 > 0)
                m_mixerMoves.push({map.device, map.target, m.data2});
            break;
        }
        break;
    }
    case MidiMessage::Type::NoteOn:
    case MidiMessage::Type::NoteOff: {
        if (m.type == MidiMessage::Type::NoteOn) {
            // Only buttons (focus actions and actions) are learned from notes.
            const Mapping armed = unpack(m_armed.load(std::memory_order_relaxed));
            if (isButton(armed)) {
                m_noteMap[std::size_t(channel)][m.data1].store(pack(armed), std::memory_order_relaxed);
                m_armed.store(0, std::memory_order_relaxed);
                m_learned.push({true, std::uint8_t(channel), m.data1, armed});
                break; // the teaching press doesn't fire; its note-off is swallowed below
            }
        }
        if (const Mapping button = unpack(m_noteMap[std::size_t(channel)][m.data1].load(std::memory_order_relaxed));
            button.kind != Mapping::Kind::None) {
            if (m.type == MidiMessage::Type::NoteOn) {
                if (button.kind == Mapping::Kind::FocusAction)
                    runAction(button.target);
                else if (button.kind == Mapping::Kind::Mixer)
                    m_mixerMoves.push({button.device, button.target, m.data2});
                else
                    m_fired.push({button.device, button.target});
            }
            break; // the note is the button: don't play it
        }
        // A note ends where it started (focus or routing may have changed since).
        std::int8_t &owner = m_noteDevice[std::size_t(channel)][m.data1 & 0x7F];
        if (m.type == MidiMessage::Type::NoteOn) {
            owner = std::int8_t(target);
            forward(target, event);
        } else {
            forward(owner >= 0 ? owner : target, event);
            owner = -1;
        }
        break;
    }
    case MidiMessage::Type::ProgramChange:
        if (m_programFocus.load(std::memory_order_relaxed) && m.data1 < m_count)
            m_focus.store(m.data1, std::memory_order_relaxed);
        else
            forward(target, event);
        break;
    case MidiMessage::Type::Other:
        forward(target, event);
        break;
    }
}

void Rack::process(const RackBlock &block) noexcept
{
    std::uint32_t nextEvent = 0, nextDeviceEvent = 0;
    for (std::uint32_t offset = 0; offset < block.frames; offset += m_maxFrames)
        processChunk(block, offset, std::min(m_maxFrames, block.frames - offset), nextEvent, nextDeviceEvent);
}

void Rack::processChunk(const RackBlock &block, std::uint32_t offset, std::uint32_t frames, std::uint32_t &nextEvent,
                        std::uint32_t &nextDeviceEvent) noexcept
{
    // Transport for this chunk (only the first chunk carries `restarted`).
    TransportState transport;
    if (block.transport) {
        transport = *block.transport;
        if (offset > 0) {
            transport.restarted = false;
            if (transport.playing)
                transport.beatAtBlockStart += double(offset) * transport.bpm / 60.0 / m_sampleRate;
        }
    }

    // Route this chunk's MIDI. Each device gets its events re-based to the chunk.
    for (int i = 0; i < m_count; ++i)
        m_devices[std::size_t(i)].midiCount = 0;
    while (nextEvent < block.midiCount && block.midi[nextEvent].frame < offset + frames) {
        MidiEvent ev = block.midi[nextEvent++];
        ev.frame -= offset;
        route(ev);
    }
    // Events addressed to devices (a song's tracks), merged in by frame.
    if (nextDeviceEvent < block.deviceMidiCount && block.deviceMidi[nextDeviceEvent].frame() < offset + frames) {
        while (nextDeviceEvent < block.deviceMidiCount && block.deviceMidi[nextDeviceEvent].frame() < offset + frames) {
            MidiEvent ev = block.deviceMidi[nextDeviceEvent].event;
            const int device = block.deviceMidi[nextDeviceEvent++].device;
            ev.frame -= offset;
            forward(device, ev);
        }
        for (int i = 0; i < m_count; ++i) { // insertion sort: both sources were in order
            Device &d = m_devices[std::size_t(i)];
            for (std::uint32_t k = 1; k < d.midiCount; ++k)
                for (std::uint32_t j = k; j > 0 && d.midi[j - 1].frame > d.midi[j].frame; --j)
                    std::swap(d.midi[j - 1], d.midi[j]);
        }
    }

    // Run every device on the shared transport.
    bool anySolo = false;
    for (int i = 0; i < m_count; ++i)
        anySolo = anySolo || m_devices[std::size_t(i)].solo.load(std::memory_order_relaxed);

    std::fill_n(block.masterL + offset, frames, 0.0f);
    std::fill_n(block.masterR + offset, frames, 0.0f);
    for (auto &bus : m_bus)
        std::fill_n(bus.data(), frames, 0.0f);

    // The devices fed by another device's output go after the others.
    std::uint32_t feeds = 0, fed = 0;
    for (int i = 0; i < m_count; ++i)
        if (const int source = m_devices[std::size_t(i)].inputSource.load(std::memory_order_relaxed);
            source >= 0 && m_devices[std::size_t(i)].instrument->inputChannels() > 0) {
            feeds |= 1u << source;
            fed |= 1u << i;
        }
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < m_count; ++i)
            if (bool((fed >> i) & 1u) == (pass == 1))
                processDevice(i, block, offset, frames, transport, anySolo, (feeds >> i) & 1u);

    // The effects: each bus through its effect, only the effect's sound into the master.
    processEffects(block, offset, frames);

    // Master level, then the master stage (soft clip + meter).
    const float masterTarget = m_masterLevel.load(std::memory_order_relaxed);
    if (!m_primed)
        m_masterGain = masterTarget;
    m_primed = true;
    const float masterStep = (masterTarget - m_masterGain) / float(frames);
    for (std::uint32_t s = 0; s < frames; ++s) {
        m_masterGain += masterStep;
        block.masterL[offset + s] *= m_masterGain;
        block.masterR[offset + s] *= m_masterGain;
    }
    m_masterGain = masterTarget;
    float *const master[2] = {block.masterL + offset, block.masterR + offset};
    m_master.process(master, 2, frames);
}

void Rack::processDevice(int i, const RackBlock &block, std::uint32_t offset, std::uint32_t frames,
                         const TransportState &transport, bool anySolo, bool feedsAnother) noexcept
{
    Device &d = m_devices[std::size_t(i)];
    float *const outputs[2] = {d.left.data(), d.right.data()};
    // Its inputs: the rack's, in order (any beyond them are silent), or another
    // device's output, summed to mono, on every input.
    const float *inputs[kInputs + 6];
    const int inputCount = std::min(d.instrument->inputChannels(), int(std::size(inputs)));
    const int source = d.inputSource.load(std::memory_order_relaxed);
    for (int c = 0; c < inputCount; ++c)
        inputs[c] = source >= 0 ? m_devices[std::size_t(source)].raw.data()
                    : block.inputs && c < kInputs ? block.inputs[c] + offset
                                                 : m_silence.data();
    d.instrument->process(AudioBlock{outputs, frames, d.midi.data(), d.midiCount,
                                     block.transport ? &transport : nullptr, inputCount > 0 ? inputs : nullptr});

    if (feedsAnother) // a device takes its sound as its input
        for (std::uint32_t s = 0; s < frames; ++s)
            d.raw[s] = 0.5f * (d.left[s] + d.right[s]);

    if (block.deviceOutputs) { // the device's own output, before the mixer
        std::copy_n(d.left.data(), frames, block.deviceOutputs[2 * i] + offset);
        std::copy_n(d.right.data(), frames, block.deviceOutputs[2 * i + 1] + offset);
    }

    // The strip's EQ (nothing at all while it's flat).
    ChannelEq::Settings eq;
    for (int b = 0; b < ChannelEq::Count; ++b)
        eq[std::size_t(b)] = d.eq[std::size_t(b)].load(std::memory_order_relaxed);
    d.eqDsp.process(eq, d.left.data(), d.right.data(), frames);

    // Fader: level x balance pan; muted (or not soloed while something is) = 0.
    const bool audible = anySolo ? d.solo.load(std::memory_order_relaxed) : !d.mute.load(std::memory_order_relaxed);
    const float level = audible ? d.level.load(std::memory_order_relaxed) : 0.0f;
    const float pan = d.pan.load(std::memory_order_relaxed);
    const float targetL = level * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    const float targetR = level * (pan >= 0.0f ? 1.0f : 1.0f + pan);

    if (!m_primed) { // start where the faders are, no fade from unity
        d.gainL = targetL;
        d.gainR = targetR;
    }
    // Ramp gains across the chunk so fader moves don't click.
    const float stepL = (targetL - d.gainL) / float(frames);
    const float stepR = (targetR - d.gainR) / float(frames);
    // Effect sends, after the fader and pan (ramped too).
    std::array<float, EffectCount> sendStep{};
    bool sending = false;
    for (int e = 0; e < EffectCount; ++e) {
        const float target = d.sends[std::size_t(e)].load(std::memory_order_relaxed);
        if (!m_primed)
            d.sendGain[std::size_t(e)] = target;
        sendStep[std::size_t(e)] = (target - d.sendGain[std::size_t(e)]) / float(frames);
        sending = sending || target > 0.0f || d.sendGain[std::size_t(e)] > 0.0f;
    }
    float peak = 0.0f;
    for (std::uint32_t s = 0; s < frames; ++s) {
        d.gainL += stepL;
        d.gainR += stepR;
        const float l = d.left[s] * d.gainL;
        const float r = d.right[s] * d.gainR;
        block.masterL[offset + s] += l;
        block.masterR[offset + s] += r;
        if (sending)
            for (int e = 0; e < EffectCount; ++e) {
                float &g = d.sendGain[std::size_t(e)];
                g += sendStep[std::size_t(e)];
                m_bus[std::size_t(2 * e)][s] += l * g;
                m_bus[std::size_t(2 * e + 1)][s] += r * g;
            }
        peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
    }
    d.gainL = targetL;
    d.gainR = targetR;
    if (peak > d.peak.load(std::memory_order_relaxed))
        d.peak.store(peak, std::memory_order_relaxed);
}

void Rack::processEffects(const RackBlock &block, std::uint32_t offset, std::uint32_t frames) noexcept
{
    const double bpm = block.transport ? std::max(block.transport->bpm, 1.0) : 120.0;
    for (int e = 0; e < EffectCount; ++e) {
        float *l = m_bus[std::size_t(2 * e)].data();
        float *r = m_bus[std::size_t(2 * e + 1)].data();
        const float level = (e == Reverb ? m_reverbLevel : m_delayLevel).load(std::memory_order_relaxed);
        bool input = false;
        for (std::uint32_t s = 0; s < frames && !input; ++s)
            input = l[s] != 0.0f || r[s] != 0.0f;
        std::uint64_t &quiet = m_quietFrames[std::size_t(e)];
        quiet = input ? 0 : quiet + frames;
        // Nothing sent for longer than the longest tail (8 s reverb, the delay's
        // echoes): skip it, and start clean next time.
        const auto ringFrames = std::uint64_t(12.0 * m_sampleRate);
        if (level <= 0.0f || quiet > ringFrames) {
            if (quiet > ringFrames && quiet - frames <= ringFrames)
                (e == Reverb ? static_cast<void>(m_reverb.clear()) : m_delay.clear());
            continue;
        }
        std::copy_n(l, frames, m_dry.data());
        std::copy_n(r, frames, m_dry.data() + frames);
        if (e == Reverb) {
            m_reverb.process(l, r, frames, m_reverbSize.load(std::memory_order_relaxed), 1.0);
        } else {
            const double beats = kDelayBeats[std::size_t(m_delayDivision.load(std::memory_order_relaxed))];
            m_delay.process(l, r, frames, beats * 60.0 / bpm, 1.0, m_delayFeedback.load(std::memory_order_relaxed));
        }
        for (std::uint32_t s = 0; s < frames; ++s) { // the effect alone, at its return level
            block.masterL[offset + s] += level * (l[s] - m_dry[s]);
            block.masterR[offset + s] += level * (r[s] - m_dry[frames + s]);
        }
    }
}

} // namespace krate
