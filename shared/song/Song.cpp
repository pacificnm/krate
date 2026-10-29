#include "song/Song.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace krate::song {

TempoMap::TempoMap(int ppq) : m_ppq(ppq > 0 ? ppq : 480)
{
    m_tempos.push_back({0, 120.0});
    m_signatures.push_back({0, 4, 4});
}

void TempoMap::addTempo(Tick tick, double bpm)
{
    if (tick < 0 || !(bpm > 0.0))
        return;
    const auto at = std::lower_bound(m_tempos.begin(), m_tempos.end(), tick,
                                     [](const TempoChange &c, Tick t) { return c.tick < t; });
    if (at != m_tempos.end() && at->tick == tick)
        at->bpm = bpm;
    else
        m_tempos.insert(at, {tick, bpm});
}

void TempoMap::addTimeSignature(Tick tick, int numerator, int denominator)
{
    if (tick < 0 || numerator < 1 || denominator < 1 || (denominator & (denominator - 1)) != 0)
        return;
    const auto at = std::lower_bound(m_signatures.begin(), m_signatures.end(), tick,
                                     [](const TimeSignature &s, Tick t) { return s.tick < t; });
    if (at != m_signatures.end() && at->tick == tick)
        *at = {tick, numerator, denominator};
    else
        m_signatures.insert(at, {tick, numerator, denominator});
}

double TempoMap::bpmAt(Tick tick) const noexcept
{
    const auto after = std::upper_bound(m_tempos.begin(), m_tempos.end(), tick,
                                        [](Tick t, const TempoChange &c) { return t < c.tick; });
    return std::prev(after)->bpm;
}

double TempoMap::secondsAt(Tick tick) const noexcept
{
    double seconds = 0.0;
    for (std::size_t i = 0; i < m_tempos.size(); ++i) {
        const Tick start = m_tempos[i].tick;
        if (start >= tick)
            break;
        const Tick end = i + 1 < m_tempos.size() ? std::min(m_tempos[i + 1].tick, tick) : tick;
        seconds += double(end - start) / m_ppq * 60.0 / m_tempos[i].bpm;
    }
    return seconds;
}

Tick TempoMap::tickAtSeconds(double seconds) const noexcept
{
    double elapsed = 0.0;
    for (std::size_t i = 0; i < m_tempos.size(); ++i) {
        const double perTick = 60.0 / m_tempos[i].bpm / m_ppq;
        const bool last = i + 1 == m_tempos.size();
        const double span = last ? 0.0 : double(m_tempos[i + 1].tick - m_tempos[i].tick) * perTick;
        if (last || elapsed + span > seconds)
            return m_tempos[i].tick + Tick(std::llround((seconds - elapsed) / perTick));
        elapsed += span;
    }
    return 0;
}

BarPosition TempoMap::barAt(Tick tick) const noexcept
{
    BarPosition pos;
    int bar = 0;
    for (std::size_t i = 0; i < m_signatures.size(); ++i) {
        const TimeSignature &time = m_signatures[i];
        const Tick length = barLength(time);
        const bool last = i + 1 == m_signatures.size();
        // A signature lasts until the next one (which starts a bar; a change
        // mid-bar starts it early, cutting the bar short).
        const Tick end = last ? tick + 1 : m_signatures[i + 1].tick;
        if (last || tick < end) {
            const Tick into = std::max<Tick>(0, tick - time.tick);
            const Tick bars = into / length;
            pos.bar = bar + int(bars);
            pos.barStart = time.tick + bars * length;
            pos.time = time;
            pos.beat = double(tick - pos.barStart) / (length / double(time.numerator));
            return pos;
        }
        bar += int((end - time.tick + length - 1) / length);
    }
    return pos;
}

Tick TempoMap::barStart(int bar) const noexcept
{
    int first = 0; // the bar number where signature i starts
    for (std::size_t i = 0; i < m_signatures.size(); ++i) {
        const TimeSignature &time = m_signatures[i];
        const Tick length = barLength(time);
        const bool last = i + 1 == m_signatures.size();
        const int bars = last ? 0 : int((m_signatures[i + 1].tick - time.tick + length - 1) / length);
        if (last || bar < first + bars)
            return time.tick + Tick(std::max(0, bar - first)) * length;
        first += bars;
    }
    return 0;
}

// Pairs note-ons and note-offs per channel and key: a note-on on a sounding
// key ends the earlier note first, and notes still on at the end are ended
// there. Also counts the notes and finds the track's main channel.
void finishTrack(Track &track, Tick end)
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

} // namespace krate::song
