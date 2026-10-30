#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace krimson {

// Krimson's parameters (docs/BRIEF.md#parameters-one-slot-at-a-time). The
// sound parameters (up to SlotParamCount) exist once per slot; the table shows
// the slot being edited. The last three are the instrument's own (Volume,
// Tempo, Octave). Keys are stable: panel, MIDI map, programs, rack sessions.
enum class ParamId : int {
    Osc1Wave, Osc2Wave, Osc2Semi, Osc2Fine, Osc2Kbd, Pw, OscMix, Fm, Sync, Ring, Analog,
    MenvA, MenvD, MenvAmt, MenvDest,
    Lfo1Rate, Lfo1Wave, Lfo1Amt, Lfo1Dest,
    Lfo2Rate, Lfo2Amt, Lfo2Dest,
    FilterType, Cutoff, Res, Drive, FenvAmt, FenvVel, KbdTrack,
    FenvA, FenvD, FenvS, FenvR,
    AenvA, AenvD, AenvS, AenvR, AmpVel, Gain,
    VoiceMode, UnisonDetune, UnisonVoices, Glide, BendRange, WheelDest, PressureDest,
    ArpOn, ArpMode, ArpRange, ArpRate, ArpHold,
    FxDrive, Chorus, DelayLevel, DelayTime, DelayFb, ReverbLevel, ReverbSize,
    SlotLevel, SlotPan,
    // The instrument's own (not per slot):
    Volume, Tempo, Octave,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);
inline constexpr int SlotParamCount = int(ParamId::Volume); // per slot: everything before Volume
inline constexpr int SlotCount = 4;                         // A..D

constexpr bool isSlotParam(int index) { return index >= 0 && index < SlotParamCount; }

namespace positions {
inline constexpr int kOsc1Wave = 4;   // Sine, Triangle, Saw, Pulse
inline constexpr int kOsc2Wave = 4;   // Triangle, Saw, Pulse, Noise
inline constexpr int kOsc2Semi = 49;  // -24 .. +24
inline constexpr int kSwitch = 2;
inline constexpr int kMenvDest = 3;   // OSC 2 pitch, FM, Pulse width
inline constexpr int kLfo1Wave = 5;   // Triangle, Saw, Square, S&H, Soft random
inline constexpr int kLfo1Dest = 5;   // OSC 1+2, OSC 2, Filter, PW, FM
inline constexpr int kLfo2Dest = 3;   // Amp, Filter, OSC 2
inline constexpr int kFilterType = 5; // LP 12, LP 24, BP, HP, Notch + LP
inline constexpr int kKbdTrack = 4;   // 0, 1/3, 2/3, 1
inline constexpr int kVoiceMode = 4;  // Poly, Mono, Legato, Unison
inline constexpr int kBendRange = 12;
inline constexpr int kWheelDest = 2;  // Morph, Vibrato
inline constexpr int kUnisonVoices = 3; // 3, 5, 7
inline constexpr int kPressureDest = 4; // Off, Filter, Vibrato, Filter + vibrato (aftertouch)
inline constexpr int kArpMode = 4;
inline constexpr int kArpRange = 4;
inline constexpr int kArpRate = 4;
inline constexpr int kChorus = 3;     // Off, Chorus, Ensemble
inline constexpr int kDelayTime = 7;
inline constexpr int kOctave = 7;     // -3 .. +3
} // namespace positions

constexpr float position(int index, int count) { return float(index) / float(count - 1); }

namespace format {
std::string percent(float v);
std::string bipolar(float v);
std::string level(float v);
std::string choice(float v, int which); // which = a ParamId with a list of names
std::string semitones(float v);
std::string cents(float v);
std::string seconds(float v);    // envelope times
std::string lfoRate(float v);
std::string cutoff(float v);
std::string glide(float v);
std::string pan(float v);
std::string bendRange(float v);
std::string arpRange(float v);
std::string tempo(float v);
std::string octave(float v);
std::string onOff(float v);
std::string unisonVoices(float v); // "3 voices"
// One formatter per list parameter (ParamDescriptor takes a plain function).
template<ParamId Id> std::string list(float v) { return choice(v, int(Id)); }
} // namespace format

namespace p = positions;
using PD = krate::ParamDescriptor;
inline constexpr std::array<PD, ParamCount> kParams{{
    {"osc1_wave",     "OSC 1 Wave",        position(2, p::kOsc1Wave),  p::kOsc1Wave,  &format::list<ParamId::Osc1Wave>},
    {"osc2_wave",     "OSC 2 Wave",        position(1, p::kOsc2Wave),  p::kOsc2Wave,  &format::list<ParamId::Osc2Wave>},
    {"osc2_semi",     "OSC 2 Semitones",   0.5f,                        p::kOsc2Semi,  &format::semitones},
    {"osc2_fine",     "OSC 2 Fine",        0.5f,                        0,             &format::cents},
    {"osc2_kbd",      "OSC 2 Kbd Track",   1.0f,                        p::kSwitch,    &format::onOff},
    {"pw",            "Pulse Width",       0.0f,                        0,             &format::percent},
    {"osc_mix",       "OSC Mix",           0.5f,                        0,             &format::percent},
    {"fm",            "FM",                0.0f,                        0,             &format::percent},
    {"sync",          "Sync",              0.0f,                        p::kSwitch,    &format::onOff},
    {"ring",          "Ring Mod",          0.0f,                        p::kSwitch,    &format::onOff},
    {"analog",        "Analog",            0.3f,                        0,             &format::percent},
    {"menv_a",        "Mod Env Attack",    0.0f,                        0,             &format::seconds},
    {"menv_d",        "Mod Env Decay",     0.4f,                        0,             &format::seconds},
    {"menv_amt",      "Mod Env Amount",    0.5f,                        0,             &format::bipolar},
    {"menv_dest",     "Mod Env Dest",      0.0f,                        p::kMenvDest,  &format::list<ParamId::MenvDest>},
    {"lfo1_rate",     "LFO 1 Rate",        0.5f,                        0,             &format::lfoRate},
    {"lfo1_wave",     "LFO 1 Wave",        0.0f,                        p::kLfo1Wave,  &format::list<ParamId::Lfo1Wave>},
    {"lfo1_amt",      "LFO 1 Amount",      0.0f,                        0,             &format::percent},
    {"lfo1_dest",     "LFO 1 Dest",        0.0f,                        p::kLfo1Dest,  &format::list<ParamId::Lfo1Dest>},
    {"lfo2_rate",     "LFO 2 Rate",        0.5f,                        0,             &format::lfoRate},
    {"lfo2_amt",      "LFO 2 Amount",      0.0f,                        0,             &format::percent},
    {"lfo2_dest",     "LFO 2 Dest",        0.0f,                        p::kLfo2Dest,  &format::list<ParamId::Lfo2Dest>},
    {"filter_type",   "Filter Type",       position(0, p::kFilterType), p::kFilterType, &format::list<ParamId::FilterType>},
    {"cutoff",        "Cutoff",            0.65f,                       0,             &format::cutoff},
    {"res",           "Resonance",         0.15f,                       0,             &format::percent},
    {"drive",         "Drive",             0.0f,                        0,             &format::percent},
    {"fenv_amt",      "Filter Env Amount", 0.35f,                       0,             &format::percent},
    {"fenv_vel",      "Filter Env Velocity", 0.0f,                      0,             &format::percent},
    {"kbd_track",     "Kbd Track",         position(2, p::kKbdTrack),   p::kKbdTrack,  &format::list<ParamId::KbdTrack>},
    {"fenv_a",        "Filter Env Attack", 0.0f,                        0,             &format::seconds},
    {"fenv_d",        "Filter Env Decay",  0.45f,                       0,             &format::seconds},
    {"fenv_s",        "Filter Env Sustain", 0.2f,                       0,             &format::percent},
    {"fenv_r",        "Filter Env Release", 0.35f,                      0,             &format::seconds},
    {"aenv_a",        "Amp Env Attack",    0.0f,                        0,             &format::seconds},
    {"aenv_d",        "Amp Env Decay",     0.45f,                       0,             &format::seconds},
    {"aenv_s",        "Amp Env Sustain",   1.0f,                        0,             &format::percent},
    {"aenv_r",        "Amp Env Release",   0.35f,                       0,             &format::seconds},
    {"amp_vel",       "Amp Velocity",      0.5f,                        0,             &format::percent},
    {"gain",          "Gain",              0.8f,                        0,             &format::level},
    {"voice_mode",    "Voice Mode",        0.0f,                        p::kVoiceMode, &format::list<ParamId::VoiceMode>},
    {"unison_detune", "Unison Detune",     0.3f,                        0,             &format::percent},
    {"unison_voices", "Unison Voices",     0.0f,                        p::kUnisonVoices, &format::unisonVoices},
    {"glide",         "Glide",             0.0f,                        0,             &format::glide},
    {"bend_range",    "Bend Range",        position(1, p::kBendRange),  p::kBendRange, &format::bendRange},
    {"wheel_dest",    "Wheel",             0.0f,                        p::kWheelDest, &format::list<ParamId::WheelDest>},
    {"pressure_dest", "Pressure",          0.0f,                        p::kPressureDest, &format::list<ParamId::PressureDest>},
    {"arp_on",        "Arpeggiator",       0.0f,                        p::kSwitch,    &format::onOff},
    {"arp_mode",      "Arp Mode",          0.0f,                        p::kArpMode,   &format::list<ParamId::ArpMode>},
    {"arp_range",     "Arp Range",         0.0f,                        p::kArpRange,  &format::arpRange},
    {"arp_rate",      "Arp Rate",          position(1, p::kArpRate),    p::kArpRate,   &format::list<ParamId::ArpRate>},
    {"arp_hold",      "Arp Hold",          0.0f,                        p::kSwitch,    &format::onOff},
    {"fx_drive",      "FX Drive",          0.0f,                        0,             &format::percent},
    {"chorus",        "Chorus",            0.0f,                        p::kChorus,    &format::list<ParamId::Chorus>},
    {"delay_level",   "Delay Level",       0.0f,                        0,             &format::percent},
    {"delay_time",    "Delay Time",        position(2, p::kDelayTime),  p::kDelayTime, &format::list<ParamId::DelayTime>},
    {"delay_fb",      "Delay Feedback",    0.3f,                        0,             &format::percent},
    {"reverb_level",  "Reverb Level",      0.0f,                        0,             &format::percent},
    {"reverb_size",   "Reverb Size",       0.5f,                        0,             &format::percent},
    {"slot_level",    "Slot Level",        0.8f,                        0,             &format::level},
    {"slot_pan",      "Slot Pan",          0.5f,                        0,             &format::pan},
    {"volume",        "Volume",            0.8f,                        0,             &format::level},
    {"tempo",         "Tempo",             0.4f,                        0,             &format::tempo},
    {"octave",        "Octave",            position(3, p::kOctave),     p::kOctave,    &format::octave},
}};
static_assert(kParams.size() <= 64, "ParamValues' changed-bits mask is 64 bits");

inline constexpr const PD &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
int index(float v, int count);
double gain(float level);         // level^2
double envSeconds(float v);       // 0.0005 .. 10 s, exponential
double lfoRateHz(float v);        // 0.05 .. 40 Hz, exponential
double cutoffHz(float v);         // 20 .. 20000 Hz, exponential
double glideSeconds(float v);     // 0 .. 2 s
int semitones(float v);           // -24 .. +24
double fineCents(float v);        // -50 .. +50
double bipolar(float v);          // -1 .. +1
double pan(float v);              // -1 .. +1
int bendSemitones(float v);       // 1 .. 12
int octaveShift(float v);         // -3 .. +3
double tempoBpm(float v);
double tempoValue(double bpm);
} // namespace units

} // namespace krimson
