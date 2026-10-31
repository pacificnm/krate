#include "konnekt/dsp/Engine.h"

#include <algorithm>
#include <cmath>

#include "core/Denormals.h"
#include "core/Midi.h"

namespace konnekt {

namespace {
std::string volumeText(float v) { return std::to_string(int(std::lround(v * 100.0f))) + "%"; }
constexpr std::array<krate::ParamDescriptor, 1> kParams{{
    {"volume", "Volume", 0.8f, 0, &volumeText},
}};
constexpr int kCcModWheel = 1;
constexpr int kCcSustain = 64;
constexpr int kCcAllNotesOff = 123;
constexpr double kTraceSeconds = 1.5;
} // namespace

Engine::Engine()
    : m_params(kParams)
    , m_buffers(std::size_t(kMaxModules) * kMaxJacks * kVoices * kBlock, 0.0f)
    , m_sums(std::size_t(kMaxModules) * kMaxJacks * kBlock, 0.0f)
    , m_silence(kBlock, 0.0f)
    , m_traces(std::size_t(kMaxModules) * kMaxJacks)
{
    m_voiceNote.fill(-1);
    m_pendingNote.fill(-1);
    for (auto &cc : m_ccMap)
        cc.store(-1, std::memory_order_relaxed);
}

std::span<const krate::ParamDescriptor> Engine::parameters() const noexcept
{
    return kParams;
}

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_rate.store(sampleRate, std::memory_order_relaxed);
    m_traceBin = std::max(1, int(kTraceSeconds * sampleRate / kTracePoints));
    for (Module *m : m_modules)
        if (m)
            m->prepare(sampleRate);
    m_volume = -1.0;
}

// --- Commands --------------------------------------------------------------------------

bool Engine::addModule(int slot, Module *module) noexcept
{
    if (slot < 0 || slot >= kMaxModules || !module)
        return false;
    Command c{Command::Kind::Add};
    c.slot = std::int8_t(slot);
    c.module = module;
    return m_commands.push(c);
}

bool Engine::removeModule(int slot) noexcept
{
    if (slot < 0 || slot >= kMaxModules)
        return false;
    Command c{Command::Kind::Remove};
    c.slot = std::int8_t(slot);
    return m_commands.push(c);
}

bool Engine::connect(int toSlot, int toJack, int fromSlot, int fromJack) noexcept
{
    if (toSlot < 0 || toSlot >= kMaxModules || fromSlot < 0 || fromSlot >= kMaxModules || toJack < 0
        || toJack >= kMaxJacks || fromJack < 0 || fromJack >= kMaxJacks)
        return false;
    Command c{Command::Kind::Connect};
    c.slot = std::int8_t(toSlot);
    c.jack = std::int8_t(toJack);
    c.fromSlot = std::int8_t(fromSlot);
    c.fromJack = std::int8_t(fromJack);
    return m_commands.push(c);
}

bool Engine::disconnect(int toSlot, int toJack) noexcept
{
    if (toSlot < 0 || toSlot >= kMaxModules || toJack < 0 || toJack >= kMaxJacks)
        return false;
    Command c{Command::Kind::Disconnect};
    c.slot = std::int8_t(toSlot);
    c.jack = std::int8_t(toJack);
    return m_commands.push(c);
}

bool Engine::setOrder(std::span<const int> slots) noexcept
{
    Command c{Command::Kind::Order};
    for (int s : slots)
        if (s >= 0 && s < kMaxModules && c.count < kMaxModules)
            c.order[std::size_t(c.count++)] = std::int8_t(s);
    return m_commands.push(c);
}

Module *Engine::popRetired() noexcept
{
    if (const auto m = m_retired.pop())
        return *m;
    return nullptr;
}

void Engine::apply(const Command &c) noexcept
{
    switch (c.kind) {
    case Command::Kind::Add:
        if (m_modules[std::size_t(c.slot)])
            m_retired.push(m_modules[std::size_t(c.slot)]);
        m_modules[std::size_t(c.slot)] = c.module;
        m_inputs[std::size_t(c.slot)] = {};
        std::fill_n(buffer(c.slot, 0, 0), std::size_t(kMaxJacks) * kVoices * kBlock, 0.0f);
        break;
    case Command::Kind::Remove:
        if (m_modules[std::size_t(c.slot)])
            m_retired.push(m_modules[std::size_t(c.slot)]);
        m_modules[std::size_t(c.slot)] = nullptr;
        m_inputs[std::size_t(c.slot)] = {};
        for (auto &inputs : m_inputs) // cables from it are gone too
            for (Source &s : inputs)
                if (s.slot == c.slot)
                    s = {};
        break;
    case Command::Kind::Connect:
        m_inputs[std::size_t(c.slot)][std::size_t(c.jack)] = {c.fromSlot, c.fromJack};
        break;
    case Command::Kind::Disconnect:
        m_inputs[std::size_t(c.slot)][std::size_t(c.jack)] = {};
        break;
    case Command::Kind::Order:
        m_order = c.order;
        m_orderCount = c.count;
        break;
    }
}

std::vector<int> Engine::signalOrder(std::span<const int> slots, std::span<const Cable> cables)
{
    // Kahn: a module runs once everything feeding it has; what's left is in a loop.
    std::vector<int> order, pending(slots.begin(), slots.end());
    std::sort(pending.begin(), pending.end());
    while (!pending.empty()) {
        bool progressed = false;
        for (std::size_t k = 0; k < pending.size(); ++k) {
            const int s = pending[k];
            const bool ready = std::none_of(cables.begin(), cables.end(), [&](const Cable &c) {
                return c.toSlot == s && c.fromSlot != s
                       && std::find(pending.begin(), pending.end(), c.fromSlot) != pending.end();
            });
            if (ready) {
                order.push_back(s);
                pending.erase(pending.begin() + std::ptrdiff_t(k));
                progressed = true;
                break;
            }
        }
        if (!progressed) { // a loop: the lowest slot goes first, its loop cable a block late
            order.push_back(pending.front());
            pending.erase(pending.begin());
        }
    }
    return order;
}

// --- MIDI Learn ----------------------------------------------------------------------------

void Engine::armLearn(int slot, int knob) noexcept
{
    m_armed.store(slot >= 0 && knob >= 0 ? slot * kMaxKnobs + knob : -1, std::memory_order_relaxed);
}

std::optional<Engine::Learned> Engine::popLearned() noexcept
{
    return m_learned.pop();
}

void Engine::setCcMapping(int cc, int slot, int knob) noexcept
{
    if (cc >= 0 && cc < 128)
        m_ccMap[std::size_t(cc)].store(slot >= 0 ? slot * kMaxKnobs + knob : -1, std::memory_order_relaxed);
}

void Engine::forgetSlot(int slot) noexcept
{
    for (auto &cc : m_ccMap)
        if (const int t = cc.load(std::memory_order_relaxed); t >= 0 && t / kMaxKnobs == slot)
            cc.store(-1, std::memory_order_relaxed);
}

std::optional<std::pair<int, int>> Engine::ccMapping(int cc) const noexcept
{
    if (cc < 0 || cc >= 128)
        return std::nullopt;
    const int t = m_ccMap[std::size_t(cc)].load(std::memory_order_relaxed);
    if (t < 0)
        return std::nullopt;
    return std::make_pair(t / kMaxKnobs, t % kMaxKnobs);
}

// --- Voices ------------------------------------------------------------------------------

void Engine::noteOn(int note, int velocity) noexcept
{
    const int voices = m_voiceCount.load(std::memory_order_relaxed);
    const float vel = float(velocity) / 127.0f;
    const float pitch = float(note - 60) / 12.0f;
    if (voices == 1) { // one voice: the newest key, legato over held keys
        for (int i = 0; i < m_heldCount; ++i)
            if (m_held[std::size_t(i)] == note) {
                std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
                --m_heldCount;
                break;
            }
        if (m_heldCount < int(m_held.size()))
            m_held[std::size_t(m_heldCount++)] = std::uint8_t(note);
        const bool legato = m_voice[0].gate > 0.5f && m_voiceNote[0] >= 0;
        m_voice[0].pitch = pitch;
        m_voice[0].velocity = legato ? m_voice[0].velocity : vel;
        m_voice[0].gate = 1.0f;
        m_voiceNote[0] = note;
        m_sustained[0] = false;
        m_shownVoice.store(0, std::memory_order_relaxed);
        return;
    }
    // The same key again, else a silent voice (released longest ago), else the oldest.
    int pick = -1;
    for (int v = 0; v < voices && pick < 0; ++v)
        if (m_voiceNote[std::size_t(v)] == note)
            pick = v;
    if (pick < 0)
        for (int v = 0; v < voices; ++v)
            if (m_voice[std::size_t(v)].gate < 0.5f && m_pendingNote[std::size_t(v)] < 0
                && (pick < 0 || m_voiceAge[std::size_t(v)] < m_voiceAge[std::size_t(pick)]))
                pick = v;
    if (pick < 0)
        for (int v = 0; v < voices; ++v)
            if (pick < 0 || m_voiceAge[std::size_t(v)] < m_voiceAge[std::size_t(pick)])
                pick = v;
    const auto p = std::size_t(pick);
    m_voiceAge[p] = ++m_counter;
    m_voiceNote[p] = note;
    m_sustained[p] = false;
    if (m_voice[p].gate > 0.5f) { // sounding: its gate closes for a block so envelopes start again
        m_voice[p].gate = 0.0f;
        m_pendingNote[p] = note;
        m_pendingVelocity[p] = vel;
    } else {
        m_voice[p].pitch = pitch;
        m_voice[p].velocity = vel;
        m_voice[p].gate = 1.0f;
    }
    m_shownVoice.store(pick, std::memory_order_relaxed);
}

void Engine::releaseVoice(int v) noexcept
{
    if (m_sustain) {
        m_sustained[std::size_t(v)] = true;
        return;
    }
    m_voice[std::size_t(v)].gate = 0.0f;
    m_pendingNote[std::size_t(v)] = -1;
}

void Engine::noteOff(int note) noexcept
{
    if (m_voiceCount.load(std::memory_order_relaxed) == 1) {
        for (int i = 0; i < m_heldCount; ++i)
            if (m_held[std::size_t(i)] == note) {
                std::copy(m_held.begin() + i + 1, m_held.begin() + m_heldCount, m_held.begin() + i);
                --m_heldCount;
                break;
            }
        if (m_voiceNote[0] != note)
            return;
        if (m_heldCount > 0) { // back to the newest key still held, legato
            m_voiceNote[0] = m_held[std::size_t(m_heldCount - 1)];
            m_voice[0].pitch = float(m_voiceNote[0] - 60) / 12.0f;
            return;
        }
        releaseVoice(0);
        return;
    }
    for (int v = 0; v < kVoices; ++v)
        if (m_voiceNote[std::size_t(v)] == note && (m_voice[std::size_t(v)].gate > 0.5f || m_pendingNote[std::size_t(v)] >= 0))
            releaseVoice(v);
}

void Engine::handleMidi(const krate::MidiEvent &e) noexcept
{
    const krate::MidiMessage m = krate::decode(e);
    switch (m.type) {
    case krate::MidiMessage::Type::NoteOn:
        noteOn(m.data1, m.data2);
        break;
    case krate::MidiMessage::Type::NoteOff:
        noteOff(m.data1);
        break;
    case krate::MidiMessage::Type::ControlChange: {
        if (const int armed = m_armed.exchange(-1, std::memory_order_relaxed); armed >= 0) {
            m_ccMap[std::size_t(m.data1)].store(armed, std::memory_order_relaxed);
            m_learned.push({m.data1, armed / kMaxKnobs, armed % kMaxKnobs});
        }
        if (const int t = m_ccMap[std::size_t(m.data1)].load(std::memory_order_relaxed); t >= 0) {
            if (Module *mod = m_modules[std::size_t(t / kMaxKnobs)])
                mod->setKnobPosition(t % kMaxKnobs, float(m.data2) / 127.0f);
        } else if (m.data1 == kCcModWheel) {
            m_modWheel = float(m.data2) / 127.0f;
        } else if (m.data1 == kCcSustain) {
            m_sustain = m.data2 >= 64;
            if (!m_sustain)
                for (int v = 0; v < kVoices; ++v)
                    if (m_sustained[std::size_t(v)]) {
                        m_sustained[std::size_t(v)] = false;
                        releaseVoice(v);
                    }
        } else if (m.data1 == kCcAllNotesOff) {
            m_heldCount = 0;
            for (int v = 0; v < kVoices; ++v) {
                m_voice[std::size_t(v)].gate = 0.0f;
                m_pendingNote[std::size_t(v)] = -1;
            }
        }
        break;
    }
    case krate::MidiMessage::Type::Other:
        if (e.size == 3 && (e.data[0] & 0xF0) == 0xE0)
            m_bend = std::clamp((((e.data[2] << 7) | e.data[1]) - 8192) / 8191.0f, -1.0f, 1.0f);
        else if (e.size >= 2 && (e.data[0] & 0xF0) == 0xD0)
            for (MidiVoice &v : m_voice)
                v.pressure = float(e.data[1]) / 127.0f;
        else if (e.size == 3 && (e.data[0] & 0xF0) == 0xA0)
            for (int v = 0; v < kVoices; ++v)
                if (m_voiceNote[std::size_t(v)] == e.data[1])
                    m_voice[std::size_t(v)].pressure = float(e.data[2]) / 127.0f;
        break;
    case krate::MidiMessage::Type::ProgramChange:
        break;
    }
}

// --- Running ------------------------------------------------------------------------------

void Engine::tap(int slot, int jack, const float *x, int frames) noexcept
{
    TraceState &t = m_traces[std::size_t(slot) * kMaxJacks + std::size_t(jack)];
    for (int i = 0; i < frames; ++i) {
        if (t.count == 0)
            t.curLow = t.curHigh = x[i];
        t.curLow = std::min(t.curLow, x[i]);
        t.curHigh = std::max(t.curHigh, x[i]);
        if (++t.count >= m_traceBin) {
            const int h = t.head.load(std::memory_order_relaxed);
            t.low[std::size_t(h)].store(t.curLow, std::memory_order_relaxed);
            t.high[std::size_t(h)].store(t.curHigh, std::memory_order_relaxed);
            t.head.store((h + 1) % kTracePoints, std::memory_order_release);
            t.count = 0;
        }
    }
    if (m_probe.load(std::memory_order_relaxed) == slot * kMaxJacks + jack) {
        int h = m_probeHead.load(std::memory_order_relaxed);
        for (int i = 0; i < frames; ++i) {
            m_probeData[std::size_t(h)].store(x[i], std::memory_order_relaxed);
            h = (h + 1) % kProbeSamples;
        }
        m_probeHead.store(h, std::memory_order_release);
    }
}

void Engine::runBlock(float *left, float *right, int frames) noexcept
{
    const int voices = m_voiceCount.load(std::memory_order_relaxed);
    const int shown = std::min(m_shownVoice.load(std::memory_order_relaxed), voices - 1);
    ProcessContext ctx;
    ctx.frames = frames;
    ctx.rate = m_rate.load(std::memory_order_relaxed);
    ctx.midi = m_voice.data();
    ctx.bend = m_bend;
    ctx.modWheel = m_modWheel;
    ctx.output[0] = left;
    ctx.output[1] = right;
    for (int k = 0; k < m_orderCount; ++k) {
        const int slot = m_order[std::size_t(k)];
        Module *m = m_modules[std::size_t(slot)];
        if (!m)
            continue;
        const ModuleInfo &info = m->info();
        ctx.voices = info.perVoice ? voices : 1;
        for (std::size_t j = 0; j < info.inputs.size(); ++j) {
            const Source src = m_inputs[std::size_t(slot)][j];
            const Module *from = src.slot >= 0 ? m_modules[std::size_t(src.slot)] : nullptr;
            if (!from || src.jack >= int(from->info().outputs.size())) {
                ctx.in[j].fill(nullptr);
                continue;
            }
            const bool fromPerVoice = from->info().perVoice;
            if (info.perVoice) {
                for (int v = 0; v < voices; ++v)
                    ctx.in[j][std::size_t(v)] = buffer(src.slot, src.jack, fromPerVoice ? v : 0);
            } else if (fromPerVoice) { // into a one-voice input: the voices mixed
                float *mix = sum(slot, int(j));
                std::copy_n(buffer(src.slot, src.jack, 0), frames, mix);
                for (int v = 1; v < voices; ++v) {
                    const float *x = buffer(src.slot, src.jack, v);
                    for (int i = 0; i < frames; ++i)
                        mix[i] += x[i];
                }
                ctx.in[j][0] = mix;
            } else {
                ctx.in[j][0] = buffer(src.slot, src.jack, 0);
            }
        }
        for (std::size_t j = 0; j < info.outputs.size(); ++j)
            for (int v = 0; v < ctx.voices; ++v)
                ctx.out[j][std::size_t(v)] = buffer(slot, int(j), v);
        m->process(ctx);
        for (std::size_t j = 0; j < info.outputs.size(); ++j)
            tap(slot, int(j), buffer(slot, int(j), info.perVoice ? shown : 0), frames);
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    krate::ScopedFlushDenormals noDenormals;
    while (const auto c = m_commands.pop())
        apply(*c);
    float *left = block.outputs[0];
    float *right = block.outputs[1];
    std::fill_n(left, block.frames, 0.0f);
    std::fill_n(right, block.frames, 0.0f);

    std::uint32_t nextEvent = 0;
    for (std::uint32_t at = 0; at < block.frames; at += kBlock) {
        const auto frames = std::min<std::uint32_t>(kBlock, block.frames - at);
        // A stolen voice's new note: its gate was closed for the block after
        // the note came (so envelopes start again); now it opens.
        for (int v = 0; v < kVoices; ++v)
            if (const int note = m_pendingNote[std::size_t(v)]; note >= 0) {
                m_voice[std::size_t(v)].pitch = float(note - 60) / 12.0f;
                m_voice[std::size_t(v)].velocity = m_pendingVelocity[std::size_t(v)];
                m_voice[std::size_t(v)].gate = 1.0f;
                m_pendingNote[std::size_t(v)] = -1;
            }
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame < at + frames)
            handleMidi(block.midi[nextEvent++]);
        runBlock(left + at, right + at, int(frames));
    }
    while (nextEvent < block.midiCount)
        handleMidi(block.midi[nextEvent++]);

    int held = 0;
    for (int v = 0; v < kVoices; ++v)
        held += m_voice[std::size_t(v)].gate > 0.5f ? 1 : 0;
    m_notesHeld.store(held, std::memory_order_relaxed);

    const double target = double(m_params.get(0)) * m_params.get(0);
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * 0.001;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
}

// --- Seeing the signals -----------------------------------------------------------------------

Trace Engine::trace(int slot, int jack) const noexcept
{
    Trace out;
    if (slot < 0 || slot >= kMaxModules || jack < 0 || jack >= kMaxJacks)
        return out;
    const TraceState &t = m_traces[std::size_t(slot) * kMaxJacks + std::size_t(jack)];
    const int head = t.head.load(std::memory_order_acquire);
    for (int k = 0; k < kTracePoints; ++k) {
        const auto at = std::size_t((head + k) % kTracePoints);
        out.low[std::size_t(k)] = t.low[at].load(std::memory_order_relaxed);
        out.high[std::size_t(k)] = t.high[at].load(std::memory_order_relaxed);
    }
    return out;
}

void Engine::setProbe(int slot, int jack) noexcept
{
    m_probe.store(slot >= 0 && jack >= 0 ? slot * kMaxJacks + jack : -1, std::memory_order_relaxed);
}

void Engine::probe(std::span<float> out) const noexcept
{
    const int head = m_probeHead.load(std::memory_order_acquire);
    const auto n = std::min<std::size_t>(out.size(), kProbeSamples);
    for (std::size_t k = 0; k < n; ++k)
        out[k] = m_probeData[std::size_t((head + int(kProbeSamples - n) + int(k)) % kProbeSamples)].load(std::memory_order_relaxed);
}

} // namespace konnekt
