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

#include "kessel/app/KesselInstrument.h"
#include "kessel/app/KitFile.h"
#include "kessel/app/PatternFile.h"
#include "core/Transport.h"
#include "tools/WavFile.h"
#include "kessel/dsp/Params.h"

using namespace kessel;

class TestKessel : public QObject
{
    Q_OBJECT

private slots:
    // kParams is indexed by ParamId, and keys never repeat.
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QCOMPARE(ParamCount, 131);
        QCOMPARE(paramDescriptor(ParamId::Accent).key, std::string_view("accent"));
        QCOMPARE(paramDescriptor(ParamId::BdDecay).key, std::string_view("bd_decay"));
        QCOMPARE(paramDescriptor(ParamId::RdTune).key, std::string_view("rd_tune"));
        QCOMPARE(paramDescriptor(ParamId::Tempo).key, std::string_view("tempo"));
        std::set<std::string_view> keys;
        for (const auto &info : kParams)
            QVERIFY(keys.insert(info.key).second);
        // The mixer's blocks run in voice order (the engine indexes them so).
        static constexpr const char *kPrefix[] = {"bd", "sd", "lt", "mt", "ht", "rs", "cp", "ch",
                                                  "oh", "cr", "rd", "me", "ck", "nz", "bm", "sm"};
        for (int d = 0; d < DrumCount; ++d)
            for (const auto &[first, suffix] : {std::pair{ParamId::BdPan, "_pan"}, std::pair{ParamId::BdDrive, "_drive"},
                                                std::pair{ParamId::BdRev, "_rev"}, std::pair{ParamId::BdDly, "_dly"}})
                QCOMPARE(paramDescriptor(ParamId(int(first) + d)).key, std::string_view(std::string(kPrefix[d]) + suffix));
        QCOMPARE(units::shuffleAmount(0.0f), 0);
        QCOMPARE(units::shuffleAmount(1.0f), 5);
        QVERIFY(std::abs(units::tempoBpm(paramDescriptor(ParamId::Tempo).defaultValue) - 120.0) < 1e-3);
    }

    void entryDescribesKessel()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("kessel"));
        QCOMPARE(entry.name, QStringLiteral("Kessel"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Kessel"));
        QCOMPARE(entry.panelComponent, QStringLiteral("Panel"));
        QCOMPARE(entry.defaultMidiChannel, 9);
        QCOMPARE(entry.actions.size(), 17);
        QVERIFY(entry.actions.contains(QStringLiteral("patternA")));
        QVERIFY(entry.actions.contains(QStringLiteral("patternH")));
        QVERIFY(entry.actions.contains(QStringLiteral("patternP")));
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
        // A-D classic, E-H empty, I-P industrial.
        const auto bank = persistence::defaultBank();
        for (int i = 0; i < kClassicPatternCount; ++i)
            QVERIFY(bank.patterns[std::size_t(i)] == factoryPattern(i));
        for (int i = kClassicPatternCount; i < 8; ++i)
            QVERIFY(bank.patterns[std::size_t(i)] == Pattern{});
        for (int i = 8; i < persistence::kPatternCount; ++i)
            QVERIFY(bank.patterns[std::size_t(i)] == factoryPattern(i - 8 + kClassicPatternCount));
        QCOMPARE(bank.current, 0);
    }

    // An older patterns.json (8 patterns) keeps its patterns, cleared tracks
    // included, and gains the industrial bank I-P.
    void olderPatternFileGainsTheIndustrialBank()
    {
        persistence::PatternBank old = persistence::defaultBank();
        old.patterns[0].tracks[std::size_t(DrumId::Bd)].fill(StepState::Off); // the user cleared A's kick
        QJsonObject json = persistence::toJson(old);
        QJsonArray patterns = json.value(u"patterns").toArray();
        while (patterns.size() > 8)
            patterns.removeLast();
        json.insert(QStringLiteral("patterns"), patterns);
        const persistence::PatternBank loaded = persistence::fromJson(json);
        QVERIFY(loaded.patterns[0] == old.patterns[0]);
        QVERIFY(loaded.patterns[8] == factoryPattern(kClassicPatternCount));
    }

    // Factory kits name real parameters only; Classic is every default; no kit keeps Volume, Tempo, Shuffle.
    void factoryKitsAreValid()
    {
        std::set<QString> keys;
        for (const auto &d : kParams)
            keys.insert(QString::fromUtf8(d.key.data(), qsizetype(d.key.size())));
        const persistence::Kit init = persistence::initKit();
        QCOMPARE(persistence::factoryKit(0).values, init.values);
        for (int i = 0; i < persistence::kKitCount; ++i) {
            const persistence::Kit kit = persistence::factoryKit(i);
            QCOMPARE(kit.values.size(), init.values.size()); // no unknown key slipped in
            for (auto it = kit.values.begin(); it != kit.values.end(); ++it) {
                QVERIFY2(keys.count(it.key()), qPrintable(it.key()));
                QVERIFY(it.value() >= 0.0 && it.value() <= 1.0);
            }
            QVERIFY(!kit.values.contains(QStringLiteral("volume")) && !kit.values.contains(QStringLiteral("tempo"))
                    && !kit.values.contains(QStringLiteral("shuffle")));
        }
        QCOMPARE(persistence::factoryKit(1).name, QStringLiteral("Industrial"));
        QCOMPARE(persistence::factoryKit(persistence::kFactoryKitCount).name, QStringLiteral("Init"));
    }

    // Choosing a kit loads it (Volume stays); an edit shows; WRITE, an arrow, WRITE stores; it all persists.
    void kitsLoadEditAndWrite()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QCOMPARE(k.kits.index(), 0);
            QVERIFY(!k.kits.edited()); // a fresh panel is the Classic kit
            k.drums.setValue(QStringLiteral("volume"), 0.3);
            k.kits.select(1);
            QCOMPARE(k.kits.name(), QStringLiteral("Industrial"));
            QVERIFY(std::abs(k.drums.value(QStringLiteral("bd_drive")) - 0.55) < 1e-6);
            QVERIFY(std::abs(k.drums.value(QStringLiteral("volume")) - 0.3) < 1e-6);
            QVERIFY(!k.kits.edited());
            k.drums.setValue(QStringLiteral("sd_crack"), 0.9);
            QVERIFY(k.kits.edited());
            k.kits.pressWrite();
            QVERIFY(k.kits.writing());
            for (int i = 0; i < 6; ++i)
                k.kits.step(1); // to slot 8 (Init)
            QCOMPARE(k.kits.target(), 7);
            k.kits.pressWrite();
            QVERIFY(!k.kits.writing());
            QCOMPARE(k.kits.index(), 7);
            QCOMPARE(k.kits.name(), QStringLiteral("Kit 8"));
            QVERIFY(!k.kits.edited());
            QCOMPARE(k.kits.kitBank().kits[1], persistence::factoryKit(1)); // Industrial untouched
        }
        QVERIFY(QFile::exists(dir.filePath("data/kits.json")));
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.kits.index(), 7);
        QVERIFY(std::abs(k.kits.kitBank().kits[7].values.value(QStringLiteral("sd_crack")) - 0.9) < 1e-6);
    }

    // A WAV file of `seconds` of a 220 Hz tone, stereo, 44.1 kHz.
    static QString writeTone(const QString &path, double seconds)
    {
        std::vector<float> x;
        for (int i = 0; i < int(seconds * 44100); ++i) {
            const auto v = float(0.5 * std::sin(2.0 * 3.141592653589793 * 220.0 * i / 44100.0));
            x.push_back(v);
            x.push_back(v);
        }
        krate::writeStereoWav(path.toStdString(), x, 44100);
        return path;
    }

    // Importing copies the file into the library and plays it; it is still
    // there after a restart; a kit WRITE stores it, and loading that kit
    // brings it back; a factory kit leaves it alone; a non-WAV file is refused.
    void samplesImportPersistAndFollowKits()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const QString kick = writeTone(dir.filePath("kick.wav"), 0.5), loop = writeTone(dir.filePath("loop.wav"), 2.0);
        QFile junk(dir.filePath("notes.wav"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("not a wav file");
        junk.close();
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QCOMPARE(k.samples.name(), QString());
            k.samples.importUrls({QUrl::fromLocalFile(kick), QUrl::fromLocalFile(junk.fileName())});
            QCOMPARE(k.samples.name(), QStringLiteral("kick.wav"));
            QVERIFY(QFile::exists(dir.filePath("data/samples/kick.wav")));
            QVERIFY(k.samples.message().contains(QStringLiteral("notes.wav")));
            QVERIFY(std::abs(k.samples.seconds() - 0.5) < 0.01);
            k.kits.select(7);
            k.kits.pressWrite();
            k.kits.pressWrite(); // store into kit 8, with kick.wav
            QCOMPARE(k.kits.kitBank().kits[7].sample, QStringLiteral("kick.wav"));
            k.samples.importUrls({QUrl::fromLocalFile(loop)});
            QCOMPARE(k.samples.names(), (QStringList{QStringLiteral("kick.wav"), QStringLiteral("loop.wav")}));
            QCOMPARE(k.samples.name(), QStringLiteral("loop.wav"));
            k.kits.select(1); // Industrial: no sample of its own
            QCOMPARE(k.samples.name(), QStringLiteral("loop.wav"));
            k.kits.select(7);
            QCOMPARE(k.samples.name(), QStringLiteral("kick.wav"));
            k.samples.step(1);
            QCOMPARE(k.samples.name(), QStringLiteral("loop.wav"));
        }
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.samples.name(), QStringLiteral("loop.wav"));
        QVERIFY(std::abs(k.samples.seconds() - 2.0) < 0.01);
    }

    void patternJsonRoundTrip()
    {
        persistence::PatternBank bank = persistence::defaultBank();
        bank.patterns[1].tracks[std::size_t(DrumId::Sd)][5] = StepState::Flam;
        bank.patterns[1].velocity[std::size_t(DrumId::Sd)][5] = 64;
        bank.patterns[1].tune[std::size_t(DrumId::Me)][2] = lockOf(0.25);
        bank.patterns[1].decay[std::size_t(DrumId::Bm)][0] = lockOf(1.0);
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
        QCOMPARE(bank.current, persistence::kPatternCount - 1);
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

    // Past the 64th parameter: learnable, and the panel is still told (bit 63 stands for them).
    void highParametersLearnAndNotify()
    {
        Engine engine;
        engine.prepare(48000.0, 256);
        QVERIFY(int(ParamId::Crush) > 63);
        engine.midiLearnMap()->setMapping(21, int(ParamId::Crush));
        std::array<float, 256> left, right;
        const krate::MidiEvent cc{0, 3, {0xB0, 21, 127}};
        krate::processStereo(engine, left.data(), right.data(), 256, &cc, 1);
        QCOMPARE(engine.param(ParamId::Crush), 1.0f);
        QCOMPARE(engine.takeParamsChangedByAudio(), std::uint64_t(1) << 63);
    }
};

QTEST_GUILESS_MAIN(TestKessel)
#include "tst_kessel.moc"
