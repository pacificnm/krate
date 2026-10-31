#include <QtTest>

#include <cmath>

#include <QFile>
#include <QTemporaryDir>

#include "app/KorrodeInstrument.h"
#include "core/Transport.h"
#include "dsp/Params.h"

class TestInstance : public QObject
{
    Q_OBJECT

private slots:
    void entryDescribesKorrode()
    {
        const krate::InstrumentEntry entry = korrode::registryEntry();
        QCOMPARE(entry.id, QStringLiteral("korrode"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Korrode"));
        QCOMPARE(entry.panelComponent, QStringLiteral("Panel"));
        QCOMPARE(entry.defaultMidiChannel, -1); // keys follow focus in the rack
        QVERIFY(entry.actions.contains(QStringLiteral("pattern1")));
        QVERIFY(entry.actions.contains(QStringLiteral("runstop")));
        QVERIFY(bool(entry.create));
    }

    // Standalone: Korrode's own controls drive the transport it is given.
    void standaloneDrivesItsTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = korrode::registryEntry().create(context);
            auto &k = static_cast<korrode::Instance &>(*instance);
            QCOMPARE(int(instance->instrument().parameters().size()), korrode::ParamCount);
            QVERIFY(k.synth.ownsTransport());
            QVERIFY(std::abs(transport.bpm() - 120.0) < 1e-3); // the Tempo knob's default (stored as float)

            k.synth.setTempo(0.5);            // 140 BPM
            QVERIFY(std::abs(transport.bpm() - 140.0) < 1e-3);

            instance->triggerAction(QStringLiteral("runstop"));
            QVERIFY(transport.playing());
            instance->triggerAction(QStringLiteral("runstop"));
            QVERIFY(!transport.playing());

            instance->triggerAction(QStringLiteral("pattern3"));
            QCOMPARE(k.pattern.patternIndex(), 2);
            instance->triggerAction(QStringLiteral("nonsense")); // ignored
            QCOMPARE(k.pattern.patternIndex(), 2);
        }
        // Knob positions and patterns land in the context's folders.
        QVERIFY(QFile::exists(dir.filePath("config/panel.json")));
        QVERIFY(QFile::exists(dir.filePath("data/patterns.json")));
    }

    // In the rack: no transport of its own; play and tempo belong to the rack.
    void inTheRackItHasNoTransport()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = korrode::registryEntry().create(context);
        auto &k = static_cast<korrode::Instance &>(*instance);
        QVERIFY(!k.synth.ownsTransport());
        QVERIFY(!k.synth.playing());
        instance->triggerAction(QStringLiteral("runstop")); // nothing to start
        QVERIFY(!k.synth.playing());
        k.synth.setTempo(0.9); // still a stored knob; just drives nothing
        QVERIFY(std::abs(k.synth.tempo() - 0.9) < 1e-6);
        instance->triggerAction(QStringLiteral("pattern5")); // patterns still work
        QCOMPARE(k.pattern.patternIndex(), 4);
    }
};

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
