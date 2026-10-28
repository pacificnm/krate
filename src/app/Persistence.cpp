#include "app/Persistence.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace k303::persistence {

namespace {

Pattern emptyPattern()
{
    Pattern p;
    for (auto &s : p.steps)
        s.gate = Gate::Rest;
    return p;
}

QString gateName(Gate gate)
{
    switch (gate) {
    case Gate::Note: return QStringLiteral("note");
    case Gate::Tie: return QStringLiteral("tie");
    case Gate::Rest: return QStringLiteral("rest");
    }
    return QStringLiteral("rest");
}

Gate gateFromName(const QString &name)
{
    if (name == u"note")
        return Gate::Note;
    if (name == u"tie")
        return Gate::Tie;
    return Gate::Rest;
}

QJsonObject stepToJson(const Step &s)
{
    return {
        {QStringLiteral("note"), s.note},
        {QStringLiteral("octave"), s.octave},
        {QStringLiteral("accent"), s.accent},
        {QStringLiteral("slide"), s.slide},
        {QStringLiteral("gate"), gateName(s.gate)},
    };
}

Step stepFromJson(const QJsonObject &o)
{
    Step s;
    s.note = std::uint8_t(std::clamp(o.value(u"note").toInt(0), 0, 12));
    s.octave = std::int8_t(std::clamp(o.value(u"octave").toInt(0), -1, 1));
    s.accent = o.value(u"accent").toBool(false);
    s.slide = o.value(u"slide").toBool(false);
    s.gate = gateFromName(o.value(u"gate").toString());
    return s;
}

} // namespace

PatternBank defaultBank()
{
    PatternBank bank;
    bank.patterns.fill(emptyPattern());
    for (int i = 0; i < kFactoryPatternCount; ++i)
        bank.patterns[std::size_t(i)] = factoryPattern(i);
    return bank;
}

bool isEmpty(const Pattern &pattern)
{
    return std::all_of(pattern.steps.begin(), pattern.steps.end(),
                       [](const Step &s) { return s.gate == Gate::Rest; });
}

namespace {

// v1 -> v2: v1 shipped only factory riff 0. Put riffs 1..3 into the first
// empty slots; never touch a slot that has notes in it.
void addV2FactoryPatterns(PatternBank *bank)
{
    std::size_t slot = 1;
    for (int riff = 1; riff < kFactoryPatternCount; ++riff) {
        while (slot < bank->patterns.size() && !isEmpty(bank->patterns[slot]))
            ++slot;
        if (slot == bank->patterns.size())
            return;
        bank->patterns[slot++] = factoryPattern(riff);
    }
}

} // namespace

QJsonObject toJson(const PatternBank &bank)
{
    QJsonArray patterns;
    for (const auto &p : bank.patterns) {
        QJsonArray steps;
        for (const auto &s : p.steps)
            steps.append(stepToJson(s));
        patterns.append(QJsonObject{{QStringLiteral("length"), p.length}, {QStringLiteral("steps"), steps}});
    }
    return {
        {QStringLiteral("version"), kPatternFileVersion},
        {QStringLiteral("current"), bank.current},
        {QStringLiteral("patterns"), patterns},
    };
}

PatternBank fromJson(const QJsonObject &json)
{
    PatternBank bank;
    bank.patterns.fill(emptyPattern());
    bank.current = std::clamp(json.value(u"current").toInt(0), 0, kPatternCount - 1);

    const QJsonArray patterns = json.value(u"patterns").toArray();
    for (int i = 0; i < kPatternCount && i < patterns.size(); ++i) {
        const QJsonObject po = patterns.at(i).toObject();
        Pattern &p = bank.patterns[std::size_t(i)];
        p.length = std::uint8_t(std::clamp(po.value(u"length").toInt(16), 1, 16));
        const QJsonArray steps = po.value(u"steps").toArray();
        for (int s = 0; s < 16 && s < steps.size(); ++s)
            p.steps[std::size_t(s)] = stepFromJson(steps.at(s).toObject());
    }
    return bank;
}

LoadResult loadPatterns(const QString &path, PatternBank *bank)
{
    *bank = defaultBank();

    QFile file(path);
    if (!file.exists())
        return LoadResult::Missing;
    if (!file.open(QIODevice::ReadOnly))
        return LoadResult::Invalid;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return LoadResult::Invalid;

    const QJsonObject json = doc.object();
    const int version = json.value(u"version").toInt(0);
    if (version > kPatternFileVersion)
        return LoadResult::NewerVersion;
    if (version < 1)
        return LoadResult::Invalid;

    *bank = fromJson(json);
    if (version == 1)
        addV2FactoryPatterns(bank);
    return LoadResult::Loaded;
}

bool savePatterns(const QString &path, const PatternBank &bank)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(toJson(bank)).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace k303::persistence
