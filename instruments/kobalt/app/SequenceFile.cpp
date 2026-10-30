#include "kobalt/app/SequenceFile.h"

#include <algorithm>

#include <QJsonArray>

namespace kobalt::persistence {

QJsonObject toJson(const SequenceBank &bank)
{
    QJsonArray patterns;
    for (const SeqPattern &p : bank.patterns) {
        QJsonArray steps;
        for (int s = 0; s < p.length; ++s) { // steps past the length are rests
            const SeqStep &step = p.steps[std::size_t(s)];
            QJsonObject o;
            switch (step.kind) {
            case SeqStep::Kind::Rest:
                steps.append(QJsonValue::Null);
                continue;
            case SeqStep::Kind::Note: o.insert(QStringLiteral("note"), int(step.note)); break;
            case SeqStep::Kind::Tie: o.insert(QStringLiteral("tie"), true); break;
            }
            if (step.legato)
                o.insert(QStringLiteral("legato"), true);
            steps.append(o);
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
        p.length = std::uint8_t(std::clamp(po.value(u"length").toInt(16), 1, kSeqMaxSteps));
        const QJsonArray steps = po.value(u"steps").toArray();
        for (int s = 0; s < p.length && s < steps.size(); ++s) {
            const QJsonObject o = steps.at(s).toObject();
            SeqStep &step = p.steps[std::size_t(s)];
            if (o.value(u"tie").toBool(false)) {
                step.kind = SeqStep::Kind::Tie;
            } else if (o.contains(u"note")) {
                step.kind = SeqStep::Kind::Note;
                step.note = std::uint8_t(std::clamp(o.value(u"note").toInt(60), 0, 127));
            } else {
                continue; // a rest
            }
            step.legato = o.value(u"legato").toBool(false);
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

} // namespace kobalt::persistence
