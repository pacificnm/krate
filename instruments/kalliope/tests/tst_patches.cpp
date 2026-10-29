#include <QtTest>

#include <array>
#include <set>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "kalliope/app/KalliopeInstrument.h"
#include "kalliope/app/PatchFile.h"
#include "kalliope/dsp/Params.h"

using namespace kalliope;
using namespace kalliope::persistence;

// Patches (docs/PLAY.md#patches): factory set, file format, select / edit / write / manual, Program Change.
class TestPatches : public QObject
{
    Q_OBJECT

    static QString keyOf(ParamId id)
    {
        const auto k = paramDescriptor(id).key;
        return QString::fromUtf8(k.data(), qsizetype(k.size()));
    }

private slots:
    void factoryPatchesAreComplete()
    {
        const PatchBank bank = defaultBank();
        std::set<QString> names;
        for (int i = 0; i < kPatchCount; ++i) {
            const Patch &patch = bank.patches[std::size_t(i)];
            // Exactly the parameters a patch stores.
            for (int p = 0; p < ParamCount; ++p)
                QCOMPARE(patch.values.contains(keyOf(ParamId(p))), inPatch(ParamId(p)));
            for (int p = 0; p < ParamCount; ++p) {
                if (!inPatch(ParamId(p)))
                    continue;
                const double v = patch.values.value(keyOf(ParamId(p)));
                QVERIFY(v >= 0.0 && v <= 1.0);
                QCOMPARE(double(krate::snapToDetent(kParams[std::size_t(p)], float(v))), double(float(v)));
            }
            if (i < 16)
                QVERIFY2(names.insert(patch.name).second, qPrintable(patch.name)); // distinct factory names
            else
                QCOMPARE(patch.name, QStringLiteral("Init"));
        }
        QCOMPARE(bank.patches[0].name, QStringLiteral("Brass"));
    }

    void jsonRoundTripAndRepair()
    {
        PatchBank bank = defaultBank();
        bank.current = 42;
        bank.patches[42].name = QStringLiteral("Mine");
        bank.patches[42].values[QStringLiteral("cutoff")] = 0.123;
        QVERIFY(fromJson(toJson(bank)) == bank);

        // Missing keys take the init value, unknown keys are ignored, values clamped, names cut to 16.
        const QJsonObject json{
            {"version", 1},
            {"current", 99},
            {"patches", QJsonArray{QJsonObject{{"name", "A very long patch name"},
                                               {"params", QJsonObject{{"res", 7.0}, {"nonsense", 0.5}}}}}},
        };
        const PatchBank repaired = fromJson(json);
        QCOMPARE(repaired.current, kPatchCount - 1);
        QCOMPARE(repaired.patches[0].name.size(), 16);
        QCOMPARE(repaired.patches[0].values.value(QStringLiteral("res")), 1.0);
        QCOMPARE(repaired.patches[0].values.value(QStringLiteral("cutoff")), initPatch().values.value(QStringLiteral("cutoff")));
        QVERIFY(!repaired.patches[0].values.contains(QStringLiteral("nonsense")));
        QCOMPARE(repaired.patches[1], initPatch());
    }

    void selectEditWriteManual()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(k.patches.manual()); // first run: the panel as it is
            QCOMPARE(k.patches.name(), QStringLiteral("Manual"));

            k.patches.select(3); // bank 1, patch 4: Bass
            QVERIFY(!k.patches.manual());
            QCOMPARE(k.patches.name(), QStringLiteral("Bass"));
            QCOMPARE(k.controller.value(QStringLiteral("cutoff")), double(0.3f));
            QCOMPARE(double(k.engine.param(ParamId::Cutoff)), double(0.3f));
            QVERIFY(!k.patches.edited());

            k.controller.setValue(QStringLiteral("cutoff"), 0.5);
            QVERIFY(k.patches.edited());
            k.controller.setValue(QStringLiteral("volume"), 0.2); // not in a patch: still edited only by cutoff
            k.controller.setValue(QStringLiteral("cutoff"), 0.3);
            QVERIFY(!k.patches.edited());

            // Write into bank 3, patch 5 (index 20).
            k.controller.setValue(QStringLiteral("res"), 0.77);
            QVERIFY(k.patches.edited());
            k.patches.toggleWrite();
            QVERIFY(k.patches.writing());
            k.patches.pressBank(2);
            QCOMPARE(k.patches.pendingBank(), 2);
            k.patches.pressPatch(4);
            QVERIFY(!k.patches.writing());
            QCOMPARE(k.patches.bank(), 2);
            QCOMPARE(k.patches.number(), 4);
            QVERIFY(!k.patches.edited());
            QCOMPARE(k.patches.name(), QStringLiteral("Patch 35")); // an Init slot gets a numbered name
            QVERIFY(QFile::exists(dir.filePath("data/patches.json")));

            // Selecting another patch, then back: the written one comes back.
            k.patches.select(0);
            QCOMPARE(k.patches.name(), QStringLiteral("Brass"));
            k.patches.pressBank(2);
            QCOMPARE(k.patches.bank(), 0); // a bank applies with the next patch press
            k.patches.pressPatch(4);
            QCOMPARE(k.controller.value(QStringLiteral("res")), double(0.77f));

            k.patches.pressManual();
            QVERIFY(k.patches.manual());
            k.controller.setValue(QStringLiteral("res"), 0.1);
            QVERIFY(!k.patches.edited()); // Manual: nothing to differ from

            k.patches.select(63);
            k.patches.step(1); // wraps to 11
            QCOMPARE(k.patches.bank(), 0);
            QCOMPARE(k.patches.number(), 0);
            k.patches.step(-1);
            QCOMPARE(k.patches.bank(), 7);
        }
        // Next start: the written patch and the selection are kept.
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(!k.patches.manual());
        QCOMPARE(k.patches.bank(), 7);
        QCOMPARE(k.patches.patchBank().patches[20].values.value(QStringLiteral("res")), double(0.77f));
        QCOMPARE(k.patches.patchBank().patches[20].name, QStringLiteral("Patch 35"));
    }

    void actionsAndProgramChange()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        instance->triggerAction(QStringLiteral("bank2"));
        instance->triggerAction(QStringLiteral("patch3"));
        QCOMPARE(k.patches.bank(), 1);
        QCOMPARE(k.patches.number(), 2);
        QCOMPARE(k.patches.name(), QStringLiteral("Tremolo Keys"));
        instance->triggerAction(QStringLiteral("arp"));
        QCOMPARE(k.controller.value(QStringLiteral("play_mode")), 0.5); // Arpeggio
        instance->triggerAction(QStringLiteral("seq"));
        QCOMPARE(k.controller.value(QStringLiteral("play_mode")), 1.0); // Sequencer
        instance->triggerAction(QStringLiteral("seq"));
        QCOMPARE(k.controller.value(QStringLiteral("play_mode")), 0.0); // back to Note
        instance->triggerAction(QStringLiteral("hold"));
        QCOMPARE(k.controller.value(QStringLiteral("arp_hold")), 1.0);
        instance->triggerAction(QStringLiteral("write"));
        QVERIFY(k.patches.writing());
        instance->triggerAction(QStringLiteral("write"));
        QVERIFY(!k.patches.writing());

        // Program Change 4: patch 15 (bank 1, number 5), Sync Lead.
        k.engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        const krate::MidiEvent pc{0, 2, {0xC1, 4, 0}};
        krate::processStereo(k.engine, l.data(), r.data(), 64, &pc, 1);
        QTRY_COMPARE(k.patches.name(), QStringLiteral("Sync Lead"));
        QCOMPARE(k.patches.bank(), 0);
        QCOMPARE(k.patches.number(), 4);
    }
};

QTEST_GUILESS_MAIN(TestPatches)
#include "tst_patches.moc"
