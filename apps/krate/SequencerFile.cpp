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
    QJsonArray chain;
    for (int i = 0; i < state.bank->chainLength; ++i)
        chain.append(QJsonArray{state.bank->chain[std::size_t(i)].pattern, state.bank->chain[std::size_t(i)].repeats});
    return {{QStringLiteral("version"), 1},
            {QStringLiteral("pattern"), state.pattern},
            {QStringLiteral("running"), state.running},
            {QStringLiteral("chainMode"), state.chainMode},
            {QStringLiteral("sequenceId"), state.sequenceId},
            {QStringLiteral("sequenceName"), state.sequenceName},
            {QStringLiteral("modified"), state.modified},
            {QStringLiteral("rangeMode"), state.rangeMode},
            {QStringLiteral("rangeFrom"), state.rangeFrom + 1},
            {QStringLiteral("rangeTo"), state.rangeTo + 1},
            {QStringLiteral("chain"), chain},
            {QStringLiteral("tracks"), tracks},
            {QStringLiteral("patterns"), patterns}};
}

SequencerState sequencerFromJson(const QJsonObject &json)
{
    SequencerState state;
    state.pattern = std::clamp(json.value(u"pattern").toInt(0), 0, kPatterns - 1);
    state.running = json.value(u"running").toBool(true);
    state.chainMode = json.value(u"chainMode").toBool(false);
    state.sequenceId = std::max<qint64>(0, json.value(u"sequenceId").toInteger(0));
    state.sequenceName = json.value(u"sequenceName").toString();
    state.modified = json.value(u"modified").toBool(false);
    state.rangeMode = json.value(u"rangeMode").toBool(false) && !state.chainMode;
    state.rangeFrom = std::clamp(json.value(u"rangeFrom").toInt(1), 1, kPatterns) - 1;
    state.rangeTo = std::clamp(json.value(u"rangeTo").toInt(4), 1, kPatterns) - 1;
    if (state.rangeFrom > state.rangeTo)
        std::swap(state.rangeFrom, state.rangeTo);
    for (const QJsonValue &v : json.value(u"chain").toArray()) {
        const QJsonArray a = v.toArray();
        if (state.bank->chainLength >= kMaxChain || a.size() < 2)
            continue;
        const int pattern = a[0].toInt(-1), repeats = a[1].toInt(0);
        if (pattern < 0 || pattern >= kPatterns || repeats < 1)
            continue;
        state.bank->chain[state.bank->chainLength++] = {std::uint8_t(pattern), std::uint8_t(std::min(repeats, kMaxRepeats))};
    }
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
