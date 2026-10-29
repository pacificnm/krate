#include "song/MidiFile.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace krate::song {

namespace {

constexpr int kDrumChannel = 9;
constexpr std::size_t kMaxFileBytes = 64u << 20;

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> data) : m_data(data) {}

    bool atEnd() const { return m_pos >= m_data.size(); }
    bool failed() const { return m_failed; }
    std::size_t pos() const { return m_pos; }
    void seek(std::size_t pos) { m_pos = pos; }

    std::uint8_t u8()
    {
        if (m_pos >= m_data.size()) {
            m_failed = true;
            return 0;
        }
        return m_data[m_pos++];
    }
    std::uint8_t peek() const { return m_pos < m_data.size() ? m_data[m_pos] : 0; }
    std::uint32_t be(int bytes)
    {
        std::uint32_t v = 0;
        for (int i = 0; i < bytes; ++i)
            v = v << 8 | u8();
        return v;
    }
    std::uint32_t vlq() // variable-length quantity, at most 4 bytes
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const std::uint8_t c = u8();
            v = v << 7 | (c & 0x7F);
            if (!(c & 0x80))
                return v;
        }
        m_failed = true;
        return v;
    }
    std::string bytes(std::uint32_t n)
    {
        if (n > m_data.size() - std::min(m_pos, m_data.size())) {
            m_failed = true;
            m_pos = m_data.size();
            return {};
        }
        std::string s(reinterpret_cast<const char *>(m_data.data() + m_pos), n);
        m_pos += n;
        return s;
    }

private:
    std::span<const std::uint8_t> m_data;
    std::size_t m_pos = 0;
    bool m_failed = false;
};

// One file track's channel events, split by channel as they're read.
struct Part
{
    std::vector<Event> events;
    int program = -1;
};

// Pairs note-ons and note-offs per channel and key: a note-on on a sounding
// key ends the earlier note first, and notes still on at the end are ended
// there. Also counts the notes and finds the track's main channel.
void closeNotes(Track &track, Tick end)
{
    std::array<std::array<std::uint8_t, 128>, 16> on{};
    std::array<int, 16> perChannel{};
    std::vector<Event> out;
    out.reserve(track.events.size());
    for (const Event &e : track.events) {
        std::uint8_t &count = on[std::size_t(e.channel())][e.data1];
        if (e.type() == 0x90) {
            if (count > 0) {
                out.push_back({e.tick, std::uint8_t(0x80 | e.channel()), e.data1, 0});
                --count;
            }
            ++count;
            ++track.noteCount;
            ++perChannel[std::size_t(e.channel())];
            track.lowestNote = track.lowestNote < 0 ? e.data1 : std::min<int>(track.lowestNote, e.data1);
            track.highestNote = std::max<int>(track.highestNote, e.data1);
        } else if (e.type() == 0x80) {
            if (count == 0)
                continue; // a stray note-off
            --count;
        }
        out.push_back(e);
    }
    for (int channel = 0; channel < 16; ++channel)
        for (int key = 0; key < 128; ++key)
            for (std::uint8_t &count = on[std::size_t(channel)][std::size_t(key)]; count > 0; --count)
                out.push_back({end, std::uint8_t(0x80 | channel), std::uint8_t(key), 0});
    track.events = std::move(out);
    track.channel = int(std::max_element(perChannel.begin(), perChannel.end()) - perChannel.begin());
    track.drums = track.noteCount > 0 && perChannel[std::size_t(kDrumChannel)] == track.noteCount;
}

// Names in files are often Latin-1; the rest of Krate expects UTF-8.
std::string toUtf8(const std::string &s)
{
    // Valid UTF-8 (which includes plain ASCII) is kept as it is.
    std::size_t i = 0;
    bool valid = true;
    while (i < s.size() && valid) {
        const auto c = std::uint8_t(s[i]);
        const int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
        if (extra < 0 || i + std::size_t(extra) >= s.size() + (extra == 0 ? 1 : 0)) {
            valid = false;
            break;
        }
        for (int k = 1; k <= extra; ++k)
            if ((std::uint8_t(s[i + std::size_t(k)]) & 0xC0) != 0x80)
                valid = false;
        i += std::size_t(extra) + 1;
    }
    if (valid)
        return s;
    std::string out;
    for (const char ch : s) {
        const auto c = std::uint8_t(ch);
        if (c < 0x80) {
            out += char(c);
        } else {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 0x3F));
        }
    }
    return out;
}

} // namespace

MidiFileResult readMidiFile(std::span<const std::uint8_t> data)
{
    Reader r(data);
    if (r.be(4) != 0x4D546864) // "MThd"
        return {std::nullopt, "not a MIDI file"};
    const std::uint32_t headerLength = r.be(4);
    const int format = int(r.be(2));
    const int trackCount = int(r.be(2));
    const int division = int(r.be(2));
    if (r.failed() || headerLength < 6)
        return {std::nullopt, "the MIDI file's header is cut short"};
    if (format > 1)
        return {std::nullopt, "MIDI file format 2 (independent sequences) isn't supported"};
    if (division & 0x8000)
        return {std::nullopt, "MIDI files timed in SMPTE frames aren't supported"};
    if (division == 0)
        return {std::nullopt, "the MIDI file has no tick resolution"};
    r.seek(8 + headerLength);

    Song song;
    song.tempo = TempoMap(division);
    struct Named
    {
        std::string name, instrument;
        std::map<int, Part> parts; // by channel (format 0 splits them)
    };
    std::vector<Named> fileTracks;
    Tick songEnd = 0;

    for (int t = 0; t < trackCount && !r.atEnd(); ++t) {
        const std::uint32_t id = r.be(4);
        const std::uint32_t length = r.be(4);
        if (r.failed())
            return {std::nullopt, "the MIDI file is cut short"};
        const std::size_t end = r.pos() + length;
        if (id != 0x4D54726B) { // not "MTrk": skip unknown chunks
            r.seek(end);
            --t;
            continue;
        }
        Named track;
        Tick tick = 0;
        std::uint8_t running = 0;
        while (r.pos() < std::min(end, data.size())) {
            tick += r.vlq();
            std::uint8_t status = r.peek();
            if (status & 0x80) {
                r.u8();
                if (status < 0xF0)
                    running = status;
                else if (status < 0xF8)
                    running = 0; // system messages cancel running status
            } else if (running) {
                status = running;
            } else {
                return {std::nullopt, "the MIDI file has data with no status byte"};
            }

            if (status == 0xFF) { // meta event
                const int type = r.u8();
                const std::string body = r.bytes(r.vlq());
                const auto byte = [&](std::size_t i) { return i < body.size() ? std::uint8_t(body[i]) : std::uint8_t(0); };
                switch (type) {
                case 0x03:
                    if (track.name.empty())
                        track.name = body;
                    break;
                case 0x04:
                    track.instrument = body;
                    break;
                case 0x51:
                    if (body.size() >= 3)
                        if (const std::uint32_t us = std::uint32_t(byte(0)) << 16 | std::uint32_t(byte(1)) << 8 | byte(2); us > 0)
                            song.tempo.addTempo(tick, 60000000.0 / us);
                    break;
                case 0x58:
                    if (body.size() >= 2 && byte(1) < 8)
                        song.tempo.addTimeSignature(tick, byte(0), 1 << byte(1));
                    break;
                case 0x2F:
                    r.seek(end);
                    break;
                default: // lyrics, markers, keys etc. are not kept
                    break;
                }
                continue;
            }
            if (status == 0xF0 || status == 0xF7) { // sysex
                r.bytes(r.vlq());
                continue;
            }
            if (status >= 0xF0)
                continue; // other system messages have no data in a file

            const int type = status & 0xF0;
            const int channel = status & 0x0F;
            const std::uint8_t d1 = r.u8() & 0x7F;
            const std::uint8_t d2 = (type == 0xC0 || type == 0xD0) ? 0 : (r.u8() & 0x7F);
            Part &part = track.parts[channel];
            switch (type) {
            case 0x90:
                part.events.push_back({tick, std::uint8_t((d2 > 0 ? 0x90 : 0x80) | channel), d1, d2});
                break;
            case 0x80:
                part.events.push_back({tick, std::uint8_t(0x80 | channel), d1, 0});
                break;
            case 0xB0:
            case 0xE0:
                part.events.push_back({tick, status, d1, d2});
                break;
            case 0xC0:
                if (part.program < 0)
                    part.program = d1;
                break;
            default: // aftertouch is not kept
                break;
            }
            songEnd = std::max(songEnd, tick);
        }
        if (r.failed())
            return {std::nullopt, "the MIDI file is cut short"};
        r.seek(end);
        songEnd = std::max(songEnd, tick);
        fileTracks.push_back(std::move(track));
    }
    if (fileTracks.empty())
        return {std::nullopt, "the MIDI file has no tracks"};

    // Format 1's first track names the song when it has no notes (the conductor track).
    if (format == 1) {
        bool notes = false;
        for (const auto &[channel, part] : fileTracks.front().parts)
            for (const Event &e : part.events)
                notes = notes || e.type() == 0x90;
        if (!notes)
            song.title = toUtf8(fileTracks.front().name);
    }

    const auto addTrack = [&](Track track) {
        closeNotes(track, songEnd);
        if (track.noteCount > 0)
            song.tracks.push_back(std::move(track));
    };
    for (std::size_t t = 0; t < fileTracks.size(); ++t) {
        Named &file = fileTracks[t];
        const std::string name = file.name.empty() ? "Track " + std::to_string(t + 1) : toUtf8(file.name);
        if (format == 0) { // the whole band in one track: a part per channel
            for (auto &[channel, part] : file.parts) {
                Track track;
                track.name = name + " ch " + std::to_string(channel + 1);
                track.instrument = toUtf8(file.instrument);
                track.program = part.program;
                track.events = std::move(part.events);
                addTrack(std::move(track));
            }
            continue;
        }
        Track track;
        track.name = name;
        track.instrument = toUtf8(file.instrument);
        for (auto &[channel, part] : file.parts) {
            if (track.program < 0)
                track.program = part.program;
            track.events.insert(track.events.end(), part.events.begin(), part.events.end());
        }
        std::stable_sort(track.events.begin(), track.events.end(),
                         [](const Event &a, const Event &b) { return a.tick < b.tick; });
        addTrack(std::move(track));
    }
    song.length = songEnd;
    return {std::move(song), {}};
}

MidiFileResult readMidiFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {std::nullopt, "can't open " + path};
    std::vector<std::uint8_t> data;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size < 0 || std::size_t(size) > kMaxFileBytes)
        return {std::nullopt, "the MIDI file is too large"};
    file.seekg(0);
    data.resize(std::size_t(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
    if (!file)
        return {std::nullopt, "can't read " + path};
    return readMidiFile(std::span<const std::uint8_t>(data));
}

} // namespace krate::song
