#include "klang/dsp/Engine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/Denormals.h"

namespace klang {

namespace {
constexpr double kVolumeSmoothingSeconds = 0.01;
// Samples come mastered near full scale and the pads add up: -10 dB puts a
// busy pattern of them level with the other instruments (measured: the starter
// patterns at -18 dB RMS, as a Korrode line).
constexpr double kOutputTrim = 0.3162;
constexpr int kCcAllSoundOff = 120;
constexpr int kCcAllNotesOff = 123;
constexpr std::uint64_t kPadParamBits = (std::uint64_t(1) << PadParamCount) - 1;
constexpr int kGateStepsPerBeat[] = {2, 4, 8}; // 1/8, 1/16, 1/32
constexpr double kDelayBeats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
} // namespace

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    for (auto &pad : m_voices)
        for (PadVoice &v : pad)
            v.prepare(sampleRate);
    m_current.fill(0);
    m_delay.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_reverbOn = false;
    m_gate = 1.0;
    m_freeBeat = 0.0;
    m_wasPlaying = false;
    m_volumeSmoothing = 1.0 - std::exp(-1.0 / (kVolumeSmoothingSeconds * sampleRate));
    m_volume = -1.0;
}

// --- Parameters: the edited pad's, and Klang's ---------------------------------

float Engine::param(int index) const noexcept
{
    return isPadParam(index) ? m_pads[std::size_t(editedPad())].get(index) : m_global.get(index);
}

void Engine::setParam(int index, float normalized) noexcept
{
    if (isPadParam(index))
        m_pads[std::size_t(editedPad())].set(index, normalized);
    else
        m_global.set(index, normalized);
}

void Engine::setParamFromHost(int index, float normalized) noexcept
{
    if (isPadParam(index))
        m_pads[std::size_t(editedPad())].setFromAudio(index, normalized);
    else
        m_global.setFromAudio(index, normalized);
}

std::uint64_t Engine::takeParamsChangedByAudio() noexcept
{
    std::uint64_t changed = m_global.takeChanged() | m_forced.exchange(0, std::memory_order_relaxed);
    for (Store &pad : m_pads)
        changed |= pad.takeChanged() & kPadParamBits;
    return changed;
}

void Engine::setEditedPad(int pad) noexcept
{
    pad = std::clamp(pad, 0, kPads - 1);
    if (m_edited.exchange(pad, std::memory_order_relaxed) != pad)
        m_forced.fetch_or(kPadParamBits, std::memory_order_relaxed);
}

PadSettings Engine::settings(int pad) const noexcept
{
    const Store &p = m_pads[std::size_t(pad)];
    const auto v = [&](ParamId id) { return p.get(int(id)); };
    using namespace positions;
    PadSettings s;
    s.gain = units::gain(v(ParamId::PadLevel));
    const double angle = double(v(ParamId::PadPan)) * 0.5 * std::numbers::pi;
    s.panLeft = std::cos(angle);
    s.panRight = std::sin(angle);
    s.tuneSemis = units::tuneSemitones(v(ParamId::Tune)) + units::fineCents(v(ParamId::Fine)) / 100.0;
    s.start = v(ParamId::Start);
    s.end = v(ParamId::End);
    s.attack = units::attackSeconds(v(ParamId::Attack));
    s.decay = units::decaySeconds(v(ParamId::Decay));
    s.mode = PadMode(units::index(v(ParamId::Mode), kMode));
    s.syncBeats = units::syncBeats(v(ParamId::Sync));
    s.reverse = units::index(v(ParamId::Reverse), kSwitch) == 1;
    s.choke = units::chokeGroup(v(ParamId::Choke));
    s.filter = FilterType(units::index(v(ParamId::Filter), kFilter));
    s.cutoffHz = units::cutoffHz(v(ParamId::Cutoff));
    s.resonance = v(ParamId::Res);
    s.filterEnvOctaves = 6.0 * v(ParamId::FilterEnv);
    s.drive = v(ParamId::Drive);
    s.bits = units::bits(v(ParamId::Bits));
    s.downsample = units::downsample(v(ParamId::Downsample));
    s.ring = v(ParamId::Ring);
    s.ringHz = units::ringHz(v(ParamId::RingFreq));
    s.gate = units::index(v(ParamId::Gate), kSwitch) == 1;
    s.delaySend = v(ParamId::DelaySend);
    s.reverbSend = v(ParamId::ReverbSend);
    return s;
}

// --- Patterns ------------------------------------------------------------------------

void Engine::setPattern(int index, const Pattern &pattern) noexcept
{
    if (index < 0 || index >= kPatterns)
        return;
    m_bank[std::size_t(index)] = pattern;
    if (index == m_pattern)
        m_gatePattern.store(pattern.gate, std::memory_order_relaxed);
}

double Engine::stepBeat(std::int64_t step) const noexcept
{
    return m_startBeat + (double(step) + ((step & 1) ? m_swing : 0.0)) * 0.25;
}

void Engine::schedule(double beat, int pad, float velocity, double pitch, bool flip, bool release) noexcept
{
    if (m_scheduledCount < kMaxScheduled)
        m_scheduled[std::size_t(m_scheduledCount++)] = {beat, std::int8_t(pad), release, velocity, float(pitch), flip};
}

void Engine::sequencerUpdate(const krate::TransportState &transport) noexcept
{
    while (auto update = m_patternUpdates.pop())
        setPattern(update->index, update->pattern);
    m_swing = 2.0 * units::swingFraction(global(ParamId::Swing)) - 1.0;

    const bool run = transport.playing && m_sequencerEnabled.load(std::memory_order_relaxed)
                     && !m_sequencerSuspended.load(std::memory_order_relaxed);
    if (!m_running) { // stopped: a cued pattern is the pattern at once
        const int cued = std::clamp(m_cued.load(std::memory_order_relaxed), 0, kPatterns - 1);
        if (cued != m_pattern) {
            m_pattern = cued;
            m_gatePattern.store(m_bank[std::size_t(cued)].gate, std::memory_order_relaxed);
        }
    }
    if (run && (!m_running || transport.restarted)) {
        m_running = true;
        // From the top, or joining a transport that's playing: from the next bar.
        m_startBeat = transport.restarted || transport.beatAtBlockStart <= 0.0
                          ? transport.beatAtBlockStart
                          : std::ceil(transport.beatAtBlockStart / 4.0 - 1e-9) * 4.0;
        m_nextStep = 0;
        m_passStep = 0;
        m_scheduledCount = 0;
    } else if (!run && m_running) {
        m_running = false;
        m_scheduledCount = 0;
        for (int pad = 0; pad < kPads; ++pad)
            if (m_settings[std::size_t(pad)].mode == PadMode::Gate)
                releasePad(pad);
    }
}

void Engine::fireStep(std::int64_t step) noexcept
{
    if (m_passStep == 0) { // a new pass: the cued pattern takes over
        const int cued = std::clamp(m_cued.load(std::memory_order_relaxed), 0, kPatterns - 1);
        if (cued != m_pattern) {
            m_pattern = cued;
            m_gatePattern.store(m_bank[std::size_t(cued)].gate, std::memory_order_relaxed);
        }
    }
    const Pattern &pattern = m_bank[std::size_t(m_pattern)];
    const int s = std::min(m_passStep, kMaxSteps - 1);
    m_playingStep.store(s, std::memory_order_relaxed);
    const double at = stepBeat(step);
    for (int pad = 0; pad < kPatternPads; ++pad) {
        const Step &st = pattern.steps[std::size_t(pad)][std::size_t(s)];
        if (!st.on)
            continue;
        const float velocity = st.accent ? 1.0f : 0.75f;
        const int repeat = std::clamp(int(st.repeat), 1, kMaxRepeat);
        const double each = 0.25 / repeat; // beats
        const bool gate = m_settings[std::size_t(pad)].mode == PadMode::Gate;
        for (int k = 0; k < repeat; ++k) {
            if (k == 0)
                hit(pad, velocity, st.pitch, st.reverse);
            else
                schedule(at + k * each, pad, velocity, st.pitch, st.reverse, false);
            if (gate) // a Gate pad holds for its part of the step
                schedule(at + (k + 0.9) * each, pad, 0.0f, 0.0, false, true);
        }
    }
    if (++m_passStep >= std::max<int>(1, pattern.length))
        m_passStep = 0;
}

// --- Hits ----------------------------------------------------------------------------

void Engine::hit(int pad, float velocity, double pitchSemis, bool flip, int note) noexcept
{
    if (pad < 0 || pad >= kPads)
        return;
    const PadSettings &s = m_settings[std::size_t(pad)];
    if (s.choke > 0) // a pad cuts the others in its group
        for (int other = 0; other < kPads; ++other)
            if (other != pad && m_settings[std::size_t(other)].choke == s.choke)
                for (PadVoice &v : m_voices[std::size_t(other)])
                    v.choke();
    // The pad's sounding voice fades out (3 ms) while the other starts.
    auto &voices = m_voices[std::size_t(pad)];
    int &current = m_current[std::size_t(pad)];
    voices[std::size_t(current)].choke();
    current ^= 1;
    voices[std::size_t(current)].start(m_samples[std::size_t(pad)], s, velocity, pitchSemis, flip, note);
}

void Engine::releasePad(int pad, int note) noexcept
{
    if (pad < 0 || pad >= kPads)
        return;
    for (PadVoice &v : m_voices[std::size_t(pad)])
        if (v.held() && (note < 0 || v.note() == note))
            v.release();
}

void Engine::takeSamples() noexcept
{
    for (int pad = 0; pad < kPads; ++pad) {
        const Sample *requested = m_requested[std::size_t(pad)].load(std::memory_order_acquire);
        if (requested == m_samples[std::size_t(pad)])
            continue;
        for (PadVoice &v : m_voices[std::size_t(pad)])
            v.kill(); // the old sample may be freed two blocks from now
        m_samples[std::size_t(pad)] = requested;
    }
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event);
    const bool chromatic = units::index(global(ParamId::KeyMode), positions::kKeyMode) == 1;
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            setParamFromHost(r.param, r.value);
        else if (m.data1 == kCcAllNotesOff || m.data1 == kCcAllSoundOff)
            for (int pad = 0; pad < kPads; ++pad)
                releasePad(pad);
        break;
    case krate::MidiMessage::Type::NoteOn: {
        const float velocity = float(m.data2) / 127.0f;
        const int pad = m.data1 - kFirstPadNote;
        if (!chromatic && pad >= 0 && pad < kPads)
            hit(pad, velocity, 0.0, false, m.data1);
        else // the edited pad, at the key's pitch
            hit(editedPad(), velocity, double(m.data1 - kChromaticRoot), false, m.data1);
        break;
    }
    case krate::MidiMessage::Type::NoteOff: {
        const int pad = m.data1 - kFirstPadNote;
        if (!chromatic && pad >= 0 && pad < kPads)
            releasePad(pad, m.data1);
        else
            releasePad(editedPad(), m.data1);
        break;
    }
    case krate::MidiMessage::Type::ProgramChange: // a pattern, from the end of this pass
        if (m.data1 < kPatterns)
            cuePattern(m.data1);
        break;
    case krate::MidiMessage::Type::Other:
        break;
    }
}

void Engine::render(float *left, float *right, std::uint32_t frames, double bpm, double beat,
                    double beatsPerFrame) noexcept
{
    const std::uint16_t gatePattern = m_gatePattern.load(std::memory_order_relaxed);
    const int gateSteps = kGateStepsPerBeat[units::index(global(ParamId::GateRate), positions::kGateRate)];
    const double gateDepth = global(ParamId::GateDepth);
    const double gateSmoothing =
        1.0 - std::exp(-1.0 / (units::gateSmoothSeconds(global(ParamId::GateSmooth)) * m_sampleRate));
    const double delaySeconds =
        kDelayBeats[units::index(global(ParamId::DelayTime), positions::kDelayTime)] * 60.0 / std::max(bpm, 1.0);
    const double delayLevel = global(ParamId::DelayLevel);
    const double delayFeedback = units::delayFeedback(global(ParamId::DelayFb));
    const double reverbLevel = global(ParamId::ReverbLevel);
    const double reverbSize = global(ParamId::ReverbSize);
    while (frames > 0) {
        const std::uint32_t n = std::min(frames, kChunk);
        // The gate: its pattern stepped by the beat, the edges smoothed.
        for (std::uint32_t i = 0; i < n; ++i) {
            const auto step = int(std::floor((beat + double(i) * beatsPerFrame) * gateSteps)) & 15;
            const double target = ((gatePattern >> step) & 1u) ? 1.0 : 0.0;
            m_gate += (target - m_gate) * gateSmoothing;
            m_gateValues[i] = float(1.0 - gateDepth * (1.0 - m_gate));
        }
        beat += double(n) * beatsPerFrame;
        std::fill_n(m_dryL.begin(), n, 0.0f);
        std::fill_n(m_dryR.begin(), n, 0.0f);
        std::fill_n(m_delayL.begin(), n, 0.0f);
        std::fill_n(m_delayR.begin(), n, 0.0f);
        std::fill_n(m_reverbL.begin(), n, 0.0f);
        std::fill_n(m_reverbR.begin(), n, 0.0f);
        const PadBuses buses{m_dryL.data(), m_dryR.data(), m_delayL.data(), m_delayR.data(), m_reverbL.data(),
                             m_reverbR.data()};
        for (int pad = 0; pad < kPads; ++pad)
            for (PadVoice &v : m_voices[std::size_t(pad)])
                v.render(buses, m_gateValues.data(), n, m_settings[std::size_t(pad)], bpm);
        // The sends, through their effects: what an effect adds to its send is its wet return.
        std::copy_n(m_delayL.begin(), n, m_tmpL.begin());
        std::copy_n(m_delayR.begin(), n, m_tmpR.begin());
        m_delay.process(m_delayL.data(), m_delayR.data(), n, delaySeconds, delayLevel, delayFeedback);
        for (std::uint32_t i = 0; i < n; ++i) {
            m_dryL[i] += m_delayL[i] - m_tmpL[i];
            m_dryR[i] += m_delayR[i] - m_tmpR[i];
        }
        if (reverbLevel > 0.0) { // runs only while it is up (cleared when it comes back: no stale tail)
            if (!m_reverbOn)
                m_reverb.clear();
            m_reverbOn = true;
            std::copy_n(m_reverbL.begin(), n, m_tmpL.begin());
            std::copy_n(m_reverbR.begin(), n, m_tmpR.begin());
            m_reverb.process(m_reverbL.data(), m_reverbR.data(), n, reverbSize, reverbLevel);
            for (std::uint32_t i = 0; i < n; ++i) {
                m_dryL[i] += m_reverbL[i] - m_tmpL[i];
                m_dryR[i] += m_reverbR[i] - m_tmpR[i];
            }
        } else {
            m_reverbOn = false;
        }
        std::copy_n(m_dryL.begin(), n, left);
        std::copy_n(m_dryR.begin(), n, right);
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
    takeSamples();
    for (int pad = 0; pad < kPads; ++pad)
        m_settings[std::size_t(pad)] = settings(pad);

    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &transport = block.transport ? *block.transport : kStopped;
    const double bpm = block.transport ? transport.bpm : units::tempoBpm(global(ParamId::Tempo));
    const double beatsPerFrame = bpm / 60.0 / m_sampleRate;
    if (m_wasPlaying && !transport.playing) // the transport stopped: loops stop with it
        for (auto &pad : m_voices)
            for (PadVoice &v : pad)
                if (v.looping())
                    v.release();
    m_wasPlaying = transport.playing;
    double beat = transport.playing ? transport.beatAtBlockStart : m_freeBeat;

    while (const auto h = m_hits.pop()) {
        if (h->velocity > 0)
            hit(h->pad, float(h->velocity) / 127.0f);
        else
            releasePad(h->pad);
    }
    sequencerUpdate(transport);

    // The voices render in runs between the frames where something happens:
    // MIDI events, pattern steps and their repeats land on their exact sample.
    std::uint32_t done = 0;
    const auto renderTo = [&](std::uint32_t frame) {
        if (frame <= done)
            return;
        render(left + done, right + done, frame - done, bpm, beat + double(done) * beatsPerFrame, beatsPerFrame);
        done = frame;
    };
    std::uint32_t nextEvent = 0;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        while (nextEvent < block.midiCount && block.midi[nextEvent].frame <= i) {
            renderTo(i);
            handleMidi(block.midi[nextEvent++]);
        }
        if (!m_running)
            continue;
        const double now = transport.beatAtBlockStart + double(i) * beatsPerFrame;
        while (now >= stepBeat(m_nextStep)) {
            renderTo(i);
            fireStep(m_nextStep++);
        }
        for (int k = 0; k < m_scheduledCount;) {
            const Scheduled e = m_scheduled[std::size_t(k)];
            if (now < e.beat) {
                ++k;
                continue;
            }
            renderTo(i);
            m_scheduled[std::size_t(k)] = m_scheduled[std::size_t(--m_scheduledCount)];
            if (e.release)
                releasePad(e.pad);
            else
                hit(e.pad, e.velocity, e.pitch, e.flip);
        }
    }
    while (nextEvent < block.midiCount) // past the block's end (malformed): at the end
        handleMidi(block.midi[nextEvent++]);
    renderTo(block.frames);
    m_playingPattern.store(m_pattern, std::memory_order_relaxed);
    if (!m_running)
        m_playingStep.store(-1, std::memory_order_relaxed);
    if (!transport.playing)
        m_freeBeat = beat + double(block.frames) * beatsPerFrame;

    std::uint32_t sounding = 0;
    for (int pad = 0; pad < kPads; ++pad)
        for (const PadVoice &v : m_voices[std::size_t(pad)])
            if (v.active())
                sounding |= 1u << pad;
    m_soundingMask.store(sounding, std::memory_order_relaxed);

    const double target = kOutputTrim * units::gain(global(ParamId::Volume));
    if (m_volume < 0.0)
        m_volume = target;
    for (std::uint32_t i = 0; i < block.frames; ++i) {
        m_volume += (target - m_volume) * m_volumeSmoothing;
        left[i] = float(left[i] * m_volume);
        right[i] = float(right[i] * m_volume);
    }
    m_blocks.fetch_add(1, std::memory_order_release);
}

} // namespace klang
