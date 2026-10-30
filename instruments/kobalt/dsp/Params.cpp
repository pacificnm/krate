#include "kobalt/dsp/Params.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

#include "dsp/Reverb.h"

namespace kobalt {

namespace units {
int index(float v, int count) { return int(std::lround(double(v) * (count - 1))); }
double gain(float level) { return double(level) * level; }
double lfoRateHz(float v) { return 0.1 * std::pow(300.0, double(v)); }
double vcoModOctaves(float v) { return double(v) * v; }
double cutoffHz(float v) { return 20.0 * std::pow(1000.0, double(v)); }
double envSeconds(float v) { return 0.001 * std::pow(10000.0, double(v)); }
double portaSeconds(float v) { return v <= 0.0f ? 0.0 : 0.005 * std::pow(400.0, double(v)); }
int bendSemitones(float v) { return index(v, positions::kBendVco); }
double leverSemitones(float v) { return 2.0 * double(v); }
int octaveShift(float v) { return index(v, positions::kOctave) - 3; }
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
std::string lfoWave(float v) { return choice<4>(v, {"Triangle", "Square", "Random", "Noise"}); }
std::string vcoMod(float v) { return std::format("{} cents", std::lround(1200.0 * units::vcoModOctaves(v))); }
std::string vcoRange(float v) { return choice<4>(v, {"16'", "8'", "4'", "2'"}); }
std::string pulseWidth(float v) { return std::format("{}%", std::lround(50.0 - 45.0 * double(v))); }
std::string pwmSrc(float v) { return choice<3>(v, {"LFO", "Manual", "Env"}); }
std::string subType(float v) { return choice<3>(v, {"1 Oct", "2 Oct", "2 Oct Pulse"}); }
std::string cutoff(float v) { return hz(units::cutoffHz(v)); }
std::string octaves8(float v) { return std::format("{:.1f} oct", 8.0 * double(v)); }
std::string octaves4(float v) { return std::format("{:.1f} oct", 4.0 * double(v)); }
std::string vcaMode(float v) { return choice<2>(v, {"Env", "Gate"}); }
std::string envTime(float v) { return seconds(units::envSeconds(v)); }
std::string envTrig(float v) { return choice<3>(v, {"Gate + Trig", "Gate", "LFO"}); }
std::string portaTime(float v) { return v <= 0.0f ? std::string("0 ms") : seconds(units::portaSeconds(v)); }
std::string portaMode(float v) { return choice<3>(v, {"Off", "Auto", "On"}); }
std::string bendVco(float v) { return std::format("{} st", units::bendSemitones(v)); }
std::string leverMod(float v) { return std::format("±{:.1f} st", units::leverSemitones(v)); }
std::string playMode(float v) { return choice<3>(v, {"Note", "Arpeggio", "Sequencer"}); }
std::string arpMode(float v) { return choice<3>(v, {"Up", "Up & Down", "Down"}); }
std::string arpRange(float v) { return std::format("{} oct", 1 + units::index(v, positions::kArpRange)); }
std::string arpRate(float v) { return choice<4>(v, {"1/8", "1/16", "1/8T", "1/16T"}); }
std::string onOff(float v) { return choice<2>(v, {"Off", "On"}); }
std::string delayTime(float v) { return choice<7>(v, {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"}); }
std::string delayFb(float v) { return std::format("{}%", std::lround(units::delayFeedback(v) * 100.0)); }
std::string reverbSize(float v) { return std::format("{:.1f} s", krate::Reverb::decaySeconds(double(v))); }
std::string octave(float v)
{
    const int shift = units::octaveShift(v);
    return shift == 0 ? std::string("0") : std::format("{:+}", shift);
}
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }

} // namespace format

} // namespace kobalt
