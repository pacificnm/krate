#include "song/SongPlayer.h"

#include <algorithm>
#include <cmath>

#include "core/Transport.h"
#include "song/Arrange.h"

namespace krate::song {

namespace {

// Assignment <-> one atomic word: bits 0-3 channel, 8-15 device + 1, bit 16 drum map.
std::int32_t pack(const Assignment &a) noexcept
{
    if (a.device < 0)
        return 0;
    return (std::clamp(a.channel, 0, 15)) | ((std::min(a.device, 254) + 1) << 8) | (a.drumMap ? 1 << 16 : 0);
}

Assignment unpack(std::int32_t v) noexcept
{
    Assignment a;
    a.device = ((v >> 8) & 0xFF) - 1;
    a.channel = v & 0x0F;
    a.drumMap = (v >> 16) & 1;
    if (a.device < 0)
        return {};
    return a;
}

constexpr int kSustain = 64, kModWheel = 1;

} // namespace

SongPlayer::SongPlayer()
{
    for (int t = 0; t < kMaxTracks; ++t) {
        m_assignment[std::size_t(t)].store(0, std::memory_order_relaxed);
        m_level[std::size_t(t)].store(1.0f, std::memory_order_relaxed);
        m_mute[std::size_t(t)].store(false, std::memory_order_relaxed);
        m_solo[std::size_t(t)].store(false, std::memory_order_relaxed);
    }
}

SongPlayer::~SongPlayer()
{
    // The audio thread has stopped calling us by now.
    while (const auto c = m_commands.pop())
        delete c->song;
    collectGarbage();
    delete m_pending;
    delete m_song;
}

// --- GUI thread -----------------------------------------------------------------------

void SongPlayer::load(std::unique_ptr<Song> song)
{
    collectGarbage();
    m_cueRequest.store(0, std::memory_order_relaxed);
    Song *raw = song.release();
    if (raw && raw->tracks.size() > std::size_t(kMaxTracks))
        raw->tracks.resize(std::size_t(kMaxTracks));
    if (!m_commands.push({Command::Kind::Load, raw, 0})) { // full: try again on the next collectGarbage()
        delete m_pending;
        m_pending = raw;
    }
}

void SongPlayer::collectGarbage()
{
    while (const auto song = m_retired.pop())
        delete *song;
    if (m_pending && m_commands.push({Command::Kind::Load, m_pending, 0}))
        m_pending = nullptr;
}

void SongPlayer::setAssignment(int track, Assignment a) noexcept
{
    if (track >= 0 && track < kMaxTracks)
        m_assignment[std::size_t(track)].store(pack(a), std::memory_order_relaxed);
}

Assignment SongPlayer::assignment(int track) const noexcept
{
    return track >= 0 && track < kMaxTracks ? unpack(m_assignment[std::size_t(track)].load(std::memory_order_relaxed))
                                            : Assignment{};
}

void SongPlayer::setLevel(int track, float level) noexcept
{
    if (track >= 0 && track < kMaxTracks)
        m_level[std::size_t(track)].store(std::clamp(level, 0.0f, 1.0f), std::memory_order_relaxed);
}

float SongPlayer::level(int track) const noexcept
{
    return track >= 0 && track < kMaxTracks ? m_level[std::size_t(track)].load(std::memory_order_relaxed) : 0.0f;
}

void SongPlayer::setMute(int track, bool mute) noexcept
{
    if (track >= 0 && track < kMaxTracks)
        m_mute[std::size_t(track)].store(mute, std::memory_order_relaxed);
}

bool SongPlayer::mute(int track) const noexcept
{
    return track >= 0 && track < kMaxTracks && m_mute[std::size_t(track)].load(std::memory_order_relaxed);
}

void SongPlayer::setSolo(int track, bool solo) noexcept
{
    if (track >= 0 && track < kMaxTracks)
        m_solo[std::size_t(track)].store(solo, std::memory_order_relaxed);
}

bool SongPlayer::solo(int track) const noexcept
{
    return track >= 0 && track < kMaxTracks && m_solo[std::size_t(track)].load(std::memory_order_relaxed);
}

void SongPlayer::setSpeed(double speed) noexcept
{
    m_speed.store(std::clamp(speed, 0.25, 2.0), std::memory_order_relaxed);
}

void SongPlayer::cue(Tick tick) noexcept
{
    tick = std::max<Tick>(0, tick);
    m_cueRequest.store(tick, std::memory_order_relaxed);
    m_cuePending.store(true, std::memory_order_relaxed);
    m_commands.push({Command::Kind::Cue, nullptr, tick});
}

// --- Audio thread ---------------------------------------------------------------------

void SongPlayer::beforeTransport(Transport &transport) noexcept
{
    if (!m_song || !m_followTempo.load(std::memory_order_relaxed))
        return;
    // Where this block starts: the next tick while playing, or, stopped or
    // jumping, the cue render() is about to take (so the jump's own block
    // already runs at the new place's tempo).
    const bool jumping = m_cuePending.load(std::memory_order_relaxed);
    const Tick at = m_playing && !jumping ? m_next : m_cueRequest.load(std::memory_order_relaxed);
    const double bpm = m_song->tempo.bpmAt(at) * m_speed.load(std::memory_order_relaxed);
    if (std::abs(bpm - transport.bpm()) > 1e-9)
        transport.setBpm(bpm);
}

void SongPlayer::send(int device, std::uint32_t frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) noexcept
{
    if (m_outCount < m_outCapacity)
        m_out[m_outCount++] = DeviceEvent{device, MidiEvent{frame, 3, {status, d1, d2}}};
}

void SongPlayer::flush(int track, std::uint32_t frame) noexcept
{
    TrackState &st = m_tracks[std::size_t(track)];
    for (int i = 0; i < st.soundingCount; ++i) {
        const Sounding &s = st.sounding[std::size_t(i)];
        send(s.device, frame, std::uint8_t(0x80 | s.channel), s.key, 0);
    }
    if (st.touched && st.applied.device >= 0) { // bend back to centre, sustain up
        const auto status = std::uint8_t(st.applied.channel);
        send(st.applied.device, frame, std::uint8_t(0xE0 | status), 0x00, 0x40);
        send(st.applied.device, frame, std::uint8_t(0xB0 | status), kSustain, 0);
    }
    st.soundingCount = 0;
    st.touched = false;
}

void SongPlayer::flushAll(std::uint32_t frame) noexcept
{
    if (!m_song)
        return;
    for (int t = 0; t < int(m_song->tracks.size()); ++t)
        flush(t, frame);
}

void SongPlayer::seek(Tick tick) noexcept
{
    if (!m_song)
        return;
    for (std::size_t t = 0; t < m_song->tracks.size(); ++t) {
        const auto &events = m_song->tracks[t].events;
        m_tracks[t].cursor = std::size_t(
            std::lower_bound(events.begin(), events.end(), tick, [](const Event &e, Tick v) { return e.tick < v; })
            - events.begin());
    }
}

void SongPlayer::apply(const Command &command, const TransportState &transport) noexcept
{
    switch (command.kind) {
    case Command::Kind::Load:
        flushAll(0);
        if (m_song && !m_retired.push(m_song)) {
            // The GUI isn't collecting: keep the newer song anyway (the old one leaks rather than freeing here).
        }
        m_song = command.song;
        for (TrackState &st : m_tracks)
            st = TrackState{};
        m_cue = 0;
        m_ended.store(false, std::memory_order_relaxed);
        m_position.store(0, std::memory_order_relaxed);
        seek(0);
        if (m_playing && m_song)
            m_beatOffset = -transport.beatAtBlockStart; // from the top, now
        break;
    case Command::Kind::Cue:
        m_cue = command.tick;
        if (command.tick == m_cueRequest.load(std::memory_order_relaxed))
            m_cuePending.store(false, std::memory_order_relaxed); // the latest one is taken
        m_position.store(m_cue, std::memory_order_relaxed);
        if (m_playing && m_song) { // jump now
            flushAll(0);
            m_beatOffset = double(m_cue) / m_song->tempo.ppq() - transport.beatAtBlockStart;
            seek(m_cue);
            m_ended.store(false, std::memory_order_relaxed);
        }
        break;
    }
}

std::uint32_t SongPlayer::render(const TransportState &transport, std::uint32_t frames, double sampleRate,
                                 DeviceEvent *out, std::uint32_t capacity) noexcept
{
    m_out = out;
    m_outCount = 0;
    m_outCapacity = capacity;
    while (const auto command = m_commands.pop())
        apply(*command, transport);
    m_rolling.store(false, std::memory_order_relaxed); // until this block is known to play
    if (!m_song || frames == 0) {
        m_playing = m_playing && transport.playing;
        return m_outCount;
    }
    const int trackCount = int(std::min<std::size_t>(m_song->tracks.size(), kMaxTracks));

    // Settings: a track that went quiet or moved to another instrument ends its notes first.
    bool anySolo = false;
    for (int t = 0; t < trackCount; ++t)
        anySolo = anySolo || m_solo[std::size_t(t)].load(std::memory_order_relaxed);
    for (int t = 0; t < trackCount; ++t) {
        TrackState &st = m_tracks[std::size_t(t)];
        const Assignment a = unpack(m_assignment[std::size_t(t)].load(std::memory_order_relaxed));
        const bool audible = a.device >= 0 && !m_mute[std::size_t(t)].load(std::memory_order_relaxed)
                             && (!anySolo || m_solo[std::size_t(t)].load(std::memory_order_relaxed));
        if (!audible || !(a == st.applied))
            flush(t, 0);
        st.applied = a;
        st.audible = audible;
    }

    if (!transport.playing) {
        if (m_playing)
            flushAll(0);
        m_playing = false;
        m_next = m_cue;
        return m_outCount;
    }
    const int ppq = m_song->tempo.ppq();
    if (!m_playing || transport.restarted) { // play starts at the cue
        flushAll(0);
        m_beatOffset = double(m_cue) / ppq - transport.beatAtBlockStart;
        seek(m_cue);
        m_playing = true;
        m_ended.store(false, std::memory_order_relaxed);
    }

    const double t0 = (transport.beatAtBlockStart + m_beatOffset) * ppq;
    {   // The song time here, for a backing track: the tick's seconds, plus the fraction of a tick.
        const Tick whole = Tick(std::floor(std::max(0.0, t0)));
        const double fraction = std::max(0.0, t0) - double(whole);
        m_blockSeconds.store(m_song->tempo.secondsAt(whole) + fraction * 60.0 / (m_song->tempo.bpmAt(whole) * ppq),
                             std::memory_order_relaxed);
        m_rolling.store(true, std::memory_order_relaxed);
    }
    const double ticksPerFrame = transport.bpm / 60.0 / sampleRate * ppq;
    const double t1 = t0 + double(frames) * ticksPerFrame;
    const auto frameOf = [&](Tick tick) {
        const double f = (double(tick) - t0) / ticksPerFrame;
        return f <= 0.0 ? 0u : std::min(frames - 1, std::uint32_t(f + 1e-6)); // (rounding noise mustn't make it early)
    };

    for (int t = 0; t < trackCount; ++t) {
        TrackState &st = m_tracks[std::size_t(t)];
        const auto &events = m_song->tracks[std::size_t(t)].events;
        const float level = m_level[std::size_t(t)].load(std::memory_order_relaxed);
        const Assignment &a = st.applied;
        while (st.cursor < events.size() && double(events[st.cursor].tick) < t1) {
            const Event &e = events[st.cursor++];
            if (!st.audible)
                continue;
            const std::uint32_t frame = frameOf(e.tick);
            const auto channel = std::uint8_t(a.channel);
            switch (e.type()) {
            case 0x90:
            case 0x80: {
                const int key = a.drumMap ? mapToBasicKit(e.data1) : e.data1;
                if (key < 0)
                    break;
                if (e.type() == 0x90) {
                    if (st.soundingCount >= int(st.sounding.size()))
                        break; // more than 128 notes held on one track: let it go
                    const int velocity = std::clamp(int(std::lround(e.data2 * level)), 1, 127);
                    if (level <= 0.0f)
                        break;
                    send(a.device, frame, std::uint8_t(0x90 | channel), std::uint8_t(key), std::uint8_t(velocity));
                    st.sounding[std::size_t(st.soundingCount++)] = {std::int16_t(a.device), channel, std::uint8_t(key)};
                } else {
                    // Only notes this track started (and still holds) are ended.
                    for (int i = 0; i < st.soundingCount; ++i) {
                        const Sounding &s = st.sounding[std::size_t(i)];
                        if (s.key == key && s.channel == channel && s.device == a.device) {
                            send(a.device, frame, std::uint8_t(0x80 | channel), std::uint8_t(key), 0);
                            st.sounding[std::size_t(i)] = st.sounding[std::size_t(--st.soundingCount)];
                            break;
                        }
                    }
                }
                break;
            }
            case 0xE0:
                if (!a.drumMap) {
                    send(a.device, frame, std::uint8_t(0xE0 | channel), e.data1, e.data2);
                    st.touched = true;
                }
                break;
            case 0xB0: // sustain and modulation; other controllers (volume, pan, effects) would fight the mixer
                if (!a.drumMap && (e.data1 == kSustain || e.data1 == kModWheel)) {
                    send(a.device, frame, std::uint8_t(0xB0 | channel), e.data1, e.data2);
                    st.touched = true;
                }
                break;
            default:
                break;
            }
        }
    }

    if (t0 >= double(m_song->length) && !m_ended.load(std::memory_order_relaxed)) {
        flushAll(frames - 1);
        m_ended.store(true, std::memory_order_relaxed);
    }
    m_position.store(Tick(t0), std::memory_order_relaxed);
    m_next = Tick(t1);

    // Tracks were rendered one after another: put the block's events in frame order (stable).
    for (std::uint32_t k = 1; k < m_outCount; ++k)
        for (std::uint32_t j = k; j > 0 && out[j - 1].event.frame > out[j].event.frame; --j)
            std::swap(out[j - 1], out[j]);
    return m_outCount;
}

} // namespace krate::song
