#include "song/Song.h"

#include <algorithm>
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

} // namespace krate::song
