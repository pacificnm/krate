#include <QtTest>

#include "core/Transport.h"

using namespace krate;

// The transport: its own time, and following an external one (a DAW over
// JACK): tempo, position, play/stop, joining at a bar, locates.
class TestTransport : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kFrames = 512;
    static double blockBeats(double bpm) { return kFrames * bpm / 60.0 / kRate; }

private slots:
    void ownTime()
    {
        Transport t;
        t.setBpm(120.0);
        QVERIFY(!t.advance(kFrames, kRate).playing);
        t.setPlaying(true);
        TransportState s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && s.restarted);
        QCOMPARE(s.beatAtBlockStart, 0.0);
        s = t.advance(kFrames, kRate);
        QVERIFY(!s.restarted);
        QVERIFY(qFuzzyCompare(s.beatAtBlockStart, blockBeats(120.0)));
        QVERIFY(!t.following());
    }

    // Bars, beats and ticks in the time signature's beats, to quarter notes.
    void fromBbt()
    {
        Transport::External e = Transport::fromBbt(true, 3, 2, 960.0, 1920.0, 4.0, 4.0, 128.0);
        QVERIFY(e.rolling);
        QCOMPARE(e.bpm, 128.0);
        QCOMPARE(e.beat, 2 * 4.0 + 1.5); // bar 3, beat 2 and a half
        e = Transport::fromBbt(false, 2, 1, 0.0, 1920.0, 6.0, 8.0, 180.0); // 6/8: eighths
        QCOMPARE(e.bpm, 90.0);
        QCOMPARE(e.beat, 3.0); // one bar of six eighths: three quarter notes
    }

    // Following: its tempo and position; starting on a bar line restarts there.
    void followsTempoAndPosition()
    {
        Transport t;
        t.setBpm(100.0);
        Transport::External e{false, 128.0, 8.0};
        t.setExternal(&e);
        TransportState s = t.advance(kFrames, kRate);
        QVERIFY(!s.playing);
        QVERIFY(t.following());
        QCOMPARE(t.bpm(), 128.0);
        e.rolling = true; // the DAW plays from bar 3
        t.setExternal(&e);
        s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && s.restarted);
        QCOMPARE(s.beatAtBlockStart, 8.0);
        QCOMPARE(s.bpm, 128.0);
        QVERIFY(t.playing());
        e.beat += blockBeats(128.0);
        t.setExternal(&e);
        s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && !s.restarted);
        QCOMPARE(s.beatAtBlockStart, e.beat);
    }

    // Starting mid-bar: playing, not restarted (the sequencers join at the next bar).
    // A locate mid-bar while playing: one block stopped, then the same.
    // A locate onto a bar line: restarted there.
    void joinsAtTheNextBar()
    {
        Transport t;
        Transport::External e{true, 120.0, 5.5};
        t.setExternal(&e);
        TransportState s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && !s.restarted);
        QCOMPARE(s.beatAtBlockStart, 5.5);

        e.beat = 17.3; // a locate, mid-bar
        t.setExternal(&e);
        s = t.advance(kFrames, kRate);
        QVERIFY(!s.playing);
        QVERIFY(t.playing()); // the panel still shows it playing
        e.beat += blockBeats(120.0);
        t.setExternal(&e);
        s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && !s.restarted);

        e.beat = 32.0; // a locate onto bar 9
        t.setExternal(&e);
        s = t.advance(kFrames, kRate);
        QVERIFY(s.playing && s.restarted);
        QCOMPARE(s.beatAtBlockStart, 32.0);
    }

    // Play and stop while following are requests to the external transport.
    void playStopAreRequests()
    {
        Transport t;
        Transport::External e{false, 120.0, 0.0};
        t.setExternal(&e);
        t.advance(kFrames, kRate);
        t.setPlaying(true);
        QVERIFY(!t.playing()); // until the external transport rolls
        QCOMPARE(t.takeRequest(), Transport::Start);
        QCOMPARE(t.takeRequest(), Transport::None);
        t.setPlaying(false);
        QCOMPARE(t.takeRequest(), Transport::Stop);
    }

    // The external transport goes away: own time again, carrying on from where it was.
    void carriesOnWhenItGoes()
    {
        Transport t;
        Transport::External e{true, 120.0, 4.0};
        t.setExternal(&e);
        t.advance(kFrames, kRate);
        t.setExternal(nullptr);
        const TransportState s = t.advance(kFrames, kRate);
        QVERIFY(!t.following());
        QVERIFY(s.playing && !s.restarted);
        QVERIFY(qFuzzyCompare(s.beatAtBlockStart, 4.0 + blockBeats(120.0)));
        t.setPlaying(false); // own time: play and stop act at once again
        QVERIFY(!t.playing());
        QCOMPARE(t.takeRequest(), Transport::None);
    }

    // A panel's TEMPO knob shows the external tempo (a 40-240 BPM knob).
    void tempoKnobFollows()
    {
        Transport t;
        QCOMPARE(followingTempoValue(t, 120.0, 40.0, 240.0), -1.0); // nothing external
        Transport::External e{false, 128.0, 0.0};
        t.setExternal(&e);
        t.advance(kFrames, kRate);
        QCOMPARE(followingTempoValue(t, 120.0, 40.0, 240.0), (128.0 - 40.0) / 200.0);
        QCOMPARE(followingTempoValue(t, 128.0, 40.0, 240.0), -1.0); // already there
    }
};

QTEST_GUILESS_MAIN(TestTransport)
#include "tst_transport.moc"
