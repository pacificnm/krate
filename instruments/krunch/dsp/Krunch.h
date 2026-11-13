#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "core/Instrument.h"
#include "core/ParamValues.h"
#include "dsp/Cabinet.h"
#include "dsp/Capture.h"
#include "dsp/DualHead.h"
#include "dsp/Gate.h"
#include "dsp/Pedal.h"
#include "dsp/Speaker.h"
#include "dsp/Tuner.h"

namespace krunch {

// Krunch: an amp capture player for the Krate rack (docs/DSP.md). It plays
// the rack's audio inputs (a guitar through the audio interface) rather than
// notes:
//
//   input -> pedals -> gate -> the channel's AMP CAPTURE (.nam, driven by GAIN,
//         then its EQ) -> volume -> FX LOOP -> CABINET (the channel's IRs A and
//         B, or the built-in speaker) -> level -> out (stereo with a stereo IR)
//
// Four channels, each with its own capture, gain, EQ, volume, gate, loop
// switch and cabinet (a channel without a capture is silent); switching
// crossfades over 15 ms.
inline constexpr int kChannels = 4;

// Parameter ids: the amp's own, then kChannelParams per channel.
enum class ParamId : int { Input, Channel, Level, FirstChannel };
enum class ChannelParam : int { Gain, Bass, Mid, Treble, Presence, Volume, Gate, Loop, CabMix, Space, Cabinet, Count };
inline constexpr int kChannelParams = int(ChannelParam::Count);
inline constexpr int ParamCount = int(ParamId::FirstChannel) + kChannels * kChannelParams;
// Index of a channel's parameter (channel 0..3).
constexpr int channelParam(int channel, ChannelParam p) { return int(ParamId::FirstChannel) + channel * kChannelParams + int(p); }

namespace format {
std::string input(float v);
std::string channel(float v);
std::string dial(float v);  // 0..10, as on an amp
std::string level(float v);
std::string gain(float v);     // how hard the capture is driven, -12..+12 dB
std::string bass(float v);     // the EQ after it
std::string mid(float v);
std::string treble(float v);
std::string presence(float v);
std::string volume(float v);
std::string gate(float v);
std::string onOff(float v);
std::string cabMix(float v); // "A 70 / B 30"
} // namespace format

namespace units {
inline constexpr int kInputs = 2;
inline constexpr double kLevelMinDb = -60.0;
inline constexpr double kLevelMaxDb = 12.0;
int input(float v);         // 0 or 1
int channel(float v);       // 0..3
double gainDb(float v);     // -12..+12 dB, noon 0: as it was captured
// The EQ after the capture: shelves at 120 Hz and 2.5 kHz (+-12 dB), a peak
// at 700 Hz (+-10 dB), a shelf at 5 kHz (+-8 dB); noon is flat.
double bassDb(float v);
double midDb(float v);
double trebleDb(float v);
double presenceDb(float v);
double levelDb(float v);    // kLevelMinDb..kLevelMaxDb; the bottom is silence
double levelGain(float v);  // 0 at the bottom
double volumeDb(float v);   // -40..+6 dB; the bottom is silence
double volumeGain(float v);
double gateDb(float v);     // the threshold, -80..-20 dBFS (0 = off)
} // namespace units

#define KRUNCH_CHANNEL_PARAMS(n, dGate)                                                                                  \
    {"ch" #n "_gain", "Channel " #n " Gain", 0.5f, 0, &format::gain},                                              \
    {"ch" #n "_bass", "Channel " #n " Bass", 0.5f, 0, &format::bass},                                              \
    {"ch" #n "_mid", "Channel " #n " Mid", 0.5f, 0, &format::mid},                                                 \
    {"ch" #n "_treble", "Channel " #n " Treble", 0.5f, 0, &format::treble},                                        \
    {"ch" #n "_presence", "Channel " #n " Presence", 0.5f, 0, &format::presence},                                  \
    {"ch" #n "_volume", "Channel " #n " Volume", 0.7f, 0, &format::volume},                                        \
    {"ch" #n "_gate", "Channel " #n " Gate", dGate, 0, &format::gate},                                              \
    {"ch" #n "_loop", "Channel " #n " Effects Loop", 1.0f, 2, &format::onOff},                                      \
    {"ch" #n "_cab_mix", "Channel " #n " Cabinet Mix", 0.5f, 0, &format::cabMix},                                \
    {"ch" #n "_space", "Channel " #n " Cabinet Space", 0.0f, 0, &format::dial},                                    \
    {"ch" #n "_cabinet", "Channel " #n " Cabinet", 1.0f, 2, &format::onOff}

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"input", "Input", 0.0f, 2, &format::input},          // the interface's input 1 or 2
    {"channel", "Channel", 0.0f, kChannels, &format::channel},
    {"level", "Level", 60.0f / 72.0f, 0, &format::level}, // -60..+12 dB, default 0 dB
    // Every knob at noon: each channel's capture as it was captured. The gate on
    // the last two (where higher-gain captures usually go).
    KRUNCH_CHANNEL_PARAMS(1, 0.00f),
    KRUNCH_CHANNEL_PARAMS(2, 0.00f),
    KRUNCH_CHANNEL_PARAMS(3, 0.35f),
    KRUNCH_CHANNEL_PARAMS(4, 0.30f),
}};
#undef KRUNCH_CHANNEL_PARAMS

class Krunch final : public krate::IInstrument
{
public:
    static constexpr std::uint32_t kBlock = 256;        // processed in pieces this long
    static constexpr double kSwitchSeconds = 0.015;     // channel crossfade

    Krunch();

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    int inputChannels() const noexcept override { return units::kInputs; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    void process(const krate::AudioBlock &block) noexcept override;

    // Peaks since the last call (any thread): the chosen input as it arrives,
    // and the amp's output.
    float takeInputPeak() noexcept { return m_inputPeak.exchange(0.0f, std::memory_order_relaxed); }
    float takeOutputPeak() noexcept { return m_outputPeak.exchange(0.0f, std::memory_order_relaxed); }
    // The gate is shut (display).
    bool gateClosed() const noexcept { return m_gateClosed.load(std::memory_order_relaxed); }

    // The cabinet's IRs: slot cabinetSlot(channel, b) holds channel's IR A (b
    // false) or B. A channel with neither plays the built-in speaker. Kernels
    // at another sample rate than prepare()'s are ignored until rebuilt.
    static constexpr int cabinetSlot(int channel, bool b) { return 2 * channel + (b ? 1 : 0); }
    Cabinet &cabinet() noexcept { return m_cabinet; }
    // The rate prepare() was given (any thread): what kernels must be built for.
    double sampleRate() const noexcept { return m_rateForKernels.load(std::memory_order_relaxed); }

    // A channel's amp capture (docs/CAPTURES.md): GAIN drives it, BASS ...
    // PRESENCE are an EQ after it; without one the channel is silent. Handed
    // over from any thread; the caller keeps it alive until blocksProcessed()
    // has moved two on. One readied for another rate than prepare()'s is
    // ignored until replaced.
    void setCapture(int channel, CaptureModel *model) noexcept
    {
        m_captures[std::size_t(channel)].store(model, std::memory_order_release);
    }
    bool playsCapture(int channel) const noexcept;


    // Pedal chains (lists handed over from any thread, see Pedalboard): in
    // front of the amp, in the loop.
    PedalChain &frontPedals() noexcept { return m_front; }
    PedalChain &loopPedals() noexcept { return m_loop; }
    // The tuner: its tap on the input (before the pedals), and muting the amp
    // while tuning (any thread).
    TunerTap &tunerTap() noexcept { return m_tunerTap; }
    void setMuted(bool muted) noexcept { m_muted.store(muted, std::memory_order_relaxed); }
    bool muted() const noexcept { return m_muted.load(std::memory_order_relaxed); }

    // Blocks processed so far (any thread): something replaced before this
    // count moved two on is no longer in use by the audio thread.
    std::uint64_t blocksProcessed() const noexcept { return m_blocks.load(std::memory_order_acquire); }

private:
    float channelValue(int channel, ChannelParam p) const noexcept { return m_params.get(channelParam(channel, p)); }
    // One channel's capture, EQ and volume, from m_in into out.
    void runChannel(int channel, float *out, std::uint32_t frames) noexcept;
    void processBlock(const float *in, float *left, float *right, std::uint32_t frames) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    double m_sampleRate = 48000.0;

    PedalChain m_front, m_loop;
    Gate m_gate;
    std::array<float, kChannels> m_volume{}; // current (ramped) channel volumes; < 0 until first used
    SpeakerFilter m_speaker;
    Cabinet m_cabinet;
    std::atomic<double> m_rateForKernels{48000.0};

    int m_active = -1;   // the channel playing
    int m_previous = -1; // the channel fading out, or -1
    double m_fade = 1.0; // 0..1 into the switch
    double m_fadeStep = 0.0;
    float m_level = -1.0f; // current (ramped) output level

    // Adds channel's cabinet into left/right, weighted per sample by `weight`.
    void addCabinet(int channel, const float *weight, std::uint32_t frames) noexcept;

    std::array<float, kBlock> m_in{}, m_a{}, m_b{};
    std::array<float, kBlock> m_fadeIn{}, m_fadeOut{}; // the playing channel's and the previous one's weights
    std::array<std::array<float, kBlock>, Cabinet::kSlots> m_slotWeights{};
    std::array<const float *, Cabinet::kSlots> m_slotWeightPtrs{};
    std::array<float, Cabinet::kSlots> m_slotPans{}; // SPACE: A to the left, B to the right
    DualHeadSplit m_dualHead; // the Dual Head pedal's second amp, at the output
    // Captures: each channel's, and the EQ after it.
    std::array<std::atomic<CaptureModel *>, kChannels> m_captures{};

    struct CaptureEq
    {
        Biquad bass, mid, treble, presence;
        std::array<float, 4> knobs{-1.0f, -1.0f, -1.0f, -1.0f};
    };
    std::array<CaptureEq, kChannels> m_captureEq;
    std::array<float, kBlock> m_speakerWeight{}, m_speakerOut{};
    std::array<float, kBlock> m_directWeight{}; // channels with their cabinet off: the amp straight out

    std::atomic<float> m_inputPeak{0.0f};
    std::atomic<float> m_outputPeak{0.0f};
    std::atomic<bool> m_gateClosed{false};
    std::atomic<std::uint64_t> m_blocks{0};
    TunerTap m_tunerTap;
    std::atomic<bool> m_muted{false};
};

} // namespace krunch
