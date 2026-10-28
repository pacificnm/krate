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
        QVERIFY(std::abs(tempoBpm(paramInfo(ParamId::Tempo).defaultValue) - 120.0) < 1e-3);
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
