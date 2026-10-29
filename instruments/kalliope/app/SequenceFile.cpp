#include "kalliope/app/SequenceFile.h"

#include <algorithm>

#include <QJsonArray>

namespace kalliope::persistence {

QJsonObject toJson(const SequenceBank &bank)
{
    QJsonArray patterns;
    for (const SeqPattern &p : bank.patterns) {
        QJsonArray steps;
        for (int s = 0; s < 16; ++s) {
            const SeqStep &step = p.steps[std::size_t(s)];
            if (step.tie) {
                steps.append(QStringLiteral("tie"));
            } else {
                QJsonArray notes;
                for (int i = 0; i < step.count; ++i)
                    notes.append(int(step.notes[std::size_t(i)]));
                steps.append(notes);
            }
        }
        patterns.append(QJsonObject{{QStringLiteral("length"), p.length}, {QStringLiteral("steps"), steps}});
    }
    return {
        {QStringLiteral("version"), kSequenceFileVersion},
        {QStringLiteral("current"), bank.current},
        {QStringLiteral("patterns"), patterns},
    };
}

SequenceBank sequencesFromJson(const QJsonObject &json)
{
    SequenceBank bank;
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kSeqPatterns - 1);
    const QJsonArray patterns = json.value(u"patterns").toArray();
    for (int i = 0; i < kSeqPatterns && i < patterns.size(); ++i) {
        const QJsonObject po = patterns.at(i).toObject();
        SeqPattern &p = bank.patterns[std::size_t(i)];
        p.length = std::uint8_t(std::clamp(po.value(u"length").toInt(16), 1, 16));
        const QJsonArray steps = po.value(u"steps").toArray();
        for (int s = 0; s < 16 && s < steps.size(); ++s) {
            SeqStep &step = p.steps[std::size_t(s)];
            const QJsonValue v = steps.at(s);
            if (v.toString() == u"tie") {
                step.tie = true;
                continue;
            }
            const QJsonArray notes = v.toArray();
            for (int n = 0; n < notes.size() && step.count < 4; ++n)
                step.notes[std::size_t(step.count++)] = std::uint8_t(std::clamp(notes.at(n).toInt(60), 0, 127));
        }
    }
    return bank;
}

LoadResult loadSequences(const QString &path, SequenceBank *bank)
{
    *bank = SequenceBank{};
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kSequenceFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = sequencesFromJson(json);
    return result;
}

bool saveSequences(const QString &path, const SequenceBank &bank)
{
    return krate::files::writeJsonFile(path, toJson(bank));
}

} // namespace kalliope::persistence
