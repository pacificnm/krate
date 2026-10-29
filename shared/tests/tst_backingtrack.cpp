#include <QtTest>

#include <cmath>
#include <complex>
#include <memory>
#include <numbers>
#include <vector>

#include "core/Transport.h"
#include "song/BackingTrack.h"
#include "song/SongPlayer.h"

using namespace krate;
using namespace krate::song;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;

// 480 ppq, 120 BPM, then 60 BPM from bar 2 (tick 1920, at 2.0 s); 20 bars long.
std::unique_ptr<Song> makeSong()
{
    auto song = std::make_unique<Song>();
    song->tempo = TempoMap(480);
    song->tempo.addTempo(0, 120.0);
    song->tempo.addTempo(1920, 60.0);
    Track t;
    t.name = "Guitar";
    t.events = {{0, 0x90, 40, 100}, {480, 0x80, 40, 0}};
    song->tracks = {t};
    song->length = 1920 * 20;
    return song;
}

// A recording at 44.1 kHz with a click (full scale, one sample) at each of `clicks` seconds.
AudioClip clipWithClicks(std::vector<double> clicks, double seconds = 10.0)
{
    AudioClip clip;
    clip.rate = 44100;
    clip.samples.assign(std::size_t(seconds * clip.rate) * 2, 0);
    for (double c : clicks) {
        const auto f = std::size_t(std::lround(c * clip.rate));
        clip.samples[2 * f] = 32767;
        clip.samples[2 * f + 1] = -32767;
    }
    return clip;
}

// The player, the backing track and a transport, as the rack runs them.
struct Rig
{
    SongPlayer player;
    BackingTrack backing{player};
    Transport transport;
    std::vector<float> left, right;

    Rig()
    {
        player.load(makeSong());
        backing.prepare(kRate, kBlock);
    }
    // As in the app, the song is loaded (a block has gone by) before play.
    void start()
    {
        block();
        left.clear();
        right.clear();
        transport.setPlaying(true);
    }
    void block()
    {
        player.beforeTransport(transport);
        const TransportState state = transport.advance(kBlock, kRate);
        std::array<DeviceEvent, 256> out{};
        player.render(state, kBlock, kRate, out.data(), std::uint32_t(out.size()));
        float l[kBlock], r[kBlock];
        float *const outputs[2] = {l, r};
        backing.process(AudioBlock{outputs, kBlock});
        left.insert(left.end(), l, l + kBlock);
        right.insert(right.end(), r, r + kBlock);
    }
    void run(double seconds)
    {
        for (long n = long(seconds * kRate / kBlock); n > 0; --n)
            block();
    }
    // The sample (from `from`) of the loudest output, and its level.
    std::pair<long, float> loudest(std::size_t from = 0) const
    {
        long at = -1;
        float peak = 0.0f;
        for (std::size_t i = from; i < left.size(); ++i)
            if (std::fabs(left[i]) > peak) {
                peak = std::fabs(left[i]);
                at = long(i);
            }
        return {at, peak};
    }
};

} // namespace

class TestBackingTrack : public QObject
{
    Q_OBJECT

private slots:
    void decodesAnMp3()
    {
        AudioClip clip;
        std::string error;
        QVERIFY2(decodeMp3(KRATE_TEST_DATA "/tone440.mp3", &clip, &error), error.c_str());
        QCOMPARE(clip.rate, 44100);
        QVERIFY(std::fabs(clip.seconds() - 0.5) < 0.06); // the encoder pads a frame or so
        // Mono on both sides, a 440 Hz tone.
        const std::size_t from = 4410, n = 8820;
        std::complex<double> at440 = 0.0, at880 = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            QCOMPARE(clip.samples[2 * (from + i)], clip.samples[2 * (from + i) + 1]);
            const double x = clip.samples[2 * (from + i)] / 32768.0;
            at440 += x * std::polar(1.0, -2.0 * std::numbers::pi * 440.0 * double(i) / 44100.0);
            at880 += x * std::polar(1.0, -2.0 * std::numbers::pi * 880.0 * double(i) / 44100.0);
        }
        QVERIFY(std::abs(at440) > 50.0 * std::abs(at880));
    }

    void refusesWhatIsntAnMp3()
    {
        AudioClip clip;
        std::string error;
        QVERIFY(!decodeMp3(KRATE_TEST_DATA "/nope.mp3", &clip, &error));
        QVERIFY(!error.empty());
    }

    void silentWhileStopped()
    {
        Rig rig;
        const AudioClip clip = clipWithClicks({0.1, 0.2, 0.3});
        rig.backing.setClip(&clip);
        rig.run(0.5);
        QCOMPARE(rig.loudest().second, 0.0f);
    }

    // Bar 1 is 0.5 s into the recording: what was recorded at 1.0 s plays 0.5 s after play.
    void playsInSyncWithTheOffset()
    {
        Rig rig;
        const AudioClip clip = clipWithClicks({1.0});
        rig.backing.setClip(&clip);
        rig.backing.setOffset(0.5);
        rig.transport.setPlaying(true);
        rig.run(1.0);
        const auto [at, peak] = rig.loudest();
        QVERIFY(peak > 0.5f);
        QVERIFY2(std::abs(at - long(0.5 * kRate)) <= 2, qPrintable(QString::number(at)));
        QVERIFY(rig.right[std::size_t(at)] < -0.5f); // the right side is the right side
    }

    // Jump to bar 2 (2.0 s by the tempo map, the tempo halving there): the
    // recording jumps with it, and fades in.
    void followsAJumpThroughATempoChange()
    {
        Rig rig;
        const AudioClip clip = clipWithClicks({2.25});
        rig.backing.setClip(&clip);
        rig.transport.setPlaying(true);
        rig.run(0.2);
        const std::size_t jump = rig.left.size();
        rig.player.cue(1920);
        rig.run(0.5);
        const auto [at, peak] = rig.loudest(jump);
        QVERIFY(peak > 0.5f);
        // Taken at the next block, at the new place's tempo: the click 0.25 s on (60 BPM).
        const long expected = long(jump) + long(0.25 * kRate);
        QVERIFY2(std::abs(at - expected) <= 2, qPrintable(QStringLiteral("%1 vs %2").arg(at).arg(expected)));
    }

    // At half speed the recording runs at half speed too.
    void followsTheSpeed()
    {
        Rig rig;
        const AudioClip clip = clipWithClicks({0.5});
        rig.player.setSpeed(0.5);
        rig.backing.setClip(&clip);
        rig.start();
        rig.run(1.5);
        const auto [at, peak] = rig.loudest();
        QVERIFY(peak > 0.2f);
        QVERIFY2(std::abs(at - long(1.0 * kRate)) <= 4, qPrintable(QString::number(at)));
    }

    // Starting mid-note fades in rather than jumping to full level.
    void startsWithoutAClick()
    {
        Rig rig;
        AudioClip clip;
        clip.rate = 44100;
        clip.samples.assign(44100 * 2 * 2, 16384); // a steady half-scale level
        rig.backing.setClip(&clip);
        rig.backing.setOffset(1.0);
        rig.transport.setPlaying(true);
        rig.run(0.05);
        QVERIFY(std::fabs(rig.left[0]) < 0.01f);
        QVERIFY(std::fabs(rig.left[480] - 0.5f) < 0.01f); // full by 10 ms
    }

    void stopsWithTheSong()
    {
        Rig rig;
        AudioClip clip;
        clip.rate = 44100;
        clip.samples.assign(44100 * 2 * 4, 16384);
        rig.backing.setClip(&clip);
        rig.transport.setPlaying(true);
        rig.run(0.2);
        rig.transport.setPlaying(false);
        const std::size_t stop = rig.left.size();
        rig.run(0.1);
        for (std::size_t i = stop + kBlock; i < rig.left.size(); ++i)
            QCOMPARE(rig.left[i], 0.0f);
    }
};

QTEST_APPLESS_MAIN(TestBackingTrack)
#include "tst_backingtrack.moc"
