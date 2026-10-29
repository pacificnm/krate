#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace kalliope {

// Kalliope's parameters, in panel order (docs/BRIEF.md#parameters). The enum
// indexes kParams; keys are stable (panel.json, midimap.json, patches, rack sessions).
enum class ParamId : int {
    LfoRate, LfoDelay, LfoWave,
    ModLfo, ModEnv, ModDest, Pwm, PwmSrc,
    CrossMod, Vco1Range, Vco1Wave,
    Vco2Range, Vco2Fine, Vco2Wave, Sync,
    Mix,
    Hpf,
    Cutoff, Res, Slope, VcfEnv, VcfEnvSrc, VcfLfo, KeyFollow,
    VcaLevel, VcaLfo,
    Env1A, Env1D, Env1S, Env1R,
    Env2A, Env2D, Env2S, Env2R,
    EnvKey,
    KeyMode, Velocity, BendRange,
    PlayMode, ArpMode, ArpRange, ArpRate, ArpHold, GenDensity,
    Chorus, DelayLevel, DelayTime, DelayFb,
    Volume,
    Tempo,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string percent(float v);
std::string level(float v);
std::string lfoRate(float v);
std::string lfoDelay(float v);
std::string lfoWave(float v);
std::string modDest(float v);
std::string pwmSrc(float v);
std::string vco1Range(float v);
std::string vco1Wave(float v);
std::string vco2Range(float v);
std::string vco2Fine(float v);
std::string vco2Wave(float v);
std::string onOff(float v);
std::string hpf(float v);
std::string cutoff(float v);
std::string slope(float v);
std::string envSrc(float v);
std::string envTime(float v);
std::string keyMode(float v);
std::string bendRange(float v);
std::string playMode(float v);
std::string arpMode(float v);
std::string arpRange(float v);
std::string arpRate(float v);
std::string chorus(float v);
std::string delayTime(float v);
std::string delayFb(float v);
std::string tempo(float v);
} // namespace format

// Positions of the detented parameters (docs/BRIEF.md#parameters).
namespace positions {
inline constexpr int kLfoWave = 5;   // Sine, Up saw, Down saw, Square, Random
inline constexpr int kModDest = 3;   // VCO-1, VCO-1 + 2, VCO-2
inline constexpr int kPwmSrc = 3;    // LFO, Manual, ENV-1
inline constexpr int kVco1Range = 4; // 16', 8', 4', 2'
inline constexpr int kVco1Wave = 4;  // Triangle, Saw, Pulse, Square
inline constexpr int kVco2Range = 5; // Low, 16', 8', 4', 2'
inline constexpr int kVco2Wave = 4;  // Sine, Saw, Pulse, Noise
inline constexpr int kSwitch = 2;    // Off, On (and other two-way switches)
inline constexpr int kKeyMode = 3;   // Poly, Unison, Solo
inline constexpr int kBendRange = 12;
inline constexpr int kPlayMode = 3;  // Note, Arpeggio, Sequencer
inline constexpr int kArpMode = 4;   // Up, Down, Up & Down, Random
inline constexpr int kArpRange = 4;  // 1..4 octaves
inline constexpr int kArpRate = 4;   // 1/8, 1/16, 1/8T, 1/16T
inline constexpr int kChorus = 3;    // Off, I, II
inline constexpr int kDelayTime = 7; // 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2
} // namespace positions

// The normalized value of position `index` of an n-position control.
constexpr float position(int index, int count) { return float(index) / float(count - 1); }

namespace p = positions;
inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"lfo_rate",    "LFO Rate",          0.5f,                         0,             &format::lfoRate},
    {"lfo_delay",   "LFO Delay",         0.0f,                         0,             &format::lfoDelay},
    {"lfo_wave",    "LFO Waveform",      0.0f,                         p::kLfoWave,   &format::lfoWave},
    {"mod_lfo",     "VCO LFO Mod",       0.0f,                         0,             &format::percent},
    {"mod_env",     "VCO Env Mod",       0.0f,                         0,             &format::percent},
    {"mod_dest",    "Mod Destination",   position(1, p::kModDest),     p::kModDest,   &format::modDest},
    {"pwm",         "Pulse Width Mod",   0.0f,                         0,             &format::percent},
    {"pwm_src",     "PWM Source",        position(1, p::kPwmSrc),      p::kPwmSrc,    &format::pwmSrc},
    {"cross_mod",   "Cross Mod",         0.0f,                         0,             &format::percent},
    {"vco1_range",  "VCO-1 Range",       position(1, p::kVco1Range),   p::kVco1Range, &format::vco1Range},
    {"vco1_wave",   "VCO-1 Waveform",    position(1, p::kVco1Wave),    p::kVco1Wave,  &format::vco1Wave},
    {"vco2_range",  "VCO-2 Range",       position(2, p::kVco2Range),   p::kVco2Range, &format::vco2Range},
    {"vco2_fine",   "VCO-2 Fine Tune",   0.5f,                         0,             &format::vco2Fine},
    {"vco2_wave",   "VCO-2 Waveform",    position(1, p::kVco2Wave),    p::kVco2Wave,  &format::vco2Wave},
    {"sync",        "Sync",              0.0f,                         p::kSwitch,    &format::onOff},
    {"mix",         "Source Mix",        0.5f,                         0,             &format::percent},
    {"hpf",         "HPF Cutoff",        0.0f,                         0,             &format::hpf},
    {"cutoff",      "VCF Cutoff",        0.6f,                         0,             &format::cutoff},
    {"res",         "Resonance",         0.1f,                         0,             &format::percent},
    {"slope",       "Slope",             1.0f,                         p::kSwitch,    &format::slope},
    {"vcf_env",     "VCF Env Mod",       0.3f,                         0,             &format::percent},
    {"vcf_env_src", "VCF Env Source",    0.0f,                         p::kSwitch,    &format::envSrc},
    {"vcf_lfo",     "VCF LFO Mod",       0.0f,                         0,             &format::percent},
    {"key_follow",  "Key Follow",        0.5f,                         0,             &format::percent},
    {"vca_level",   "VCA Level",         0.8f,                         0,             &format::level},
    {"vca_lfo",     "VCA LFO Mod",       0.0f,                         0,             &format::percent},
    {"env1_a",      "ENV-1 Attack",      0.0f,                         0,             &format::envTime},
    {"env1_d",      "ENV-1 Decay",       0.45f,                        0,             &format::envTime},
    {"env1_s",      "ENV-1 Sustain",     0.3f,                         0,             &format::percent},
    {"env1_r",      "ENV-1 Release",     0.35f,                        0,             &format::envTime},
    {"env2_a",      "ENV-2 Attack",      0.0f,                         0,             &format::envTime},
    {"env2_d",      "ENV-2 Decay",       0.35f,                        0,             &format::envTime},
    {"env2_s",      "ENV-2 Sustain",     1.0f,                         0,             &format::percent},
    {"env2_r",      "ENV-2 Release",     0.35f,                        0,             &format::envTime},
    {"env_key",     "Env Key Follow",    0.0f,                         p::kSwitch,    &format::onOff},
    {"key_mode",    "Key Mode",          0.0f,                         p::kKeyMode,   &format::keyMode},
    {"velocity",    "Velocity",          0.5f,                         0,             &format::percent},
    {"bend_range",  "Bend Range",        position(1, p::kBendRange),   p::kBendRange, &format::bendRange},
    {"play_mode",   "Play Mode",         0.0f,                         p::kPlayMode,  &format::playMode},
    {"arp_mode",    "Arp Mode",          0.0f,                         p::kArpMode,   &format::arpMode},
    {"arp_range",   "Arp Range",         0.0f,                         p::kArpRange,  &format::arpRange},
    {"arp_rate",    "Rate",              position(1, p::kArpRate),     p::kArpRate,   &format::arpRate},
    {"arp_hold",    "Hold",              0.0f,                         p::kSwitch,    &format::onOff},
    {"gen_density", "Density",           0.6f,                         0,             &format::percent},
    {"chorus",      "Chorus",            position(1, p::kChorus),      p::kChorus,    &format::chorus},
    {"delay_level", "Delay Level",       0.0f,                         0,             &format::percent},
    {"delay_time",  "Delay Time",        position(2, p::kDelayTime),   p::kDelayTime, &format::delayTime},
    {"delay_fb",    "Delay Feedback",    0.3f,                         0,             &format::delayFb},
    {"volume",      "Volume",            0.8f,                         0,             &format::level},
    {"tempo",       "Tempo",             0.4f,                         0,             &format::tempo}, // 120 BPM in 40..240
}};

inline constexpr const krate::ParamDescriptor &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

// True for the parameters a patch stores (all but Volume, Tempo, Hold and the
// generator's Density; docs/PLAY.md#patches).
constexpr bool inPatch(ParamId id)
{
    return id != ParamId::Volume && id != ParamId::Tempo && id != ParamId::ArpHold && id != ParamId::GenDensity;
}

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
int index(float v, int count);   // detent position 0..count-1
double gain(float level);        // level^2
double lfoRateHz(float v);       // 0.05..30 Hz, exponential
double lfoDelaySeconds(float v); // 0..5 s
double hpfHz(float v);           // 0 = off, else 20..1000 Hz, exponential
double cutoffHz(float v);        // 20..20000 Hz, exponential
double envSeconds(float v);      // 0.001..10 s, exponential
double fineCents(float v);       // -50..+50
int bendSemitones(float v);      // 1..12
double delayFeedback(float v);   // 0..0.9
double tempoBpm(float v);
double tempoValue(double bpm);
} // namespace units

} // namespace kalliope
