#pragma once

#include <array>
#include <cmath>
#include <optional>
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

struct ParamInfo {
    ParamId id;
    std::string_view key;
    std::string_view label;
    float defaultValue; // normalized 0..1
};

inline constexpr std::array<ParamInfo, ParamCount> kParams{{
    {ParamId::Tuning,    "tuning",    "Tuning",       0.5f},
    {ParamId::Cutoff,    "cutoff",    "Cut Off Freq", 0.5f},
    {ParamId::Resonance, "resonance", "Resonance",    0.5f},
    {ParamId::EnvMod,    "envmod",    "Env Mod",      0.5f},
    {ParamId::Decay,     "decay",     "Decay",        0.5f},
    {ParamId::Accent,    "accent",    "Accent",       0.5f},
    {ParamId::Waveform,  "waveform",  "Waveform",     0.0f}, // 0 = saw
    {ParamId::Volume,    "volume",    "Volume",       0.8f},
    {ParamId::Tempo,     "tempo",     "Tempo",        0.4f}, // 120 BPM in 40..240
    {ParamId::Scale,     "scale",     "Scale",        0.0f}, // 1/16
    {ParamId::PlayMode,  "playmode",  "Play Mode",    0.0f}, // Forward
}};

constexpr const ParamInfo &paramInfo(ParamId id)
{
    return kParams[static_cast<std::size_t>(id)];
}

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

// Selector positions: the normalized value snaps to evenly spaced detents.
inline constexpr int kScalePositions = 3;
inline constexpr int kPlayModePositions = 5;
inline int detent(float v, int positions) { return int(std::lround(double(v) * (positions - 1))); }
// 1/16 = 4 steps per beat, 1/32 = 8, 1/16T (triplet sixteenths) = 6.
inline int scaleStepsPerBeat(float v) { constexpr int steps[] = {4, 8, 6}; return steps[detent(v, kScalePositions)]; }
inline int playMode(float v) { return detent(v, kPlayModePositions); } // as Sequencer::PlayMode

} // namespace units

} // namespace k303
