#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace krunch {

// In-place radix-2 complex FFT of one fixed size (a power of two). Tables are
// built in the constructor; transforms don't allocate.
class Fft
{
public:
    explicit Fft(std::size_t size);

    std::size_t size() const noexcept { return m_size; }
    void forward(std::complex<float> *data) const noexcept { transform(data, false); }
    // Unscaled: the result is size() times the input of forward().
    void inverse(std::complex<float> *data) const noexcept { transform(data, true); }

private:
    void transform(std::complex<float> *data, bool inverse) const noexcept;

    std::size_t m_size;
    std::vector<std::size_t> m_reversed;      // bit-reversal permutation
    std::vector<std::complex<float>> m_twiddles; // e^(-2 pi i k / size), k < size / 2
};

} // namespace krunch
