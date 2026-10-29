#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace krunch::tools {

// A stand-in for a guitar's DI signal, for renders and tests when there is no
// recording: plucked strings (Karplus-Strong with a damping filter), each note
// a pluck at a time, open or palm-muted. Not a guitar sound to listen to on its
// own; enough to drive an amp the way a guitar does.
class SyntheticGuitar
{
public:
    struct Note
    {
        double start = 0.0;   // seconds
        double length = 0.5;  // until the string is damped
        double hz = 82.41;
        double velocity = 1.0;
        bool palmMuted = false;
    };

    explicit SyntheticGuitar(double sampleRate)
        : m_rate(sampleRate)
    {
    }

    // Adds one plucked string into `out` (mono, at the sample rate).
    void pluck(std::vector<float> &out, const Note &note)
    {
        const auto period = std::max<std::size_t>(2, std::size_t(std::lround(m_rate / note.hz)));
        std::vector<double> line(period);
        std::uniform_real_distribution<double> noise(-1.0, 1.0);
        double smooth = 0.0;
        for (double &s : line) { // a pick: noise, softened
            smooth += 0.5 * (noise(m_random) - smooth);
            s = smooth;
        }
        const double loss = note.palmMuted ? 0.990 : 0.9985;
        const double brightness = note.palmMuted ? 0.25 : 0.5; // damping filter mix
        const auto start = std::size_t(note.start * m_rate);
        const auto stop = std::min(out.size(), start + std::size_t((note.length + 0.05) * m_rate));
        const auto damp = start + std::size_t(note.length * m_rate);
        double previous = 0.0, gain = 0.30 * note.velocity;
        std::size_t pos = 0;
        for (std::size_t n = start; n < stop; ++n) {
            const double current = line[pos];
            // Average with the previous sample (the string's losses), more for a muted string.
            const double next = loss * (brightness * current + (1.0 - brightness) * previous);
            previous = current;
            line[pos] = next;
            pos = pos + 1 == period ? 0 : pos + 1;
            if (n >= damp) // the hand stops the string
                gain *= 0.995;
            out[n] += float(current * gain);
        }
    }

    // A riff at `bpm`: two bars of palm-muted chugs on the low string with
    // accented power chords, a bar of ringing power chords, a bar of single
    // notes higher up. Returns the DI signal.
    std::vector<float> riff(double bpm = 120.0)
    {
        const double sixteenth = 60.0 / bpm / 4.0;
        const double bar = sixteenth * 16.0;
        std::vector<float> out(std::size_t((4.0 * bar + 1.5) * m_rate), 0.0f);
        const auto chord = [this, &out](double t, double length, double root, bool muted, double velocity) {
            pluck(out, {t, length, root, velocity, muted});
            pluck(out, {t + 0.004, length, root * 1.4983, velocity * 0.9, muted}); // the fifth
            pluck(out, {t + 0.008, length, root * 2.0, velocity * 0.8, muted});    // the octave
        };
        constexpr double E2 = 82.41, G2 = 98.00, A2 = 110.0, D3 = 146.83;
        for (int b = 0; b < 2; ++b)
            for (int s = 0; s < 16; ++s) {
                const double t = b * bar + s * sixteenth;
                if (s == 6 || s == 14)
                    chord(t, sixteenth * 2.0, s == 6 ? G2 : A2, false, 1.0);
                else if (s != 7 && s != 15)
                    pluck(out, {t, sixteenth * 0.9, E2, s % 4 == 0 ? 1.0 : 0.8, true});
            }
        const double ringing[] = {E2, G2, A2, D3};
        for (int i = 0; i < 4; ++i)
            chord(2.0 * bar + i * 4.0 * sixteenth, 4.0 * sixteenth, ringing[i], false, 1.0);
        const double lead[] = {329.63, 392.00, 440.00, 493.88, 440.00, 392.00, 329.63, 293.66};
        for (int i = 0; i < 8; ++i)
            pluck(out, {3.0 * bar + i * 2.0 * sixteenth, i == 7 ? bar : 2.0 * sixteenth, lead[i], 0.9, false});
        return out;
    }

private:
    double m_rate;
    std::mt19937 m_random{4242u};
};

} // namespace krunch::tools
