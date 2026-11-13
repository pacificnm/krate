#include "dsp/Tuner.h"

#include <algorithm>
#include <cmath>

#include "dsp/Biquad.h"

namespace krunch {

namespace tuner {

namespace {
// Lowest string first. MIDI: E2 = 40, A2 = 45, D3 = 50, G3 = 55, B3 = 59, E4 = 64.
constexpr std::array kTunings{
    Tuning{"chromatic", "Chromatic", "Any note", 0, {}, false},
    Tuning{"standard", "Standard E", "Guitar", 6, {40, 45, 50, 55, 59, 64}, false},
    Tuning{"eb", "E♭ Standard (½ step down)", "Guitar", 6, {39, 44, 49, 54, 58, 63}, true},
    Tuning{"d", "D Standard (whole step down)", "Guitar", 6, {38, 43, 48, 53, 57, 62}, false},
    Tuning{"cs", "C♯ Standard", "Guitar", 6, {37, 42, 47, 52, 56, 61}, false},
    Tuning{"c", "C Standard", "Guitar", 6, {36, 41, 46, 51, 55, 60}, true},
    Tuning{"b", "B Standard", "Guitar", 6, {35, 40, 45, 50, 54, 59}, false},
    Tuning{"dropd", "Drop D", "Guitar", 6, {38, 45, 50, 55, 59, 64}, false},
    Tuning{"dropcs", "Drop C♯ (Drop D♭)", "Guitar", 6, {37, 44, 49, 54, 58, 63}, true},
    Tuning{"dropc", "Drop C", "Guitar", 6, {36, 43, 48, 53, 57, 62}, false},
    Tuning{"dropb", "Drop B", "Guitar", 6, {35, 42, 47, 52, 56, 61}, false},
    Tuning{"dropa", "Drop A", "Guitar", 6, {33, 40, 45, 50, 54, 59}, false},
    Tuning{"dadgad", "DADGAD", "Guitar", 6, {38, 45, 50, 55, 57, 62}, false},
    Tuning{"opend", "Open D", "Guitar", 6, {38, 45, 50, 54, 57, 62}, false},
    Tuning{"openg", "Open G", "Guitar", 6, {38, 43, 50, 55, 59, 62}, false},
    Tuning{"opene", "Open E", "Guitar", 6, {40, 47, 52, 56, 59, 64}, false},
    Tuning{"openc", "Open C", "Guitar", 6, {36, 43, 48, 55, 60, 64}, false},
    Tuning{"7standard", "7-String Standard (B)", "7- and 8-string", 7, {35, 40, 45, 50, 55, 59, 64}, false},
    Tuning{"7dropa", "7-String Drop A", "7- and 8-string", 7, {33, 40, 45, 50, 55, 59, 64}, false},
    Tuning{"8standard", "8-String Standard (F♯)", "7- and 8-string", 8, {30, 35, 40, 45, 50, 55, 59, 64}, false},
    Tuning{"bass4", "Bass Standard (4)", "Bass", 4, {28, 33, 38, 43}, false},
    Tuning{"bass4dropd", "Bass Drop D (4)", "Bass", 4, {26, 33, 38, 43}, false},
    Tuning{"bass4eb", "Bass E♭ Standard (4)", "Bass", 4, {27, 32, 37, 42}, true},
    Tuning{"bass5", "Bass Standard (5)", "Bass", 5, {23, 28, 33, 38, 43}, false},
};
} // namespace

std::span<const Tuning> tunings() noexcept { return kTunings; }

const Tuning &tuning(std::string_view id) noexcept
{
    for (const Tuning &t : kTunings)
        if (id == t.id)
            return t;
    return kTunings[1];
}

double noteHz(double note, double referenceHz) noexcept { return referenceHz * std::exp2((note - 69.0) / 12.0); }

double noteOf(double hz, double referenceHz) noexcept { return 69.0 + 12.0 * std::log2(hz / referenceHz); }

std::string noteName(int note, bool flats)
{
    static constexpr std::array<const char *, 12> sharp{"C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"};
    static constexpr std::array<const char *, 12> flat{"C", "D♭", "D", "E♭", "E", "F", "G♭", "G", "A♭", "A", "B♭", "B"};
    const int pc = ((note % 12) + 12) % 12;
    return (flats ? flat : sharp)[std::size_t(pc)];
}

Reading read(const Tuning &tuning, double hz, double referenceHz) noexcept
{
    Reading r;
    if (hz <= 0.0)
        return r;
    const double played = noteOf(hz, referenceHz);
    if (tuning.count == 0) {
        r.note = int(std::lround(played));
    } else {
        double best = 1e9;
        for (int s = 0; s < tuning.count; ++s) {
            const double off = std::fabs(played - tuning.notes[std::size_t(s)]);
            if (off < best) {
                best = off;
                r.string = s;
            }
        }
        r.note = tuning.notes[std::size_t(r.string)];
    }
    r.cents = (played - r.note) * 100.0;
    return r;
}

} // namespace tuner

namespace {
constexpr double kAnalysedRate = 11000.0; // about: decimate down to at least this
constexpr double kWindowSeconds = 0.08;
constexpr std::size_t kPreroll = 64;      // decimated samples
constexpr double kThreshold = 0.2;        // YIN's: the first dip under it is the period
constexpr double kSilence = 0.001;        // RMS, -60 dBFS: nothing playing

// Parabola through (−1, a), (0, b), (1, c): where its extremum is, −0.5..0.5.
double vertex(double a, double b, double c)
{
    const double den = a - 2.0 * b + c;
    return std::fabs(den) < 1e-20 ? 0.0 : std::clamp(0.5 * (a - c) / den, -0.5, 0.5);
}
} // namespace

void PitchDetector::prepare(double sampleRate)
{
    m_rate = sampleRate;
    m_decimation = std::max<std::size_t>(1, std::size_t(sampleRate / kAnalysedRate));
    const double fs = sampleRate / double(m_decimation);
    m_maxLag = std::size_t(std::ceil(fs / kMinHz)) + 2;
    m_minLag = std::max<std::size_t>(2, std::size_t(fs / kMaxHz));
    m_window = std::max(m_maxLag, std::size_t(kWindowSeconds * fs));
    m_preroll = kPreroll;
    const std::size_t decimated = m_preroll + m_window + m_maxLag;
    m_needed = decimated * m_decimation;
    m_full.assign(m_needed, 0.0f);
    m_low.assign(decimated, 0.0f);
    m_diff.assign(m_maxLag + 1, 0.0);
}

PitchDetector::Result PitchDetector::detect(std::span<const float> samples)
{
    Result result;
    if (m_needed == 0 || samples.size() < m_needed)
        return result;
    samples = samples.last(m_needed);

    // Lowpass (4th-order Butterworth at a quarter of the analysed rate) and decimate.
    const double fs = m_rate / double(m_decimation);
    krate::Biquad lp1, lp2;
    lp1.setLowpass(m_rate, 0.25 * fs, 0.5412);
    lp2.setLowpass(m_rate, 0.25 * fs, 1.3066);
    for (std::size_t n = 0; n < m_needed; ++n)
        m_full[n] = lp2.process(lp1.process(samples[n]));
    for (std::size_t k = 0; k < m_low.size(); ++k)
        m_low[k] = m_full[k * m_decimation];

    const float *y = m_low.data() + m_preroll;
    double energy = 0.0;
    for (std::size_t j = 0; j < m_window; ++j)
        energy += double(y[j]) * y[j];
    result.level = std::sqrt(energy / double(m_window));
    if (result.level < kSilence)
        return result;

    // YIN: the difference function, cumulative-mean normalised.
    m_diff[0] = 1.0;
    double running = 0.0;
    for (std::size_t tau = 1; tau <= m_maxLag; ++tau) {
        double d = 0.0;
        for (std::size_t j = 0; j < m_window; ++j) {
            const double e = double(y[j]) - y[j + tau];
            d += e * e;
        }
        running += d;
        m_diff[tau] = running > 0.0 ? d * double(tau) / running : 1.0;
    }
    std::size_t tau = 0;
    for (std::size_t t = m_minLag; t < m_maxLag; ++t)
        if (m_diff[t] < kThreshold) {
            while (t + 1 < m_maxLag && m_diff[t + 1] < m_diff[t])
                ++t;
            tau = t;
            break;
        }
    if (tau == 0)
        return result;
    result.clarity = std::clamp(1.0 - m_diff[tau], 0.0, 1.0);
    const double coarse = (double(tau) + vertex(m_diff[tau - 1], m_diff[tau], m_diff[tau + 1])) * double(m_decimation);

    // Refined at the full rate: the plain difference near that lag.
    const float *x = m_full.data() + m_preroll * m_decimation;
    const std::size_t window = m_window * m_decimation;
    const std::size_t top = m_maxLag * m_decimation - 1; // x[window + top] is in the buffer
    const auto diff = [&](std::size_t lag) {
        double d = 0.0;
        for (std::size_t j = 0; j < window; ++j) {
            const double e = double(x[j]) - x[j + lag];
            d += e * e;
        }
        return d;
    };
    const std::size_t centre = std::size_t(std::lround(coarse));
    const std::size_t from = std::max<std::size_t>(2, centre > m_decimation ? centre - m_decimation : 0);
    const std::size_t to = std::min(top - 1, centre + m_decimation);
    std::size_t best = from;
    double bestD = 1e300, before = 0.0, after = 0.0;
    std::vector<double> d(to - from + 1);
    for (std::size_t lag = from; lag <= to; ++lag)
        d[lag - from] = diff(lag);
    for (std::size_t lag = from; lag <= to; ++lag)
        if (d[lag - from] < bestD) {
            bestD = d[lag - from];
            best = lag;
        }
    before = best > from ? d[best - 1 - from] : diff(best - 1);
    after = best < to ? d[best + 1 - from] : diff(best + 1);
    const double period = double(best) + vertex(before, bestD, after);
    result.hz = m_rate / period;
    if (result.hz < kMinHz * 0.97 || result.hz > kMaxHz * 1.03)
        result.hz = 0.0;
    return result;
}

void TunerTap::write(const float *in, std::uint32_t frames) noexcept
{
    const bool open = m_open.load(std::memory_order_relaxed);
    if (open && !m_wasOpen)
        m_written.store(0, std::memory_order_relaxed);
    m_wasOpen = open;
    if (!open)
        return;
    std::uint64_t at = m_written.load(std::memory_order_relaxed);
    for (std::uint32_t n = 0; n < frames; ++n, ++at)
        m_ring[std::size_t(at % kSize)].store(in ? in[n] : 0.0f, std::memory_order_relaxed);
    m_written.store(at, std::memory_order_release);
}

bool TunerTap::read(std::span<float> out) const noexcept
{
    const std::uint64_t written = m_written.load(std::memory_order_acquire);
    if (out.size() > kSize / 2 || written < out.size())
        return false;
    const std::uint64_t start = written - out.size();
    for (std::size_t n = 0; n < out.size(); ++n)
        out[n] = m_ring[std::size_t((start + n) % kSize)].load(std::memory_order_relaxed);
    return true;
}

} // namespace krunch
