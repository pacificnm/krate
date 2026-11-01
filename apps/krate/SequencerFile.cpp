#include "SequencerFile.h"

#include <algorithm>

#include <QJsonArray>

using namespace krate::seq;

namespace {

QJsonObject arrangeToJson(const SequencerState &state)
{
    QJsonArray lanes;
    for (const auto &lane : state.arrangement.lanes) {
        QJsonArray clips;
        for (const Clip &c : lane)
            clips.append(c.offsetBars ? QJsonArray{c.startBar, c.bars, c.pattern, c.midi, c.offsetBars}
                                      : QJsonArray{c.startBar, c.bars, c.pattern, c.midi});
        lanes.append(clips);
    }
    QJsonArray midi;
    for (const MidiClip &m : state.arrangement.midi) {
        QJsonArray notes;
        for (const ClipNote &n : m.notes)
            notes.append(QJsonArray{qint64(n.start), qint64(n.length), n.pitch, n.velocity});
        midi.append(notes);
    }
    QJsonArray automation;
    for (const AutomationData &lane : state.automation) {
        QJsonArray points;
        for (const krate::AutomationPoint &p : lane.points)
            points.append(QJsonArray{p.beat, double(p.value)});
        automation.append(QJsonObject{{QStringLiteral("device"), lane.device},
                                      {QStringLiteral("param"), lane.param},
                                      {QStringLiteral("points"), points}});
    }
    return {{QStringLiteral("automation"), automation},
            {QStringLiteral("songMode"), state.songMode},
            {QStringLiteral("start"), state.startBar},
            {QStringLiteral("loop"), QJsonArray{state.loopOn, state.loopFrom, state.loopTo}},
            {QStringLiteral("punch"), QJsonArray{state.punchOn, state.punchFrom, state.punchTo}},
            {QStringLiteral("lanes"), lanes},
            {QStringLiteral("midi"), midi}};
}

void arrangeFromJson(const QJsonObject &json, SequencerState &state)
{
    state.songMode = json.value(u"songMode").toBool(false);
    state.startBar = std::clamp(json.value(u"start").toInt(0), 0, kMaxBars - 1);
    const auto range = [](const QJsonValue &v, bool &on, int &from, int &to) {
        const QJsonArray a = v.toArray();
        if (a.size() < 3)
            return;
        from = std::clamp(a[1].toInt(0), 0, kMaxBars - 1);
        to = std::clamp(a[2].toInt(from + 4), from + 1, kMaxBars);
        on = a[0].toBool(false);
    };
    range(json.value(u"loop"), state.loopOn, state.loopFrom, state.loopTo);
    range(json.value(u"punch"), state.punchOn, state.punchFrom, state.punchTo);
    for (const QJsonValue &v : json.value(u"automation").toArray()) {
        const QJsonObject o = v.toObject();
        AutomationData lane{o.value(u"device").toString(), o.value(u"param").toString(), {}};
        if (lane.device.isEmpty() || lane.param.isEmpty())
            continue;
        for (const QJsonValue &p : o.value(u"points").toArray()) {
            const QJsonArray a = p.toArray();
            if (a.size() < 2 || a[0].toDouble(-1.0) < 0.0)
                continue;
            lane.points.push_back({a[0].toDouble(), float(std::clamp(a[1].toDouble(), 0.0, 1.0))});
        }
        std::stable_sort(lane.points.begin(), lane.points.end(),
                         [](const krate::AutomationPoint &a, const krate::AutomationPoint &b) { return a.beat < b.beat; });
        state.automation.push_back(std::move(lane));
    }
    for (const QJsonValue &v : json.value(u"midi").toArray()) {
        MidiClip clip;
        for (const QJsonValue &n : v.toArray()) {
            const QJsonArray a = n.toArray();
            if (a.size() < 4 || a[0].toInteger(-1) < 0 || a[1].toInteger(0) < 1 || a[2].toInt(-1) < 0 || a[2].toInt() > 127)
                continue;
            clip.notes.push_back({std::uint32_t(a[0].toInteger()), std::uint32_t(a[1].toInteger()),
                                  std::uint8_t(a[2].toInt()), std::uint8_t(std::clamp(a[3].toInt(100), 1, 127))});
        }
        std::stable_sort(clip.notes.begin(), clip.notes.end(),
                         [](const ClipNote &a, const ClipNote &b) { return a.start < b.start; });
        state.arrangement.midi.push_back(std::move(clip));
    }
    const QJsonArray lanes = json.value(u"lanes").toArray();
    for (int t = 0; t < kTracks && t < lanes.size(); ++t) {
        auto &lane = state.arrangement.lanes[std::size_t(t)];
        for (const QJsonValue &v : lanes[t].toArray()) {
            const QJsonArray a = v.toArray();
            if (a.size() < 4)
                continue;
            Clip c{a[0].toInt(-1), a[1].toInt(0), a[2].toInt(-1), a[3].toInt(-1), std::max(0, a[4].toInt(0))};
            const bool kind = (c.pattern >= 0 && c.pattern < kPatterns && c.midi < 0)
                              || (c.midi >= 0 && c.midi < int(state.arrangement.midi.size()) && c.pattern < 0);
            if (c.startBar < 0 || c.bars < 1 || c.endBar() > kMaxBars || !kind)
                continue;
            if (!lane.empty() && lane.back().endBar() > c.startBar)
                continue; // out of order or overlapping
            lane.push_back(c);
        }
    }
}

} // namespace

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
            {QStringLiteral("patterns"), patterns},
            {QStringLiteral("arrange"), arrangeToJson(state)}};
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
    arrangeFromJson(json.value(u"arrange").toObject(), state);
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
