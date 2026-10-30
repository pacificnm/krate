#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace klang {

// Klang's parameters (docs/BRIEF.md#parameters-one-pad-at-a-time). The pad
// parameters (up to PadParamCount) exist once per pad; the table shows the
// edited pad's. Keys are stable (panel.json, pads.json, midimap.json, rack sessions).
enum class ParamId : int {
    // Per pad
    PadLevel, PadPan, Tune, Fine, Start, End, Attack, Decay, Mode, Sync, Reverse, Choke,
    Filter, Cutoff, Res, FilterEnv, Drive, Bits, Downsample, Ring, RingFreq, Gate, DelaySend, ReverbSend,
    // Klang's own
    Swing, KeyMode, GateRate, GateDepth, GateSmooth,
    DelayLevel, DelayTime, DelayFb, ReverbLevel, ReverbSize,
    Volume, Tempo,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);
inline constexpr int PadParamCount = int(ParamId::Swing); // per pad: everything before Swing
constexpr bool isPadParam(int index) { return index >= 0 && index < PadParamCount; }

namespace format {
std::string percent(float v);
std::string level(float v);
std::string pan(float v);
std::string tune(float v);
std::string fine(float v);
std::string attack(float v);
std::string decay(float v);
std::string mode(float v);
std::string sync(float v);
std::string onOff(float v);
std::string choke(float v);
std::string filter(float v);
std::string hz(float v);
std::string octaves6(float v);
std::string bits(float v);
std::string downsample(float v);
std::string ringFreq(float v);
std::string swing(float v);
std::string keyMode(float v);
std::string gateRate(float v);
std::string gateSmooth(float v);
std::string delayTime(float v);
std::string delayFb(float v);
std::string reverbSize(float v);
std::string tempo(float v);
} // namespace format

// Positions of the detented parameters.
namespace positions {
inline constexpr int kTune = 49;      // -24 .. +24 semitones
inline constexpr int kMode = 3;       // One shot, Gate, Loop
inline constexpr int kSync = 6;       // Off, 1, 2, 4, 8, 16 beats
inline constexpr int kSwitch = 2;     // Off, On
inline constexpr int kChoke = 5;      // None, 1..4
inline constexpr int kFilter = 4;     // Off, LP, HP, BP
inline constexpr int kBits = 16;      // Off, 16 .. 2
inline constexpr int kDownsample = 6; // 1, 1/2, 1/4, 1/8, 1/16, 1/32
inline constexpr int kKeyMode = 2;    // Pads, Chromatic
inline constexpr int kGateRate = 3;   // 1/8, 1/16, 1/32
inline constexpr int kDelayTime = 7;  // 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2
} // namespace positions

constexpr float position(int index, int count) { return float(index) / float(count - 1); }

namespace p = positions;
inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"pad_level",   "Level",         0.8f,                       0,              &format::level},
    {"pad_pan",     "Pan",           0.5f,                       0,              &format::pan},
    {"tune",        "Tune",          position(24, p::kTune),     p::kTune,       &format::tune},
    {"fine",        "Fine",          0.5f,                       0,              &format::fine},
    {"start",       "Start",         0.0f,                       0,              &format::percent},
    {"end",         "End",           1.0f,                       0,              &format::percent},
    {"attack",      "Attack",        0.0f,                       0,              &format::attack},
    {"decay",       "Decay",         1.0f,                       0,              &format::decay},
    {"mode",        "Mode",          0.0f,                       p::kMode,       &format::mode},
    {"sync",        "Sync",          0.0f,                       p::kSync,       &format::sync},
    {"reverse",     "Reverse",       0.0f,                       p::kSwitch,     &format::onOff},
    {"choke",       "Choke",         0.0f,                       p::kChoke,      &format::choke},
    {"filter",      "Filter",        0.0f,                       p::kFilter,     &format::filter},
    {"cutoff",      "Cutoff",        0.7f,                       0,              &format::hz},
    {"res",         "Resonance",     0.1f,                       0,              &format::percent},
    {"filter_env",  "Filter Env",    0.0f,                       0,              &format::octaves6},
    {"drive",       "Drive",         0.0f,                       0,              &format::percent},
    {"bits",        "Bits",          0.0f,                       p::kBits,       &format::bits},
    {"downsample",  "Rate",          0.0f,                       p::kDownsample, &format::downsample},
    {"ring",        "Ring",          0.0f,                       0,              &format::percent},
    {"ring_freq",   "Ring Freq",     0.5f,                       0,              &format::ringFreq},
    {"gate",        "Gate",          0.0f,                       p::kSwitch,     &format::onOff},
    {"delay_send",  "Delay Send",    0.0f,                       0,              &format::percent},
    {"reverb_send", "Reverb Send",   0.0f,                       0,              &format::percent},
    {"swing",       "Swing",         0.0f,                       0,              &format::swing},
    {"key_mode",    "Keys",          0.0f,                       p::kKeyMode,    &format::keyMode},
    {"gate_rate",   "Gate Rate",     position(1, p::kGateRate),  p::kGateRate,   &format::gateRate},
    {"gate_depth",  "Gate Depth",    1.0f,                       0,              &format::percent},
    {"gate_smooth", "Gate Smooth",   0.2f,                       0,              &format::gateSmooth},
    {"delay_level", "Delay",         0.5f,                       0,              &format::percent},
    {"delay_time",  "Delay Time",    position(3, p::kDelayTime), p::kDelayTime,  &format::delayTime},
    {"delay_fb",    "Delay Feedback", 0.4f,                      0,              &format::delayFb},
    {"reverb_level", "Reverb",       0.5f,                       0,              &format::percent},
    {"reverb_size", "Reverb Size",   0.5f,                       0,              &format::reverbSize},
    {"volume",      "Volume",        0.8f,                       0,              &format::level},
    {"tempo",       "Tempo",         0.4f,                       0,              &format::tempo}, // 120 BPM
}};

inline constexpr const krate::ParamDescriptor &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
int index(float v, int count);
double gain(float level);          // level^2
int tuneSemitones(float v);        // -24..+24
double fineCents(float v);         // -50..+50
double attackSeconds(float v);     // 0..2, squared
double decaySeconds(float v);      // 0.01..10 s, exponential; < 0 = Full (at the top)
int syncBeats(float v);            // 0 = off, else 1, 2, 4, 8, 16
int chokeGroup(float v);           // 0 = none, 1..4
double cutoffHz(float v);          // 20..20000, exponential
int bits(float v);                 // 0 = off, else 16..2
int downsample(float v);           // 1, 2, 4, 8, 16, 32
double ringHz(float v);            // 20..4000, exponential
double swingFraction(float v);     // 0.5..0.75
double gateSmoothSeconds(float v); // 0.001..0.03
double delayFeedback(float v);     // 0..0.9
double tempoBpm(float v);
double tempoValue(double bpm);
} // namespace units

} // namespace klang
