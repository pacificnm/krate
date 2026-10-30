#include "SongImport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include "song/Arrange.h"

using namespace krate::seq;
using krate::song::Event;
using krate::song::Score;
using krate::song::Song;

namespace songimport {

int barCount(const Song &song, const Score *score)
{
    if (score && !score->bars.empty())
        return int(score->bars.size());
    return song.tempo.barAt(std::max<Tick>(song.length - 1, 0)).bar + 1;
}

std::vector<BarSpan> barSpans(const Song &song, const Score *score, int from, int to)
{
    std::vector<BarSpan> spans;
    const int count = barCount(song, score);
    from = std::max(from, 0);
    to = std::min(to, count - 1);
    for (int bar = from; bar <= to; ++bar) {
        Tick start = -1;
        if (score && !score->playOrder.empty()) {
            for (std::size_t k = 0; k < score->playOrder.size(); ++k)
                if (score->playOrder[k] == bar) {
                    start = score->playStarts[k];
                    break;
                }
            if (start < 0)
                continue; // never played
        } else {
            start = song.tempo.barStart(bar);
        }
        const int played = song.tempo.barAt(start).bar;
        spans.push_back({start, song.tempo.barStart(played + 1)});
    }
    return spans;
}

namespace {

struct RawNote
{
    Tick start, end;
    int key, velocity;
};

std::vector<RawNote> notesOf(const krate::song::Track &track)
{
    std::vector<RawNote> notes;
    std::map<int, std::pair<Tick, int>> open; // channel * 128 + key -> (start, velocity)
    for (const Event &e : track.events) {
        const int type = e.status & 0xF0, slot = (e.status & 0x0F) * 128 + e.data1;
        if (type == 0x90 && e.data2 > 0) {
            if (const auto it = open.find(slot); it != open.end()) // the same key again: the earlier one ends
                notes.push_back({it->second.first, e.tick, e.data1, it->second.second});
            open[slot] = {e.tick, e.data2};
        } else if (type == 0x80 || type == 0x90) {
            if (const auto it = open.find(slot); it != open.end()) {
                notes.push_back({it->second.first, e.tick, e.data1, it->second.second});
                open.erase(it);
            }
        }
    }
    std::sort(notes.begin(), notes.end(), [](const RawNote &a, const RawNote &b) { return a.start < b.start; });
    return notes;
}

// The grids tried, coarsest first; a finer one is taken only when it moves fewer notes.
constexpr std::array<int, 3> kTryGrids{0, 2, 1}; // 1/16, 1/16T, 1/32

} // namespace

Result convert(const Song &song, int track, const std::vector<BarSpan> &bars, Options options)
{
    Result result;
    if (track < 0 || track >= int(song.tracks.size()) || bars.empty())
        return result;
    const double toSeq = double(kTicksPerBeat) / song.tempo.ppq();
    const std::vector<RawNote> raw = notesOf(song.tracks[std::size_t(track)]);

    // Bars into patterns of up to 16 beats (at least one bar each; a longer bar is cut).
    std::size_t b = 0;
    while (b < bars.size()) {
        Chunk chunk;
        chunk.firstBar = int(b);
        double ticks = 0.0;
        while (b < bars.size()) {
            const double barTicks = double(bars[b].end - bars[b].start) * toSeq;
            if (chunk.barCount > 0 && ticks + barTicks > kMaxTicks + 0.5)
                break;
            ticks += barTicks;
            ++chunk.barCount;
            ++b;
        }
        chunk.length = std::uint16_t(std::clamp<long>(std::lround(ticks), 1, kMaxTicks));

        // The notes starting in its bars, in pattern ticks (exact). A note may run
        // on into the next bar when the song plays it straight after.
        struct Exact
        {
            double start, length;
            int key, velocity;
        };
        std::vector<Exact> exact;
        double offset = 0.0;
        for (int i = 0; i < chunk.barCount; ++i) {
            const BarSpan &span = bars[std::size_t(chunk.firstBar + i)];
            Tick runEnd = span.end; // the end of the run of bars that follow on in the song
            for (int j = i + 1; j < chunk.barCount && bars[std::size_t(chunk.firstBar + j)].start == runEnd; ++j)
                runEnd = bars[std::size_t(chunk.firstBar + j)].end;
            for (const RawNote &n : raw) {
                if (n.start < span.start || n.start >= span.end)
                    continue;
                int key = n.key;
                if (options.drumMap && (key = krate::song::mapToBasicKit(n.key)) < 0) {
                    ++result.report.dropped;
                    continue;
                }
                exact.push_back({offset + double(n.start - span.start) * toSeq,
                                 double(std::min(n.end, runEnd) - n.start) * toSeq, key, n.velocity});
            }
            offset += double(span.end - span.start) * toSeq;
        }

        // The grid that moves the fewest notes.
        const auto moved = [&exact](int ticks) {
            int count = 0;
            for (const Exact &e : exact)
                if (std::fabs(e.start - std::round(e.start / ticks) * ticks) > 0.5)
                    ++count;
            return count;
        };
        int best = kTryGrids[0], bestMoved = moved(kGridTicks[std::size_t(best)]);
        for (std::size_t g = 1; g < kTryGrids.size(); ++g)
            if (const int m = moved(kGridTicks[std::size_t(kTryGrids[g])]); m < bestMoved) {
                best = kTryGrids[g];
                bestMoved = m;
            }
        chunk.grid = std::uint8_t(best);
        const int grid = kGridTicks[std::size_t(best)];
        result.report.snapped += bestMoved;

        // Snapped notes; a doubled note (same start and key) keeps the louder.
        std::vector<Note> notes;
        for (const Exact &e : exact) {
            const int start = std::min(int(std::lround(e.start / grid)) * grid, chunk.length - grid);
            const int length = std::max(grid, int(std::lround(e.length / grid)) * grid);
            const Note n{std::uint16_t(std::max(start, 0)), std::uint16_t(std::min(length, kMaxTicks)),
                         std::uint8_t(std::clamp(e.key, 0, 127)), std::uint8_t(std::clamp(e.velocity, 1, 127))};
            const auto same = std::find_if(notes.begin(), notes.end(),
                                           [&n](const Note &o) { return o.start == n.start && o.pitch == n.pitch; });
            if (same != notes.end()) {
                same->velocity = std::max(same->velocity, n.velocity);
                ++result.report.reduced;
            } else {
                notes.push_back(n);
            }
        }
        if (options.mono) { // a chord keeps its top note
            std::vector<Note> top;
            for (const Note &n : notes) {
                const auto at = std::find_if(top.begin(), top.end(), [&n](const Note &o) { return o.start == n.start; });
                if (at == top.end()) {
                    top.push_back(n);
                    continue;
                }
                if (n.pitch > at->pitch)
                    *at = n;
                ++result.report.reduced;
            }
            notes = std::move(top);
        }
        if (notes.size() > std::size_t(kMaxNotes)) {
            result.report.overflow += int(notes.size()) - kMaxNotes;
            notes.resize(std::size_t(kMaxNotes));
        }
        result.report.notes += int(notes.size());
        chunk.notes = std::move(notes);
        result.chunks.push_back(std::move(chunk));
    }
    return result;
}

} // namespace songimport
