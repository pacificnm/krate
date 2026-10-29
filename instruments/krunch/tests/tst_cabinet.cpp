#include <QtTest>

#include <cmath>
#include <cstring>
#include <numbers>
#include <random>
#include <vector>

#include "dsp/Cabinet.h"
#include "dsp/Krunch.h"
#include "dsp/Wav.h"

using namespace krunch;

namespace {

constexpr double kRate = 48000.0;

std::vector<float> noise(std::size_t n, unsigned seed)
{
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> d(-1.0f, 1.0f);
    std::vector<float> v(n);
    for (float &x : v)
        x = d(random);
    return v;
}

WavData wavOf(std::vector<float> left, std::vector<float> right = {}, int rate = 48000)
{
    WavData w;
    w.rate = rate;
    w.channels = right.empty() ? 1 : 2;
    w.left = std::move(left);
    w.right = std::move(right);
    return w;
}

// The IR as the kernel plays it: unit energy, then the level trim.
std::vector<float> normalised(const std::vector<float> &h, double sidesEnergy)
{
    const double scale = std::pow(10.0, -6.5 / 20.0) / std::sqrt(sidesEnergy);
    std::vector<float> out(h.size());
    for (std::size_t i = 0; i < h.size(); ++i)
        out[i] = float(h[i] * scale);
    return out;
}

double energy(const std::vector<float> &h)
{
    double e = 0.0;
    for (float v : h)
        e += double(v) * v;
    return e;
}

std::vector<float> convolve(const std::vector<float> &x, const std::vector<float> &h)
{
    std::vector<float> y(x.size(), 0.0f);
    for (std::size_t n = 0; n < x.size(); ++n) {
        double sum = 0.0;
        for (std::size_t t = 0; t < h.size() && t <= n; ++t)
            sum += double(h[t]) * x[n - t];
        y[n] = float(sum);
    }
    return y;
}

// Runs the cabinet over x in blocks of the given sizes (cycled), slot `slot` at weight 1.
std::pair<std::vector<float>, std::vector<float>> run(Cabinet &cab, const std::vector<float> &x, int slot,
                                                      std::vector<std::uint32_t> blocks = {128})
{
    std::vector<float> left(x.size(), 0.0f), right(x.size(), 0.0f), ones(4096, 1.0f);
    std::array<const float *, Cabinet::kSlots> weights{};
    weights[std::size_t(slot)] = ones.data();
    std::size_t at = 0, b = 0;
    while (at < x.size()) {
        const auto frames = std::uint32_t(std::min<std::size_t>(blocks[b++ % blocks.size()], x.size() - at));
        cab.process(x.data() + at, frames, weights.data(), left.data() + at, right.data() + at);
        at += frames;
    }
    return {left, right};
}

float maxError(const std::vector<float> &a, const std::vector<float> &b)
{
    float e = 0.0f;
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        e = std::max(e, std::fabs(a[i] - b[i]));
    return e;
}

// A WAV file's bytes: 16 / 24-bit PCM or float.
std::vector<std::uint8_t> wavBytes(const std::vector<float> &interleaved, int channels, int bits, bool isFloat, int rate)
{
    std::vector<std::uint8_t> b;
    const auto put = [&b](std::uint32_t v, int n) {
        for (int i = 0; i < n; ++i)
            b.push_back(std::uint8_t(v >> (8 * i)));
    };
    const std::uint32_t data = std::uint32_t(interleaved.size() * std::size_t(bits / 8));
    b.insert(b.end(), {'R', 'I', 'F', 'F'});
    put(36 + data, 4);
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put(16, 4);
    put(isFloat ? 3 : 1, 2);
    put(std::uint32_t(channels), 2);
    put(std::uint32_t(rate), 4);
    put(std::uint32_t(rate * channels * bits / 8), 4);
    put(std::uint32_t(channels * bits / 8), 2);
    put(std::uint32_t(bits), 2);
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    put(data, 4);
    for (float s : interleaved) {
        if (isFloat) {
            std::uint32_t v;
            std::memcpy(&v, &s, 4);
            put(v, 4);
        } else if (bits == 16) {
            put(std::uint32_t(std::int32_t(std::lround(s * 32767.0f))) & 0xFFFF, 2);
        } else {
            put(std::uint32_t(std::int32_t(std::lround(s * 8388607.0f))) & 0xFFFFFF, 3);
        }
    }
    return b;
}

} // namespace

class TestCabinet : public QObject
{
    Q_OBJECT

private slots:
    // The partitioned convolution equals the direct one, whatever the host's block sizes.
    void matchesDirectConvolution()
    {
        const auto h = noise(1000, 1); // several partitions and a partial one
        const auto x = noise(5000, 2);
        auto kernel = IrKernel::build(wavOf(h), kRate);
        QVERIFY(kernel);
        QCOMPARE(kernel->partitions(), std::size_t(8));
        const auto expected = convolve(x, normalised(h, energy(h)));
        for (const auto &blocks : {std::vector<std::uint32_t>{128}, {64}, {1000}, {1, 7, 300, 33}}) {
            Cabinet cab;
            cab.prepare();
            cab.setKernel(0, kernel.get());
            const auto [left, right] = run(cab, x, 0, blocks);
            QVERIFY2(maxError(left, expected) < 2e-4f, qPrintable(QString::number(maxError(left, expected))));
            QCOMPARE(left, right); // a mono IR on both sides
        }
    }

    void noDelay()
    {
        std::vector<float> h(300, 0.0f);
        h[0] = 1.0f;
        auto kernel = IrKernel::build(wavOf(h), kRate);
        Cabinet cab;
        cab.prepare();
        cab.setKernel(3, kernel.get());
        std::vector<float> x(256, 0.0f);
        x[10] = 1.0f;
        const auto [left, right] = run(cab, x, 3);
        QVERIFY(std::fabs(left[10] - float(std::pow(10.0, -6.5 / 20.0))) < 1e-6f);
        QCOMPARE(left[9], 0.0f);
    }

    // A slot that starts playing late is right at once: the input's history is shared.
    void aSlotStartingLateIsRightAtOnce()
    {
        const auto h = noise(2000, 3), x = noise(6000, 4);
        auto kernel = IrKernel::build(wavOf(h), kRate);
        Cabinet cab;
        cab.prepare();
        cab.setKernel(5, kernel.get());
        // Slot 5 silent (no weights) for the first 4000 samples, then playing.
        std::vector<float> left(x.size(), 0.0f), right(x.size(), 0.0f), ones(4096, 1.0f);
        std::array<const float *, Cabinet::kSlots> none{}, five{};
        five[5] = ones.data();
        for (std::size_t at = 0; at < x.size(); at += 100)
            cab.process(x.data() + at, 100, (at < 4000 ? none : five).data(), left.data() + at, right.data() + at);
        const auto expected = convolve(x, normalised(h, energy(h)));
        QCOMPARE(left[3999], 0.0f);
        QVERIFY(maxError(std::vector<float>(left.begin() + 4000, left.end()),
                         std::vector<float>(expected.begin() + 4000, expected.end()))
                < 2e-4f);
    }

    void stereoIr()
    {
        const auto l = noise(500, 5), r = noise(500, 6), x = noise(2000, 7);
        auto kernel = IrKernel::build(wavOf(l, r), kRate);
        QCOMPARE(kernel->sides(), 2);
        Cabinet cab;
        cab.prepare();
        cab.setKernel(0, kernel.get());
        const auto [left, right] = run(cab, x, 0);
        const double e = (energy(l) + energy(r)) / 2.0;
        QVERIFY(maxError(left, convolve(x, normalised(l, e))) < 2e-4f);
        QVERIFY(maxError(right, convolve(x, normalised(r, e))) < 2e-4f);
    }

    void weightsScaleAndMix()
    {
        const auto h = noise(300, 8), x = noise(1000, 9);
        auto kernel = IrKernel::build(wavOf(h), kRate);
        Cabinet cab;
        cab.prepare();
        cab.setKernel(0, kernel.get());
        cab.setKernel(1, kernel.get());
        std::vector<float> left(x.size(), 0.0f), right(x.size(), 0.0f), quarter(x.size(), 0.25f), threeQ(x.size(), 0.75f);
        std::array<const float *, Cabinet::kSlots> w{};
        w[0] = quarter.data();
        w[1] = threeQ.data();
        cab.process(x.data(), std::uint32_t(x.size()), w.data(), left.data(), right.data());
        QVERIFY(maxError(left, convolve(x, normalised(h, energy(h)))) < 2e-4f); // 1/4 + 3/4 of the same
    }

    void longIrsAreCut()
    {
        auto kernel = IrKernel::build(wavOf(noise(IrKernel::kMaxPartitions * IrKernel::kBlock + 5000, 10)), kRate);
        QCOMPARE(kernel->partitions(), IrKernel::kMaxPartitions);
    }

    void silentIrIsRefused()
    {
        std::string error;
        QVERIFY(!IrKernel::build(wavOf(std::vector<float>(100, 0.0f)), kRate, &error));
        QVERIFY(!error.empty());
    }

    // Recorded at 44.1 kHz, played at 48: same length in time, same response.
    void resampledToTheEnginesRate()
    {
        std::vector<float> h(4410, 0.0f);
        for (std::size_t i = 0; i < h.size(); ++i) // a decaying 1 kHz ring
            h[i] = float(std::exp(-double(i) / 400.0) * std::sin(2.0 * std::numbers::pi * 1000.0 * double(i) / 44100.0));
        auto kernel = IrKernel::build(wavOf(h, {}, 44100), kRate);
        QCOMPARE(kernel->taps(), std::size_t(4800));
        Cabinet cab;
        cab.prepare();
        cab.setKernel(0, kernel.get());
        // A 1 kHz sine rings through; a 5 kHz one barely.
        const auto level = [&](double hz) {
            std::vector<float> x(9600);
            for (std::size_t i = 0; i < x.size(); ++i)
                x[i] = float(std::sin(2.0 * std::numbers::pi * hz * double(i) / kRate));
            Cabinet c;
            c.prepare();
            c.setKernel(0, kernel.get());
            const auto out = run(c, x, 0).first;
            double sum = 0.0;
            for (std::size_t i = 4800; i < out.size(); ++i)
                sum += double(out[i]) * out[i];
            return std::sqrt(sum / 4800.0);
        };
        QVERIFY(level(1000.0) > 10.0 * level(5000.0));
    }

    // --- WAV files ----------------------------------------------------------------

    void readsWavEncodings()
    {
        const std::vector<float> stereo{0.5f, -0.25f, 0.125f, 0.75f, -1.0f, 0.0f};
        for (const auto &[bits, isFloat] : {std::pair{16, false}, {24, false}, {32, true}}) {
            WavData wav;
            QVERIFY(readWav(wavBytes(stereo, 2, bits, isFloat, 44100), &wav));
            QCOMPARE(wav.rate, 44100);
            QCOMPARE(wav.channels, 2);
            QCOMPARE(wav.left.size(), std::size_t(3));
            QVERIFY(std::fabs(wav.left[1] - 0.125f) < 1e-4f);
            QVERIFY(std::fabs(wav.right[0] + 0.25f) < 1e-4f);
            QVERIFY(std::fabs(wav.left[2] + 1.0f) < 1e-4f);
        }
        WavData mono;
        QVERIFY(readWav(wavBytes({0.5f, 0.25f}, 1, 24, false, 48000), &mono));
        QVERIFY(mono.right.empty());
    }

    void refusesWhatIsntAWav()
    {
        WavData wav;
        std::string error;
        const std::vector<std::uint8_t> junk{'n', 'o', 'p', 'e'};
        QVERIFY(!readWav(junk, &wav, &error));
        QVERIFY(!error.empty());
        auto eightBit = wavBytes({0.5f}, 1, 16, false, 48000);
        eightBit[34] = 8; // bits per sample
        QVERIFY(!readWav(eightBit, &wav, &error));
    }

    // --- In the amp ---------------------------------------------------------------

    void ampPlaysTheChannelsIrAndMixesB()
    {
        std::vector<float> h(64, 0.0f);
        h[0] = 1.0f; // A: straight through
        std::vector<float> hb(64, 0.0f);
        hb[0] = -1.0f; // B: inverted
        auto a = IrKernel::build(wavOf(h), kRate), b = IrKernel::build(wavOf(hb), kRate);
        const auto level = [&](bool withA, bool withB, float mix) {
            Krunch amp;
            amp.prepare(kRate, 128);
            amp.setParam(channelParam(0, ChannelParam::CabMix), mix);
            if (withA)
                amp.cabinet().setKernel(Krunch::cabinetSlot(0, false), a.get());
            if (withB)
                amp.cabinet().setKernel(Krunch::cabinetSlot(0, true), b.get());
            std::vector<float> in(9600), left(9600), right(9600);
            for (std::size_t i = 0; i < in.size(); ++i)
                in[i] = 0.1f * float(std::sin(2.0 * std::numbers::pi * 220.0 * double(i) / kRate));
            float *const outs[2] = {left.data(), right.data()};
            const float *const ins[2] = {in.data(), in.data()};
            krate::AudioBlock block{outs, 9600};
            block.inputs = ins;
            amp.process(block);
            double sum = 0.0;
            for (std::size_t i = 4800; i < left.size(); ++i)
                sum += double(left[i]) * left[i];
            return std::sqrt(sum / 4800.0);
        };
        const double speaker = level(false, false, 0.5f), justA = level(true, false, 0.5f);
        QVERIFY(speaker > 0.0 && justA > 0.0);
        QVERIFY(std::fabs(justA - speaker) > 0.01 * speaker);               // A replaced the built-in speaker
        QVERIFY(level(true, true, 0.5f) < 0.01 * justA);                    // half and half of opposites: nothing
        QVERIFY(std::fabs(level(true, true, 0.0f) - justA) < 1e-3 * justA); // all A
        QVERIFY(std::fabs(level(false, true, 0.0f) - justA) < 1e-3 * justA); // B alone, whatever the mix
    }

    void kernelsAtAnotherRateAreIgnored()
    {
        std::vector<float> h(64, 0.0f);
        h[0] = 1.0f;
        auto at44 = IrKernel::build(wavOf(h), 44100.0);
        Krunch amp, plain;
        amp.prepare(kRate, 128);
        plain.prepare(kRate, 128);
        amp.cabinet().setKernel(0, at44.get());
        std::vector<float> in(2048, 0.05f), l1(2048), r1(2048), l2(2048), r2(2048);
        const float *const ins[2] = {in.data(), in.data()};
        float *const o1[2] = {l1.data(), r1.data()}, *const o2[2] = {l2.data(), r2.data()};
        krate::AudioBlock b1{o1, 2048}, b2{o2, 2048};
        b1.inputs = b2.inputs = ins;
        amp.process(b1);
        plain.process(b2);
        QCOMPARE(l1, l2); // the built-in speaker, as without
    }
};

QTEST_APPLESS_MAIN(TestCabinet)
#include "tst_cabinet.moc"
