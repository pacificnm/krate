#include "dsp/Cabinet.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "dsp/Wav.h"

namespace krunch {

namespace {

// Windowed-sinc resampling (Blackman, 32 zero crossings each side at the
// lower of the two rates), for impulse responses recorded at another rate.
std::vector<float> resample(const std::vector<float> &in, double from, double to)
{
    if (from == to)
        return in;
    const double ratio = to / from;
    const double cutoff = std::min(1.0, ratio); // of the input's Nyquist
    const double halfWidth = 32.0 / cutoff;     // in input samples
    const auto length = std::size_t(std::ceil(double(in.size()) * ratio));
    std::vector<float> out(length);
    for (std::size_t n = 0; n < length; ++n) {
        const double t = double(n) / ratio;
        const auto first = std::size_t(std::max(0.0, std::ceil(t - halfWidth)));
        const auto last = std::min(in.size() - 1, std::size_t(std::floor(t + halfWidth)));
        double sum = 0.0;
        for (std::size_t k = first; k <= last; ++k) {
            const double d = t - double(k);
            const double x = std::numbers::pi * d * cutoff;
            const double sinc = d == 0.0 ? 1.0 : std::sin(x) / x;
            const double w = 0.42 + 0.5 * std::cos(std::numbers::pi * d / halfWidth) + 0.08 * std::cos(2.0 * std::numbers::pi * d / halfWidth);
            sum += double(in[k]) * sinc * cutoff * w;
        }
        out[n] = float(sum);
    }
    return out;
}

} // namespace

std::unique_ptr<IrKernel> IrKernel::build(const WavData &wav, double sampleRate, std::string *error)
{
    if (wav.left.empty() || wav.rate <= 0 || sampleRate <= 0.0) {
        if (error)
            *error = "empty impulse response";
        return nullptr;
    }
    auto kernel = std::make_unique<IrKernel>();
    kernel->m_rate = sampleRate;
    kernel->m_sides = wav.right.empty() ? 1 : 2;

    std::array<std::vector<float>, 2> sides;
    sides[0] = resample(wav.left, wav.rate, sampleRate);
    if (kernel->m_sides == 2)
        sides[1] = resample(wav.right, wav.rate, sampleRate);

    // Longer than we hold: cut, fading the last 10 ms out.
    const std::size_t maxTaps = kMaxPartitions * kBlock;
    for (int s = 0; s < kernel->m_sides; ++s) {
        auto &h = sides[std::size_t(s)];
        if (h.size() > maxTaps) {
            h.resize(maxTaps);
            const auto fade = std::min(maxTaps, std::size_t(0.010 * sampleRate));
            for (std::size_t i = 0; i < fade; ++i)
                h[maxTaps - 1 - i] *= float(i) / float(fade);
        }
    }

    // Level: unit energy (averaged over the sides), so every IR plays about
    // as loud as the next whatever level it was recorded at, then about as
    // loud as the built-in speaker.
    constexpr double kLevelDb = -6.5;
    double energy = 0.0;
    for (int s = 0; s < kernel->m_sides; ++s)
        for (float v : sides[std::size_t(s)])
            energy += double(v) * v;
    energy /= kernel->m_sides;
    if (energy <= 1e-20) {
        if (error)
            *error = "the impulse response is silent";
        return nullptr;
    }
    const float scale = float(std::pow(10.0, kLevelDb / 20.0) / std::sqrt(energy));

    const std::size_t taps = sides[0].size();
    kernel->m_taps = taps;
    kernel->m_partitions = std::max<std::size_t>(1, (taps + kBlock - 1) / kBlock);
    const Fft fft(kFftSize);
    for (int s = 0; s < kernel->m_sides; ++s) {
        const auto &h = sides[std::size_t(s)];
        Side &side = kernel->m_side[std::size_t(s)];
        for (std::size_t i = 0; i < kBlock; ++i)
            side.head[i] = i < h.size() ? h[i] * scale : 0.0f;
        side.tail.assign((kernel->m_partitions - 1) * kFftSize, {});
        for (std::size_t p = 1; p < kernel->m_partitions; ++p) {
            std::complex<float> *bins = side.tail.data() + (p - 1) * kFftSize;
            for (std::size_t i = 0; i < kBlock; ++i) {
                const std::size_t tap = p * kBlock + i;
                bins[i] = tap < h.size() ? h[tap] * scale : 0.0f;
            }
            fft.forward(bins);
        }
    }
    return kernel;
}

Cabinet::Cabinet()
    : m_spectra(IrKernel::kMaxPartitions * IrKernel::kFftSize)
{
    for (auto &k : m_kernels)
        k.store(nullptr, std::memory_order_relaxed);
}

void Cabinet::reset() noexcept
{
    m_history.fill(0.0f);
    m_historyPos = 0;
    m_previousBlock.fill(0.0f);
    m_block.fill(0.0f);
    m_pos = 0;
    m_blockIndex = 0;
    std::fill(m_spectra.begin(), m_spectra.end(), std::complex<float>());
    m_spectrum = 0;
    for (SlotState &s : m_slots)
        s.tailBlock = -1;
}

void Cabinet::computeTail(int slot, const IrKernel &kernel) noexcept
{
    SlotState &state = m_slots[std::size_t(slot)];
    state.kernel = &kernel;
    state.tailBlock = m_blockIndex;
    constexpr std::size_t N = IrKernel::kFftSize;
    for (int s = 0; s < kernel.m_sides; ++s) {
        m_accumulator.fill({});
        // Output block j gets partition p applied to the input spectrum of block j - p.
        const IrKernel::Side &side = kernel.m_side[std::size_t(s)];
        for (std::size_t p = 1; p < kernel.m_partitions; ++p) {
            const std::size_t ring = (m_spectrum + IrKernel::kMaxPartitions - (p - 1)) % IrKernel::kMaxPartitions;
            const std::complex<float> *x = m_spectra.data() + ring * N;
            const std::complex<float> *h = side.tail.data() + (p - 1) * N;
            for (std::size_t k = 0; k < N; ++k)
                m_accumulator[k] += x[k] * h[k];
        }
        m_fft.inverse(m_accumulator.data());
        auto &tail = state.tail[std::size_t(s)];
        for (std::size_t i = 0; i < kBlock; ++i) // overlap-save: the second half is valid
            tail[i] = m_accumulator[kBlock + i].real() / float(N);
    }
}

void Cabinet::process(const float *in, std::uint32_t frames, const float *const *weights, float *left, float *right) noexcept
{
    std::array<const IrKernel *, kSlots> kernels{};
    for (int s = 0; s < kSlots; ++s)
        kernels[std::size_t(s)] = weights[s] ? kernel(s) : nullptr;
    const auto ensureTails = [&] {
        for (int s = 0; s < kSlots; ++s)
            if (const IrKernel *k = kernels[std::size_t(s)];
                k && (m_slots[std::size_t(s)].kernel != k || m_slots[std::size_t(s)].tailBlock != m_blockIndex))
                computeTail(s, *k);
    };
    ensureTails();

    for (std::uint32_t n = 0; n < frames; ++n) {
        const float x = in[n];
        m_historyPos = m_historyPos == 0 ? kBlock - 1 : m_historyPos - 1;
        m_history[m_historyPos] = x;
        m_history[m_historyPos + kBlock] = x;
        m_block[m_pos] = x;
        // window[t] is the input t samples ago.
        const float *window = m_history.data() + m_historyPos;

        for (int s = 0; s < kSlots; ++s) {
            const IrKernel *k = kernels[std::size_t(s)];
            if (!k)
                continue;
            const float w = weights[s][n];
            const SlotState &state = m_slots[std::size_t(s)];
            for (int side = 0; side < k->m_sides; ++side) {
                const auto &head = k->m_side[std::size_t(side)].head;
                float y = state.tail[std::size_t(side)][m_pos];
                for (std::size_t t = 0; t < kBlock; ++t)
                    y += head[t] * window[t];
                if (k->m_sides == 1) {
                    left[n] += w * y;
                    right[n] += w * y;
                } else {
                    (side == 0 ? left : right)[n] += w * y;
                }
            }
        }

        if (++m_pos == kBlock) { // a block complete: its spectrum, then the next tails
            m_pos = 0;
            for (std::size_t i = 0; i < kBlock; ++i) {
                m_work[i] = m_previousBlock[i];
                m_work[kBlock + i] = m_block[i];
            }
            m_fft.forward(m_work.data());
            m_spectrum = (m_spectrum + 1) % IrKernel::kMaxPartitions;
            std::copy(m_work.begin(), m_work.end(), m_spectra.begin() + std::ptrdiff_t(m_spectrum * IrKernel::kFftSize));
            m_previousBlock = m_block;
            ++m_blockIndex;
            ensureTails();
        }
    }
    m_processed.fetch_add(1, std::memory_order_release);
}

} // namespace krunch
