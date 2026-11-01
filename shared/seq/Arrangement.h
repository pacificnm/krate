#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "seq/Pattern.h"

// The arranger's song (docs/DAW.md, D3): the sequencer's 8 tracks as lanes,
// each holding clips laid out across bars (4 beats each). A pattern clip
// plays one of the bank's patterns, that track's part of it, looping for as
// long as the clip lasts; a MIDI clip holds notes of its own (recorded from
// the keyboard). A clip can start partway in (`offsetBars`: trimmed at its
// start, or the second half of a split). Built on the GUI thread, handed to
// the sequencer whole, read only by the audio thread.
namespace krate::seq {

inline constexpr int kBeatsPerBar = 4;
inline constexpr int kMaxBars = 999;

struct ClipNote
{
    std::uint32_t start = 0;  // ticks from the clip's start
    std::uint32_t length = 1; // ticks, >= 1
    std::uint8_t pitch = 60;
    std::uint8_t velocity = 100;
    bool operator==(const ClipNote &) const = default;
};

struct Clip
{
    int startBar = 0;
    int bars = 1;     // >= 1
    int pattern = -1; // a pattern clip: the bank's pattern (0..kPatterns-1)
    int midi = -1;    // a MIDI clip: its index in Arrangement::midi
    int offsetBars = 0; // how far into its pattern (round and round) or notes it starts
    int endBar() const noexcept { return startBar + bars; }
    bool operator==(const Clip &) const = default;
};

struct MidiClip
{
    std::vector<ClipNote> notes; // in start order
    bool operator==(const MidiClip &) const = default;
};

struct Arrangement
{
    std::array<std::vector<Clip>, kTracks> lanes{}; // each by start, not overlapping
    std::vector<MidiClip> midi;

    // The bar after the last clip (0: empty).
    int endBar() const noexcept
    {
        int end = 0;
        for (const auto &lane : lanes)
            for (const Clip &c : lane)
                end = c.endBar() > end ? c.endBar() : end;
        return end;
    }
    bool operator==(const Arrangement &) const = default;
};

} // namespace krate::seq
