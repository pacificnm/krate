#include "kobalt/dsp/Sequence.h"

#include <algorithm>

namespace kobalt {

namespace {
class Random
{
public:
    explicit Random(std::uint32_t seed) noexcept : m_state(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() noexcept
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }
    double unit() noexcept { return double(next() >> 8) / double(1u << 24); } // 0..1

private:
    std::uint32_t m_state;
};

constexpr double kTieChance = 0.12;    // of playing steps that follow a note
constexpr double kLegatoChance = 0.25; // of notes that follow a note: the one before slides into it
} // namespace

SeqPattern generatePattern(std::span<const int> pool, int octaves, double density, int length,
                           std::uint32_t seed) noexcept
{
    SeqPattern pattern;
    pattern.length = std::uint8_t(std::clamp(length, 1, kSeqMaxSteps));

    // The pool, low to high without repeats, over the octaves.
    std::array<int, 16> base{};
    int baseCount = 0;
    for (int n : pool)
        if (baseCount < int(base.size()) && std::find(base.begin(), base.begin() + baseCount, n) == base.begin() + baseCount)
            base[std::size_t(baseCount++)] = n;
    if (baseCount == 0) {
        std::copy(kDefaultScale.begin(), kDefaultScale.end(), base.begin());
        baseCount = int(kDefaultScale.size());
    }
    std::sort(base.begin(), base.begin() + baseCount);
    std::array<int, 64> notes{};
    int count = 0;
    octaves = std::clamp(octaves, 1, 3);
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < baseCount && count < int(notes.size()); ++i)
            if (const int n = base[std::size_t(i)] + 12 * o; n <= 127)
                notes[std::size_t(count++)] = n;
    if (count == 0) // the pool was all above the top of the keyboard
        notes[std::size_t(count++)] = 127;

    Random random(seed);
    int index = -1;    // where the line is in the pool
    int previous = -1; // the step before, if it played (a note or a tie)
    for (int s = 0; s < pattern.length; ++s) {
        SeqStep &step = pattern.steps[std::size_t(s)];
        if (s > 0 && random.unit() >= density) {
            previous = -1;
            continue;
        }
        if (previous >= 0 && random.unit() < kTieChance) {
            step.kind = SeqStep::Kind::Tie;
            previous = s;
            continue;
        }
        // Mostly small moves through the pool, sometimes a leap.
        const double r = random.unit();
        if (index < 0 || r < 0.15) {
            index = int(random.next() % std::uint32_t(count));
        } else {
            const int size = r < 0.75 ? 1 : 2;
            const int move = (random.next() & 1) ? size : -size;
            index = std::clamp(index + move, 0, count - 1);
        }
        step.kind = SeqStep::Kind::Note;
        step.note = std::uint8_t(notes[std::size_t(index)]);
        if (previous >= 0 && random.unit() < kLegatoChance)
            pattern.steps[std::size_t(previous)].legato = true; // slides into this one
        previous = s;
    }
    return pattern;
}

} // namespace kobalt
