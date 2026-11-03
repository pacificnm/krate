#include "app/HeadFile.h"

#include <algorithm>
#include <cmath>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace krunch {

namespace {

const char *const kTubeKeys[int(PreTube::Count)] = {"high_gain", "smooth", "medium", "low_gain"};

QString tubeKey(PreTube tube) { return QString::fromLatin1(kTubeKeys[std::clamp(int(tube), 0, int(PreTube::Count) - 1)]); }

PreTube tubeOf(const QString &key)
{
    for (int t = 0; t < int(PreTube::Count); ++t)
        if (key == QLatin1String(kTubeKeys[t]))
            return PreTube(t);
    return PreTube::HighGain;
}

// A number from the file, brought into [lo, hi] (the default when missing or not a number).
double number(const QJsonObject &o, const char *key, double fallback, double lo, double hi)
{
    const QJsonValue v = o.value(QLatin1String(key));
    const double x = v.isDouble() && std::isfinite(v.toDouble()) ? v.toDouble() : fallback;
    return std::clamp(x, lo, hi);
}

} // namespace

HeadDesign explicitHead(Voice voice, PreTube tube, PowerTube power)
{
    HeadDesign h = builtInHead(voice);
    h.channelTube = false;
    h.tube.fill(tube);
    h.channelPower = false;
    h.power = powerTubeDesign(power);
    return h;
}

QJsonObject HeadFile::toJson() const
{
    const HeadDesign &d = design;
    QJsonArray stages;
    for (int i = 0; i < d.stages; ++i) {
        const TriodeStage::Design &s = d.stage[std::size_t(i)];
        stages.append(QJsonObject{
            {QStringLiteral("tube"), tubeKey(d.channelTube ? PreTube::HighGain : d.tube[std::size_t(i)])},
            {QStringLiteral("cathode_ohms"), s.cathodeOhms},
            {QStringLiteral("bypass_hz"), s.bypassHz},
            {QStringLiteral("coupling_hz"), s.couplingHz},
            {QStringLiteral("rolloff_hz"), s.rolloffHz},
            {QStringLiteral("into"), i >= 2 ? d.divider[std::size_t(i)] : 1.0},
        });
    }
    const ToneStack::Parts &t = d.stack;
    const PowerTubeDesign &p = d.power;
    return {
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("name"), name},
        {QStringLiteral("fit"), fit},
        {QStringLiteral("preamp"),
         QJsonObject{{QStringLiteral("stages"), stages},
                     {QStringLiteral("gain_pot_min_db"), d.potMinDb},
                     {QStringLiteral("pad_into_second"), d.padIntoSecond},
                     {QStringLiteral("tone_stack"),
                      QJsonObject{{QStringLiteral("r1"), t.r1}, {QStringLiteral("r2"), t.r2}, {QStringLiteral("r3"), t.r3},
                                  {QStringLiteral("r4"), t.r4}, {QStringLiteral("c1"), t.c1}, {QStringLiteral("c2"), t.c2},
                                  {QStringLiteral("c3"), t.c3}}},
                     {QStringLiteral("top_cut_hz"), d.topCutHz},
                     {QStringLiteral("trim_db"), d.trimDb}}},
        {QStringLiteral("power"),
         QJsonObject{{QStringLiteral("headroom"), p.headroom},
                     {QStringLiteral("knee"), p.knee},
                     {QStringLiteral("bias"), p.bias},
                     {QStringLiteral("sag"), p.sag},
                     {QStringLiteral("voice_hz"), p.voiceHz},
                     {QStringLiteral("voice_db"), p.voiceDb},
                     {QStringLiteral("voice_q"), p.voiceQ},
                     {QStringLiteral("master_offset_db"), d.masterOffsetDb},
                     {QStringLiteral("low_cut_hz"), d.lowCutHz},
                     {QStringLiteral("high_cut_hz"), d.highCutHz},
                     {QStringLiteral("peak_hz"), d.peakHz},
                     {QStringLiteral("peak_db"), d.peakDb},
                     {QStringLiteral("peak_q"), d.peakQ},
                     {QStringLiteral("output_db"), d.outputDb}}},
    };
}

std::optional<HeadFile> HeadFile::fromJson(const QJsonObject &json, QString *error)
{
    const auto fail = [error](const QString &why) {
        if (error)
            *error = why;
        return std::nullopt;
    };
    if (json.value(u"version").toInt() < 1 || json.value(u"version").toInt() > kVersion)
        return fail(QStringLiteral("not a head this version of Krunch reads"));
    const QJsonObject pre = json.value(u"preamp").toObject(), pow = json.value(u"power").toObject();
    const QJsonArray stages = pre.value(u"stages").toArray();
    if (stages.isEmpty() || pow.isEmpty())
        return fail(QStringLiteral("no preamp stages or power amp in it"));

    HeadFile file;
    file.name = json.value(u"name").toString();
    file.fit = json.value(u"fit").toObject();
    HeadDesign &d = file.design;
    d.channelTube = false;
    d.channelPower = false;
    d.stages = int(std::min<qsizetype>(stages.size(), HeadDesign::kMaxStages));
    for (int i = 0; i < d.stages; ++i) {
        const QJsonObject s = stages.at(i).toObject();
        d.tube[std::size_t(i)] = tubeOf(s.value(u"tube").toString());
        TriodeStage::Design &st = d.stage[std::size_t(i)];
        st.cathodeOhms = number(s, "cathode_ohms", 1500.0, 100.0, 47000.0);
        st.bypassHz = number(s, "bypass_hz", 10.0, 0.0, 10000.0);
        st.couplingHz = number(s, "coupling_hz", 10.0, 1.0, 2000.0);
        st.rolloffHz = number(s, "rolloff_hz", 12000.0, 500.0, 40000.0);
        if (i >= 2)
            d.divider[std::size_t(i)] = number(s, "into", 0.3, 0.001, 1.0);
    }
    d.potMinDb = number(pre, "gain_pot_min_db", -30.0, -80.0, 0.0);
    d.padIntoSecond = number(pre, "pad_into_second", 1.0, 0.001, 1.0);
    const QJsonObject t = pre.value(u"tone_stack").toObject();
    const ToneStack::Parts b = ToneStack::kBritish;
    d.stack = {number(t, "r1", b.r1, 1e3, 1e7), number(t, "r2", b.r2, 1e3, 1e7), number(t, "r3", b.r3, 1e2, 1e6),
               number(t, "r4", b.r4, 1e2, 1e6),  number(t, "c1", b.c1, 1e-12, 1e-6), number(t, "c2", b.c2, 1e-10, 1e-5),
               number(t, "c3", b.c3, 1e-10, 1e-5)};
    d.topCutHz = number(pre, "top_cut_hz", 11000.0, 1000.0, 20000.0);
    d.trimDb = number(pre, "trim_db", 0.0, -60.0, 60.0);
    d.power = {number(pow, "headroom", 1.0, 0.05, 10.0), number(pow, "knee", 2.5, 0.5, 10.0),
               number(pow, "bias", 0.35, 0.0, 2.0),      number(pow, "sag", 1.0, 0.0, 5.0),
               number(pow, "voice_hz", 800.0, 20.0, 15000.0), number(pow, "voice_db", 0.0, -24.0, 24.0),
               number(pow, "voice_q", 0.7, 0.1, 10.0)};
    d.masterOffsetDb = number(pow, "master_offset_db", 0.0, -40.0, 40.0);
    d.lowCutHz = number(pow, "low_cut_hz", 0.0, 0.0, 1000.0);
    d.highCutHz = number(pow, "high_cut_hz", 0.0, 0.0, 20000.0);
    if (d.highCutHz > 0.0)
        d.highCutHz = std::max(d.highCutHz, 500.0);
    d.peakHz = number(pow, "peak_hz", 1000.0, 20.0, 15000.0);
    d.peakDb = number(pow, "peak_db", 0.0, -24.0, 24.0);
    d.peakQ = number(pow, "peak_q", 0.7, 0.1, 10.0);
    d.outputDb = number(pow, "output_db", 0.0, -60.0, 60.0);
    return file;
}

std::optional<HeadFile> HeadFile::read(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("could not open it");
        return std::nullopt;
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parse);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("not JSON: ") + parse.errorString();
        return std::nullopt;
    }
    return fromJson(doc.object(), error);
}

bool HeadFile::write(const QString &path) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(toJson()).toJson());
    return f.commit();
}

} // namespace krunch
