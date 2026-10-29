#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace kalliope {

// The step sequencer's data (docs/SEQUENCER.md#patterns). Trivially copyable:
// whole patterns go to the audio thread through a lock-free queue.
struct SeqStep
{
    std::array<std::uint8_t, 4> notes{}; // MIDI notes; the first `count` are used
    std::uint8_t count = 0;              // 0 = rest (unless tie)
    bool tie = false;                    // hold the previous step's notes

    bool operator==(const SeqStep &) const = default;
};

struct SeqPattern
{
    std::array<SeqStep, 16> steps{};
    std::uint8_t length = 16; // 1..16

    bool operator==(const SeqPattern &) const = default;
};

inline constexpr int kSeqPatterns = 8;
inline constexpr int kSeqTransposeCentre = 60; // a key transposes by its distance from middle C

// The pattern generator (docs/SEQUENCER.md#pattern-generator): a new pattern of
// `length` steps from the notes in `pool` (spread over `octaves`), with
// `density` 0..1 the chance a step plays. Deterministic for a given seed.
SeqPattern generatePattern(std::span<const int> pool, int octaves, double density, int length,
                           std::uint32_t seed) noexcept;

// The generator's notes when nothing is held and the pattern is empty: C minor pentatonic from C3.
inline constexpr std::array<int, 5> kDefaultScale{48, 51, 53, 55, 58};

} // namespace kalliope
