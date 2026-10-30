#pragma once

#include <array>
#include <memory>

#include <QJsonObject>
#include <QString>

#include "seq/Pattern.h"

// The rack sequencer's file, ~/.local/share/krate/sequencer.json
// (docs/SEQUENCER.md): the bank, each track's target (by key: "k303",
// "krimson:B"; "" = none) and mute, RUN, CHAIN, the chosen pattern and the
// chain ([pattern, repeats], 0-based pattern). Notes are [start, length,
// pitch, velocity] in ticks (96 per beat).
struct SequencerState
{
    std::unique_ptr<krate::seq::Bank> bank = std::make_unique<krate::seq::Bank>();
    std::array<QString, krate::seq::kTracks> targets;
    std::array<bool, krate::seq::kTracks> mutes{};
    bool running = true;
    bool chainMode = false;
    int pattern = 0;
};

QJsonObject sequencerToJson(const SequencerState &state);
// Anything missing or out of range falls back to the defaults (an empty bank).
SequencerState sequencerFromJson(const QJsonObject &json);
