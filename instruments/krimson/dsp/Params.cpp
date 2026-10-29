#include "krimson/dsp/Params.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

namespace krimson {

namespace units {
int index(float v, int count) { return int(std::lround(double(v) * (count - 1))); }
double gain(float level) { return double(level) * level; }
double envSeconds(float v) { return 0.0005 * std::pow(20000.0, double(v)); }
double lfoRateHz(float v) { return 0.05 * std::pow(800.0, double(v)); }
double cutoffHz(float v) { return 20.0 * std::pow(1000.0, double(v)); }
double glideSeconds(float v) { return 2.0 * double(v) * double(v); }
int semitones(float v) { return index(v, positions::kOsc2Semi) - 24; }
double fineCents(float v) { return (double(v) - 0.5) * 100.0; }
double bipolar(float v) { return 2.0 * double(v) - 1.0; }
double pan(float v) { return 2.0 * double(v) - 1.0; }
int bendSemitones(float v) { return 1 + index(v, positions::kBendRange); }
int octaveShift(float v) { return index(v, positions::kOctave) - 3; }
double tempoBpm(float v) { return kTempoMinBpm + (kTempoMaxBpm - kTempoMinBpm) * double(v); }
double tempoValue(double bpm) { return (bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm); }
} // namespace units

namespace format {

namespace {
std::string hz(double f)
{
    if (f >= 1000.0)
        return std::format("{:.2f} kHz", f / 1000.0);
    if (f >= 10.0)
        return std::format("{} Hz", std::lround(f));
    return std::format("{:.2f} Hz", f);
}
std::string time(double s)
{
    return s < 1.0 ? std::format("{:.1f} ms", s * 1000.0) : std::format("{:.2f} s", s);
}
} // namespace

std::string percent(float v) { return std::format("{}%", std::lround(v * 100.0f)); }
std::string bipolar(float v) { return std::format("{:+}%", std::lround(units::bipolar(v) * 100.0)); }
std::string level(float v)
{
    const double g = units::gain(v);
    return g <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(g));
}
std::string choice(float v, int which)
{
    std::array<std::string_view, 8> names{};
    int count = 0;
    const auto set = [&](std::initializer_list<std::string_view> list) {
        for (auto n : list)
            names[std::size_t(count++)] = n;
    };
    switch (ParamId(which)) {
    case ParamId::Osc1Wave: set({"Sine", "Triangle", "Saw", "Pulse"}); break;
    case ParamId::Osc2Wave: set({"Triangle", "Saw", "Pulse", "Noise"}); break;
    case ParamId::MenvDest: set({"OSC 2 pitch", "FM", "Pulse width"}); break;
    case ParamId::Lfo1Wave: set({"Triangle", "Saw", "Square", "S&H", "Soft random"}); break;
    case ParamId::Lfo1Dest: set({"OSC 1+2", "OSC 2", "Filter", "Pulse width", "FM"}); break;
    case ParamId::Lfo2Dest: set({"Amp", "Filter", "OSC 2"}); break;
    case ParamId::FilterType: set({"LP 12", "LP 24", "Band-pass", "High-pass", "Notch + LP"}); break;
    case ParamId::KbdTrack: set({"0", "1/3", "2/3", "1"}); break;
    case ParamId::VoiceMode: set({"Poly", "Mono", "Legato", "Unison"}); break;
    case ParamId::WheelDest: set({"Morph", "Vibrato"}); break;
    case ParamId::ArpMode: set({"Up", "Down", "Up & Down", "Random"}); break;
    case ParamId::ArpRate: set({"1/8", "1/16", "1/8T", "1/16T"}); break;
    case ParamId::Chorus: set({"Off", "Chorus", "Ensemble"}); break;
    case ParamId::DelayTime: set({"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"}); break;
    default: return percent(v);
    }
    return std::string(names[std::size_t(units::index(v, count))]);
}
std::string semitones(float v) { return std::format("{:+}", units::semitones(v)); }
std::string cents(float v) { return std::format("{:+} cents", std::lround(units::fineCents(v))); }
std::string seconds(float v) { return time(units::envSeconds(v)); }
std::string lfoRate(float v) { return hz(units::lfoRateHz(v)); }
std::string cutoff(float v) { return hz(units::cutoffHz(v)); }
std::string glide(float v) { return v <= 0.0f ? std::string("Off") : time(units::glideSeconds(v)); }
std::string pan(float v)
{
    const auto p = std::lround(units::pan(v) * 100.0);
    return p == 0 ? std::string("C") : std::format("{}{}", p < 0 ? "L" : "R", std::abs(p));
}
std::string bendRange(float v) { return std::format("{} st", units::bendSemitones(v)); }
std::string arpRange(float v) { return std::format("{} oct", 1 + units::index(v, positions::kArpRange)); }
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }
std::string octave(float v)
{
    const int s = units::octaveShift(v);
    return s == 0 ? std::string("0") : std::format("{:+}", s);
}
std::string onOff(float v) { return v > 0.5f ? std::string("On") : std::string("Off"); }

} // namespace format

} // namespace krimson
