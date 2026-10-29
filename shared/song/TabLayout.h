#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "song/Score.h"

// Lays out one track of a Score as guitar tab (docs/SONG_PLAYER.md#ui): bars
// packed into systems across a given width, each system a staff of the
// track's strings with fret numbers, technique marks and rhythm stems below.
// The result is a list of simple drawing items per system, so the app's
// painter stays trivial and the layout can be tested. Also maps a song
// position to the cursor and a point to a bar. No Qt.
namespace krate::song {

struct TabStyle
{
    double lineGap = 13.0;   // between strings
    double fretSize = 12.0;  // fret numbers' font size
    double smallSize = 10.0; // bar numbers, techniques, tuning
    double headerWidth = 26.0;
    double topSpace = 36.0;  // above the top string: bar numbers, techniques, endings
    double rhythmSpace = 30.0;
    double systemGap = 14.0;
};

struct TabItem
{
    enum class Kind : std::uint8_t { Line, Text, Dot } kind = Kind::Line;
    enum class Tone : std::uint8_t { Ink, Faint, Accent } tone = Tone::Ink;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0; // Line: ends; Text: centre (x1, y1); Dot: centre, radius x2
    double width = 1.0;                    // Line thickness
    std::string text;                      // UTF-8
    double size = 12.0;
    bool bold = false;
    bool knockout = false; // Text: clear the staff lines behind it
};

struct TabBarBox
{
    int bar = 0;                 // written (master) bar index
    double x0 = 0, x1 = 0;       // bar lines
    double contentX0 = 0, contentX1 = 0; // where its time runs, start to end
    Tick length = 0;             // in score ticks
    std::vector<Tick> ticks;     // beat starts (all voices), from the bar's start
    std::vector<double> xs;      // their x
};

struct TabSystem
{
    double top = 0, height = 0;
    double staffTop = 0, staffBottom = 0; // the top and bottom strings' y
    std::vector<TabBarBox> bars;
    std::vector<TabItem> items;
};

struct TabLayoutResult
{
    std::vector<TabSystem> systems;
    double height = 0;
    int strings = 0;
    std::vector<std::pair<int, int>> barPlace; // per written bar: (system, index in it), or (-1, -1)
};

// Empty for drum tracks and tracks without strings.
TabLayoutResult layoutTab(const Score &score, int track, double width, const TabStyle &style = {});

struct TabCursor
{
    int system = -1; // -1: nowhere (no layout, or past the end)
    double x = 0;
    int bar = -1;    // written bar
};

// Where the cursor goes for a song position (score ticks as played, repeats expanded).
TabCursor cursorAt(const Score &score, const TabLayoutResult &layout, Tick songTick);

// The written bar at a point, or -1.
int barAt(const TabLayoutResult &layout, double x, double y);

} // namespace krate::song
