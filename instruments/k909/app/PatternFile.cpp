#include "k909/app/PatternFile.h"

#include <algorithm>

#include <QJsonArray>

namespace k909::persistence {

namespace {
constexpr std::array<const char *, DrumCount> kTrackKeys{"bd", "sd", "lt", "mt", "ht", "rs", "cp", "ch",
                                                          "oh", "cr", "rd", "me", "ck", "nz", "bm"};

int clampStepsPerBeat(int steps)
{
    return steps == 3 || steps == 4 || steps == 6 || steps == 8 ? steps : 4;
}
} // namespace

QString trackKey(int track)
{
    return track >= 0 && track < DrumCount ? QString::fromLatin1(kTrackKeys[std::size_t(track)]) : QString();
}

PatternBank defaultBank()
{
    PatternBank bank;
    for (int i = 0; i < kClassicPatternCount; ++i)
        bank.patterns[std::size_t(i)] = factoryPattern(i);
    for (int i = kClassicPatternCount; i < kFactoryPatternCount; ++i)
        bank.patterns[std::size_t(8 + i - kClassicPatternCount)] = factoryPattern(i);
    return bank;
}

QJsonObject toJson(const PatternBank &bank)
{
    QJsonArray patterns;
    for (const Pattern &p : bank.patterns) {
        QJsonObject tracks;
        for (int t = 0; t < DrumCount; ++t) {
            const auto &steps = p.tracks[std::size_t(t)];
            if (std::all_of(steps.begin(), steps.end(), [](StepState s) { return s == StepState::Off; }))
                continue; // empty tracks are left out
            QString text(16, u'0');
            for (int i = 0; i < 16; ++i)
                text[i] = QChar(u'0' + int(steps[std::size_t(i)]));
            tracks.insert(trackKey(t), text);
        }
        patterns.append(QJsonObject{
            {QStringLiteral("length"), p.length},
            {QStringLiteral("scale"), p.stepsPerBeat},
            {QStringLiteral("order"), p.order},
            {QStringLiteral("tracks"), tracks},
        });
    }
    return {
        {QStringLiteral("version"), kPatternFileVersion},
        {QStringLiteral("current"), bank.current},
        {QStringLiteral("patterns"), patterns},
    };
}

PatternBank fromJson(const QJsonObject &json)
{
    PatternBank bank = defaultBank();
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kPatternCount - 1);
    const QJsonArray patterns = json.value(u"patterns").toArray();
    for (int i = 0; i < kPatternCount && i < patterns.size(); ++i) {
        const QJsonObject po = patterns.at(i).toObject();
        Pattern &p = bank.patterns[std::size_t(i)];
        p = Pattern{};
        p.length = std::uint8_t(std::clamp(po.value(u"length").toInt(16), 1, 16));
        p.stepsPerBeat = std::uint8_t(clampStepsPerBeat(po.value(u"scale").toInt(4)));
        p.order = std::uint8_t(std::clamp(po.value(u"order").toInt(0), 0, 3));
        const QJsonObject tracks = po.value(u"tracks").toObject();
        for (int t = 0; t < DrumCount; ++t) {
            const QString text = tracks.value(trackKey(t)).toString();
            for (int s = 0; s < 16 && s < text.size(); ++s) {
                const int v = text.at(s).unicode() - u'0';
                p.tracks[std::size_t(t)][std::size_t(s)] = v == 2 ? StepState::Accent
                                                         : v == 1 ? StepState::On
                                                                  : StepState::Off;
            }
        }
    }
    return bank;
}

LoadResult loadPatterns(const QString &path, PatternBank *bank)
{
    *bank = defaultBank();
    QJsonObject json;
    const LoadResult result = krate::files::readVersionedJson(path, kPatternFileVersion, &json);
    if (result == LoadResult::Loaded)
        *bank = fromJson(json);
    return result;
}

bool savePatterns(const QString &path, const PatternBank &bank)
{
    return krate::files::writeJsonFile(path, toJson(bank));
}

} // namespace k909::persistence
