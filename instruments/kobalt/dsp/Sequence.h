#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace kobalt {

// The step sequencer's data (docs/BRIEF.md#step-sequencer). Trivially
// copyable: whole patterns go to the audio thread through a lock-free queue.
struct SeqStep
{
    enum class Kind : std::uint8_t { Rest, Note, Tie };

    Kind kind = Kind::Rest;
    std::uint8_t note = 60;
    // The note (or tie) runs straight into the next step's note: no gap, the
    // envelope goes on (with Env Trigger Gate), Portamento Auto glides.
    bool legato = false;

    bool operator==(const SeqStep &) const = default;
};

inline constexpr int kSeqMaxSteps = 64;
inline constexpr int kSeqPatterns = 8;
inline constexpr int kSeqTransposeCentre = 60; // a key transposes by its distance from middle C

struct SeqPattern
{
    std::array<SeqStep, kSeqMaxSteps> steps{};
    std::uint8_t length = 16; // 1..kSeqMaxSteps

    bool operator==(const SeqPattern &) const = default;
};

// The pattern generator (docs/BRIEF.md#step-sequencer): a new pattern of
// `length` steps from the notes in `pool` (spread over `octaves`), `density`
// 0..1 the chance a step plays; some notes tie on, some slide legato into the
// next. Deterministic for a given seed.
SeqPattern generatePattern(std::span<const int> pool, int octaves, double density, int length,
                           std::uint32_t seed) noexcept;

// The generator's notes when nothing is held and the pattern is empty: C minor pentatonic from C2.
inline constexpr std::array<int, 5> kDefaultScale{36, 39, 41, 43, 46};

} // namespace kobalt
