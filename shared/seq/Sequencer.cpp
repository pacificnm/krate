#include "seq/Sequencer.h"

#include <algorithm>
#include <cmath>

namespace krate::seq {

namespace {

// Target <-> one atomic word: bits 0-3 channel, 8-15 device + 1.
std::int32_t pack(const Target &t) noexcept
{
    if (t.device < 0)
        return 0;
    return std::clamp(t.channel, 0, 15) | ((std::min(t.device, 254) + 1) << 8);
}

Target unpack(std::int32_t v) noexcept
{
    Target t;
    t.device = ((v >> 8) & 0xFF) - 1;
    t.channel = v & 0x0F;
    if (t.device < 0)
        return {};
    return t;
}

constexpr double kBeatsPerBar = 4.0;

} // namespace

Sequencer::Sequencer()
{
    for (int t = 0; t < kTracks; ++t) {
        m_target[std::size_t(t)].store(0, std::memory_order_relaxed);
        m_mute[std::size_t(t)].store(false, std::memory_order_relaxed);
    }
}

Sequencer::~Sequencer()
{
    // The audio thread has stopped calling us by now.
    while (const auto b = m_banks.pop())
        delete *b;
    collectGarbage();
    delete m_pending;
    delete m_backlog;
    delete m_bank;
}

// --- GUI thread -----------------------------------------------------------------------

void Sequencer::setBank(std::unique_ptr<Bank> bank)
{
    collectGarbage();
    Bank *raw = bank.release();
    if (!m_banks.push(raw)) { // full: the newest wins, on the next collectGarbage()
        delete m_pending;
        m_pending = raw;
    }
}

void Sequencer::collectGarbage()
{
    while (const auto b = m_retired.pop())
        delete *b;
    if (m_pending && m_banks.push(m_pending))
        m_pending = nullptr;
}

void Sequencer::setTarget(int track, Target t) noexcept
{
    if (track >= 0 && track < kTracks)
        m_target[std::size_t(track)].store(pack(t), std::memory_order_relaxed);
}

Target Sequencer::target(int track) const noexcept
{
    if (track < 0 || track >= kTracks)
        return {};
    return unpack(m_target[std::size_t(track)].load(std::memory_order_relaxed));
}

void Sequencer::setMute(int track, bool mute) noexcept
{
    if (track >= 0 && track < kTracks)
        m_mute[std::size_t(track)].store(mute, std::memory_order_relaxed);
}

bool Sequencer::mute(int track) const noexcept
{
    return track >= 0 && track < kTracks && m_mute[std::size_t(track)].load(std::memory_order_relaxed);
}

void Sequencer::cue(int pattern) noexcept
{
    m_cued.store(std::clamp(pattern, 0, kPatterns - 1), std::memory_order_relaxed);
}

// --- Audio thread ---------------------------------------------------------------------

std::uint32_t Sequencer::render(const TransportState &transport, std::uint32_t frames, double sampleRate,
                                DeviceEvent *out, std::uint32_t capacity) noexcept
{
    m_out = out;
    m_outCount = 0;
    m_outCapacity = capacity;
    m_frames = frames;

    // New banks; the old ones go back to the GUI for deletion (one waits here
    // while the GUI hasn't collected, and no newer bank is taken until it's gone).
    if (m_backlog && m_retired.push(m_backlog))
        m_backlog = nullptr;
    while (!m_backlog) {
        const auto b = m_banks.pop();
        if (!b)
            break;
        if (m_bank && !m_retired.push(m_bank))
            m_backlog = m_bank;
        m_bank = *b;
    }

    // Retargeted or muted tracks end their notes.
    for (int t = 0; t < kTracks; ++t) {
        TrackState &ts = m_tracks[std::size_t(t)];
        const Target want = unpack(m_target[std::size_t(t)].load(std::memory_order_relaxed));
        if (want != ts.applied) {
            flush(t, 0);
            ts.applied = want;
        }
        if (m_mute[std::size_t(t)].load(std::memory_order_relaxed))
            flush(t, 0);
    }

    const int cued = m_cued.load(std::memory_order_relaxed);
    const bool run = transport.playing && m_running.load(std::memory_order_relaxed) && m_bank && frames > 0;
    if (!run) {
        if (m_playing)
            flushAll(0);
        m_playing = false;
        m_pattern = cued;
        m_isPlaying.store(false, std::memory_order_relaxed);
        m_playingPattern.store(m_pattern, std::memory_order_relaxed);
        m_tick.store(0, std::memory_order_relaxed);
        return m_outCount;
    }

    const double framesPerBeat = 60.0 / std::max(transport.bpm, 1.0) * sampleRate;
    const double b0 = transport.beatAtBlockStart;
    const double b1 = b0 + double(frames) / framesPerBeat;
    // Starting (play pressed, RUN switched on) or the transport went back: the
    // pattern starts on the bar.
    if (transport.restarted || !m_playing || b0 < m_loopStart) {
        if (m_playing)
            flushAll(0);
        m_playing = true;
        m_pattern = cued;
        m_loopStart = std::floor(b0 / kBeatsPerBar + 1e-9) * kBeatsPerBar;
    }

    double from = b0;
    bool tickSet = false;
    for (int guard = 0; from < b1 && guard < 4096; ++guard) {
        const Pattern &p = m_bank->patterns[std::size_t(m_pattern)];
        const double loopEnd = m_loopStart + double(std::max<int>(p.length, 1)) / kTicksPerBeat;
        if (loopEnd <= from) { // a whole pass behind (a shorter pattern arrived): next pass
            m_loopStart = loopEnd;
            m_pattern = cued;
            continue;
        }
        if (!tickSet) {
            m_tick.store(int((from - m_loopStart) * kTicksPerBeat), std::memory_order_relaxed);
            m_playingPattern.store(m_pattern, std::memory_order_relaxed);
            tickSet = true;
        }
        const double to = std::min(b1, loopEnd);
        for (int t = 0; t < kTracks; ++t) {
            if (m_tracks[std::size_t(t)].applied.device < 0 || m_mute[std::size_t(t)].load(std::memory_order_relaxed))
                continue;
            const TrackNotes &notes = p.tracks[std::size_t(t)];
            for (int n = 0; n < notes.count; ++n) {
                const Note &note = notes.notes[std::size_t(n)];
                if (note.start >= p.length)
                    continue;
                const double on = m_loopStart + double(note.start) / kTicksPerBeat;
                if (on >= from && on < to)
                    play(t, note, on, b0, framesPerBeat);
            }
        }
        if (to >= loopEnd) {
            m_loopStart = loopEnd;
            m_pattern = cued;
        }
        from = to;
    }
    m_isPlaying.store(true, std::memory_order_relaxed);

    endNotes(b0, b1, framesPerBeat);

    // By frame; at the same frame, offs before ons (a note repeated on the next step).
    for (std::uint32_t i = 1; i < m_outCount; ++i)
        for (std::uint32_t j = i; j > 0; --j) {
            const DeviceEvent &a = out[j - 1], &b = out[j];
            const bool aOn = (a.event.data[0] & 0xF0) == 0x90, bOn = (b.event.data[0] & 0xF0) == 0x90;
            if (a.event.frame > b.event.frame || (a.event.frame == b.event.frame && aOn && !bOn))
                std::swap(out[j - 1], out[j]);
            else
                break;
        }
    return m_outCount;
}

void Sequencer::play(int track, const Note &note, double onBeat, double b0, double framesPerBeat) noexcept
{
    TrackState &ts = m_tracks[std::size_t(track)];
    const std::uint32_t frame = frameAt(onBeat, b0, framesPerBeat);
    const auto device = std::int16_t(ts.applied.device);
    const auto channel = std::uint8_t(ts.applied.channel);
    // The same key still sounding on this track: end it first.
    for (int i = 0; i < ts.count; ++i) {
        Sounding &s = ts.sounding[std::size_t(i)];
        if (s.device == device && s.channel == channel && s.pitch == note.pitch) {
            send(s.device, frame, std::uint8_t(0x80 | s.channel), s.pitch, 0);
            s = ts.sounding[std::size_t(--ts.count)];
            break;
        }
    }
    if (ts.count >= int(ts.sounding.size()))
        return;
    send(device, frame, std::uint8_t(0x90 | channel), note.pitch, std::uint8_t(std::clamp<int>(note.velocity, 1, 127)));
    ts.sounding[std::size_t(ts.count++)] = {onBeat + double(std::max<int>(note.length, 1)) / kTicksPerBeat, device,
                                            channel, note.pitch};
}

void Sequencer::endNotes(double b0, double b1, double framesPerBeat) noexcept
{
    for (TrackState &ts : m_tracks)
        for (int i = 0; i < ts.count;) {
            const Sounding &s = ts.sounding[std::size_t(i)];
            if (s.offBeat < b1) {
                send(s.device, frameAt(std::max(s.offBeat, b0), b0, framesPerBeat), std::uint8_t(0x80 | s.channel),
                     s.pitch, 0);
                ts.sounding[std::size_t(i)] = ts.sounding[std::size_t(--ts.count)];
            } else {
                ++i;
            }
        }
}

void Sequencer::flush(int track, std::uint32_t frame) noexcept
{
    TrackState &ts = m_tracks[std::size_t(track)];
    for (int i = 0; i < ts.count; ++i) {
        const Sounding &s = ts.sounding[std::size_t(i)];
        send(s.device, frame, std::uint8_t(0x80 | s.channel), s.pitch, 0);
    }
    ts.count = 0;
}

void Sequencer::flushAll(std::uint32_t frame) noexcept
{
    for (int t = 0; t < kTracks; ++t)
        flush(t, frame);
}

void Sequencer::send(int device, std::uint32_t frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) noexcept
{
    if (m_outCount < m_outCapacity)
        m_out[m_outCount++] = DeviceEvent{device, MidiEvent{frame, 3, {status, d1, d2}, true}};
}

std::uint32_t Sequencer::frameAt(double beat, double b0, double framesPerBeat) const noexcept
{
    const double f = std::floor((beat - b0) * framesPerBeat + 1e-6);
    return std::uint32_t(std::clamp(f, 0.0, double(m_frames - 1)));
}

} // namespace krate::seq
