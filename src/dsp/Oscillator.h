#pragma once

namespace k303 {

// Band-limited saw / square using PolyBLEP correction at the discontinuities.
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
