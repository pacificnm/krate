#include <QtTest>

#include <array>
#include <cmath>

#include <QFile>
#include <QTemporaryDir>

#include "core/Transport.h"
#include "kobalt/app/KobaltInstrument.h"
#include "kobalt/dsp/Params.h"

using namespace kobalt;

// Kobalt's registry entry and panel controller: the transport standalone and
// in the rack, the on-screen keyboard, Octave, learned CCs.
class TestKobalt : public QObject
{
    Q_OBJECT

private slots:
    void patchesLeaveOutPanelSettings()
    {
        QVERIFY(!inPatch(ParamId::Volume));
        QVERIFY(!inPatch(ParamId::Tempo));
        QVERIFY(!inPatch(ParamId::ArpHold));
        QVERIFY(!inPatch(ParamId::Octave));
        QVERIFY(inPatch(ParamId::Cutoff));
        QVERIFY(inPatch(ParamId::PlayMode));
        QVERIFY(inPatch(ParamId::PortaMode));
        QVERIFY(!inPatch(ParamId::GenDensity));
    }

    void entryDescribesKobalt()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("kobalt"));
        QCOMPARE(entry.name, QStringLiteral("Kobalt"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Kobalt"));
        QCOMPARE(entry.panelComponent, QStringLiteral("Panel"));
        QCOMPARE(entry.defaultMidiChannel, 6); // channel 7
        QVERIFY(entry.extraMidiChannels.isEmpty());
        QVERIFY(entry.playsNotes);
        QCOMPARE(entry.actions.size(), 28);
        for (const char *action : {"bank8", "patch1", "write", "arp", "seq", "rest", "tie", "legato", "generate", "octave_up"})
            QVERIFY2(entry.actions.contains(QString::fromLatin1(action)), action);
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

    // The on-screen keyboard: a note goes through the queue, is held, sounds.
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
        QVERIFY(k.engine.voice().gate());
        QCOMPARE(k.engine.voice().note(), 64);
        k.controller.noteOff(64);
        krate::processStereo(k.engine, l.data(), r.data(), 64);
        QVERIFY(!k.engine.noteHeld(64));
        QVERIFY(!k.engine.voice().gate());
        QCOMPARE(k.controller.detents(QStringLiteral("env_trig")), 3);
        QCOMPARE(k.controller.detents(QStringLiteral("cutoff")), 0);
    }

    // Octave shifts MIDI keys; a key's note-off matches its note-on even if
    // Octave changed in between; the on-screen keyboard is not shifted again.
    void octaveShiftsMidiKeys()
    {
        Engine engine;
        engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        const auto send = [&](std::uint8_t status, std::uint8_t note) {
            const krate::MidiEvent e{0, 3, {status, note, 100}};
            krate::processStereo(engine, l.data(), r.data(), 64, &e, 1);
        };
        engine.setParam(ParamId::Octave, position(4, positions::kOctave)); // +1
        send(0x90, 60);
        QVERIFY(engine.noteHeld(72));
        engine.setParam(ParamId::Octave, position(1, positions::kOctave)); // -2, while held
        send(0x80, 60);
        QVERIFY(!engine.noteHeld(72));
        send(0x90, 60);
        QVERIFY(engine.noteHeld(36));
        send(0x80, 60);
        engine.postNote(64, 100); // the on-screen keyboard: as shown
        krate::processStereo(engine, l.data(), r.data(), 64);
        QVERIFY(engine.noteHeld(64));
        QCOMPARE(paramDescriptor(ParamId::Octave).format(position(4, positions::kOctave)), std::string("+1"));
    }

    // A note plays, and learned CCs move their parameters (snapped to detents).
    void engineLearnsCcsAndPlaysNotes()
    {
        Engine engine;
        engine.prepare(48000.0, 128);
        engine.midiLearnMap()->setMapping(21, int(ParamId::Cutoff));
        engine.midiLearnMap()->setMapping(22, int(ParamId::EnvTrig));
        std::array<float, 128> left, right;
        const std::array<krate::MidiEvent, 3> midi{{
            {0, 3, {0x96, 60, 100}},
            {5, 3, {0xB6, 21, 127}},
            {6, 3, {0xB6, 22, 70}}, // 0.55 -> Gate
        }};
        krate::processStereo(engine, left.data(), right.data(), 128, midi.data(), 3);
        float peak = 0.0f;
        for (std::size_t i = 0; i < left.size(); ++i) {
            QCOMPARE(left[i], right[i]); // no delay: mono
            peak = std::max(peak, std::fabs(left[i]));
        }
        QVERIFY(peak > 0.0f);
        QCOMPARE(engine.param(ParamId::Cutoff), 1.0f);
        QCOMPARE(engine.param(ParamId::EnvTrig), 0.5f);
    }
};

QTEST_GUILESS_MAIN(TestKobalt)
#include "tst_kobalt.moc"
