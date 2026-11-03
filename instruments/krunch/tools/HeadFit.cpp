#include "HeadFit.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <numeric>
#include <thread>

#include "dsp/Capture.h"
#include "dsp/Krunch.h"

namespace krunch::fit {

namespace {

constexpr double kPi = 3.14159265358979323846;

// --- Material

double rms(const float *x, std::size_t n)
{
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        s += double(x[i]) * x[i];
    return n ? std::sqrt(s / double(n)) : 0.0;
}

// --- Rendering: one channel, every knob at noon, the cabinet off.

void setNoon(Krunch &amp)
{
    const auto set = [&amp](const char *key, float v) {
        for (std::size_t p = 0; p < kParams.size(); ++p)
            if (kParams[p].key == key)
                amp.setParam(int(p), v);
    };
    set("channel", 0.0f);
    set("master", 0.5f);
    set("sag", 0.5f);
    set("resonance", 0.5f);
    set("power_tube", 0.0f);
    for (const char *k : {"ch1_gain", "ch1_bass", "ch1_mid", "ch1_treble", "ch1_presence"})
        set(k, 0.5f);
    set("ch1_gate", 0.0f);
    set("ch1_loop", 0.0f);
    set("ch1_cabinet", 0.0f);
}

std::vector<float> run(Krunch &amp, const Window &window, int leadIn)
{
    amp.prepare(kRate, Krunch::kBlock);
    setNoon(amp);
    const std::size_t n = window.audio.size();
    std::vector<float> out(n), right(Krunch::kBlock);
    for (std::size_t at = 0; at < n; at += Krunch::kBlock) {
        const auto frames = std::uint32_t(std::min<std::size_t>(Krunch::kBlock, n - at));
        float *outputs[2] = {out.data() + at, right.data()};
        const float *inputs[2] = {window.audio.data() + at, window.audio.data() + at};
        amp.process(krate::AudioBlock{outputs, frames, nullptr, 0, nullptr, inputs});
    }
    out.erase(out.begin(), out.begin() + std::min<std::ptrdiff_t>(leadIn, std::ptrdiff_t(n)));
    return out;
}

// --- Analysis

void fft(std::vector<std::complex<double>> &a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Power in bands (edges in Hz) for each frame of `size`, every `hop`, Hann windowed; dB.
std::vector<float> bands(const std::vector<float> &x, std::size_t size, std::size_t hop, const std::vector<double> &edges)
{
    std::vector<float> out;
    std::vector<double> window(size);
    for (std::size_t i = 0; i < size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2.0 * kPi * double(i) / double(size));
    std::vector<std::complex<double>> buf(size);
    const std::size_t nb = edges.size() - 1;
    for (std::size_t start = 0; start + size <= x.size(); start += hop) {
        for (std::size_t i = 0; i < size; ++i)
            buf[i] = double(x[start + i]) * window[i];
        fft(buf);
        for (std::size_t b = 0; b < nb; ++b) {
            const auto k0 = std::size_t(std::ceil(edges[b] * double(size) / kRate));
            const auto k1 = std::max(k0 + 1, std::size_t(std::ceil(edges[b + 1] * double(size) / kRate)));
            double p = 0.0;
            for (std::size_t k = k0; k < k1 && k < size / 2; ++k)
                p += std::norm(buf[k]);
            out.push_back(float(10.0 * std::log10(p + 1e-20)));
        }
    }
    return out;
}

std::vector<double> bandEdges(double from, double to, double perOctave)
{
    std::vector<double> e;
    for (double f = from; f < to * 1.0001; f *= std::pow(2.0, 1.0 / perOctave))
        e.push_back(f);
    return e;
}

const std::vector<double> &longEdges()
{
    static const std::vector<double> e = bandEdges(50.0, 16000.0, 6.0);
    return e;
}
const std::vector<double> &shortEdges()
{
    static const std::vector<double> e = bandEdges(200.0, 16000.0, 2.0);
    return e;
}

// Mean |a - b - offset| over cells, each floored at the reference's loudest minus `range` dB.
double distance(const std::vector<float> &a, const std::vector<float> &b, double offset, double range)
{
    if (a.empty() || a.size() != b.size())
        return 0.0;
    const double floor = double(*std::max_element(a.begin(), a.end())) - range;
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
        sum += std::fabs(std::max(double(a[i]), floor) - std::max(double(b[i]) + offset, floor));
    return sum / double(a.size());
}

} // namespace

Material buildMaterial(const std::vector<std::vector<float>> &takes, int windows, double seconds, double leadInSeconds)
{
    Material m;
    m.leadIn = int(leadInSeconds * kRate);
    const auto length = std::size_t(seconds * kRate);
    const auto total = std::size_t(m.leadIn) + length;
    // Every place a window could start (a quarter second apart), with its level.
    struct Spot
    {
        std::size_t take, at;
        double level;
    };
    std::vector<Spot> spots;
    for (std::size_t t = 0; t < takes.size(); ++t) {
        const auto &x = takes[t];
        for (float s : x)
            m.diPeak = std::max(m.diPeak, double(std::fabs(s)));
        for (std::size_t at = 0; at + total <= x.size(); at += std::size_t(0.25 * kRate))
            spots.push_back({t, at, rms(x.data() + at + std::size_t(m.leadIn), length)});
    }
    const double loudest = spots.empty() ? 0.0 : std::max_element(spots.begin(), spots.end(), [](auto &a, auto &b) {
                                                     return a.level < b.level;
                                                 })->level;
    std::erase_if(spots, [loudest](const Spot &s) { return s.level < loudest * 0.03; }); // playing, not silence (-30 dB)
    std::sort(spots.begin(), spots.end(), [](const Spot &a, const Spot &b) { return a.level < b.level; });
    // Evenly through the levels: soft to hard.
    for (int w = 0; w < windows && !spots.empty(); ++w) {
        const Spot &s = spots[std::size_t((double(w) + 0.5) / windows * double(spots.size()))];
        const auto &x = takes[s.take];
        m.windows.push_back({std::vector<float>(x.begin() + std::ptrdiff_t(s.at), x.begin() + std::ptrdiff_t(s.at + total)), 0.0});
    }
    // Steady tones, low to high, soft to as hard as the guitar hits.
    const double peak = m.diPeak > 0.0 ? m.diPeak : 0.5;
    for (double hz : {98.0, 220.0, 660.0})
        for (double level : {0.1, 0.35, 1.0}) {
            Window w;
            w.toneHz = hz;
            w.audio.resize(std::size_t(m.leadIn) + std::size_t(0.3 * kRate));
            for (std::size_t i = 0; i < w.audio.size(); ++i)
                w.audio[i] = float(peak * level * std::sin(2.0 * kPi * hz * double(i) / kRate));
            m.windows.push_back(std::move(w));
        }
    return m;
}

std::vector<float> renderCapture(CaptureModel &capture, const Window &window, int leadIn)
{
    Krunch amp;
    amp.setCapture(0, &capture);
    return run(amp, window, leadIn);
}

std::vector<float> renderHead(const HeadDesign &head, const Window &window, int leadIn)
{
    thread_local Krunch amp; // (a big object: one per thread, prepared afresh each time)
    amp.setCapture(0, nullptr);
    amp.setHead(0, &head);
    std::vector<float> out = run(amp, window, leadIn);
    amp.setHead(0, nullptr);
    return out;
}

Features analyse(const std::vector<float> &out, double toneHz)
{
    Features f;
    f.longBands = bands(out, 4096, 1024, longEdges());
    f.shortBands = bands(out, 512, 128, shortEdges());
    const std::size_t step = std::size_t(0.010 * kRate);
    for (std::size_t at = 0; at + step <= out.size(); at += step)
        f.envelope.push_back(float(20.0 * std::log10(rms(out.data() + at, step) + 1e-10)));
    f.powerDb = 20.0 * std::log10(rms(out.data(), out.size()) + 1e-10);
    if (toneHz > 0.0) {
        const std::size_t size = 16384;
        std::vector<std::complex<double>> buf(size);
        const std::size_t n = std::min(size, out.size());
        for (std::size_t i = 0; i < n; ++i)
            buf[i] = double(out[i]) * (0.5 - 0.5 * std::cos(2.0 * kPi * double(i) / double(n)));
        fft(buf);
        const auto level = [&](double hz) {
            const auto k = std::size_t(std::lround(hz * double(size) / kRate));
            double p = 0.0;
            for (std::size_t j = k > 3 ? k - 3 : 0; j <= k + 3 && j < size / 2; ++j)
                p = std::max(p, std::norm(buf[j]));
            return 10.0 * std::log10(p + 1e-30);
        };
        const double fundamental = level(toneHz);
        for (int h = 2; h <= 10; ++h)
            f.harmonics.push_back(float(std::max(-80.0, level(toneHz * h) - fundamental)));
    }
    return f;
}

Score compare(const std::vector<Features> &reference, const std::vector<Features> &candidate)
{
    Score s;
    // One level for the head (its output), from the guitar windows: power-averaged.
    double refPower = 0.0, candPower = 0.0;
    for (std::size_t w = 0; w < reference.size(); ++w)
        if (reference[w].harmonics.empty()) {
            refPower += std::pow(10.0, reference[w].powerDb / 10.0);
            candPower += std::pow(10.0, candidate[w].powerDb / 10.0);
        }
    s.levelDb = 10.0 * std::log10((refPower + 1e-30) / (candPower + 1e-30));
    int guitar = 0, tones = 0;
    for (std::size_t w = 0; w < reference.size(); ++w) {
        const Features &r = reference[w], &c = candidate[w];
        s.tone += distance(r.longBands, c.longBands, s.levelDb, 60.0);
        s.attack += distance(r.shortBands, c.shortBands, s.levelDb, 50.0);
        s.envelope += distance(r.envelope, c.envelope, s.levelDb, 50.0);
        if (!r.harmonics.empty()) {
            double h = 0.0;
            for (std::size_t i = 0; i < r.harmonics.size() && i < c.harmonics.size(); ++i)
                h += std::fabs(double(r.harmonics[i]) - double(c.harmonics[i]));
            s.harmonics += h / double(std::max<std::size_t>(1, r.harmonics.size()));
            ++tones;
        } else {
            ++guitar;
        }
    }
    const double windows = double(std::max<std::size_t>(1, reference.size()));
    s.tone /= windows;
    s.attack /= windows;
    s.envelope /= windows;
    s.harmonics /= double(std::max(1, tones));
    s.total = s.tone + 0.5 * s.attack + 0.5 * s.envelope + 0.5 * s.harmonics;
    (void)guitar;
    return s;
}

// --- The search space

namespace {
// A head as the space sees it: its own tubes and power amp, its output
// filters at the space's open ends when it has none (as good as off).
HeadDesign spaced(HeadDesign h)
{
    h.channelTube = false;
    h.channelPower = false;
    if (h.lowCutHz <= 0.0)
        h.lowCutHz = 30.0;
    if (h.highCutHz <= 0.0)
        h.highCutHz = 16000.0;
    return h;
}
} // namespace

Space::Space(const HeadDesign &start)
    : m_start(spaced(start))
{
    const auto add = [this](std::string name, double lo, double hi, bool log, std::function<double &(HeadDesign &)> ref) {
        m_params.push_back({std::move(name), lo, hi, log, std::move(ref)});
    };
    for (int i = 0; i < start.stages; ++i) {
        const auto s = std::size_t(i);
        const std::string n = "stage" + std::to_string(i + 1) + ".";
        add(n + "cathode_ohms", 500.0, 15000.0, true, [s](HeadDesign &h) -> double & { return h.stage[s].cathodeOhms; });
        add(n + "bypass_hz", 1.0, 3000.0, true, [s](HeadDesign &h) -> double & { return h.stage[s].bypassHz; });
        add(n + "coupling_hz", 2.0, 300.0, true, [s](HeadDesign &h) -> double & { return h.stage[s].couplingHz; });
        add(n + "rolloff_hz", 2000.0, 20000.0, true, [s](HeadDesign &h) -> double & { return h.stage[s].rolloffHz; });
        if (i >= 2)
            add(n + "into", 0.02, 1.0, true, [s](HeadDesign &h) -> double & { return h.divider[s]; });
    }
    add("gain_pot_min_db", -60.0, -6.0, false, [](HeadDesign &h) -> double & { return h.potMinDb; });
    add("pad_into_second", 0.02, 1.0, true, [](HeadDesign &h) -> double & { return h.padIntoSecond; });
    add("top_cut_hz", 3000.0, 18000.0, true, [](HeadDesign &h) -> double & { return h.topCutHz; });
    add("trim_db", -24.0, 30.0, false, [](HeadDesign &h) -> double & { return h.trimDb; });
    add("power.headroom", 0.3, 3.0, true, [](HeadDesign &h) -> double & { return h.power.headroom; });
    add("power.knee", 1.2, 5.0, false, [](HeadDesign &h) -> double & { return h.power.knee; });
    add("power.bias", 0.05, 0.8, false, [](HeadDesign &h) -> double & { return h.power.bias; });
    add("power.sag", 0.0, 2.5, false, [](HeadDesign &h) -> double & { return h.power.sag; });
    add("power.voice_hz", 80.0, 5000.0, true, [](HeadDesign &h) -> double & { return h.power.voiceHz; });
    add("power.voice_db", -8.0, 8.0, false, [](HeadDesign &h) -> double & { return h.power.voiceDb; });
    add("power.voice_q", 0.4, 3.0, true, [](HeadDesign &h) -> double & { return h.power.voiceQ; });
    add("master_offset_db", -20.0, 20.0, false, [](HeadDesign &h) -> double & { return h.masterOffsetDb; });
    add("low_cut_hz", 30.0, 200.0, true, [](HeadDesign &h) -> double & { return h.lowCutHz; });
    add("high_cut_hz", 2500.0, 16000.0, true, [](HeadDesign &h) -> double & { return h.highCutHz; });
    add("peak_hz", 150.0, 6000.0, true, [](HeadDesign &h) -> double & { return h.peakHz; });
    add("peak_db", -10.0, 10.0, false, [](HeadDesign &h) -> double & { return h.peakDb; });
    add("peak_q", 0.4, 3.0, true, [](HeadDesign &h) -> double & { return h.peakQ; });
}

std::vector<double> Space::encode(const HeadDesign &head) const
{
    HeadDesign h = spaced(head);
    std::vector<double> x;
    for (const Param &p : m_params) {
        const double v = std::clamp(p.ref(h), p.lo, p.hi);
        x.push_back(p.log ? std::log(v / p.lo) / std::log(p.hi / p.lo) : (v - p.lo) / (p.hi - p.lo));
    }
    return x;
}

HeadDesign Space::decode(const std::vector<double> &x) const
{
    HeadDesign h = m_start;
    for (std::size_t i = 0; i < m_params.size(); ++i) {
        const Param &p = m_params[i];
        const double u = std::clamp(i < x.size() ? x[i] : 0.5, 0.0, 1.0);
        p.ref(h) = p.log ? p.lo * std::pow(p.hi / p.lo, u) : p.lo + u * (p.hi - p.lo);
    }
    return h;
}

// --- The search: separable CMA-ES (Ros and Hansen, 2008)

SearchResult search(const std::function<std::vector<double>(const std::vector<std::vector<double>> &)> &evaluate,
                    std::vector<double> start, double sigma, int evaluations, std::mt19937 &rng,
                    const std::function<void(int, double)> &progress)
{
    const int n = int(start.size());
    const int lambda = std::max(8, 4 + int(3.0 * std::log(double(n))) * 2);
    const int mu = lambda / 2;
    std::vector<double> w(static_cast<std::size_t>(mu));
    for (int i = 0; i < mu; ++i)
        w[std::size_t(i)] = std::log(mu + 0.5) - std::log(i + 1.0);
    const double wsum = std::accumulate(w.begin(), w.end(), 0.0);
    for (double &v : w)
        v /= wsum;
    double w2 = 0.0;
    for (double v : w)
        w2 += v * v;
    const double mueff = 1.0 / w2;
    const double cs = (mueff + 2.0) / (n + mueff + 5.0);
    const double ds = 1.0 + 2.0 * std::max(0.0, std::sqrt((mueff - 1.0) / (n + 1.0)) - 1.0) + cs;
    const double cc = (4.0 + mueff / n) / (n + 4.0 + 2.0 * mueff / n);
    const double c1 = (n + 2.0) / 3.0 * 2.0 / ((n + 1.3) * (n + 1.3) + mueff);
    const double cmu = std::min(1.0 - c1, (n + 2.0) / 3.0 * 2.0 * (mueff - 2.0 + 1.0 / mueff) / ((n + 2.0) * (n + 2.0) + mueff));
    const double chiN = std::sqrt(double(n)) * (1.0 - 1.0 / (4.0 * n) + 1.0 / (21.0 * n * n));

    std::vector<double> m = std::move(start), C(std::size_t(n), 1.0), pc(std::size_t(n), 0.0), ps(std::size_t(n), 0.0);
    SearchResult result;
    result.best = m; // the start, as good as it is
    result.bestValue = evaluate({m}).front();
    result.evaluations = 1;
    std::normal_distribution<double> normal;
    int generation = 0;
    while (result.evaluations + lambda <= evaluations) {
        std::vector<std::vector<double>> z(static_cast<std::size_t>(lambda), std::vector<double>(static_cast<std::size_t>(n))), x = z;
        for (int k = 0; k < lambda; ++k)
            for (int i = 0; i < n; ++i) {
                z[std::size_t(k)][std::size_t(i)] = normal(rng);
                x[std::size_t(k)][std::size_t(i)] =
                    std::clamp(m[std::size_t(i)] + sigma * std::sqrt(C[std::size_t(i)]) * z[std::size_t(k)][std::size_t(i)], 0.0, 1.0);
            }
        const std::vector<double> f = evaluate(x);
        result.evaluations += lambda;
        std::vector<int> order(static_cast<std::size_t>(lambda));
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&f](int a, int b) { return f[std::size_t(a)] < f[std::size_t(b)]; });
        if (f[std::size_t(order[0])] < result.bestValue) {
            result.bestValue = f[std::size_t(order[0])];
            result.best = x[std::size_t(order[0])];
        }
        // The new mean from the best half (as clipped: what was evaluated).
        std::vector<double> old = m;
        for (int i = 0; i < n; ++i) {
            double v = 0.0;
            for (int k = 0; k < mu; ++k)
                v += w[std::size_t(k)] * x[std::size_t(order[std::size_t(k)])][std::size_t(i)];
            m[std::size_t(i)] = v;
        }
        double psNorm = 0.0;
        for (int i = 0; i < n; ++i) {
            const double step = (m[std::size_t(i)] - old[std::size_t(i)]) / sigma;
            ps[std::size_t(i)] = (1.0 - cs) * ps[std::size_t(i)] + std::sqrt(cs * (2.0 - cs) * mueff) * step / std::sqrt(C[std::size_t(i)]);
            psNorm += ps[std::size_t(i)] * ps[std::size_t(i)];
        }
        psNorm = std::sqrt(psNorm);
        const bool hsig = psNorm / std::sqrt(1.0 - std::pow(1.0 - cs, 2.0 * (generation + 1))) / chiN < 1.4 + 2.0 / (n + 1.0);
        for (int i = 0; i < n; ++i) {
            const double step = (m[std::size_t(i)] - old[std::size_t(i)]) / sigma;
            pc[std::size_t(i)] = (1.0 - cc) * pc[std::size_t(i)] + (hsig ? std::sqrt(cc * (2.0 - cc) * mueff) * step : 0.0);
            double rankMu = 0.0;
            for (int k = 0; k < mu; ++k) {
                const double y = (x[std::size_t(order[std::size_t(k)])][std::size_t(i)] - old[std::size_t(i)]) / sigma;
                rankMu += w[std::size_t(k)] * y * y;
            }
            C[std::size_t(i)] = std::max(1e-12, (1.0 - c1 - cmu) * C[std::size_t(i)] + c1 * pc[std::size_t(i)] * pc[std::size_t(i)] + cmu * rankMu);
        }
        sigma *= std::exp(cs / ds * (psNorm / chiN - 1.0));
        sigma = std::clamp(sigma, 1e-4, 0.5);
        ++generation;
        if (progress)
            progress(result.evaluations, result.bestValue);
    }
    return result;
}

void parallelFor(int n, int threads, const std::function<void(int)> &fn)
{
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    const int count = std::max(1, std::min(threads, n));
    for (int t = 0; t < count; ++t)
        pool.emplace_back([&] {
            for (int i = next++; i < n; i = next++)
                fn(i);
        });
    for (std::thread &t : pool)
        t.join();
}

} // namespace krunch::fit
