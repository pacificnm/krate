#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "core/Instrument.h"
#include "dsp/Engine.h"
#include "kalliope/dsp/Engine.h"
#include "kessel/dsp/Engine.h"
#include "kobalt/dsp/Engine.h"
#include "krimson/dsp/Engine.h"

// Every instrument, prepared again, plays the same notes as the very same
// sound, whatever it played before: an export renders each stem in a pass of
// its own (docs/DAW.md, D6), and they must add up to the mix. With the
// transport playing, so the instruments' own sequencers run too.
class TestRepeatable : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 256;

    // Prepares, then plays `notes` (frame, status, key, velocity) for `frames`, from beat 0.
    static std::vector<float> render(krate::IInstrument &instrument, const std::vector<std::array<int, 4>> &notes,
                                     std::uint64_t frames)
    {
        instrument.prepare(kRate, kBlock);
        std::vector<float> out, l(kBlock), r(kBlock);
        std::size_t next = 0;
        for (std::uint64_t at = 0; at < frames; at += kBlock) {
            std::vector<krate::MidiEvent> midi;
            while (next < notes.size() && std::uint64_t(notes[next][0]) < at + kBlock) {
                const auto &n = notes[next++];
                midi.push_back({std::uint32_t(std::uint64_t(n[0]) - at), 3,
                                {std::uint8_t(n[1]), std::uint8_t(n[2]), std::uint8_t(n[3])}});
            }
            krate::TransportState t;
            t.playing = true;
            t.restarted = at == 0;
            t.bpm = 120.0;
            t.beatAtBlockStart = double(at) / 24000.0;
            krate::processStereo(instrument, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()), &t);
            out.insert(out.end(), l.begin(), l.end());
            out.insert(out.end(), r.begin(), r.end());
        }
        return out;
    }

    // A phrase on `channel`, rendered, something else, then the phrase again.
    static void check(krate::IInstrument &instrument, int channel)
    {
        const auto on = std::uint8_t(0x90 | channel), off = std::uint8_t(0x80 | channel);
        const std::vector<std::array<int, 4>> phrase{{0, on, 48, 100},     {0, on, 55, 90},       {6000, off, 48, 0},
                                                     {9000, on, 60, 110},  {15000, off, 55, 0},   {20000, off, 60, 0},
                                                     {24000, on, 52, 80},  {36000, off, 52, 0}};
        const std::vector<float> first = render(instrument, phrase, 48000);
        QVERIFY(std::any_of(first.begin(), first.end(), [](float x) { return std::fabs(x) > 1e-3f; }));
        render(instrument, {{0, on, 70, 127}, {3000, on, 40, 60}, {7000, off, 70, 0}}, 12000); // something else
        const std::vector<float> again = render(instrument, phrase, 48000);
        QVERIFY(again == first);
    }

private slots:
    void korrode()
    {
        korrode::Engine e;
        check(e, 0);
    }
    void kessel()
    {
        kessel::Engine e;
        check(e, 9);
    }
    void kalliope()
    {
        kalliope::Engine e;
        check(e, 1);
    }
    void krimson()
    {
        krimson::Engine e;
        check(e, 2);
    }
    void kobalt()
    {
        kobalt::Engine e;
        check(e, 6);
    }
};

QTEST_GUILESS_MAIN(TestRepeatable)
#include "tst_repeatable.moc"
