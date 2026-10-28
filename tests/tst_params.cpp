#include <QtTest>

#include <set>
#include <string_view>

#include "dsp/ParamStore.h"
#include "dsp/Params.h"

using namespace k303;

class TestParams : public QObject
{
    Q_OBJECT

private slots:
    void tableOrderMatchesEnum()
    {
        for (int i = 0; i < ParamCount; ++i)
            QCOMPARE(static_cast<int>(kParams[i].id), i);
    }

    void keysAreUnique()
    {
        std::set<std::string_view> keys;
        for (const auto &info : kParams)
            QVERIFY(keys.insert(info.key).second);
    }

    void keyRoundTrip()
    {
        for (const auto &info : kParams) {
            const auto id = paramFromKey(info.key);
            QVERIFY(id.has_value());
            QCOMPARE(static_cast<int>(*id), static_cast<int>(info.id));
        }
        QVERIFY(!paramFromKey("nonexistent").has_value());
    }

    void unitRanges()
    {
        using namespace units;
        QCOMPARE(tuningSemitones(0.0f), -12.0);
        QCOMPARE(tuningSemitones(0.5f), 0.0);
        QCOMPARE(tuningSemitones(1.0f), 12.0);
        QVERIFY(qFuzzyCompare(cutoffHz(0.0f), kCutoffMinHz));
        QVERIFY(qFuzzyCompare(cutoffHz(1.0f), kCutoffMaxHz));
        QVERIFY(qFuzzyCompare(decaySeconds(0.0f), kDecayMinS));
        QVERIFY(qFuzzyCompare(decaySeconds(1.0f), kDecayMaxS));
        QCOMPARE(volumeGain(0.0f), 0.0);
        QCOMPARE(volumeGain(1.0f), 1.0);
        QVERIFY(!waveformIsSquare(0.0f));
        QVERIFY(waveformIsSquare(1.0f));
        // The tempo default is 120 BPM.
        QVERIFY(std::abs(tempoBpm(paramDescriptor(ParamId::Tempo).defaultValue) - 120.0) < 1e-3);
    }

    void selectorDetents()
    {
        using namespace units;
        QCOMPARE(scaleStepsPerBeat(0.0f), 4);
        QCOMPARE(scaleStepsPerBeat(0.5f), 8);
        QCOMPARE(scaleStepsPerBeat(1.0f), 6);
        QCOMPARE(scaleStepsPerBeat(0.2f), 4);  // snaps to the nearest position
        QCOMPARE(playMode(0.0f), 0);
        QCOMPARE(playMode(0.75f), 3);
        QCOMPARE(playMode(1.0f), 4);
        // A MIDI CC of 127 reaches the last position.
        QCOMPARE(playMode(127.0f / 127.0f), 4);
    }

    void storeDefaultsAndClamp()
    {
        ParamStore store;
        for (const auto &info : kParams)
            QCOMPARE(store.get(info.id), info.defaultValue);
        store.set(ParamId::Cutoff, 2.0f);
        QCOMPARE(store.get(ParamId::Cutoff), 1.0f);
        store.set(ParamId::Cutoff, -1.0f);
        QCOMPARE(store.get(ParamId::Cutoff), 0.0f);
    }

    void storeChangedByAudio()
    {
        ParamStore store;
        QCOMPARE(store.takeChangedByAudio(), std::uint64_t(0));
        store.setFromAudio(ParamId::Resonance, 0.9f);
        store.setFromAudio(ParamId::Tempo, 0.1f);
        const auto mask = store.takeChangedByAudio();
        QCOMPARE(mask, (std::uint64_t(1) << int(ParamId::Resonance)) | (std::uint64_t(1) << int(ParamId::Tempo)));
        QCOMPARE(store.takeChangedByAudio(), std::uint64_t(0));
        QCOMPARE(store.get(ParamId::Resonance), 0.9f);
    }

    // Display text for every parameter (docs/DSP.md#parameters).
    void formatText_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<float>("value");
        QTest::addColumn<QString>("text");
        QTest::newRow("tuning low") << "tuning" << 0.0f << "-12.0 st";
        QTest::newRow("tuning centre") << "tuning" << 0.5f << "0.0 st";
        QTest::newRow("tuning high") << "tuning" << 1.0f << "+12.0 st";
        QTest::newRow("cutoff low") << "cutoff" << 0.0f << "60 Hz";
        QTest::newRow("cutoff high") << "cutoff" << 1.0f << "5.00 kHz";
        QTest::newRow("resonance") << "resonance" << 0.5f << "50%";
        QTest::newRow("resonance half-percent") << "resonance" << 0.005f << "1%";
        QTest::newRow("envmod") << "envmod" << 1.0f << "4.0 oct";
        QTest::newRow("decay short") << "decay" << 0.0f << "200 ms";
        QTest::newRow("decay long") << "decay" << 1.0f << "2.00 s";
        QTest::newRow("accent") << "accent" << 1.0f << "100%";
        QTest::newRow("waveform saw") << "waveform" << 0.0f << "Saw";
        QTest::newRow("waveform square") << "waveform" << 1.0f << "Square";
        QTest::newRow("volume off") << "volume" << 0.0f << "-inf dB";
        QTest::newRow("volume full") << "volume" << 1.0f << "0.0 dB";
        QTest::newRow("volume default") << "volume" << 0.8f << "-5.8 dB";
        QTest::newRow("tempo default") << "tempo" << 0.4f << "120 BPM";
        QTest::newRow("scale 1/32") << "scale" << 0.5f << "1/32";
        QTest::newRow("scale 1/16T") << "scale" << 1.0f << "1/16T";
        QTest::newRow("playmode fwd+rev") << "playmode" << 0.5f << "Fwd+Rev";
        QTest::newRow("playmode random") << "playmode" << 1.0f << "Random";
    }

    void formatText()
    {
        QFETCH(QString, key);
        QFETCH(float, value);
        QFETCH(QString, text);
        const auto id = paramFromKey(key.toStdString());
        QVERIFY(id.has_value());
        QCOMPARE(QString::fromStdString(paramDescriptor(*id).format(value)), text);
    }

    void everyParameterIsDescribed()
    {
        for (const auto &p : kParams) {
            QVERIFY(!p.key.empty());
            QVERIFY(!p.label.empty());
            QVERIFY(p.format != nullptr);
            QVERIFY(p.detents == 0 || p.detents >= 2);
            QVERIFY(!p.format(0.0f).empty());
            QVERIFY(!p.format(p.defaultValue).empty());
            QVERIFY(!p.format(1.0f).empty());
        }
        QCOMPARE(paramDescriptor(ParamId::Waveform).detents, 2);
        QCOMPARE(paramDescriptor(ParamId::Scale).detents, 3);
        QCOMPARE(paramDescriptor(ParamId::PlayMode).detents, 5);
        QCOMPARE(paramDescriptor(ParamId::Cutoff).detents, 0);
    }

    void snapping()
    {
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::Cutoff), 0.37f), 0.37f); // continuous
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::Waveform), 0.49f), 0.0f);
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::Waveform), 0.51f), 1.0f);
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::Scale), 0.6f), 0.5f);
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::PlayMode), 0.6f), 0.5f);
        QCOMPARE(snapToDetent(paramDescriptor(ParamId::PlayMode), 0.9f), 1.0f);
    }

    void defaultsAreNormalized()
    {
        for (const auto &info : kParams) {
            QVERIFY(info.defaultValue >= 0.0f);
            QVERIFY(info.defaultValue <= 1.0f);
        }
    }
};

QTEST_APPLESS_MAIN(TestParams)
#include "tst_params.moc"
