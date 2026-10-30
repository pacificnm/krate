#include "k909/app/PatternFile.h"

#include <algorithm>

#include <QJsonArray>

namespace k909::persistence {

QString trackKey(int track);

namespace {
constexpr std::array<const char *, DrumCount> kTrackKeys{"bd", "sd", "lt", "mt", "ht", "rs", "cp", "ch",
                                                          "oh", "cr", "rd", "me", "ck", "nz", "bm"};

// A track's step details as a JSON array, or nothing when they are all 0.
QJsonArray detailArray(const std::array<std::uint8_t, 16> &bytes)
{
    if (std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b == 0; }))
        return {};
    QJsonArray array;
    for (std::uint8_t b : bytes)
        array.append(int(b));
    return array;
}

void readDetails(const QJsonObject &details, int track, int maximum, std::array<std::uint8_t, 16> *bytes)
{
    const QJsonArray array = details.value(trackKey(track)).toArray();
    for (int s = 0; s < 16 && s < array.size(); ++s)
        (*bytes)[std::size_t(s)] = std::uint8_t(std::clamp(array.at(s).toInt(0), 0, maximum));
}

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
        QJsonObject tracks, velocity, tune, decay;
        for (int t = 0; t < DrumCount; ++t) {
            const auto add = [&](QJsonObject &into, const StepBytes &bytes) {
                if (const QJsonArray a = detailArray(bytes[std::size_t(t)]); !a.isEmpty())
                    into.insert(trackKey(t), a);
            };
            add(velocity, p.velocity);
            add(tune, p.tune);
            add(decay, p.decay);
            const auto &steps = p.tracks[std::size_t(t)];
            if (std::all_of(steps.begin(), steps.end(), [](StepState s) { return s == StepState::Off; }))
                continue; // empty tracks are left out
            QString text(16, u'0');
            for (int i = 0; i < 16; ++i)
                text[i] = QChar(u'0' + int(steps[std::size_t(i)]));
            tracks.insert(trackKey(t), text);
        }
        QJsonObject po{
            {QStringLiteral("length"), p.length},
            {QStringLiteral("scale"), p.stepsPerBeat},
            {QStringLiteral("order"), p.order},
            {QStringLiteral("tracks"), tracks},
        };
        // Step details only where there are any (docs/SEQUENCER.md#step-details).
        if (!velocity.isEmpty())
            po.insert(QStringLiteral("velocity"), velocity);
        if (!tune.isEmpty())
            po.insert(QStringLiteral("tune"), tune);
        if (!decay.isEmpty())
            po.insert(QStringLiteral("decay"), decay);
        patterns.append(po);
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
                p.tracks[std::size_t(t)][std::size_t(s)] = v == 3   ? StepState::Flam
                                                         : v == 2 ? StepState::Accent
                                                         : v == 1 ? StepState::On
                                                                  : StepState::Off;
            }
            readDetails(po.value(u"velocity").toObject(), t, 127, &p.velocity[std::size_t(t)]);
            readDetails(po.value(u"tune").toObject(), t, 255, &p.tune[std::size_t(t)]);
            readDetails(po.value(u"decay").toObject(), t, 255, &p.decay[std::size_t(t)]);
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
