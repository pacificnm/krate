#include <QtTest>

#include <cmath>
#include <vector>

#include <QTemporaryDir>

#include "krimson/app/KrimsonInstrument.h"
#include "krimson/dsp/Engine.h"

using namespace krimson;

// Morph (docs/DSP.md#morph, docs/PLAY.md#morph).
class TestMorph : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    // Brightness: the RMS of the first difference over the RMS (high content rises it).
    static double brightness(const std::vector<float> &x)
    {
        double d = 0.0, s = 0.0;
        for (std::size_t i = x.size() / 2 + 1; i < x.size(); ++i) {
            d += double(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
            s += double(x[i]) * x[i];
        }
        return std::sqrt(d / s);
    }

    // A dark saw with a bright morph target; plays one note with the given wheel and velocity.
    static std::vector<float> play(bool wheelMorphs, int wheel, int velocity, bool target = true)
    {
        Engine e;
        e.prepare(kRate, kBlock);
        e.setParam(ParamId::OscMix, 0.0f);
        e.setParam(ParamId::Cutoff, 0.25f);
        e.setParam(ParamId::FenvAmt, 0.0f);
        e.setParam(ParamId::WheelDest, wheelMorphs ? 0.0f : 1.0f);
        if (target)
            e.setMorph(0, ParamId::Cutoff, 0.9f);
        std::vector<float> out, l(kBlock), r(kBlock);
        const krate::MidiEvent events[2] = {{0, 3, {0xB0, 1, std::uint8_t(wheel)}},
                                            {0, 3, {0x90, 48, std::uint8_t(velocity)}}};
        for (int b = 0; b < 200; ++b) {
            krate::processStereo(e, l.data(), r.data(), kBlock, b == 0 ? events : nullptr, b == 0 ? 2 : 0);
            out.insert(out.end(), l.begin(), l.end());
        }
        return out;
    }

private slots:
    void blendMovesContinuousSettings()
    {
        SlotSettings a, b;
        a.cutoffOctaves = 8.0;
        b.cutoffOctaves = 12.0;
        a.aenv.decay = 0.01;
        b.aenv.decay = 1.0;
        a.filterType = MultiFilter::Type::Lp12;
        b.filterType = MultiFilter::Type::Hp;
        const SlotSettings half = blend(a, b, 0.5);
        QCOMPARE(half.cutoffOctaves, 10.0);
        QVERIFY(std::abs(half.aenv.decay - 0.1) < 1e-9); // times blend in the log domain
        QCOMPARE(half.filterType, MultiFilter::Type::Lp12); // switches don't morph
        QCOMPARE(blend(a, b, 0.0).cutoffOctaves, 8.0);
        QCOMPARE(blend(a, b, 1.0).cutoffOctaves, 12.0);
    }

    void theWheelSweepsTheTargets()
    {
        const double down = brightness(play(true, 0, 100));
        const double up = brightness(play(true, 127, 100));
        QVERIFY2(up > 2.0 * down, qPrintable(QStringLiteral("%1 vs %2").arg(up).arg(down)));
        // No target: the wheel changes nothing (it isn't on vibrato either).
        QCOMPARE(brightness(play(true, 127, 100, false)), brightness(play(true, 0, 100, false)));
    }

    // With the wheel on vibrato, each note's velocity morphs instead.
    void velocityMorphsPerNote()
    {
        const double soft = brightness(play(false, 0, 10));
        const double hard = brightness(play(false, 0, 127));
        QVERIFY(hard > 2.0 * soft);
    }

    void onlyContinuousParametersMorph()
    {
        Engine e;
        e.setMorph(0, ParamId::FilterType, 1.0f);
        e.setMorph(0, ParamId::Sync, 1.0f);
        e.setMorph(0, ParamId::Volume, 1.0f); // not a slot parameter
        QCOMPARE(e.morph(0, ParamId::FilterType), -1.0f);
        QCOMPARE(e.morph(0, ParamId::Sync), -1.0f);
        QVERIFY(!e.hasMorph(0));
        e.setMorph(2, ParamId::Res, 0.7f);
        QVERIFY(e.hasMorph(2) && !e.hasMorph(0));
        e.clearMorph(2);
        QVERIFY(!e.hasMorph(2));
    }

    // The panel's side: morph edit, per-slot targets, slots.json.
    void controllerAndStorage()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(!k.controller.morphEdit());
            instance->triggerAction(QStringLiteral("morph"));
            QVERIFY(k.controller.morphEdit());
            QVERIFY(k.controller.morphable(QStringLiteral("cutoff")));
            QVERIFY(!k.controller.morphable(QStringLiteral("filter_type")));
            k.controller.setMorph(QStringLiteral("cutoff"), 0.9);
            k.controller.setMorph(QStringLiteral("fm"), 0.6);
            k.controller.clearMorph(QStringLiteral("fm"));
            QCOMPARE(k.controller.morph(QStringLiteral("cutoff")), double(0.9f));
            QCOMPARE(k.controller.morph(QStringLiteral("fm")), -1.0);
            k.slotSetup.pressSlot(1, false); // slot B: its own (no) targets
            QCOMPARE(k.controller.morph(QStringLiteral("cutoff")), -1.0);
            k.controller.setMorph(QStringLiteral("res"), 0.4);
            k.slotSetup.pressSlot(0, false);
        }
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(double(k.engine.morph(0, ParamId::Cutoff)), double(0.9f));
        QCOMPARE(double(k.engine.morph(1, ParamId::Res)), double(0.4f));
        QCOMPARE(k.engine.morph(0, ParamId::Fm), -1.0f);
        k.controller.clearAllMorph();
        QVERIFY(!k.engine.hasMorph(0));
        QVERIFY(k.engine.hasMorph(1));
    }
};

QTEST_GUILESS_MAIN(TestMorph)
#include "tst_morph.moc"
