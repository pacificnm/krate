#pragma once

#include <array>
#include <cstdint>

namespace klang {

// Klang's patterns (docs/BRIEF.md#scope-v1): 16 pads × up to 64 steps of
// 1/16, and the trance gate's 16 steps. Trivially copyable: a pattern goes to
// the audio thread whole, through a lock-free queue.
struct Step
{
    bool on = false;
    bool accent = false;
    bool reverse = false;   // this hit plays backwards (on top of the pad's REVERSE)
    std::uint8_t repeat = 1; // hits in the step: 1..4 (rolls, stutters)
    std::int8_t pitch = 0;   // semitones, -12..+12

    bool operator==(const Step &) const = default;
};

inline constexpr int kPatternPads = 16;
inline constexpr int kMaxSteps = 64;
inline constexpr int kPatterns = 16;
inline constexpr int kMaxRepeat = 4;
inline constexpr std::uint16_t kDefaultGate = 0x7777; // on, on, on, off ...

struct Pattern
{
    std::array<std::array<Step, kMaxSteps>, kPatternPads> steps{}; // [pad][step]
    std::uint8_t length = 16;          // 1..kMaxSteps
    std::uint16_t gate = kDefaultGate; // bit n = gate step n open

    bool operator==(const Pattern &) const = default;
};

} // namespace klang
