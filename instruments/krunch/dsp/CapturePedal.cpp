#include "dsp/CapturePedal.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

constexpr double kTiltHz = 800.0;
constexpr double kTiltDb = 6.0; // TONE: the treble up and the bass down by this much at 10 (and back at 0)

double inputDb(float v) { return -18.0 + 36.0 * double(v); }
double levelDb(float v) { return -24.0 + 36.0 * double(v); }
std::string db(double v) { return std::format("{:+.1f} dB", std::round(v * 10.0) / 10.0 + 0.0); }
std::string inputText(float v) { return db(inputDb(v)); }
std::string levelText(float v) { return db(levelDb(v)); }
std::string toneText(float v) { return db(kTiltDb * (2.0 * double(v) - 1.0)); }
float gain(double db) { return float(std::pow(10.0, db / 20.0)); }

} // namespace

const PedalInfo CapturePedal::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"input", "INPUT", 0.5f, &inputText},
        {"tone", "TONE", 0.5f, &toneText},
        {"level", "LEVEL", 2.0f / 3.0f, &levelText},
    };
    PedalInfo info{"capture", "Capture", "#30343B", knobs, false};
    info.capture = true;
    return info;
}();

CapturePedal::~CapturePedal() = default;

void CapturePedal::setModel(std::shared_ptr<CaptureModel> model, std::uint64_t blocks)
{
    m_model.store(model.get(), std::memory_order_release);
    if (m_live)
        m_retired.push_back({std::move(m_live), blocks});
    m_live = std::move(model);
}

void CapturePedal::collect(std::uint64_t blocks)
{
    std::erase_if(m_retired, [blocks](const Retired &r) { return blocks >= r.after + 2; });
}

void CapturePedal::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tilt.setCutoff(sampleRate, kTiltHz);
}

void CapturePedal::resetEffect() noexcept
{
    m_tilt.reset();
    m_input = -1.0f;
    m_level = -1.0f;
}

void CapturePedal::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    CaptureModel *model = m_model.load(std::memory_order_acquire);
    if (!model || model->sampleRate() != m_rate)
        return; // none yet, or readied for another rate (a new one is coming): the guitar as it is

    const float inputTarget = gain(inputDb(knob(Input)));
    const float levelTarget = gain(levelDb(knob(Level)));
    if (m_input < 0.0f)
        m_input = inputTarget, m_level = levelTarget;
    const float inputStep = (inputTarget - m_input) / float(frames);
    const float levelStep = (levelTarget - m_level) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_input += inputStep;
        buffer[n] *= m_input;
    }
    m_input = inputTarget;

    model->process(buffer, frames);

    const float tilt = gain(kTiltDb * (2.0 * double(knob(Tone)) - 1.0)); // treble up by this, bass down by it
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float low = m_tilt.lowpass(buffer[n]);
        m_level += levelStep;
        buffer[n] = (low / tilt + (buffer[n] - low) * tilt) * m_level;
    }
    m_level = levelTarget;
}

} // namespace krunch
