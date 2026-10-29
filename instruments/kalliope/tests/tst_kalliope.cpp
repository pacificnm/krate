#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string_view>

#include <QFile>
#include <QTemporaryDir>

#include "core/Transport.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "kalliope/dsp/Params.h"

using namespace kalliope;

class TestKalliope : public QObject
{
    Q_OBJECT

private slots:
    // kParams is indexed by ParamId; keys never repeat; every value formats.
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QCOMPARE(ParamCount, 49);
        QCOMPARE(paramDescriptor(ParamId::LfoRate).key, std::string_view("lfo_rate"));
        QCOMPARE(paramDescriptor(ParamId::Cutoff).key, std::string_view("cutoff"));
        QCOMPARE(paramDescriptor(ParamId::Env2R).key, std::string_view("env2_r"));
        QCOMPARE(paramDescriptor(ParamId::DelayFb).key, std::string_view("delay_fb"));
        QCOMPARE(paramDescriptor(ParamId::Tempo).key, std::string_view("tempo"));
        std::set<std::string_view> keys;
        for (const auto &info : kParams) {
            QVERIFY(keys.insert(info.key).second);
            QVERIFY(info.defaultValue >= 0.0f && info.defaultValue <= 1.0f);
            for (float v : {0.0f, 0.5f, 1.0f})
                QVERIFY(!info.format(v).empty());
            // Detented defaults sit on a detent.
            QCOMPARE(krate::snapToDetent(info, info.defaultValue), info.defaultValue);
        }
    }

    void displayTexts()
    {
        const auto text = [](ParamId id, float v) { return paramDescriptor(id).format(v); };
        QCOMPARE(text(ParamId::Vco1Range, position(1, positions::kVco1Range)), std::string("8'"));
        QCOMPARE(text(ParamId::Vco2Range, 0.0f), std::string("Low"));
        QCOMPARE(text(ParamId::Vco2Wave, 1.0f), std::string("Noise"));
        QCOMPARE(text(ParamId::Slope, 1.0f), std::string("-24 dB"));
        QCOMPARE(text(ParamId::KeyMode, 0.5f), std::string("Unison"));
        QCOMPARE(text(ParamId::Hpf, 0.0f), std::string("Off"));
        QCOMPARE(text(ParamId::Cutoff, 1.0f), std::string("20.00 kHz"));
        QCOMPARE(text(ParamId::Env1A, 0.0f), std::string("1 ms"));
        QCOMPARE(text(ParamId::Env1A, 1.0f), std::string("10.00 s"));
        QCOMPARE(text(ParamId::Vco2Fine, 0.5f), std::string("+0 cents"));
        QCOMPARE(text(ParamId::BendRange, paramDescriptor(ParamId::BendRange).defaultValue), std::string("2 st"));
        QCOMPARE(text(ParamId::DelayTime, paramDescriptor(ParamId::DelayTime).defaultValue), std::string("1/8"));
        QCOMPARE(text(ParamId::ArpRange, 1.0f), std::string("4 oct"));
    }

    void patchesLeaveOutPanelSettings()
    {
        QVERIFY(!inPatch(ParamId::Volume));
        QVERIFY(!inPatch(ParamId::Tempo));
        QVERIFY(!inPatch(ParamId::ArpHold));
        QVERIFY(inPatch(ParamId::Cutoff));
        QVERIFY(inPatch(ParamId::ArpOn));
    }

    void entryDescribesKalliope()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("kalliope"));
        QCOMPARE(entry.name, QStringLiteral("Kalliope"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Kalliope"));
        QCOMPARE(entry.panelComponent, QStringLiteral("Panel"));
        QCOMPARE(entry.defaultMidiChannel, 1);
        QCOMPARE(entry.actions.size(), 19);
        QVERIFY(entry.actions.contains(QStringLiteral("bank8")));
        QVERIFY(entry.actions.contains(QStringLiteral("patch1")));
        QVERIFY(entry.actions.contains(QStringLiteral("write")));
        QVERIFY(bool(entry.create));
    }

    void standaloneDrivesItsTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(k.controller.ownsTransport());
            QVERIFY(std::abs(transport.bpm() - 120.0) < 1e-3);
            k.controller.setValue(QStringLiteral("tempo"), units::tempoValue(90.0));
            QVERIFY(std::abs(transport.bpm() - 90.0) < 1e-3);
            k.controller.setPlaying(true);
            QVERIFY(transport.playing());
        }
        QVERIFY(QFile::exists(dir.filePath("config/panel.json")));
    }

    void inTheRackItHasNoTransport()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(!k.controller.ownsTransport());
        k.controller.setPlaying(true);
        QVERIFY(!k.controller.playing());
    }

    // The on-screen keyboard: a note goes through the queue, is held, and the
    // controller's held-notes view follows.
    void onScreenKeyboardPlays()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        k.controller.noteOn(64);
        QVERIFY(!k.engine.noteHeld(64)); // applied at the next block
        krate::processStereo(k.engine, l.data(), r.data(), 64);
        QVERIFY(k.engine.noteHeld(64));
        QVERIFY(k.controller.noteHeld(64));
        QCOMPARE(k.engine.activeVoices(), 1);
        k.controller.noteOff(64);
        krate::processStereo(k.engine, l.data(), r.data(), 64);
        QVERIFY(!k.engine.noteHeld(64));
        QCOMPARE(k.controller.detents(QStringLiteral("vco2_range")), 5);
        QCOMPARE(k.controller.detents(QStringLiteral("cutoff")), 0);
    }

    // A note plays, and learned CCs move their parameters (snapped to detents).
    void engineLearnsCcsAndPlaysNotes()
    {
        Engine engine;
        engine.prepare(48000.0, 128);
        engine.setParam(ParamId::Chorus, 0.0f); // dry: both sides the same
        engine.midiLearnMap()->setMapping(21, int(ParamId::Cutoff));
        engine.midiLearnMap()->setMapping(22, int(ParamId::KeyMode));
        std::array<float, 128> left, right;
        const std::array<krate::MidiEvent, 3> midi{{
            {0, 3, {0x91, 60, 100}},
            {5, 3, {0xB1, 21, 127}},
            {6, 3, {0xB1, 22, 70}}, // 0.55 -> Unison
        }};
        krate::processStereo(engine, left.data(), right.data(), 128, midi.data(), 3);
        float peak = 0.0f;
        for (std::size_t i = 0; i < left.size(); ++i) {
            QCOMPARE(left[i], right[i]); // no chorus: mono
            peak = std::max(peak, std::fabs(left[i]));
        }
        QVERIFY(peak > 0.0f);
        QCOMPARE(engine.param(ParamId::Cutoff), 1.0f);
        QCOMPARE(engine.param(ParamId::KeyMode), 0.5f);
    }
};

QTEST_GUILESS_MAIN(TestKalliope)
#include "tst_kalliope.moc"
