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
    while (const auto a = m_arrangements.pop())
        delete *a;
    delete m_pendingArrangement;
    delete m_arrangementBacklog;
    delete m_arrangement;
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
    while (const auto a = m_retiredArrangements.pop())
        delete *a;
    if (m_pendingArrangement && m_arrangements.push(m_pendingArrangement))
        m_pendingArrangement = nullptr;
}

void Sequencer::setArrangement(std::unique_ptr<Arrangement> arrangement)
{
    collectGarbage();
    Arrangement *raw = arrangement.release();
    if (!m_arrangements.push(raw)) {
        delete m_pendingArrangement;
        m_pendingArrangement = raw;
    }
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

void Sequencer::setRange(int from, int to) noexcept
{
    from = std::clamp(from, 0, kPatterns - 1);
    to = std::clamp(to, 0, kPatterns - 1);
    if (from > to)
        std::swap(from, to);
    m_range.store(from << 8 | to, std::memory_order_relaxed);
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
    if (m_arrangementBacklog && m_retiredArrangements.push(m_arrangementBacklog))
        m_arrangementBacklog = nullptr;
    while (!m_arrangementBacklog) {
        const auto a = m_arrangements.pop();
        if (!a)
            break;
        if (m_arrangement && !m_retiredArrangements.push(m_arrangement))
            m_arrangementBacklog = m_arrangement;
        m_arrangement = *a;
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

    // SONG mode switched: what was sounding ends, the other mode starts afresh.
    const bool song = m_songMode.load(std::memory_order_relaxed);
    if (song != m_inSong) {
        flushAll(0);
        m_inSong = song;
        m_playing = false;
    }
    if (song) {
        renderSong(transport, transport.playing && m_running.load(std::memory_order_relaxed) && frames > 0, sampleRate);
        return m_outCount;
    }

    const int cued = m_cued.load(std::memory_order_relaxed);
    const bool run = transport.playing && m_running.load(std::memory_order_relaxed) && m_bank && frames > 0;
    if (!run) {
        if (m_playing)
            flushAll(0);
        m_playing = false;
        m_pattern = startPattern(cued);
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
        m_pattern = startPattern(cued);
        m_loopStart = std::floor(b0 / kBeatsPerBar + 1e-9) * kBeatsPerBar;
    }
    // CHAIN switched on or off while playing: from the next pass.
    const bool chain = m_chainMode.load(std::memory_order_relaxed) && m_bank->chainLength > 0;
    if (chain != m_chaining) {
        m_chaining = chain;
        m_step = 0;
        m_pass = -1; // the next pass starts the chain
    }

    double from = b0;
    bool tickSet = false;
    for (int guard = 0; from < b1 && guard < 4096; ++guard) {
        const Pattern &p = m_bank->patterns[std::size_t(m_pattern)];
        const double loopEnd = m_loopStart + double(std::max<int>(p.length, 1)) / kTicksPerBeat;
        if (loopEnd <= from) { // a whole pass behind (a shorter pattern arrived): next pass
            m_loopStart = loopEnd;
            m_pattern = next(cued);
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
            m_pattern = next(cued);
        }
        from = to;
    }
    m_isPlaying.store(true, std::memory_order_relaxed);
    m_chainStep.store(m_chaining && m_pass >= 0 ? m_step : -1, std::memory_order_relaxed);
    m_chainPass.store(std::max(m_pass, 0), std::memory_order_relaxed);

    endNotes(b0, b1, framesPerBeat);
    sortOut();
    return m_outCount;
}

void Sequencer::sortOut() noexcept
{
    // By frame; at the same frame, offs before ons (a note repeated on the next step).
    DeviceEvent *out = m_out;
    for (std::uint32_t i = 1; i < m_outCount; ++i)
        for (std::uint32_t j = i; j > 0; --j) {
            const DeviceEvent &a = out[j - 1], &b = out[j];
            const bool aOn = (a.event.data[0] & 0xF0) == 0x90, bOn = (b.event.data[0] & 0xF0) == 0x90;
            if (a.event.frame > b.event.frame || (a.event.frame == b.event.frame && aOn && !bOn))
                std::swap(out[j - 1], out[j]);
            else
                break;
        }
}

void Sequencer::renderSong(const TransportState &transport, bool run, double sampleRate) noexcept
{
    if (!run || !m_arrangement) {
        if (m_playing)
            flushAll(0);
        m_playing = false;
        m_isPlaying.store(false, std::memory_order_relaxed);
        return;
    }
    const double framesPerBeat = 60.0 / std::max(transport.bpm, 1.0) * sampleRate;
    const double b0 = transport.beatAtBlockStart;
    const double b1 = b0 + double(m_frames) / framesPerBeat;
    // Started, or not where the last block left off (a jump, a loop going round).
    if (transport.restarted || !m_playing || std::fabs(b0 - m_songNext) > 1e-6) {
        if (m_playing)
            flushAll(0);
        m_playing = true;
    }
    m_songNext = b1;

    const Arrangement &song = *m_arrangement;
    for (int t = 0; t < kTracks; ++t) {
        if (m_tracks[std::size_t(t)].applied.device < 0 || m_mute[std::size_t(t)].load(std::memory_order_relaxed))
            continue;
        for (const Clip &clip : song.lanes[std::size_t(t)]) {
            const double cs = double(clip.startBar) * kBeatsPerBar, ce = double(clip.endBar()) * kBeatsPerBar;
            if (ce <= b0)
                continue;
            if (cs >= b1)
                break; // the lane is in order
            const double from = std::max(b0, cs), to = std::min(b1, ce);
            if (clip.pattern >= 0 && clip.pattern < kPatterns && m_bank) {
                // That track's part of the pattern, round and round.
                const Pattern &p = m_bank->patterns[std::size_t(clip.pattern)];
                const TrackNotes &notes = p.tracks[std::size_t(t)];
                const double length = double(std::max<int>(p.length, 1)) / kTicksPerBeat;
                for (double ps = cs + std::floor((from - cs) / length) * length; ps < to; ps += length)
                    for (int n = 0; n < notes.count; ++n) {
                        const Note &note = notes.notes[std::size_t(n)];
                        if (note.start >= p.length)
                            continue;
                        const double on = ps + double(note.start) / kTicksPerBeat;
                        if (on >= from && on < to)
                            play(t, note.pitch, note.velocity, on,
                                 std::min(on + double(std::max<int>(note.length, 1)) / kTicksPerBeat, ce), b0,
                                 framesPerBeat);
                    }
            } else if (clip.midi >= 0 && clip.midi < int(song.midi.size())) {
                for (const ClipNote &note : song.midi[std::size_t(clip.midi)].notes) {
                    const double on = cs + double(note.start) / kTicksPerBeat;
                    if (on >= to)
                        break;
                    if (on >= from)
                        play(t, note.pitch, note.velocity, on,
                             std::min(on + double(std::max<std::uint32_t>(note.length, 1)) / kTicksPerBeat, ce), b0,
                             framesPerBeat);
                }
            }
        }
    }
    m_isPlaying.store(true, std::memory_order_relaxed);
    endNotes(b0, b1, framesPerBeat);
    sortOut();
}

int Sequencer::startPattern(int cued) noexcept
{
    m_chaining = m_bank && m_chainMode.load(std::memory_order_relaxed) && m_bank->chainLength > 0;
    m_step = 0;
    m_pass = 0;
    if (!m_chaining) {
        m_chainStep.store(-1, std::memory_order_relaxed);
        if (m_rangeMode.load(std::memory_order_relaxed))
            return m_range.load(std::memory_order_relaxed) >> 8;
        return cued;
    }
    m_chainStep.store(0, std::memory_order_relaxed);
    m_chainPass.store(0, std::memory_order_relaxed);
    return std::min<int>(m_bank->chain[0].pattern, kPatterns - 1);
}

int Sequencer::next(int cued) noexcept
{
    if (!m_chaining || !m_bank || m_bank->chainLength == 0) {
        if (!m_rangeMode.load(std::memory_order_relaxed))
            return cued;
        const int range = m_range.load(std::memory_order_relaxed), from = range >> 8, to = range & 0xFF;
        const int following = m_pattern + 1;
        return following < from || following > to ? from : following;
    }
    if (m_pass < 0) { // the chain starts now
        m_step = 0;
        m_pass = 0;
    } else if (++m_pass >= std::max<int>(m_bank->chain[std::size_t(m_step)].repeats, 1)) {
        m_pass = 0;
        m_step = (m_step + 1) % m_bank->chainLength;
    }
    if (m_step >= m_bank->chainLength) // a shorter chain arrived
        m_step = 0;
    return std::min<int>(m_bank->chain[std::size_t(m_step)].pattern, kPatterns - 1);
}

void Sequencer::play(int track, const Note &note, double onBeat, double b0, double framesPerBeat) noexcept
{
    play(track, note.pitch, note.velocity, onBeat, onBeat + double(std::max<int>(note.length, 1)) / kTicksPerBeat, b0,
         framesPerBeat);
}

void Sequencer::play(int track, std::uint8_t pitch, std::uint8_t velocity, double onBeat, double offBeat, double b0,
                     double framesPerBeat) noexcept
{
    TrackState &ts = m_tracks[std::size_t(track)];
    const std::uint32_t frame = frameAt(onBeat, b0, framesPerBeat);
    const auto device = std::int16_t(ts.applied.device);
    const auto channel = std::uint8_t(ts.applied.channel);
    // The same key still sounding on this track: end it first.
    for (int i = 0; i < ts.count; ++i) {
        Sounding &s = ts.sounding[std::size_t(i)];
        if (s.device == device && s.channel == channel && s.pitch == pitch) {
            send(s.device, frame, std::uint8_t(0x80 | s.channel), s.pitch, 0);
            s = ts.sounding[std::size_t(--ts.count)];
            break;
        }
    }
    if (ts.count >= int(ts.sounding.size()))
        return;
    send(device, frame, std::uint8_t(0x90 | channel), pitch, std::uint8_t(std::clamp<int>(velocity, 1, 127)));
    ts.sounding[std::size_t(ts.count++)] = {offBeat, device, channel, pitch};
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
