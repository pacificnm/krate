#include <QtTest>

#include <cmath>
#include <vector>

#include "dsp/Krunch.h"

using namespace krunch;

namespace {

// One block of the amp with the two interface inputs given (nullptr inputs: none).
struct Run
{
    std::vector<float> left, right;

    Run(Krunch &amp, const std::vector<float> &in1, const std::vector<float> &in2, bool withInputs = true)
        : left(in1.size(), 7.0f)
        , right(in1.size(), 7.0f)
    {
        float *const outputs[2] = {left.data(), right.data()};
        const float *const inputs[2] = {in1.data(), in2.data()};
        krate::AudioBlock block{outputs, std::uint32_t(in1.size())};
        block.inputs = withInputs ? inputs : nullptr;
        amp.process(block);
    }
};

std::vector<float> sine(std::size_t frames, float amplitude)
{
    std::vector<float> s(frames);
    for (std::size_t i = 0; i < frames; ++i)
        s[i] = amplitude * float(std::sin(2.0 * M_PI * 110.0 * double(i) / 48000.0));
    return s;
}

} // namespace

class TestKrunch : public QObject
{
    Q_OBJECT

private slots:
    void playsTheInterfaceInputs()
    {
        Krunch amp;
        QCOMPARE(amp.inputChannels(), 2);
        QCOMPARE(amp.outputChannels(), 2);
    }

    void unityByDefaultOnBothSides()
    {
        Krunch amp;
        amp.prepare(48000.0, 256);
        const auto in = sine(256, 0.5f);
        const Run run(amp, in, std::vector<float>(256, 0.0f));
        for (std::size_t i = 0; i < in.size(); ++i) {
            QCOMPARE(run.left[i], in[i]);
            QCOMPARE(run.right[i], in[i]);
        }
    }

    void inputKnobChoosesTheInput()
    {
        Krunch amp;
        amp.prepare(48000.0, 64);
        const std::vector<float> one(64, 0.25f), two(64, -0.5f);
        amp.setParam(int(ParamId::Input), 1.0f);
        const Run run(amp, one, two);
        QCOMPARE(run.left[10], -0.5f);
        amp.setParam(int(ParamId::Input), 0.0f);
        const Run back(amp, one, two);
        QCOMPARE(back.left[10], 0.25f);
    }

    void levelScalesAndBottomIsSilence()
    {
        Krunch amp;
        amp.prepare(48000.0, 64);
        const std::vector<float> in(64, 0.5f), none(64, 0.0f);
        Run(amp, in, none); // settle at 0 dB
        amp.setParam(int(ParamId::Level), 54.0f / 72.0f); // -6 dB
        Run(amp, in, none); // the ramp
        const Run quieter(amp, in, none);
        QVERIFY(std::fabs(quieter.left[0] - 0.5f * float(std::pow(10.0, -6.0 / 20.0))) < 1e-6f);
        amp.setParam(int(ParamId::Level), 0.0f);
        Run(amp, in, none);
        const Run silent(amp, in, none);
        QCOMPARE(silent.left[0], 0.0f);
        QCOMPARE(silent.right[63], 0.0f);
    }

    void levelChangesRampWithoutJumps()
    {
        Krunch amp;
        amp.prepare(48000.0, 256);
        const std::vector<float> in(256, 1.0f), none(256, 0.0f);
        Run(amp, in, none);
        amp.setParam(int(ParamId::Level), 0.0f);
        const Run ramp(amp, in, none);
        float biggestStep = 0.0f;
        for (std::size_t i = 1; i < ramp.left.size(); ++i)
            biggestStep = std::max(biggestStep, std::fabs(ramp.left[i] - ramp.left[i - 1]));
        QVERIFY(biggestStep < 0.01f);
        QVERIFY(ramp.left.back() < 1e-6f);
    }

    void noInputsIsSilence()
    {
        Krunch amp;
        amp.prepare(48000.0, 64);
        const Run run(amp, std::vector<float>(64, 0.5f), std::vector<float>(64, 0.5f), false);
        QCOMPARE(run.left[0], 0.0f);
        QCOMPARE(run.right[63], 0.0f);
    }

    void peaksForTheMeters()
    {
        Krunch amp;
        amp.prepare(48000.0, 64);
        std::vector<float> in(64, 0.0f);
        in[5] = -0.8f;
        amp.setParam(int(ParamId::Level), 54.0f / 72.0f);
        Run(amp, in, std::vector<float>(64, 0.0f));
        Run(amp, in, std::vector<float>(64, 0.0f));
        QCOMPARE(amp.takeInputPeak(), 0.8f);
        QVERIFY(std::fabs(amp.takeOutputPeak() - 0.8f * float(std::pow(10.0, -6.0 / 20.0))) < 1e-6f);
        QCOMPARE(amp.takeInputPeak(), 0.0f); // taken
    }

    void displayText()
    {
        QCOMPARE(QString::fromStdString(format::input(0.0f)), QStringLiteral("In 1"));
        QCOMPARE(QString::fromStdString(format::input(1.0f)), QStringLiteral("In 2"));
        QCOMPARE(QString::fromStdString(format::level(60.0f / 72.0f)), QStringLiteral("+0.0 dB"));
        QCOMPARE(QString::fromStdString(format::level(0.0f)), QStringLiteral("-inf dB"));
    }
};

QTEST_APPLESS_MAIN(TestKrunch)
#include "tst_krunch.moc"
