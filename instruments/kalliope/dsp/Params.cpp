#include "kalliope/dsp/Params.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

namespace kalliope {

namespace units {
int index(float v, int count) { return int(std::lround(double(v) * (count - 1))); }
double gain(float level) { return double(level) * level; }
double lfoRateHz(float v) { return 0.05 * std::pow(30.0 / 0.05, double(v)); }
double lfoDelaySeconds(float v) { return 5.0 * double(v); }
double hpfHz(float v) { return v <= 0.0f ? 0.0 : 20.0 * std::pow(1000.0 / 20.0, double(v)); }
double cutoffHz(float v) { return 20.0 * std::pow(1000.0, double(v)); }
double envSeconds(float v) { return 0.001 * std::pow(10000.0, double(v)); }
double fineCents(float v) { return (double(v) - 0.5) * 100.0; }
int bendSemitones(float v) { return 1 + index(v, positions::kBendRange); }
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

std::string hz(double f)
{
    if (f >= 1000.0)
        return std::format("{:.2f} kHz", f / 1000.0);
    if (f >= 10.0)
        return std::format("{} Hz", std::lround(f));
    return std::format("{:.2f} Hz", f);
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
std::string lfoRate(float v) { return hz(units::lfoRateHz(v)); }
std::string lfoDelay(float v) { return seconds(units::lfoDelaySeconds(v)); }
std::string lfoWave(float v) { return choice<5>(v, {"Sine", "Up saw", "Down saw", "Square", "Random"}); }
std::string modDest(float v) { return choice<3>(v, {"VCO-1", "VCO-1 + 2", "VCO-2"}); }
std::string pwmSrc(float v) { return choice<3>(v, {"LFO", "Manual", "ENV-1"}); }
std::string vco1Range(float v) { return choice<4>(v, {"16'", "8'", "4'", "2'"}); }
std::string vco1Wave(float v) { return choice<4>(v, {"Triangle", "Saw", "Pulse", "Square"}); }
std::string vco2Range(float v) { return choice<5>(v, {"Low", "16'", "8'", "4'", "2'"}); }
std::string vco2Fine(float v) { return std::format("{:+} cents", std::lround(units::fineCents(v))); }
std::string vco2Wave(float v) { return choice<4>(v, {"Sine", "Saw", "Pulse", "Noise"}); }
std::string onOff(float v) { return choice<2>(v, {"Off", "On"}); }
std::string hpf(float v) { return v <= 0.0f ? std::string("Off") : hz(units::hpfHz(v)); }
std::string cutoff(float v) { return hz(units::cutoffHz(v)); }
std::string slope(float v) { return choice<2>(v, {"-12 dB", "-24 dB"}); }
std::string envSrc(float v) { return choice<2>(v, {"ENV-1", "ENV-2"}); }
std::string envTime(float v) { return seconds(units::envSeconds(v)); }
std::string keyMode(float v) { return choice<3>(v, {"Poly", "Unison", "Solo"}); }
std::string bendRange(float v) { return std::format("{} st", units::bendSemitones(v)); }
std::string playMode(float v) { return choice<3>(v, {"Note", "Arpeggio", "Sequencer"}); }
std::string arpMode(float v) { return choice<4>(v, {"Up", "Down", "Up & Down", "Random"}); }
std::string arpRange(float v) { return std::format("{} oct", 1 + units::index(v, positions::kArpRange)); }
std::string arpRate(float v) { return choice<4>(v, {"1/8", "1/16", "1/8T", "1/16T"}); }
std::string chorus(float v) { return choice<3>(v, {"Off", "I", "II"}); }
std::string delayTime(float v) { return choice<7>(v, {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"}); }
std::string delayFb(float v) { return std::format("{}%", std::lround(units::delayFeedback(v) * 100.0)); }
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }

} // namespace format

} // namespace kalliope
