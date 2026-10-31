#include <QtTest>

#include <cmath>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "kalliope/dsp/Params.h"
#include "kobalt/dsp/Params.h"
#include "konnekt/app/Translate.h"
#include "krimson/dsp/Params.h"

using namespace konnekt;

// A Konnekt patch into Kalliope, Kobalt and Krimson presets.
class TestTranslate : public QObject
{
    Q_OBJECT

    static QJsonObject read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject{};
    }
    static QList<QJsonObject> all()
    {
        QList<QJsonObject> out;
        for (const char *dir : {KONNEKT_PRESETS, KONNEKT_LESSONS})
            for (const QString &f : QDir(QString::fromLatin1(dir)).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name))
                out << read(QString::fromLatin1(dir) + u'/' + f);
        return out;
    }
    static QJsonObject byTitle(const QString &title)
    {
        for (const QJsonObject &p : all())
            if (p.value(u"title").toString() == title)
                return p;
        return {};
    }
    // The descriptor for `key` in the target's table, if a preset stores it.
    static const krate::ParamDescriptor *param(SendTarget t, const QString &key)
    {
        const std::string k = key.toStdString();
        switch (t) {
        case SendTarget::Kalliope:
            for (std::size_t i = 0; i < kalliope::kParams.size(); ++i)
                if (kalliope::kParams[i].key == k && kalliope::inPatch(kalliope::ParamId(i)))
                    return &kalliope::kParams[i];
            break;
        case SendTarget::Kobalt:
            for (std::size_t i = 0; i < kobalt::kParams.size(); ++i)
                if (kobalt::kParams[i].key == k && kobalt::inPatch(kobalt::ParamId(i)))
                    return &kobalt::kParams[i];
            break;
        case SendTarget::Krimson:
            for (std::size_t i = 0; i < std::size_t(krimson::SlotParamCount); ++i)
                if (krimson::kParams[i].key == k)
                    return &krimson::kParams[i];
            break;
        }
        return nullptr;
    }
    static double value(const Translation &t, const char *key)
    {
        for (const auto &[k, v] : t.params)
            if (k == QLatin1String(key))
                return v;
        return -1.0;
    }

private slots:
    // Every preset and lesson, into every synth: only keys the synth stores,
    // values in range and on their detents, a report.
    void everyPatchIntoEverySynth()
    {
        const QList<QJsonObject> patches = all();
        QCOMPARE(patches.size(), 94);
        for (const QJsonObject &p : patches)
            for (const SendTarget t : {SendTarget::Kalliope, SendTarget::Kobalt, SendTarget::Krimson}) {
                const Translation tr = translate(p, t);
                const QString where = p.value(u"title").toString() + QStringLiteral(" → ") + targetName(t);
                QVERIFY2(!tr.params.empty(), qPrintable(where));
                QVERIFY2(tr.score >= 0 && tr.score <= 100, qPrintable(where));
                QVERIFY2(!(tr.exact + tr.approx + tr.lost).isEmpty(), qPrintable(where));
                QSet<QString> seen;
                for (const auto &[key, v] : tr.params) {
                    const krate::ParamDescriptor *d = param(t, key);
                    QVERIFY2(d, qPrintable(where + u' ' + key));
                    QVERIFY2(!seen.contains(key), qPrintable(where + QStringLiteral(" twice: ") + key));
                    seen << key;
                    QVERIFY2(v >= 0.0 && v <= 1.0 && std::isfinite(v), qPrintable(where + u' ' + key));
                    QVERIFY2(std::fabs(krate::snapToDetent(*d, float(v)) - v) < 1e-5, qPrintable(where + QStringLiteral(" off its detents: ") + key));
                }
            }
    }

    // The filter-envelope lesson into Kalliope: the ladder's settings, the
    // envelope's depth and times, as they were.
    void filterEnvelopeIntoKalliope()
    {
        const Translation t = translate(byTitle(QStringLiteral("A filter envelope")), SendTarget::Kalliope);
        QVERIFY(std::fabs(kalliope::units::cutoffHz(float(value(t, "cutoff"))) - 200.0) < 0.5);
        QVERIFY(std::fabs(value(t, "res") - 0.55) < 1e-6);
        QVERIFY(std::fabs(value(t, "vcf_env") - 5.0 / 8.0) < 1e-6);
        QVERIFY(std::fabs(kalliope::units::envSeconds(float(value(t, "env1_d"))) - 0.25) < 0.001);
        QVERIFY(std::fabs(value(t, "env1_s") - 0.1) < 1e-6);
        QVERIFY(std::fabs(value(t, "env2_s") - 0.8) < 1e-6);
        QCOMPARE(value(t, "vco1_wave"), 1.0 / 3.0); // saw
        QCOMPARE(value(t, "vco1_range"), 0.0);       // 16': an octave down
        QVERIFY(t.lost.isEmpty());
        QVERIFY2(t.score >= 80, qPrintable(QString::number(t.score)));
    }

    // FM: the carrier is VCO-1, the modulator VCO-2, LIN FM is CROSS MOD.
    void fmIntoKalliope()
    {
        const Translation t = translate(byTitle(QStringLiteral("Glass Bell Lead")), SendTarget::Kalliope);
        QVERIFY(std::fabs(value(t, "cross_mod") - 0.5) < 1e-6);
        QCOMPARE(value(t, "vco2_wave"), 0.0); // sine
    }

    // Ring mod and sync come across to Krimson; a sequencer is reported lost.
    void krimsonRingAndLosses()
    {
        QCOMPARE(value(translate(byTitle(QStringLiteral("Anvil Lead")), SendTarget::Krimson), "ring"), 1.0);
        QCOMPARE(value(translate(byTitle(QStringLiteral("Sync Scream")), SendTarget::Krimson), "sync"), 1.0);
        const Translation seq = translate(byTitle(QStringLiteral("Body Music Groove")), SendTarget::Krimson);
        QVERIFY(seq.lost.join(u' ').contains(QStringLiteral("Sequencer")));
        QVERIFY(seq.score < 70);
    }

    // Kobalt: its own OTA filter exactly; a second oscillator two octaves
    // down becomes the sub; glide becomes portamento; delay follows tempo.
    void intoKobalt()
    {
        const Translation pluck = translate(byTitle(QStringLiteral("Rubber Pluck Bass")), SendTarget::Kobalt);
        QVERIFY(std::fabs(value(pluck, "res") - 0.6) < 1e-6);
        QVERIFY(std::fabs(kobalt::units::cutoffHz(float(value(pluck, "cutoff"))) - 200.0) < 0.5);
        QCOMPARE(value(pluck, "saw_level"), 1.0);
        const Translation grind = translate(byTitle(QStringLiteral("Industrial Grind Bass")), SendTarget::Kobalt);
        QVERIFY(value(grind, "sub_level") > 0.5);
        QCOMPARE(value(grind, "sub_type"), 0.0); // one octave down
        const Translation glide = translate(byTitle(QStringLiteral("Square Glide Lead")), SendTarget::Kobalt);
        QCOMPARE(value(glide, "porta_mode"), 1.0);
        QVERIFY(value(glide, "pulse_level") > 0.9);
        QVERIFY(value(glide, "delay_level") > 0.2);
    }

    // The best fit: Kobalt for a gliding one-oscillator lead (Kalliope has no
    // glide), not for a pad; a patch with separate filter and amp envelopes
    // and drive fits Kalliope better than Kobalt (one envelope, no drive).
    void scoresRankTheFit()
    {
        const QJsonObject pad = byTitle(QStringLiteral("Warm Analog Strings"));
        QVERIFY(translate(pad, SendTarget::Kalliope).score > translate(pad, SendTarget::Kobalt).score);
        const QJsonObject glide = byTitle(QStringLiteral("Square Glide Lead"));
        QVERIFY(translate(glide, SendTarget::Kobalt).score > translate(glide, SendTarget::Kalliope).score);
        const QJsonObject pluck = byTitle(QStringLiteral("Rubber Pluck Bass"));
        QVERIFY(translate(pluck, SendTarget::Kalliope).score > translate(pluck, SendTarget::Kobalt).score);
        QCOMPARE(translate(QJsonObject{{QStringLiteral("modules"), QJsonArray{}}}, SendTarget::Kobalt).score, 0);
    }
};

QTEST_GUILESS_MAIN(TestTranslate)
#include "tst_translate.moc"
