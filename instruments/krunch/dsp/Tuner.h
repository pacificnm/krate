#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace krunch {

// The tuner (docs/MANUAL.md#tuner): the guitar's pitch, read from the input
// as it arrives (before the pedals), against a tuning's open strings and a
// reference pitch for A4.
namespace tuner {

inline constexpr double kDefaultReferenceHz = 440.0;
inline constexpr double kMinReferenceHz = 400.0;
inline constexpr double kMaxReferenceHz = 480.0;
inline constexpr int kMaxStrings = 8;

struct Tuning
{
    const char *id;    // saved
    const char *name;
    const char *group; // "Guitar", "7-string", ...
    int count;         // open strings, lowest first; 0: chromatic (the nearest note)
    std::array<int, kMaxStrings> notes; // MIDI note numbers (E2 = 40)
    bool flats;        // spelled with flats (E♭ Standard) rather than sharps
};

// Every tuning, Chromatic first, then Standard E.
std::span<const Tuning> tunings() noexcept;
// By id; unknown ids get Standard E.
const Tuning &tuning(std::string_view id) noexcept;
inline constexpr const char *kDefaultTuning = "standard";

// The pitch of MIDI note `note` with A4 (note 69) at referenceHz.
double noteHz(double note, double referenceHz) noexcept;
// hz as a (fractional) MIDI note number.
double noteOf(double hz, double referenceHz) noexcept;
// "E", "F♯", "B♭" (without the octave).
std::string noteName(int note, bool flats);
inline int octave(int note) { return note / 12 - 1; }

struct Reading
{
    int note = -1;      // the target: the nearest note, or the tuning's nearest string's note
    int string = -1;    // that string (0 = the lowest), or -1 in Chromatic
    double cents = 0.0; // how far hz is from the target: below is flat (negative)
};
// hz against a tuning: the string nearest to it (by cents), or in Chromatic
// the nearest note.
Reading read(const Tuning &tuning, double hz, double referenceHz) noexcept;

} // namespace tuner

// Finds the pitch of a stretch of mono signal (YIN, at a quarter or so of the
// rate, then refined at the full rate): 25 Hz (a 5-string bass's low B, tuned
// down) to 1.5 kHz. Not real-time safe (allocates in prepare()); for the UI
// thread.
class PitchDetector
{
public:
    static constexpr double kMinHz = 25.0;
    static constexpr double kMaxHz = 1500.0;

    void prepare(double sampleRate);
    double sampleRate() const noexcept { return m_rate; }
    // Samples detect() wants: the most recent this many (about 100 ms).
    std::size_t windowSize() const noexcept { return m_needed; }

    struct Result
    {
        double hz = 0.0;      // 0: no clear pitch
        double clarity = 0.0; // 0..1 (1: a pure tone)
        double level = 0.0;   // RMS of the window
    };
    // The pitch of `samples` (windowSize() of them; more are fine, the last ones used).
    Result detect(std::span<const float> samples);

private:
    double m_rate = 0.0;
    std::size_t m_decimation = 1;  // full-rate samples per analysed sample
    std::size_t m_window = 0;      // YIN window (decimated samples)
    std::size_t m_maxLag = 0, m_minLag = 2;
    std::size_t m_preroll = 0;     // decimated samples the lowpass settles over
    std::size_t m_needed = 0;      // full-rate samples
    std::vector<float> m_full, m_low; // lowpassed (full rate), decimated
    std::vector<double> m_diff;
};

// The tuner's tap on the input: the audio thread writes the input into it while
// the tuner is open; the UI thread reads the latest samples.
class TunerTap
{
public:
    static constexpr std::size_t kSize = 32768; // plenty at 192 kHz

    void setOpen(bool open) noexcept { m_open.store(open, std::memory_order_relaxed); }
    bool open() const noexcept { return m_open.load(std::memory_order_relaxed); }
    // Audio thread: in (or silence when null).
    void write(const float *in, std::uint32_t frames) noexcept;
    // Any thread: the latest out.size() samples (out.size() <= kSize / 2);
    // false until that many have been written since it opened.
    bool read(std::span<float> out) const noexcept;

private:
    std::atomic<bool> m_open{false};
    bool m_wasOpen = false; // audio thread
    std::array<std::atomic<float>, kSize> m_ring{};
    std::atomic<std::uint64_t> m_written{0}; // since it opened
};

} // namespace krunch
