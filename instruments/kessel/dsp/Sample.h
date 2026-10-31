#pragma once

#include <string>
#include <vector>

namespace kessel {

// A sample for the SAMPLE voice (docs/DSP.md#sample): mono, at its own rate,
// peak-normalized. Made and freed off the audio thread; the engine only reads
// it (Engine::setSample).
struct Sample
{
    std::vector<float> data;
    double rate = 48000.0;
    std::string name;

    static constexpr double kMaxSeconds = 30.0; // longer files are cut here
    static constexpr float kPeak = 0.9f;        // normalized to this peak

    // From one or two channels at `rate` (the channels averaged, cut to
    // kMaxSeconds, normalized). Empty data if the input is silent or empty.
    static Sample fromChannels(const std::vector<float> &left, const std::vector<float> &right, double rate,
                               std::string name);
};

} // namespace Kessel
