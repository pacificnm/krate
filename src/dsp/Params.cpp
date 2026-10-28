#include "dsp/Params.h"

#include <algorithm>
#include <format>

namespace k303 {

std::optional<ParamId> paramFromKey(std::string_view key)
{
    for (int i = 0; i < ParamCount; ++i) {
        if (kParams[std::size_t(i)].key == key)
            return ParamId(i);
    }
    return std::nullopt;
}

float snapToDetent(const ParamDescriptor &param, float normalized)
{
    if (param.detents < 2)
        return normalized;
    return float(units::detent(normalized, param.detents)) / float(param.detents - 1);
}

namespace format {

std::string tuning(float v)
{
    const double st = units::tuningSemitones(v);
    return std::format("{}{:.1f} st", st >= 0.05 ? "+" : "", st);
}

std::string cutoff(float v)
{
    const double hz = units::cutoffHz(v);
    return hz >= 1000.0 ? std::format("{:.2f} kHz", hz / 1000.0) : std::format("{} Hz", std::lround(hz));
}

std::string percent(float v)
{
    return std::format("{}%", std::lround(v * 100.0f)); // float, as the panel always showed
}

std::string envMod(float v)
{
    return std::format("{:.1f} oct", units::envModOctaves(v));
}

std::string decay(float v)
{
    const double s = units::decaySeconds(v);
    return s >= 1.0 ? std::format("{:.2f} s", s) : std::format("{} ms", std::lround(s * 1000.0));
}

std::string waveform(float v)
{
    return units::waveformIsSquare(v) ? "Square" : "Saw";
}

std::string volume(float v)
{
    const double gain = units::volumeGain(v);
    return gain <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(gain));
}

std::string tempo(float v)
{
    return std::format("{} BPM", std::lround(units::tempoBpm(v)));
}

std::string scale(float v)
{
    constexpr const char *names[] = {"1/16", "1/32", "1/16T"};
    return names[units::detent(v, paramDescriptor(ParamId::Scale).detents)];
}

std::string playMode(float v)
{
    constexpr const char *names[] = {"Forward", "Reverse", "Fwd+Rev", "Invert", "Random"};
    return names[units::detent(v, paramDescriptor(ParamId::PlayMode).detents)];
}

} // namespace format

} // namespace k303
