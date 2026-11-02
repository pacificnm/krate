#include "dsp/Tube.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace krunch {

namespace {

struct Koren
{
    double mu, ex, kg1, kp, kvb;
};

// Koren's parameters for the four kinds of preamp triode (mu 100, 70, 60, 21.5).
constexpr Koren kKoren[int(PreTube::Count)] = {
    {100.0, 1.4, 1060.0, 600.0, 300.0},
    {70.0, 1.4, 1060.0, 500.0, 300.0},
    {60.0, 1.35, 460.0, 300.0, 300.0},
    {21.5, 1.3, 1180.0, 84.0, 300.0},
};

double plateCurrent(const Koren &k, double vgk, double vpk)
{
    if (vpk <= 0.0)
        return 0.0;
    const double e1 = vpk / k.kp * std::log1p(std::exp(std::min(k.kp * (1.0 / k.mu + vgk / std::sqrt(k.kvb + vpk * vpk)), 50.0)));
    return e1 > 0.0 ? 2.0 * std::pow(e1, k.ex) / k.kg1 : 0.0;
}

double onePoleCoefficient(double fs, double hz) { return 1.0 - std::exp(-2.0 * std::numbers::pi * std::min(hz, 0.45 * fs) / fs); }

} // namespace

TriodeCurve::TriodeCurve(PreTube tube)
{
    const Koren &k = kKoren[std::clamp(int(tube), 0, int(PreTube::Count) - 1)];
    m_mu = k.mu;
    for (int i = 0; i < kPoints; ++i) {
        const double vgk = kMinGrid + (kMaxGrid - kMinGrid) * double(i) / (kPoints - 1);
        // The plate where the load line meets the tube: Vp = B+ - R * Ip(Vgk, Vp).
        double lo = 0.0, hi = kSupply;
        for (int it = 0; it < 60; ++it) {
            const double vp = 0.5 * (lo + hi);
            (kSupply - kPlateLoad * plateCurrent(k, vgk, vp) > vp ? lo : hi) = vp;
        }
        m_plate[std::size_t(i)] = float(0.5 * (lo + hi));
    }
}

const TriodeCurve &TriodeCurve::get(PreTube tube)
{
    static const TriodeCurve kCurves[int(PreTube::Count)] = {TriodeCurve(PreTube::HighGain), TriodeCurve(PreTube::Smooth),
                                                            TriodeCurve(PreTube::Medium), TriodeCurve(PreTube::LowGain)};
    return kCurves[std::clamp(int(tube), 0, int(PreTube::Count) - 1)];
}

double TriodeCurve::plate(double vgk) const noexcept
{
    const double x = (std::clamp(vgk, kMinGrid, kMaxGrid) - kMinGrid) * (kPoints - 1) / (kMaxGrid - kMinGrid);
    const int i = std::min(int(x), kPoints - 2);
    const double f = x - double(i);
    return double(m_plate[std::size_t(i)]) + f * double(m_plate[std::size_t(i) + 1] - m_plate[std::size_t(i)]);
}

void TriodeStage::prepare(double rate, PreTube tube, const Design &design) noexcept
{
    m_curve = &TriodeCurve::get(tube);
    m_design = design;
    m_rate = rate;
    // At rest the cathode carries the plate current: Vk = Rk * Ip(-Vk). (Bisection:
    // vk - Rk * Ip(-vk) rises with vk; a plain iteration diverges for larger Rk.)
    double lo = 0.0, hi = -TriodeCurve::kMinGrid;
    for (int it = 0; it < 60; ++it) {
        const double mid = 0.5 * (lo + hi);
        (mid - design.cathodeOhms * m_curve->current(-mid) < 0.0 ? lo : hi) = mid;
    }
    const double vk = 0.5 * (lo + hi);
    m_restCathode = vk;
    m_restPlate = m_curve->plate(-vk);
    m_gain = (m_curve->plate(-vk - 0.01) - m_curve->plate(-vk + 0.01)) / 0.02;
    m_couplingA = onePoleCoefficient(rate, design.couplingHz);
    // Driven positive, the grid conducts: the coupling capacitor charges
    // through it faster than it drains through the grid leak (the source's
    // impedance against the leak's: about 4 times).
    m_gridA = std::min(0.5, 4.0 * m_couplingA);
    // The cathode follows the stage's current through its bypass capacitor
    // (kept stable: the follower's loop gain is up to about 20).
    m_cathodeA = std::min(design.bypassHz > 0.0 ? onePoleCoefficient(rate, design.bypassHz) : 1.0, 0.08);
    m_rolloff.setCutoff(rate, design.rolloffHz);
    // (No reset: a knob moving re-prepares the stages, which carry on as they are.)
}

void TriodeStage::reset() noexcept
{
    m_coupling = 0.0;
    m_cathode = m_restCathode;
    m_rolloff.reset();
}

double TriodeStage::process(double vin) noexcept
{
    double vgk = vin - m_coupling - m_cathode;
    // The coupling capacitor: drains through the grid leak (a high-pass), and
    // charges through the grid when it's driven past the cathode.
    m_coupling += m_couplingA * (vin - m_coupling) + (vgk > 0.0 ? m_gridA * vgk : 0.0);
    if (vgk > 0.0) // the conducting grid holds itself just above the cathode
        vgk = vgk / (1.0 + 2.0 * vgk);
    const double plate = m_curve->plate(vgk);
    const double current = (TriodeCurve::kSupply - plate) / TriodeCurve::kPlateLoad;
    m_cathode += m_cathodeA * (m_design.cathodeOhms * current - m_cathode);
    return double(m_rolloff.lowpass(float(plate - m_restPlate)));
}

// The output pairs. Headroom: where the drive starts to clip; knee: how hard
// its ceiling is; bias: class AB idle; sag: the supply's droop; voicing.
const PowerTubeDesign &powerTubeDesign(PowerTube tube)
{
    static const PowerTubeDesign kDesigns[int(PowerTube::Count)] = {
        {1.00, 2.5, 0.35, 1.0, 800.0, 1.5, 0.7},  // British pentode: early, mid-forward breakup
        {1.25, 2.0, 0.45, 0.7, 600.0, -1.5, 0.6}, // American beam: clean headroom, scooped
        {0.60, 1.6, 0.60, 1.6, 2000.0, 2.0, 0.8}, // Small pentode: class A-ish, chimey, compresses early
        {1.60, 3.5, 0.40, 0.4, 120.0, 1.5, 0.8},  // Big beam: huge headroom, tight big lows
    };
    return kDesigns[std::clamp(int(tube), 0, int(PowerTube::Count) - 1)];
}

double pushPull(const PowerTubeDesign &d, double x, double supply) noexcept
{
    // Each tube's current: a 3/2 law from its idle, under a soft ceiling that
    // the sagging supply lowers.
    const double ceiling = d.headroom / (1.0 + d.sag * supply);
    const auto half = [&](double v) {
        const double e = std::max(0.0, v + d.bias);
        const double i = e * std::sqrt(e);
        return i / std::pow(1.0 + std::pow(i / ceiling, d.knee), 1.0 / d.knee);
    };
    // Unity small-signal gain at idle (with the full supply); a pair with less
    // headroom made up for, so changing tubes changes the feel, not mostly the level.
    const double slope = 3.0 * std::sqrt(d.bias);
    return (half(x) - half(-x)) / slope / std::sqrt(d.headroom);
}

} // namespace krunch
