#include <QtTest>

#include <array>
#include <set>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "krimson/app/KrimsonInstrument.h"
#include "krimson/app/ProgramFile.h"

using namespace krimson;
using namespace krimson::persistence;

// Programs and performances (docs/PLAY.md#programs-and-performances).
class TestPrograms : public QObject
{
    Q_OBJECT

    static QString keyOf(int i)
    {
        const auto k = kParams[std::size_t(i)].key;
        return QString::fromUtf8(k.data(), qsizetype(k.size()));
    }

private slots:
    void factoryProgramsAreComplete()
    {
        const ProgramBank bank = defaultBank();
        std::set<QString> names;
        for (int i = 0; i < kProgramCount; ++i) {
            const Program &p = bank.programs[std::size_t(i)];
            QCOMPARE(int(p.values.size()), SlotParamCount);
            for (int k = 0; k < SlotParamCount; ++k) {
                const double v = p.values.value(keyOf(k), -1.0);
                QVERIFY2(v >= 0.0 && v <= 1.0, qPrintable(p.name + " " + keyOf(k)));
                QCOMPARE(double(krate::snapToDetent(kParams[std::size_t(k)], float(v))), double(float(v)));
            }
            for (auto it = p.morph.begin(); it != p.morph.end(); ++it) { // targets: continuous controls only
                int k = 0;
                while (keyOf(k) != it.key())
                    ++k;
                QVERIFY2(Engine::morphable(ParamId(k)), qPrintable(p.name + " morphs " + it.key()));
            }
            if (i < 16)
                QVERIFY2(names.insert(p.name).second, qPrintable(p.name));
            else
                QCOMPARE(p.name, QStringLiteral("Init"));
        }
        QCOMPARE(bank.current, (std::array<int, 4>{0, 1, 2, 3}));
    }

    void jsonRoundTrip()
    {
        ProgramBank bank = defaultBank();
        bank.current = {5, 6, 7, 63};
        bank.programs[40].name = QStringLiteral("Mine");
        bank.programs[40].morph[QStringLiteral("res")] = 0.25;
        QVERIFY(programsFromJson(toJson(bank)) == bank);
        // Detented morph targets and unknown keys are dropped; values clamped.
        const QJsonObject json{{"version", 1},
                               {"programs", QJsonArray{QJsonObject{{"params", QJsonObject{{"res", 5.0}, {"bogus", 1.0}}},
                                                                   {"morph", QJsonObject{{"filter_type", 1.0}}}}}}};
        const ProgramBank repaired = programsFromJson(json);
        QCOMPARE(repaired.programs[0].values.value(QStringLiteral("res")), 1.0);
        QVERIFY(!repaired.programs[0].values.contains(QStringLiteral("bogus")));
        QVERIFY(repaired.programs[0].morph.isEmpty());
    }

    void selectEditStore()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            // First run: slots A-D on programs 11-14, unedited.
            QCOMPARE(k.programs.number(), 0);
            QCOMPARE(k.programs.name(), QStringLiteral("Sync Lead"));
            QVERIFY(!k.programs.edited());
            QCOMPARE(k.engine.slotParam(1, ParamId::Fm), 0.35f); // B: FM Bells
            QVERIFY(k.engine.hasMorph(1));

            k.programs.select(3); // Acid 12 into A
            QCOMPARE(double(k.engine.slotParam(0, ParamId::Res)), double(0.8f));
            QCOMPARE(double(k.engine.morph(0, ParamId::Cutoff)), double(0.55f));
            k.controller.setValue(QStringLiteral("res"), 0.5);
            QVERIFY(k.programs.edited());
            k.controller.setMorph(QStringLiteral("res"), 0.3); // a morph change is an edit too
            k.controller.setValue(QStringLiteral("res"), 0.8);
            QVERIFY(k.programs.edited());

            // STORE, ▲ twice (to 26), STORE: written there, the slot now on it.
            k.programs.pressStore();
            QVERIFY(k.programs.storing());
            QCOMPARE(k.programs.storeTarget(), 3);
            k.programs.step(1);
            k.programs.step(1);
            QCOMPARE(k.programs.storeTarget(), 5);
            k.programs.pressStore();
            QVERIFY(!k.programs.storing());
            QCOMPARE(k.programs.number(), 5);
            QVERIFY(!k.programs.edited());
            QCOMPARE(k.programs.bank().programs[5].morph.value(QStringLiteral("res")), double(0.3f));

            k.programs.step(-1); // not storing: selects 15 (Unison Lead)
            QCOMPARE(k.programs.name(), QStringLiteral("Unison Lead"));
        }
        // The next start: the stored program and the slots' numbers are kept.
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.programs.bank().programs[5].values.value(QStringLiteral("res")), double(0.8f));
        QCOMPARE(k.programs.number(), 4);
        QVERIFY(!k.programs.edited()); // the slot kept its sound (slots.json)
    }

    // Program Change on a slot's channel loads that slot only.
    void programChangePerSlot()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        const krate::MidiEvent pc{0, 2, {0xC3, 12, 0}}; // channel 4: slot B, program 25 (Sub Bass... index 12)
        krate::processStereo(k.engine, l.data(), r.data(), 64, &pc, 1);
        QTRY_COMPARE(k.programs.bank().current[1], 12);
        QCOMPARE(k.programs.bank().current[0], 0);
        instance->triggerAction(QStringLiteral("bank2"));   // edited slot (A): program 21
        QCOMPARE(k.programs.number(), 8);
        instance->triggerAction(QStringLiteral("program3")); // 23 in the same bank
        QCOMPARE(k.programs.number(), 10);
    }

    // A performance keeps every slot's sound and setup, and gives them back.
    void performances()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(!k.programs.performanceUsed(2));
            k.slotSetup.pressSlot(1, true); // A + B lit
            k.slotSetup.setSplitNote(60);
            k.engine.setSlotParam(2, ParamId::Cutoff, 0.123f);
            k.programs.savePerformance(2);
            QVERIFY(k.programs.performanceUsed(2));
            // Change everything, then load it back.
            k.slotSetup.pressSlot(3, false);
            k.slotSetup.setSplitNote(-1);
            k.engine.setSlotParam(2, ParamId::Cutoff, 0.9f);
            k.programs.select(9);
            k.programs.loadPerformance(2);
            QVERIFY(k.slotSetup.isLit(0) && k.slotSetup.isLit(1) && !k.slotSetup.isLit(3));
            QCOMPARE(k.slotSetup.splitNote(), 60);
            QCOMPARE(k.engine.slotParam(2, ParamId::Cutoff), 0.123f);
        }
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(k.programs.performanceUsed(2));
        QCOMPARE(k.programs.performance(), 2);
    }
};

QTEST_GUILESS_MAIN(TestPrograms)
#include "tst_programs.moc"
