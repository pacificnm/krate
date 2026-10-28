#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string_view>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "k909/app/K909Instrument.h"
#include "k909/app/PatternFile.h"
#include "core/Transport.h"
#include "k909/dsp/Params.h"

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

    void defaultBankHasTheFactoryPatterns()
    {
        const auto bank = persistence::defaultBank();
        for (int i = 0; i < kFactoryPatternCount; ++i)
            QVERIFY(bank.patterns[std::size_t(i)] == factoryPattern(i));
        for (int i = kFactoryPatternCount; i < persistence::kPatternCount; ++i)
            QVERIFY(bank.patterns[std::size_t(i)] == Pattern{});
        QCOMPARE(bank.current, 0);
    }

    void patternJsonRoundTrip()
    {
        persistence::PatternBank bank = persistence::defaultBank();
        bank.current = 5;
        Pattern &p = bank.patterns[5];
        p.length = 12;
        p.stepsPerBeat = 6;
        p.order = 3;
        p.tracks[std::size_t(DrumId::Rd)][15] = StepState::Accent;
        p.tracks[std::size_t(DrumId::Lt)][0] = StepState::On;
        QVERIFY(persistence::fromJson(persistence::toJson(bank)) == bank);

        // The format in docs/SEQUENCER.md#storage: tracks as strings, empty ones left out.
        const QJsonObject json = persistence::toJson(bank);
        const QJsonObject five = json.value(u"patterns").toArray().at(5).toObject();
        QCOMPARE(five.value(u"tracks").toObject().value(u"rd").toString(), QStringLiteral("0000000000000002"));
        QVERIFY(!five.value(u"tracks").toObject().contains(u"bd"));
    }

    void badPatternJsonIsClamped()
    {
        const QJsonObject json{
            {"version", 1},
            {"current", 42},
            {"patterns", QJsonArray{QJsonObject{{"length", 99}, {"scale", 5}, {"order", -3},
                                                {"tracks", QJsonObject{{"bd", "21x9"}, {"zz", "1111"}}}}}},
        };
        const auto bank = persistence::fromJson(json);
        QCOMPARE(bank.current, 7);
        const Pattern &p = bank.patterns[0];
        QCOMPARE(int(p.length), 16);
        QCOMPARE(int(p.stepsPerBeat), 4);
        QCOMPARE(int(p.order), 0);
        QCOMPARE(p.tracks[0][0], StepState::Accent);
        QCOMPARE(p.tracks[0][1], StepState::On);
        QCOMPARE(p.tracks[0][2], StepState::Off);
        QCOMPARE(p.tracks[0][3], StepState::Off);
    }

    // Edits are saved (here: on destruction) and come back on the next start.
    void controllerSavesAndRestoresTheBank()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QCOMPARE(k.drums.patternIndex(), 0);
            instance->triggerAction(QStringLiteral("patternC"));
            QCOMPARE(k.drums.patternIndex(), 2);
            k.drums.setSelectedTrack(int(DrumId::Cp));
            k.drums.clearPattern();
            k.drums.cycleStep(3);
            k.drums.cycleStep(3); // accent
            k.drums.setLength(7);
            k.drums.setScaleIndex(2); // 1/8 triplet
            k.drums.setOrderIndex(1); // reverse
            QCOMPARE(k.drums.stepState(int(DrumId::Cp), 3), 2);
        }
        QVERIFY(QFile::exists(dir.filePath("data/patterns.json")));
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.drums.patternIndex(), 2);
        QCOMPARE(k.drums.stepState(int(DrumId::Cp), 3), 2);
        QCOMPARE(k.drums.stepState(int(DrumId::Bd), 0), 0);
        QCOMPARE(k.drums.length(), 7);
        QCOMPARE(k.drums.scaleIndex(), 2);
        QCOMPARE(k.drums.orderIndex(), 1);
        // The engine starts on the saved pattern.
        QCOMPARE(k.engine.playingSlot(), 2);
        QVERIFY(k.engine.pattern() == k.drums.bank().patterns[2]);
        QCOMPARE(int(k.engine.pattern().stepsPerBeat), 3);
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
