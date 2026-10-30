#include "klang/dsp/Params.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

#include "dsp/Reverb.h"

namespace klang {

namespace units {
int index(float v, int count) { return int(std::lround(double(v) * (count - 1))); }
double gain(float level) { return double(level) * level; }
int tuneSemitones(float v) { return index(v, positions::kTune) - 24; }
double fineCents(float v) { return (double(v) - 0.5) * 100.0; }
double attackSeconds(float v) { return 2.0 * double(v) * v; }
double decaySeconds(float v) { return v >= 0.999f ? -1.0 : 0.01 * std::pow(1000.0, double(v)); }
int syncBeats(float v)
{
    static constexpr int kBeats[] = {0, 1, 2, 4, 8, 16};
    return kBeats[index(v, positions::kSync)];
}
int chokeGroup(float v) { return index(v, positions::kChoke); }
double cutoffHz(float v) { return 20.0 * std::pow(1000.0, double(v)); }
int bits(float v)
{
    const int i = index(v, positions::kBits);
    return i == 0 ? 0 : 17 - i; // 16 .. 2
}
int downsample(float v) { return 1 << index(v, positions::kDownsample); }
double ringHz(float v) { return 20.0 * std::pow(200.0, double(v)); }
double swingFraction(float v) { return 0.5 + 0.25 * double(v); }
double gateSmoothSeconds(float v) { return 0.001 + 0.029 * double(v); }
double delayFeedback(float v) { return 0.9 * double(v); }
double tempoBpm(float v) { return kTempoMinBpm + (kTempoMaxBpm - kTempoMinBpm) * double(v); }
double tempoValue(double bpm) { return (bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm); }
} // namespace units

namespace format {

namespace {
template<std::size_t N>
std::string choice(float v, const std::array<std::string_view, N> &names)
{
    return std::string(names[std::size_t(units::index(v, int(N)))]);
}

std::string frequency(double f)
{
    if (f >= 1000.0)
        return std::format("{:.2f} kHz", f / 1000.0);
    return std::format("{} Hz", std::lround(f));
}

std::string seconds(double s)
{
    return s < 1.0 ? std::format("{} ms", std::lround(s * 1000.0)) : std::format("{:.2f} s", s);
}
} // namespace

std::string percent(float v) { return std::format("{}%", std::lround(v * 100.0f)); }
std::string level(float v)
{
    const double g = units::gain(v);
    return g <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(g));
}
std::string pan(float v)
{
    const long p = std::lround((double(v) - 0.5) * 200.0);
    return p == 0 ? std::string("C") : p < 0 ? std::format("L{}", -p) : std::format("R{}", p);
}
std::string tune(float v) { return std::format("{:+} st", units::tuneSemitones(v)); }
std::string fine(float v) { return std::format("{:+} cents", std::lround(units::fineCents(v))); }
std::string attack(float v) { return seconds(units::attackSeconds(v)); }
std::string decay(float v)
{
    const double s = units::decaySeconds(v);
    return s < 0.0 ? std::string("Full") : seconds(s);
}
std::string mode(float v) { return choice<3>(v, {"One shot", "Gate", "Loop"}); }
std::string sync(float v)
{
    const int beats = units::syncBeats(v);
    return beats == 0 ? std::string("Off") : beats == 1 ? std::string("1 beat") : std::format("{} beats", beats);
}
std::string onOff(float v) { return choice<2>(v, {"Off", "On"}); }
std::string choke(float v)
{
    const int g = units::chokeGroup(v);
    return g == 0 ? std::string("None") : std::format("Group {}", g);
}
std::string filter(float v) { return choice<4>(v, {"Off", "Low-pass", "High-pass", "Band-pass"}); }
std::string hz(float v) { return frequency(units::cutoffHz(v)); }
std::string octaves6(float v) { return std::format("{:.1f} oct", 6.0 * double(v)); }
std::string bits(float v)
{
    const int b = units::bits(v);
    return b == 0 ? std::string("Off") : std::format("{} bits", b);
}
std::string downsample(float v)
{
    const int d = units::downsample(v);
    return d == 1 ? std::string("Full") : std::format("1/{}", d);
}
std::string ringFreq(float v) { return frequency(units::ringHz(v)); }
std::string swing(float v) { return std::format("{}%", std::lround(units::swingFraction(v) * 100.0)); }
std::string keyMode(float v) { return choice<2>(v, {"Pads", "Chromatic"}); }
std::string gateRate(float v) { return choice<3>(v, {"1/8", "1/16", "1/32"}); }
std::string gateSmooth(float v) { return seconds(units::gateSmoothSeconds(v)); }
std::string delayTime(float v) { return choice<7>(v, {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"}); }
std::string delayFb(float v) { return std::format("{}%", std::lround(units::delayFeedback(v) * 100.0)); }
std::string reverbSize(float v) { return std::format("{:.1f} s", krate::Reverb::decaySeconds(double(v))); }
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }

} // namespace format

} // namespace klang
