#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace k303 {

// Order is the ParamStore index. Keys are stable: they are the ids written to
// midimap.json (see docs/DSP.md#parameters), so never rename one.
enum class ParamId : int {
    Tuning,
    Cutoff,
    Resonance,
    EnvMod,
    Decay,
    Accent,
    Waveform,
    Volume,
    Tempo,
    Scale,      // step length: 1/16, 1/32, 1/16T
    PlayMode,   // Forward, Reverse, Fwd+Rev, Invert, Random
    Count
};

inline constexpr int ParamCount = static_cast<int>(ParamId::Count);

// Everything a host, MIDI Learn, a generic panel or a session file needs to
// know about a parameter without knowing the instrument
// (docs/SHARED_FOUNDATIONS.MD#parameters).
struct ParamDescriptor {
    ParamId id;
    std::string_view key;     // stable: saved in panel.json / midimap.json; never rename
    std::string_view label;   // panel label
    float defaultValue;       // normalized 0..1
    int detents;              // 0 = continuous; N = selector or switch with N positions
    std::string (*format)(float normalized); // display text, e.g. "1.25 kHz"; not realtime-safe
};

namespace format {
std::string tuning(float v);
std::string cutoff(float v);
std::string percent(float v);
std::string envMod(float v);
std::string decay(float v);
std::string waveform(float v);
std::string volume(float v);
std::string tempo(float v);
std::string scale(float v);
std::string playMode(float v);
} // namespace format

inline constexpr std::array<ParamDescriptor, ParamCount> kParams{{
    {ParamId::Tuning,    "tuning",    "Tuning",       0.5f, 0, &format::tuning},
    {ParamId::Cutoff,    "cutoff",    "Cut Off Freq", 0.5f, 0, &format::cutoff},
    {ParamId::Resonance, "resonance", "Resonance",    0.5f, 0, &format::percent},
    {ParamId::EnvMod,    "envmod",    "Env Mod",      0.5f, 0, &format::envMod},
    {ParamId::Decay,     "decay",     "Decay",        0.5f, 0, &format::decay},
    {ParamId::Accent,    "accent",    "Accent",       0.5f, 0, &format::percent},
    {ParamId::Waveform,  "waveform",  "Waveform",     0.0f, 2, &format::waveform}, // 0 = saw
    {ParamId::Volume,    "volume",    "Volume",       0.8f, 0, &format::volume},
    {ParamId::Tempo,     "tempo",     "Tempo",        0.4f, 0, &format::tempo},    // 120 BPM in 40..240
    {ParamId::Scale,     "scale",     "Scale",        0.0f, 3, &format::scale},    // 1/16
    {ParamId::PlayMode,  "playmode",  "Play Mode",    0.0f, 5, &format::playMode}, // Forward
}};

constexpr const ParamDescriptor &paramDescriptor(ParamId id)
{
    return kParams[static_cast<std::size_t>(id)];
}

// Snap a normalized value to the nearest detent position (unchanged if continuous).
float snapToDetent(const ParamDescriptor &param, float normalized);

std::optional<ParamId> paramFromKey(std::string_view key);

// --- Normalized (0..1) -> engine units. Ranges are the starting values in
// docs/DSP.md#parameters; tune them there and here together.
namespace units {

inline constexpr double kCutoffMinHz = 60.0;
inline constexpr double kCutoffMaxHz = 5000.0;
inline constexpr double kDecayMinS = 0.2;
inline constexpr double kDecayMaxS = 2.0;
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;

inline double tuningSemitones(float v) { return (v - 0.5) * 24.0; }            // -12..+12
inline double cutoffHz(float v) { return kCutoffMinHz * std::pow(kCutoffMaxHz / kCutoffMinHz, v); }
inline double envModOctaves(float v) { return 4.0 * v; }                        // 0..4
inline double decaySeconds(float v) { return kDecayMinS * std::pow(kDecayMaxS / kDecayMinS, v); }
inline bool waveformIsSquare(float v) { return v >= 0.5f; }
inline double volumeGain(float v) { return double(v) * v * v; }                 // audio taper
inline double tempoBpm(float v) { return kTempoMinBpm + (kTempoMaxBpm - kTempoMinBpm) * v; }

// Selector position 0..detents-1 of a normalized value.
inline int detent(float v, int positions) { return int(std::lround(double(v) * (positions - 1))); }
// 1/16 = 4 steps per beat, 1/32 = 8, 1/16T (triplet sixteenths) = 6.
inline int scaleStepsPerBeat(float v)
{
    constexpr int steps[] = {4, 8, 6};
    return steps[detent(v, paramDescriptor(ParamId::Scale).detents)];
}
inline int playMode(float v) { return detent(v, paramDescriptor(ParamId::PlayMode).detents); } // as Sequencer::PlayMode

} // namespace units

} // namespace k303
