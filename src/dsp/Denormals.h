#pragma once

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace k303 {

// Enables flush-to-zero / denormals-are-zero for the current scope and restores
// the previous mode on exit. Put one at the top of every audio callback.
class ScopedFlushDenormals
{
public:
#if defined(__SSE__) || defined(__x86_64__)
    ScopedFlushDenormals() noexcept : m_saved(_mm_getcsr())
    {
        _mm_setcsr(m_saved | 0x8040); // FTZ (bit 15) | DAZ (bit 6)
    }
    ~ScopedFlushDenormals() { _mm_setcsr(m_saved); }

private:
    unsigned int m_saved;
#else
    ScopedFlushDenormals() noexcept = default;
#endif

public:
    ScopedFlushDenormals(const ScopedFlushDenormals &) = delete;
    ScopedFlushDenormals &operator=(const ScopedFlushDenormals &) = delete;
};

} // namespace k303
