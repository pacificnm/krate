#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

#include "dsp/Head.h"

namespace krunch {
class CaptureModel;
}

// Fitting a head to an amp capture (docs/HEADS.md): the same dry guitar
// through the capture and through Krunch's tube amp built as a candidate
// head, both with every knob at noon and no cabinet, compared on what an amp
// is: its tone, how it distorts as you play harder, its envelope; a search
// moves the head's insides until they match.
namespace krunch::fit {

inline constexpr double kRate = 48000.0;
inline constexpr float kClipped = 0.98f; // a take this loud clipped at the interface (-0.2 dBFS)

// What both amps play: windows of the dry takes, from the softest playing to
// the hardest, and steady tones at three levels (for the harmonics). Each
// window starts with a lead-in that's played but not compared (the amps settle).
struct Window
{
    std::vector<float> audio; // the lead-in, then the part compared
    double toneHz = 0.0;      // a steady tone (its harmonics are compared), or 0
};
struct Material
{
    std::vector<Window> windows;
    int leadIn = 0; // frames
    double diPeak = 0.0;      // (of what's used: not counting clipped places)
    int clippedSpots = 0;     // places left out because the take clipped there
};
Material buildMaterial(const std::vector<std::vector<float>> &takes, int windows = 10, double seconds = 0.75,
                       double leadInSeconds = 0.3);

// Every knob at noon, the cabinet off: the capture's sound, or the head's.
std::vector<float> renderCapture(CaptureModel &capture, const Window &window, int leadIn);
std::vector<float> renderHead(const HeadDesign &head, const Window &window, int leadIn);

// What's compared, per window.
struct Features
{
    std::vector<float> longBands, shortBands; // dB per band per frame (85 ms and 2.7 ms frames)
    std::vector<float> envelope;              // dB, 10 ms
    std::vector<float> harmonics;             // the 2nd to 10th against the fundamental, dB (tones)
    double powerDb = -200.0;                  // the window's level
};
Features analyse(const std::vector<float> &out, double toneHz);

struct Score
{
    double total = 0.0;
    double tone = 0.0, attack = 0.0, envelope = 0.0, harmonics = 0.0; // average dB apart
    double levelDb = 0.0; // how much louder the capture is (what the head's output gains)
};
// The reference's features and the candidate's, window by window.
Score compare(const std::vector<Features> &reference, const std::vector<Features> &candidate);

// The head's insides the search moves, each mapped to 0..1. The number of
// stages, their tubes and the tone stack stay the starting head's.
class Space
{
public:
    explicit Space(const HeadDesign &start);
    int dims() const { return int(m_params.size()); }
    std::vector<double> encode(const HeadDesign &head) const;
    HeadDesign decode(const std::vector<double> &x) const;
    const std::string &name(int i) const { return m_params[std::size_t(i)].name; }

private:
    struct Param
    {
        std::string name;
        double lo, hi;
        bool log;
        std::function<double &(HeadDesign &)> ref;
    };
    HeadDesign m_start;
    std::vector<Param> m_params;
};

// Separable CMA-ES over [0,1]^n: minimises f, calling it for a whole
// generation at once (so it can run them in parallel). Returns the best found.
struct SearchResult
{
    std::vector<double> best;
    double bestValue = 1e300;
    int evaluations = 0;
};
SearchResult search(const std::function<std::vector<double>(const std::vector<std::vector<double>> &)> &evaluate,
                    std::vector<double> start, double sigma, int evaluations, std::mt19937 &rng,
                    const std::function<void(int, double)> &progress = {});

// Runs fn(i) for i in [0, n) on up to `threads` threads.
void parallelFor(int n, int threads, const std::function<void(int)> &fn);

} // namespace krunch::fit
