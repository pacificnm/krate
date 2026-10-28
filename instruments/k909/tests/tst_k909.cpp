#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string_view>

#include <QFile>
#include <QTemporaryDir>

#include "app/K909Instrument.h"
#include "core/Transport.h"
#include "dsp/Params.h"

using namespace k909;

class TestK909 : public QObject
{
    Q_OBJECT

private slots:
    // kParams is indexed by ParamId, and keys never repeat.
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QCOMPARE(ParamCount, 31);
        QCOMPARE(paramDescriptor(ParamId::Accent).key, std::string_view("accent"));
        QCOMPARE(paramDescriptor(ParamId::BdDecay).key, std::string_view("bd_decay"));
        QCOMPARE(paramDescriptor(ParamId::RdTune).key, std::string_view("rd_tune"));
        QCOMPARE(paramDescriptor(ParamId::Tempo).key, std::string_view("tempo"));
        std::set<std::string_view> keys;
        for (const auto &info : kParams)
            QVERIFY(keys.insert(info.key).second);
        QCOMPARE(units::shuffleAmount(0.0f), 0);
        QCOMPARE(units::shuffleAmount(1.0f), 5);
        QVERIFY(std::abs(units::tempoBpm(paramDescriptor(ParamId::Tempo).defaultValue) - 120.0) < 1e-3);
    }

    void entryDescribesK909()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("k909"));
        QCOMPARE(entry.name, QStringLiteral("K909"));
        QCOMPARE(entry.qmlModule, QStringLiteral("K909"));
        QCOMPARE(entry.panelComponent, QStringLiteral("Panel"));
        QCOMPARE(entry.defaultMidiChannel, 9);
        QCOMPARE(entry.actions.size(), 9);
        QVERIFY(entry.actions.contains(QStringLiteral("patternA")));
        QVERIFY(entry.actions.contains(QStringLiteral("patternH")));
        QVERIFY(entry.actions.contains(QStringLiteral("startstop")));
        QVERIFY(bool(entry.create));
    }

    // Standalone: START/STOP and Tempo drive the transport it is given.
    void standaloneDrivesItsTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(k.drums.ownsTransport());
            QVERIFY(std::abs(transport.bpm() - 120.0) < 1e-3);

            k.drums.setValue(QStringLiteral("tempo"), units::tempoValue(150.0));
            QVERIFY(std::abs(transport.bpm() - 150.0) < 1e-3);

            instance->triggerAction(QStringLiteral("startstop"));
            QVERIFY(transport.playing());
            instance->triggerAction(QStringLiteral("startstop"));
            QVERIFY(!transport.playing());
        }
        QVERIFY(QFile::exists(dir.filePath("config/panel.json")));
    }

    // In the rack: no transport of its own.
    void inTheRackItHasNoTransport()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(!k.drums.ownsTransport());
        instance->triggerAction(QStringLiteral("startstop"));
        QVERIFY(!k.drums.playing());
    }

    // A learned CC moves its parameter, and a note on the same block plays.
    void engineLearnsCcsAndPlaysNotes()
    {
        Engine engine;
        engine.prepare(48000.0, 256);
        engine.midiLearnMap()->setMapping(20, int(ParamId::BdDecay));

        std::array<float, 256> left, right;
        const std::array<krate::MidiEvent, 2> midi{{
            {0, 3, {0x99, 36, 127}},
            {10, 3, {0xB0, 20, 127}},
        }};
        krate::processStereo(engine, left.data(), right.data(), 256, midi.data(), 2);
        QVERIFY(engine.sounding(DrumId::Bd));
        QVERIFY(*std::max_element(left.begin(), left.end()) > 0.0f);
        QCOMPARE(engine.param(ParamId::BdDecay), 1.0f);
        QCOMPARE(engine.takeParamsChangedByAudio(), std::uint64_t(1) << int(ParamId::BdDecay));
    }
};

QTEST_GUILESS_MAIN(TestK909)
#include "tst_k909.moc"
