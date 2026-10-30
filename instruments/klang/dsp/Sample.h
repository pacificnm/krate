#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace klang {

// A sample a pad plays (docs/BRIEF.md#sound-engine): one or two channels of
// float at its own rate. Made and freed off the audio thread; the engine only
// reads it (Engine::setSample).
struct Sample
{
    std::vector<float> left;
    std::vector<float> right; // empty: mono
    double rate = 48000.0;
    std::string name;

    static constexpr double kMaxSeconds = 120.0; // longer files are cut here

    std::size_t frames() const noexcept { return left.size(); }
    bool stereo() const noexcept { return !right.empty(); }

    // From one or two channels (cut to kMaxSeconds; right dropped if it isn't the left's length).
    static Sample fromChannels(std::vector<float> left, std::vector<float> right, double rate, std::string name)
    {
        Sample s;
        const auto max = std::size_t(kMaxSeconds * std::max(rate, 1.0));
        if (left.size() > max)
            left.resize(max);
        if (right.size() != left.size())
            right.clear();
        s.left = std::move(left);
        s.right = std::move(right);
        s.rate = rate > 0.0 ? rate : 48000.0;
        s.name = std::move(name);
        return s;
    }
};

} // namespace klang
