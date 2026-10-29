#include "kalliope/dsp/Sequence.h"

#include <algorithm>

namespace kalliope {

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

constexpr double kTieChance = 0.125; // of playing steps that follow a note
} // namespace

SeqPattern generatePattern(std::span<const int> pool, int octaves, double density, int length,
                           std::uint32_t seed) noexcept
{
    SeqPattern pattern;
    pattern.length = std::uint8_t(std::clamp(length, 1, 16));

    // The pool, low to high without repeats, over the octaves.
    std::array<int, 64> notes{};
    int count = 0;
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
    octaves = std::clamp(octaves, 1, 4);
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < baseCount && count < int(notes.size()); ++i)
            if (const int n = base[std::size_t(i)] + 12 * o; n <= 127)
                notes[std::size_t(count++)] = n;

    Random random(seed);
    int index = -1;       // where the melody is in the pool
    bool previous = false; // the previous step played (a tie may follow)
    for (int s = 0; s < pattern.length; ++s) {
        SeqStep &step = pattern.steps[std::size_t(s)];
        const bool play = s == 0 || random.unit() < density;
        if (!play) {
            previous = false;
            continue;
        }
        if (previous && random.unit() < kTieChance) {
            step.tie = true;
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
        step.notes[0] = std::uint8_t(notes[std::size_t(index)]);
        step.count = 1;
        previous = true;
    }
    return pattern;
}

} // namespace kalliope
