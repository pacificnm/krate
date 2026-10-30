#pragma once

#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dsp/Fft.h"

namespace krate {
struct WavData;
}

namespace krunch {

using krate::WavData;

// An impulse response, ready for the Cabinet at one sample rate: resampled,
// level-normalised and split into the direct head (the first kBlock taps) and
// the spectra of the following kBlock-long partitions. Built off the audio
// thread (it allocates); immutable afterwards.
class IrKernel
{
public:
    static constexpr std::size_t kBlock = 128;
    static constexpr std::size_t kFftSize = 2 * kBlock;
    static constexpr std::size_t kMaxPartitions = 512; // 65536 taps: 1.4 s at 48 kHz

    // nullptr (with *error) if the WAV holds nothing usable.
    static std::unique_ptr<IrKernel> build(const WavData &wav, double sampleRate, std::string *error = nullptr);

    double sampleRate() const noexcept { return m_rate; }
    int sides() const noexcept { return m_sides; } // 1 mono, 2 stereo
    std::size_t partitions() const noexcept { return m_partitions; }
    std::size_t taps() const noexcept { return m_taps; }

private:
    friend class Cabinet;
    struct Side
    {
        std::array<float, kBlock> head{};                // taps 0..kBlock-1
        std::vector<std::complex<float>> tail;           // partitions 1..: kFftSize bins each
    };

    double m_rate = 0.0;
    int m_sides = 1;
    std::size_t m_partitions = 1;
    std::size_t m_taps = 0;
    std::array<Side, 2> m_side;
};

// The speaker cabinet: convolves the power amp's output with impulse responses,
// with no delay (docs/DSP.md#cabinet). The input's history is shared by every
// slot, so a slot (a channel's IR A or B) can start playing at any moment and
// be right at once. Uniformly partitioned: the first kBlock taps directly, the
// rest by FFT each kBlock samples.
//
// Kernels are handed over with setKernel() from any thread; the caller keeps
// them alive until retired() says the audio thread can no longer see them.
class Cabinet
{
public:
    static constexpr int kSlots = 8;
    static constexpr std::size_t kBlock = IrKernel::kBlock;

    Cabinet();

    void prepare() noexcept { reset(); }
    void reset() noexcept;

    // Any thread. nullptr empties the slot.
    void setKernel(int slot, const IrKernel *kernel) noexcept { m_kernels[std::size_t(slot)].store(kernel, std::memory_order_release); }
    const IrKernel *kernel(int slot) const noexcept { return m_kernels[std::size_t(slot)].load(std::memory_order_acquire); }
    // Counts processed blocks: a kernel replaced before this count moved by two is no longer in use.
    std::uint64_t blocksProcessed() const noexcept { return m_processed.load(std::memory_order_acquire); }

    // Audio thread. Adds weights[slot][n] times slot's output to left/right
    // (a mono IR on both sides); slots with no weights (nullptr) or no kernel
    // are skipped. frames <= the weights' length.
    void process(const float *in, std::uint32_t frames, const float *const *weights, float *left, float *right) noexcept;

private:
    struct SlotState
    {
        const IrKernel *kernel = nullptr; // the one the tail was computed for
        std::int64_t tailBlock = -1;
        std::array<std::array<float, kBlock>, 2> tail{};
    };

    void computeTail(int slot, const IrKernel &kernel) noexcept;

    Fft m_fft{IrKernel::kFftSize};
    std::array<std::atomic<const IrKernel *>, kSlots> m_kernels{};
    std::array<SlotState, kSlots> m_slots;

    std::array<float, 2 * kBlock> m_history{}; // the last kBlock inputs, doubled for a contiguous window
    std::size_t m_historyPos = 0;
    std::array<float, kBlock> m_previousBlock{}, m_block{};
    std::size_t m_pos = 0;           // within the current block
    std::int64_t m_blockIndex = 0;   // blocks completed
    std::vector<std::complex<float>> m_spectra; // the inputs' spectra, newest at m_spectrum (ring of kMaxPartitions)
    std::size_t m_spectrum = 0;
    std::array<std::complex<float>, IrKernel::kFftSize> m_work{}, m_accumulator{};
    std::atomic<std::uint64_t> m_processed{0};
};

} // namespace krunch
