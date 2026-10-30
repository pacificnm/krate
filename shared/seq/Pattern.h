#pragma once

#include <array>
#include <cstdint>

// The rack sequencer's data (docs/SEQUENCER.md): 16 patterns, each up to 4
// bars, each holding 8 tracks of notes. Plain values, no Qt, no allocation.
namespace krate::seq {

inline constexpr int kTracks = 8;
inline constexpr int kPatterns = 16;
inline constexpr int kTicksPerBeat = 96;
inline constexpr int kMaxTicks = 16 * kTicksPerBeat; // 4 bars of 4/4
inline constexpr int kMaxNotes = 512;                 // per track, per pattern

// Grid steps: 1/16, 1/32, 1/16T, 1/8T (ticks per step).
inline constexpr std::array<int, 4> kGridTicks{24, 12, 16, 32};

struct Note
{
    std::uint16_t start = 0;  // ticks from the pattern's start, < its length
    std::uint16_t length = 0; // ticks, >= 1
    std::uint8_t pitch = 60;
    std::uint8_t velocity = 100; // 1..127

    bool operator==(const Note &) const = default;
};

struct TrackNotes
{
    std::array<Note, kMaxNotes> notes{};
    std::uint16_t count = 0;

    bool add(Note n) noexcept
    {
        if (count >= kMaxNotes)
            return false;
        notes[count++] = n;
        return true;
    }
};

struct Pattern
{
    std::array<TrackNotes, kTracks> tracks{};
    std::uint16_t length = 4 * kTicksPerBeat; // ticks: 1 bar by default
    std::uint8_t grid = 0;                    // index into kGridTicks

    int gridTicks() const noexcept { return kGridTicks[std::size_t(grid < kGridTicks.size() ? grid : 0)]; }
    bool empty() const noexcept
    {
        for (const TrackNotes &t : tracks)
            if (t.count > 0)
                return false;
        return true;
    }
};

struct Bank
{
    std::array<Pattern, kPatterns> patterns{};
};

} // namespace krate::seq
