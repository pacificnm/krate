#include "dsp/Fft.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace krunch {

Fft::Fft(std::size_t size)
    : m_size(size)
    , m_reversed(size)
    , m_twiddles(size / 2)
{
    std::size_t bits = 0;
    while ((std::size_t(1) << bits) < size)
        ++bits;
    for (std::size_t i = 0; i < size; ++i) {
        std::size_t r = 0;
        for (std::size_t b = 0; b < bits; ++b)
            r |= ((i >> b) & 1u) << (bits - 1 - b);
        m_reversed[i] = r;
    }
    for (std::size_t k = 0; k < size / 2; ++k)
        m_twiddles[k] = std::polar(1.0f, float(-2.0 * std::numbers::pi * double(k) / double(size)));
}

void Fft::transform(std::complex<float> *data, bool inverse) const noexcept
{
    for (std::size_t i = 0; i < m_size; ++i)
        if (i < m_reversed[i])
            std::swap(data[i], data[m_reversed[i]]);
    for (std::size_t length = 2; length <= m_size; length <<= 1) {
        const std::size_t half = length / 2, stride = m_size / length;
        for (std::size_t start = 0; start < m_size; start += length)
            for (std::size_t k = 0; k < half; ++k) {
                std::complex<float> w = m_twiddles[k * stride];
                if (inverse)
                    w = std::conj(w);
                const std::complex<float> odd = data[start + k + half] * w;
                data[start + k + half] = data[start + k] - odd;
                data[start + k] += odd;
            }
    }
}

} // namespace krunch
