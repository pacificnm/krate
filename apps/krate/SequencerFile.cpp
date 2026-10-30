#include "SequencerFile.h"

#include <algorithm>

#include <QJsonArray>

using namespace krate::seq;

QJsonObject sequencerToJson(const SequencerState &state)
{
    QJsonArray tracks;
    for (int t = 0; t < kTracks; ++t)
        tracks.append(QJsonObject{{QStringLiteral("target"), state.targets[std::size_t(t)]},
                                  {QStringLiteral("mute"), state.mutes[std::size_t(t)]}});
    QJsonArray patterns;
    for (const Pattern &p : state.bank->patterns) {
        QJsonArray notes;
        for (const TrackNotes &tn : p.tracks) {
            QJsonArray list;
            for (int n = 0; n < tn.count; ++n) {
                const Note &note = tn.notes[std::size_t(n)];
                list.append(QJsonArray{note.start, note.length, note.pitch, note.velocity});
            }
            notes.append(list);
        }
        patterns.append(QJsonObject{{QStringLiteral("length"), p.length},
                                    {QStringLiteral("grid"), p.grid},
                                    {QStringLiteral("tracks"), notes}});
    }
    return {{QStringLiteral("version"), 1},
            {QStringLiteral("pattern"), state.pattern},
            {QStringLiteral("running"), state.running},
            {QStringLiteral("tracks"), tracks},
            {QStringLiteral("patterns"), patterns}};
}

SequencerState sequencerFromJson(const QJsonObject &json)
{
    SequencerState state;
    state.pattern = std::clamp(json.value(u"pattern").toInt(0), 0, kPatterns - 1);
    state.running = json.value(u"running").toBool(true);
    const QJsonArray tracks = json.value(u"tracks").toArray();
    for (int t = 0; t < kTracks && t < tracks.size(); ++t) {
        const QJsonObject o = tracks[t].toObject();
        state.targets[std::size_t(t)] = o.value(u"target").toString();
        state.mutes[std::size_t(t)] = o.value(u"mute").toBool(false);
    }
    const QJsonArray patterns = json.value(u"patterns").toArray();
    for (int i = 0; i < kPatterns && i < patterns.size(); ++i) {
        const QJsonObject o = patterns[i].toObject();
        Pattern &p = state.bank->patterns[std::size_t(i)];
        p.length = std::uint16_t(std::clamp(o.value(u"length").toInt(4 * kTicksPerBeat), 1, kMaxTicks));
        p.grid = std::uint8_t(std::clamp(o.value(u"grid").toInt(0), 0, int(kGridTicks.size()) - 1));
        const QJsonArray notes = o.value(u"tracks").toArray();
        for (int t = 0; t < kTracks && t < notes.size(); ++t)
            for (const QJsonValue &v : notes[t].toArray()) {
                const QJsonArray a = v.toArray();
                if (a.size() < 4)
                    continue;
                const int start = a[0].toInt(-1), length = a[1].toInt(0), pitch = a[2].toInt(-1), velocity = a[3].toInt(0);
                if (start < 0 || start >= p.length || length < 1 || pitch < 0 || pitch > 127)
                    continue;
                p.tracks[std::size_t(t)].add({std::uint16_t(start), std::uint16_t(std::min(length, kMaxTicks)),
                                              std::uint8_t(pitch), std::uint8_t(std::clamp(velocity, 1, 127))});
            }
    }
    return state;
}
