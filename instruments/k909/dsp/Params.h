#pragma once

#include <array>
#include <string>

#include "core/ParamDescriptor.h"

namespace k909 {

// K909's parameters, in panel order (docs/BRIEF.md#parameters). The enum
// indexes kParams; keys are stable (panel.json, midimap.json, rack sessions).
enum class ParamId : int {
    Accent,
    BdTune, BdLevel, BdAttack, BdDecay,
    SdTune, SdLevel, SdTone, SdSnappy,
    LtTune, LtLevel, LtDecay,
    MtTune, MtLevel, MtDecay,
    HtTune, HtLevel, HtDecay,
    RsLevel,
    CpLevel,
    ChLevel, ChDecay,
    OhLevel, OhDecay,
    CrLevel, CrTune,
    RdLevel, RdTune,
    Volume,
    Shuffle,
    Tempo,
    Count
};
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string percent(float v);
std::string level(float v);
std::string shuffle(float v);
std::string tempo(float v);
} // namespace format

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"accent",    "Total Accent", 0.5f, 0, &format::percent},
    {"bd_tune",   "BD Tune",      0.5f, 0, &format::percent},
    {"bd_level",  "BD Level",     0.8f, 0, &format::level},
    {"bd_attack", "BD Attack",    0.5f, 0, &format::percent},
    {"bd_decay",  "BD Decay",     0.5f, 0, &format::percent},
    {"sd_tune",   "SD Tune",      0.5f, 0, &format::percent},
    {"sd_level",  "SD Level",     0.8f, 0, &format::level},
    {"sd_tone",   "SD Tone",      0.5f, 0, &format::percent},
    {"sd_snappy", "SD Snappy",    0.5f, 0, &format::percent},
    {"lt_tune",   "LT Tune",      0.5f, 0, &format::percent},
    {"lt_level",  "LT Level",     0.7f, 0, &format::level},
    {"lt_decay",  "LT Decay",     0.5f, 0, &format::percent},
    {"mt_tune",   "MT Tune",      0.5f, 0, &format::percent},
    {"mt_level",  "MT Level",     0.7f, 0, &format::level},
    {"mt_decay",  "MT Decay",     0.5f, 0, &format::percent},
    {"ht_tune",   "HT Tune",      0.5f, 0, &format::percent},
    {"ht_level",  "HT Level",     0.7f, 0, &format::level},
    {"ht_decay",  "HT Decay",     0.5f, 0, &format::percent},
    {"rs_level",  "Rim Level",    0.7f, 0, &format::level},
    {"cp_level",  "Clap Level",   0.7f, 0, &format::level},
    {"ch_level",  "CH Level",     0.7f, 0, &format::level},
    {"ch_decay",  "CH Decay",     0.4f, 0, &format::percent},
    {"oh_level",  "OH Level",     0.7f, 0, &format::level},
    {"oh_decay",  "OH Decay",     0.4f, 0, &format::percent},
    {"cr_level",  "Crash Level",  0.6f, 0, &format::level},
    {"cr_tune",   "Crash Tune",   0.5f, 0, &format::percent},
    {"rd_level",  "Ride Level",   0.6f, 0, &format::level},
    {"rd_tune",   "Ride Tune",    0.5f, 0, &format::percent},
    {"volume",    "Volume",       0.8f, 0, &format::level},
    {"shuffle",   "Shuffle",      0.0f, 6, &format::shuffle},  // straight .. fully swung
    {"tempo",     "Tempo",        0.4f, 0, &format::tempo},    // 120 BPM in 40..240
}};

inline constexpr const krate::ParamDescriptor &paramDescriptor(ParamId id) { return kParams[std::size_t(id)]; }

namespace units {
inline constexpr double kTempoMinBpm = 40.0;
inline constexpr double kTempoMaxBpm = 240.0;
inline double gain(float level) { return double(level) * level; }
inline double tempoBpm(float v) { return kTempoMinBpm + (kTempoMaxBpm - kTempoMinBpm) * v; }
inline double tempoValue(double bpm) { return (bpm - kTempoMinBpm) / (kTempoMaxBpm - kTempoMinBpm); }
int shuffleAmount(float v); // 0..5
} // namespace units

} // namespace k909
