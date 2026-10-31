#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "core/ParamDescriptor.h"

namespace korrode {

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
    Sweep,      // accent sweep depth
    Drive,      // overdrive after the VCA
    Tone,       // the overdrive's tone
    DelayLevel,
    DelayTime,  // 1/16 .. 1/2, tempo-synced
    DelayFeedback,
    ReverbLevel,
    ReverbSize,
    Width,      // of the effects (the dry bass stays centred)
    Density,    // RANDOMIZE: the share of steps that play (not a sound setting)
    Count
};

inline constexpr int ParamCount = static_cast<int>(ParamId::Count);

using krate::ParamDescriptor;
using krate::snapToDetent;

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
std::string delayTime(float v);
std::string delayFeedback(float v);
std::string reverbSize(float v);
} // namespace format

inline constexpr int kDelayTimes = 7; // 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2

// Indexed by ParamId: kParams[int(id)] describes id.
inline constexpr std::array<ParamDescriptor, ParamCount> kParams{{
    {"tuning",    "Tuning",       0.5f, 0, &format::tuning},
    {"cutoff",    "Cut Off Freq", 0.5f, 0, &format::cutoff},
    {"resonance", "Resonance",    0.5f, 0, &format::percent},
    {"envmod",    "Env Mod",      0.5f, 0, &format::envMod},
    {"decay",     "Decay",        0.5f, 0, &format::decay},
    {"accent",    "Accent",       0.5f, 0, &format::percent},
    {"waveform",  "Waveform",     0.0f, 2, &format::waveform}, // 0 = saw
    {"volume",    "Volume",       0.8f, 0, &format::volume},
    {"tempo",     "Tempo",        0.4f, 0, &format::tempo},    // 120 BPM in 40..240
    {"scale",     "Scale",        0.0f, 3, &format::scale},    // 1/16
    {"playmode",  "Play Mode",    0.0f, 5, &format::playMode}, // Forward
    {"sweep",     "Accent Sweep", 0.5f, 0, &format::percent},
    {"drive",     "Drive",        0.0f, 0, &format::percent},
    {"tone",      "Tone",         0.6f, 0, &format::percent},
    {"delay_level", "Delay Level", 0.0f, 0, &format::percent},
    {"delay_time", "Delay Time",  0.5f, kDelayTimes, &format::delayTime}, // 1/8.
    {"delay_fb",  "Delay Feedback", 0.35f, 0, &format::delayFeedback},
    {"reverb_level", "Reverb Level", 0.0f, 0, &format::percent},
    {"reverb_size", "Reverb Size", 0.4f, 0, &format::reverbSize},
    {"width",     "Width",        0.5f, 0, &format::percent},
    {"density",   "Density",      0.85f, 0, &format::percent},
}};

constexpr const ParamDescriptor &paramDescriptor(ParamId id)
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

// Selector position 0..detents-1 of a normalized value.
inline int detent(float v, int positions) { return int(std::lround(double(v) * (positions - 1))); }
// 1/16 = 4 steps per beat, 1/32 = 8, 1/16T (triplet sixteenths) = 6.
inline int scaleStepsPerBeat(float v)
{
    constexpr int steps[] = {4, 8, 6};
    return steps[detent(v, paramDescriptor(ParamId::Scale).detents)];
}
inline int playMode(float v) { return detent(v, paramDescriptor(ParamId::PlayMode).detents); } // as Sequencer::PlayMode

inline double sweepOctaves(float v) { return 4.0 * v; }       // accent sweep at full Accent: 0..4 (default 2)
inline double delayBeats(float v)
{
    constexpr double beats[kDelayTimes] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
    return beats[detent(v, kDelayTimes)];
}
inline double delayFeedback(float v) { return 0.9 * v; }      // 0..90 %

} // namespace units

} // namespace Korrode
