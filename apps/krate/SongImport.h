#pragma once

#include <vector>

#include "seq/Pattern.h"
#include "song/Score.h"
#include "song/Song.h"

// Copying bars of a song's track into the rack sequencer (docs/SEQUENCER.md,
// Q3): the bars become patterns of up to 4 bars (16 beats), the notes fitted
// to each pattern's grid. No Qt.
namespace songimport {

using krate::song::Tick;

// A bar of the song, in its ticks (play order: repeats expanded).
struct BarSpan
{
    Tick start = 0, end = 0;
};

// How many bars there are to pick from: written bars for a tab file, the
// tempo map's bars for a MIDI file alone.
int barCount(const krate::song::Song &song, const krate::song::Score *score);
// Bars `from` .. `to` (0-based, inclusive). A written bar is taken from the
// first time it plays.
std::vector<BarSpan> barSpans(const krate::song::Song &song, const krate::song::Score *score, int from, int to);

struct Options
{
    bool drumMap = false; // map drum-kit notes to Kessel's voices (unmapped ones are dropped)
    bool mono = false;    // one note at a time (Korrode): a chord keeps its top note
};

// One pattern's worth.
struct Chunk
{
    int firstBar = 0, barCount = 0; // index into the spans given
    std::uint16_t length = 0;       // ticks (96 per beat)
    std::uint8_t grid = 0;          // index into krate::seq::kGridTicks
    std::vector<krate::seq::Note> notes;
};

struct Report
{
    int notes = 0;    // copied
    int snapped = 0;  // moved to the grid
    int dropped = 0;  // drum notes Kessel has no voice for
    int reduced = 0;  // chord notes left out (mono), or doubles after snapping
    int overflow = 0; // past a pattern track's note limit
};

struct Result
{
    std::vector<Chunk> chunks;
    Report report;
};

Result convert(const krate::song::Song &song, int track, const std::vector<BarSpan> &bars, Options options);

} // namespace songimport
