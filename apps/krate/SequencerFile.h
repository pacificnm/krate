#pragma once

#include <array>
#include <memory>

#include <QJsonObject>
#include <QString>

#include "seq/Arrangement.h"
#include "seq/Pattern.h"

// The rack sequencer's file, ~/.local/share/krate/sequencer.json
// (docs/SEQUENCER.md): the bank, each track's target (by key: "korrode",
// "krimson:B"; "" = none) and mute, RUN, CHAIN, RANGE (from, to: 1-based in
// the file), the chosen pattern and the
// chain ([pattern, repeats], 0-based pattern). Notes are [start, length,
// pitch, velocity] in ticks (96 per beat). "arrange" holds the arranger's
// song (docs/DAW.md, D3): SONG mode, where play starts, the loop and punch
// ranges (bars, 0-based, [from, to)), each lane's clips [startBar, bars,
// pattern, midi(, offsetBars)] (-1 = not that kind) and the MIDI clips' notes.
struct SequencerState
{
    std::unique_ptr<krate::seq::Bank> bank = std::make_unique<krate::seq::Bank>();
    std::array<QString, krate::seq::kTracks> targets;
    std::array<bool, krate::seq::kTracks> mutes{};
    bool running = true;
    bool chainMode = false;
    bool rangeMode = false;
    int rangeFrom = 0, rangeTo = 3; // 0-based, inclusive
    int pattern = 0;
    // The arranger.
    krate::seq::Arrangement arrangement;
    bool songMode = false;
    int startBar = 0;
    bool loopOn = false, punchOn = false;
    int loopFrom = 0, loopTo = 4, punchFrom = 0, punchTo = 4;
    // The library's sequence this came from (docs/SEQUENCER.md#saved-sequences;
    // 0 = none), its name, and whether it changed since it was opened or saved.
    qint64 sequenceId = 0;
    QString sequenceName;
    bool modified = false;
};

QJsonObject sequencerToJson(const SequencerState &state);
// Anything missing or out of range falls back to the defaults (an empty bank).
SequencerState sequencerFromJson(const QJsonObject &json);
