#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "song/Song.h"

// A tab file's score (docs/SONG_PLAYER.md): what the tab view draws. Bars as
// written (repeats not expanded), each track's beats with durations, and
// notes with string, fret and techniques. Times inside a bar are in the
// score's ticks (Score::ppq per quarter note), from the bar's start. No Qt.
namespace krate::song {

// Note techniques (bits).
enum Technique : std::uint32_t {
    TieDestination = 1u << 0, // continues the previous note on the string (no new attack)
    HammerOn = 1u << 1,       // a hammer-on or pull-off starts here (to the next note)
    HammerTarget = 1u << 2,   // reached by a hammer-on or pull-off
    Slide = 1u << 3,
    DeadNote = 1u << 4,
    PalmMute = 1u << 5,
    Harmonic = 1u << 6,
    Vibrato = 1u << 7,
    Accent = 1u << 8,
    Bend = 1u << 9,
    LetRing = 1u << 10,
    GhostNote = 1u << 11,
};

struct BendPoint
{
    double offset = 0.0;    // 0..1 of the note's length
    double semitones = 0.0; // above the fretted pitch
};

struct ScoreNote
{
    int string = 0; // 0 = the lowest string
    int fret = 0;
    int pitch = 0;  // MIDI note number as it sounds (drums: the kit's note)
    std::uint32_t techniques = 0;
    std::vector<BendPoint> bend; // when Bend: origin, (middle,) destination
};

struct ScoreBeat
{
    Tick start = 0;    // from the bar's start
    Tick length = 0;
    int noteValue = 4; // 1 whole, 2 half, 4 quarter ... 128
    int dots = 0;
    int tupletNum = 0, tupletDen = 0; // e.g. 3:2 for triplets; 0 = none
    int velocity = 95;                // from the beat's dynamic (MIDI 1..127)
    std::vector<ScoreNote> notes;     // empty: a rest
};

struct ScoreBar
{
    std::vector<std::vector<ScoreBeat>> voices; // up to four voices
};

struct ScoreTrack
{
    std::string name;
    std::string instrument;
    std::vector<int> tuning; // MIDI pitch per string, lowest string first; empty for drums
    int capo = 0;
    bool drums = false;
    int program = -1;
    std::vector<ScoreBar> bars; // one per master bar
};

struct MasterBar
{
    TimeSignature time;            // tick = where the bar starts as written (not played)
    bool repeatStart = false;
    bool repeatEnd = false;
    int repeatCount = 0;           // times the section plays, at a repeat end
    std::uint32_t endings = 0;     // alternate ending: bit n = plays on pass n + 1 (0 = always)
};

struct TempoMark
{
    int bar = 0;           // master bar index
    double position = 0.0; // 0..1 into the bar
    double bpm = 120.0;    // quarter notes per minute
};

struct Score
{
    std::string title, artist;
    int ppq = 960;
    std::vector<MasterBar> bars;
    std::vector<TempoMark> tempos; // in bar and position order
    std::vector<ScoreTrack> tracks;
    std::vector<int> playOrder;   // bar indices as played (repeats expanded)
    std::vector<Tick> playStarts; // each played bar's start in Song ticks
};

} // namespace krate::song
