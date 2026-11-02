#include "dsp/Capture.h"

#include <cmath>
#include <exception>
#include <filesystem>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace krunch {

CaptureModel::~CaptureModel() = default;

std::unique_ptr<CaptureModel> CaptureModel::load(const std::string &path, double sampleRate, int maxFrames, std::string *error)
{
    std::unique_ptr<CaptureModel> model(new CaptureModel);
    try {
        model->m_dsp = nam::get_dsp(std::filesystem::path(path));
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return nullptr;
    }
    if (!model->m_dsp || model->m_dsp->NumInputChannels() != 1 || model->m_dsp->NumOutputChannels() != 1) {
        if (error)
            *error = model->m_dsp ? "not a mono model" : "not a model";
        return nullptr;
    }
    model->m_rate = sampleRate;
    model->m_expected = model->m_dsp->GetExpectedSampleRate();
    model->m_dsp->Reset(sampleRate, maxFrames); // readies and prewarms it
    model->m_out.assign(std::size_t(maxFrames), 0.0f);
    if (model->m_dsp->HasLoudness()) // captures play at a common level, as NAM's own player does
        model->m_outputGain = float(std::pow(10.0, (-18.0 - model->m_dsp->GetLoudness()) / 20.0));
    return model;
}

void CaptureModel::process(float *buffer, std::uint32_t frames) noexcept
{
    float *in[1] = {buffer};
    float *out[1] = {m_out.data()};
    m_dsp->process(in, out, int(frames));
    for (std::uint32_t n = 0; n < frames; ++n)
        buffer[n] = m_out[n] * m_outputGain;
}

} // namespace krunch
