#pragma once

namespace k303 {

// Band-limited saw / square (docs/DSP.md#oscillator). The saw is a PolyBLEP
// ramp with a slight curve, as a capacitor charging through the hardware's
// current source bends it; the square is made from that saw by a
// transistor-style shaper (a steep tanh comparator with a small offset), so
// its rising edge is a fast slope, its falling edge the saw's reset, and its
// pulse a little uneven, as on the hardware.
class Oscillator
{
public:
    enum class Shape { Saw, Square };

    void setSampleRate(double sampleRate) noexcept;
    void setFrequency(double hz) noexcept;
    void setShape(Shape shape) noexcept { m_shape = shape; }
    void reset() noexcept { m_phase = 0.0; }

    float next() noexcept;

private:
    void updateIncrement() noexcept;

    double m_sampleRate = 48000.0;
    double m_frequency = 220.0;
    double m_phase = 0.0;      // 0..1
    double m_increment = 0.0;  // phase per sample
    Shape m_shape = Shape::Saw;
};

} // namespace k303
