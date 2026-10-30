#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace kobalt {

// Kobalt's parameters, in panel order (docs/BRIEF.md#parameters). The enum
// indexes kParams; keys are stable (panel.json, midimap.json, patches, rack sessions).
enum class ParamId : int {
    LfoRate, LfoWave,
    VcoMod, VcoRange, Pw, PwmSrc,
    PulseLevel, SawLevel, SubLevel, SubType, NoiseLevel,
    Cutoff, Res, VcfEnv, VcfMod, KeyFollow,
    VcaMode,
    EnvA, EnvD, EnvS, EnvR, EnvTrig,
    PortaTime, PortaMode,
    BendVco, BendVcf, LeverMod,
    Analog, Velocity,
    PlayMode, ArpMode, ArpRange, ArpRate, ArpHold,
    DelayLevel, DelayTime, DelayFb, ReverbLevel, ReverbSize,
    Octave,
    Volume,
    Tempo,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string percent(float v);
std::string level(float v);
std::string lfoRate(float v);
std::string lfoWave(float v);
std::string vcoMod(float v);
std::string vcoRange(float v);
std::string pulseWidth(float v);
std::string pwmSrc(float v);
std::string subType(float v);
std::string cutoff(float v);
std::string octaves8(float v);
std::string octaves4(float v);
std::string vcaMode(float v);
std::string envTime(float v);
std::string envTrig(float v);
std::string portaTime(float v);
std::string portaMode(float v);
std::string bendVco(float v);
std::string leverMod(float v);
std::string playMode(float v);
std::string arpMode(float v);
std::string arpRange(float v);
std::string arpRate(float v);
std::string onOff(float v);
std::string delayTime(float v);
std::string delayFb(float v);
std::string reverbSize(float v);
std::string octave(float v);
std::string tempo(float v);
} // namespace format

// Positions of the detented parameters (docs/BRIEF.md#parameters).
namespace positions {
inline constexpr int kLfoWave = 4;   // Triangle, Square, Random, Noise
inline constexpr int kVcoRange = 4;  // 16', 8', 4', 2'
inline constexpr int kPwmSrc = 3;    // LFO, Manual, Env
inline constexpr int kSubType = 3;   // 1 Oct, 2 Oct, 2 Oct Pulse
inline constexpr int kVcaMode = 2;   // Env, Gate
inline constexpr int kEnvTrig = 3;   // Gate + Trig, Gate, LFO
inline constexpr int kPortaMode = 3; // Off, Auto, On
inline constexpr int kBendVco = 13;  // 0..12 semitones
inline constexpr int kPlayMode = 3;  // Note, Arpeggio, Sequencer
inline constexpr int kArpMode = 3;   // Up, Up & Down, Down
inline constexpr int kArpRange = 3;  // 1..3 octaves
inline constexpr int kArpRate = 4;   // 1/8, 1/16, 1/8T, 1/16T
inline constexpr int kSwitch = 2;    // Off, On
inline constexpr int kDelayTime = 7; // 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2
inline constexpr int kOctave = 7;    // -3 .. +3 octaves on the keys
} // namespace positions

// The normalized value of position `index` of an n-position control.
constexpr float position(int index, int count) { return float(index) / float(count - 1); }

namespace p = positions;
inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"lfo_rate",    "LFO Rate",          0.5f,                         0,             &format::lfoRate},
    {"lfo_wave",    "LFO Waveform",      0.0f,                         p::kLfoWave,   &format::lfoWave},
    {"vco_mod",     "VCO Mod",           0.0f,                         0,             &format::vcoMod},
    {"vco_range",   "Range",             position(1, p::kVcoRange),    p::kVcoRange,  &format::vcoRange},
    {"pw",          "Pulse Width",       0.0f,                         0,             &format::pulseWidth},
    {"pwm_src",     "PWM Source",        position(1, p::kPwmSrc),      p::kPwmSrc,    &format::pwmSrc},
    {"pulse_level", "Pulse",             0.0f,                         0,             &format::percent},
    {"saw_level",   "Saw",               0.8f,                         0,             &format::percent},
    {"sub_level",   "Sub",               0.0f,                         0,             &format::percent},
    {"sub_type",    "Sub Type",          0.0f,                         p::kSubType,   &format::subType},
    {"noise_level", "Noise",             0.0f,                         0,             &format::percent},
    {"cutoff",      "Cutoff",            0.55f,                        0,             &format::cutoff},
    {"res",         "Resonance",         0.1f,                         0,             &format::percent},
    {"vcf_env",     "VCF Env",           0.35f,                        0,             &format::octaves8},
    {"vcf_mod",     "VCF Mod",           0.0f,                         0,             &format::octaves4},
    {"key_follow",  "Keyboard",          0.5f,                         0,             &format::percent},
    {"vca_mode",    "VCA Mode",          0.0f,                         p::kVcaMode,   &format::vcaMode},
    {"env_a",       "Attack",            0.0f,                         0,             &format::envTime},
    {"env_d",       "Decay",             0.45f,                        0,             &format::envTime},
    {"env_s",       "Sustain",           0.5f,                         0,             &format::percent},
    {"env_r",       "Release",           0.3f,                         0,             &format::envTime},
    {"env_trig",    "Env Trigger",       0.0f,                         p::kEnvTrig,   &format::envTrig},
    {"porta_time",  "Portamento",        0.3f,                         0,             &format::portaTime},
    {"porta_mode",  "Portamento Mode",   0.0f,                         p::kPortaMode, &format::portaMode},
    {"bend_vco",    "Bend VCO",          position(2, p::kBendVco),     p::kBendVco,   &format::bendVco},
    {"bend_vcf",    "Bend VCF",          0.0f,                         0,             &format::octaves4},
    {"lever_mod",   "Lever Mod",         0.5f,                         0,             &format::leverMod},
    {"analog",      "Analog",            0.3f,                         0,             &format::percent},
    {"velocity",    "Velocity",          0.25f,                        0,             &format::percent},
    {"play_mode",   "Play Mode",         0.0f,                         p::kPlayMode,  &format::playMode},
    {"arp_mode",    "Arp Mode",          0.0f,                         p::kArpMode,   &format::arpMode},
    {"arp_range",   "Arp Range",         0.0f,                         p::kArpRange,  &format::arpRange},
    {"arp_rate",    "Rate",              position(1, p::kArpRate),     p::kArpRate,   &format::arpRate},
    {"arp_hold",    "Hold",              0.0f,                         p::kSwitch,    &format::onOff},
    {"delay_level", "Delay Level",       0.0f,                         0,             &format::percent},
    {"delay_time",  "Delay Time",        position(2, p::kDelayTime),   p::kDelayTime, &format::delayTime},
    {"delay_fb",    "Delay Feedback",    0.3f,                         0,             &format::delayFb},
    {"reverb_level", "Reverb Level",     0.0f,                         0,             &format::percent},
    {"reverb_size", "Reverb Size",       0.4f,                         0,             &format::reverbSize},
    {"octave",      "Octave",            position(3, p::kOctave),      p::kOctave,    &format::octave},
    {"volume",      "Volume",            0.8f,                         0,             &format::level},
    {"tempo",       "Tempo",             0.4f,                         0,             &format::tempo}, // 120 BPM in 40..240
}};

inline constexpr const krate::ParamDescriptor &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

// True for the parameters a patch stores (all but Volume, Tempo, Hold and
// the keys' Octave).
constexpr bool inPatch(ParamId id)
{
    return id != ParamId::Volume && id != ParamId::Tempo && id != ParamId::ArpHold && id != ParamId::Octave;
}

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
int index(float v, int count);   // detent position 0..count-1
double gain(float level);        // level^2
double lfoRateHz(float v);       // 0.1..30 Hz, exponential
double vcoModOctaves(float v);   // 0..1 octave, squared
double cutoffHz(float v);        // 20..20000 Hz, exponential
double envSeconds(float v);      // 0.001..10 s, exponential
double portaSeconds(float v);    // 0 (v = 0), else 5 ms .. 2 s, exponential
int bendSemitones(float v);      // 0..12
double leverSemitones(float v);  // 0..2
int octaveShift(float v);        // -3..+3
double delayFeedback(float v);   // 0..0.9
double tempoBpm(float v);
double tempoValue(double bpm);
} // namespace units

} // namespace kobalt
