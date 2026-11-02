#pragma once

#include <array>
#include <cstdint>

#include "dsp/Filters.h"

namespace krunch {

// Preamp tubes (docs/DSP.md#tubes): triodes by how much they amplify.
// High gain is the classic preamp tube (mu 100); Smooth a little less, with a
// softer knee; Medium has more headroom; Low gain barely clips at all.
enum class PreTube : int { HighGain, Smooth, Medium, LowGain, Count };

// Power tubes: the output pair's character (headroom, how hard it clips,
// how much the supply sags, its crossover, its voicing).
enum class PowerTube : int { BritishPentode, AmericanBeam, SmallPentode, BigBeam, Count };

// A triode's plate voltage for each grid-to-cathode voltage, on a 100 k plate
// load from a 250 V supply: Koren's triode equations (1996), solved once per
// tube type and kept as a table (built on first use, off the audio thread).
class TriodeCurve
{
public:
    static constexpr double kSupply = 250.0;   // B+, volts
    static constexpr double kPlateLoad = 100e3; // ohms
    static constexpr double kMinGrid = -16.0, kMaxGrid = 2.0;
    static constexpr int kPoints = 2048;

    static const TriodeCurve &get(PreTube tube);

    // Plate voltage and current at this grid-to-cathode voltage (linear in the table).
    double plate(double vgk) const noexcept;
    double current(double vgk) const noexcept { return (kSupply - plate(vgk)) / kPlateLoad; }
    double mu() const noexcept { return m_mu; }

private:
    explicit TriodeCurve(PreTube tube);
    std::array<float, kPoints> m_plate{};
    double m_mu = 100.0;
};

// One common-cathode triode stage at the oversampled rate, in volts:
// - the coupling capacitor in front (a high-pass), which the grid charges when
//   it's driven positive (grid current): the stage then biases itself colder
//   and recovers over the capacitor's time (blocking, touch, sag-like feel);
// - the cathode resistor, its voltage following the stage's average current
//   through the bypass capacitor (bias shift: the compression of a tube
//   stage pushed hard), and partial bypass: less gain below a frequency;
// - the plate: the triode curve; out, the swing around the plate's rest
//   voltage, inverted as a real stage inverts, and the Miller roll-off.
class TriodeStage
{
public:
    struct Design
    {
        double cathodeOhms = 1500.0;
        double bypassHz = 10.0;     // below it, the cathode isn't bypassed: less gain (0: none at all)
        double couplingHz = 10.0;   // the coupling capacitor in front
        double rolloffHz = 12000.0; // Miller capacitance, treble caps
    };

    // The tube and the circuit (its state is kept: reset() for a fresh start).
    void prepare(double rate, PreTube tube, const Design &design) noexcept;
    void reset() noexcept; // at rest
    // A grid voltage in, a plate swing out (volts; negative when the grid goes up).
    double process(double vin) noexcept;
    // The stage's small-signal gain at its operating point (for staging and tests).
    double gain() const noexcept { return m_gain; }

private:
    const TriodeCurve *m_curve = nullptr;
    Design m_design;
    double m_rate = 192000.0;
    double m_restCathode = 1.5;  // volts at rest
    double m_restPlate = 150.0;
    double m_gain = 60.0;
    // State.
    double m_coupling = 0.0; // the coupling capacitor's charge, volts
    double m_cathode = 1.5;  // the cathode's voltage (through its bypass capacitor)
    double m_couplingA = 0.0, m_gridA = 0.0, m_cathodeA = 0.0;
    OnePole m_rolloff;
};

// The power amp's output pair, push-pull class AB: each tube's current
// against its drive (a 3/2 law with a soft ceiling), the two subtracted, so
// it clips symmetrically with a little crossover when biased cold.
struct PowerTubeDesign
{
    double headroom;   // the drive where it starts to clip, relative
    double knee;       // how hard the ceiling is (higher: harder)
    double bias;       // class AB idle: lower is colder (more crossover)
    double sag;        // how much the supply droops under load
    double voiceHz, voiceDb, voiceQ; // its voicing (a mid push, a scoop...)
};
const PowerTubeDesign &powerTubeDesign(PowerTube tube);
// The pair's output for a drive x (the design's units: about 1 at full power).
double pushPull(const PowerTubeDesign &d, double x, double supply) noexcept;

} // namespace krunch
