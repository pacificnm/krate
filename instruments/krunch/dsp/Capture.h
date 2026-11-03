#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nam {
class DSP;
}

namespace krunch {

// An amp capture (docs/CAPTURES.md): a capture model (a .nam
// file) of a real amp, played in place of Krunch's own preamp and power
// amp. load() parses and builds it and readies it for a rate and block size
// (it allocates: never on the audio thread); process() is for the audio thread.
class CaptureModel
{
public:
    ~CaptureModel();

    // nullptr and *error if the file isn't a model this engine plays.
    static std::unique_ptr<CaptureModel> load(const std::string &path, double sampleRate, int maxFrames, std::string *error);

    double sampleRate() const noexcept { return m_rate; }         // what it was readied for
    double expectedRate() const noexcept { return m_expected; }   // what it was trained at (0: any)
    // Its output brought to a common loudness (kLoudnessDb), when the file says how loud it is.
    static constexpr double kLoudnessDb = -8.0;
    float outputGain() const noexcept { return m_outputGain; }

    // Mono, in place, frames <= maxFrames.
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    CaptureModel() = default;
    std::unique_ptr<nam::DSP> m_dsp;
    double m_rate = 48000.0, m_expected = 0.0;
    float m_outputGain = 1.0f;
    std::vector<float> m_out;
};

} // namespace krunch
