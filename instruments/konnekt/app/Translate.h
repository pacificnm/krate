#pragma once

#include <utility>
#include <vector>

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace konnekt {

// Turning a Konnekt patch into a preset of one of Krate's synths
// (docs/MANUAL.md#send-to-an-instrument). Those synths have a fixed shape
// (oscillators, a filter, envelopes, an LFO, effects), so the patch is read for
// that shape: the voice the cables lead back from the Output (oscillators,
// noise, ring mod, filter, VCA, the envelopes and LFOs that move them, glide,
// voices, effects), and each part is set on the synth as closely as it allows.
// What can't be is said in the report, never silently changed.
enum class SendTarget { Kalliope, Kobalt, Krimson };

struct Translation
{
    SendTarget target = SendTarget::Kalliope;
    std::vector<std::pair<QString, double>> params; // key -> stored value 0..1
    QStringList exact;  // came across as it is
    QStringList approx; // came across, changed
    QStringList lost;   // the synth can't do it
    int score = 0;      // 0..100: how much of the patch came across
};

QString targetName(SendTarget target);
QString targetId(SendTarget target); // "kalliope": its data folder
Translation translate(const QJsonObject &patch, SendTarget target);

} // namespace konnekt
