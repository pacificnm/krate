#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A song to play along with (docs/SONG_PLAYER.md): its tracks' events and the
// timeline (tempo map and time signatures). Read from a MIDI file
// (MidiFile.h) or, later, a tab file. No Qt.
//
// Time is counted in ticks, `ppq` ticks per quarter note. The rack's transport
// counts beats in quarter notes, so a tick's transport beat is tick / ppq.
namespace krate::song {

using Tick = std::int64_t;

struct TempoChange
{
    Tick tick = 0;
    double bpm = 120.0; // quarter notes per minute
};

struct TimeSignature
{
    Tick tick = 0; // starts a bar
    int numerator = 4;
    int denominator = 4; // 1, 2, 4, 8, 16, 32
};

// Where a tick falls in the bars.
struct BarPosition
{
    int bar = 0;        // 0-based
    double beat = 0.0;  // 0-based, in the time signature's beats (eighths in 6/8)
    Tick barStart = 0;  // the bar's first tick
    TimeSignature time; // the bar's time signature
};

// Tempo and meter over the song's ticks.
class TempoMap
{
public:
    explicit TempoMap(int ppq = 480);

    int ppq() const noexcept { return m_ppq; }

    // Changes are kept sorted by tick; one at the same tick replaces the earlier
    // one. Without changes the song is 120 BPM in 4/4.
    void addTempo(Tick tick, double bpm);
    void addTimeSignature(Tick tick, int numerator, int denominator);
    const std::vector<TempoChange> &tempos() const noexcept { return m_tempos; }
    const std::vector<TimeSignature> &timeSignatures() const noexcept { return m_signatures; }

    double bpmAt(Tick tick) const noexcept;
    double beatAt(Tick tick) const noexcept { return double(tick) / m_ppq; } // quarter notes
    double secondsAt(Tick tick) const noexcept;
    Tick tickAtSeconds(double seconds) const noexcept;

    BarPosition barAt(Tick tick) const noexcept;
    Tick barStart(int bar) const noexcept; // bars past the last signature repeat it
    Tick barLength(const TimeSignature &time) const noexcept { return Tick(m_ppq) * 4 * time.numerator / time.denominator; }

private:
    int m_ppq;
    std::vector<TempoChange> m_tempos;        // first at tick 0
    std::vector<TimeSignature> m_signatures; // first at tick 0
};

// One channel voice message at a tick. Note-ons with velocity 0 are stored as note-offs.
struct Event
{
    Tick tick = 0;
    std::uint8_t status = 0; // 0x80 note off, 0x90 note on, 0xB0 CC, 0xE0 pitch bend (channel in the low nibble)
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;

    int type() const noexcept { return status & 0xF0; }
    int channel() const noexcept { return status & 0x0F; }
};

struct Track
{
    std::string name;       // as in the file (bytes; usually ASCII or UTF-8)
    std::string instrument; // the file's instrument name, if any
    int channel = 0;        // 0..15 (the file's; a track is one channel)
    int program = -1;       // the first program change, -1 = none
    bool drums = false;     // channel 10, the drum-kit channel
    std::vector<Event> events; // in tick order; every note-on has its note-off
    int noteCount = 0;
    int lowestNote = -1, highestNote = -1;
};

struct Song
{
    std::string title;
    TempoMap tempo;
    std::vector<Track> tracks; // only tracks with notes or controllers
    Tick length = 0;           // the last event's tick
};

} // namespace krate::song
