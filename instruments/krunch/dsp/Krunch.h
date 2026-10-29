#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "core/Instrument.h"
#include "core/ParamValues.h"
#include "dsp/Cabinet.h"
#include "dsp/Gate.h"
#include "dsp/Pedal.h"
#include "dsp/PowerAmp.h"
#include "dsp/Preamp.h"

namespace krunch {

// Krunch: a guitar amp for the Krate rack (docs/DSP.md). It plays the rack's
// audio inputs (a guitar through the audio interface) rather than notes:
//
//   input -> pedals -> gate -> PREAMP (the channel's) -> volume -> FX LOOP
//         -> POWER AMP -> CABINET (the channel's IRs A and B, or the built-in
//            speaker) -> level -> out (stereo with a stereo IR)
//
// Four channels, each with its own voice, gain, EQ, volume, gate, loop
// switch and cabinet; switching crossfades over 15 ms.
inline constexpr int kChannels = 4;

// Parameter ids: the amp's own, then kChannelParams per channel.
enum class ParamId : int { Input, Channel, Master, Sag, Resonance, Level, FirstChannel };
enum class ChannelParam : int { Voice, Gain, Bass, Mid, Treble, Presence, Volume, Gate, Loop, CabMix, Count };
inline constexpr int kChannelParams = int(ChannelParam::Count);
inline constexpr int ParamCount = int(ParamId::FirstChannel) + kChannels * kChannelParams;
// Index of a channel's parameter (channel 0..3).
constexpr int channelParam(int channel, ChannelParam p) { return int(ParamId::FirstChannel) + channel * kChannelParams + int(p); }

namespace format {
std::string input(float v);
std::string channel(float v);
std::string dial(float v);  // 0..10, as on an amp
std::string level(float v);
std::string voice(float v);
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
Voice voice(float v);
double levelDb(float v);    // kLevelMinDb..kLevelMaxDb; the bottom is silence
double levelGain(float v);  // 0 at the bottom
double volumeDb(float v);   // -40..+6 dB; the bottom is silence
double volumeGain(float v);
double gateDb(float v);     // the threshold, -80..-20 dBFS (0 = off)
} // namespace units

#define KRUNCH_CHANNEL_PARAMS(n, dVoice, dGain, dBass, dMid, dTreble, dPresence, dVolume, dGate)                      \
    {"ch" #n "_voice", "Channel " #n " Voice", dVoice, int(Voice::Count), &format::voice},                          \
    {"ch" #n "_gain", "Channel " #n " Gain", dGain, 0, &format::dial},                                              \
    {"ch" #n "_bass", "Channel " #n " Bass", dBass, 0, &format::dial},                                              \
    {"ch" #n "_mid", "Channel " #n " Mid", dMid, 0, &format::dial},                                                 \
    {"ch" #n "_treble", "Channel " #n " Treble", dTreble, 0, &format::dial},                                        \
    {"ch" #n "_presence", "Channel " #n " Presence", dPresence, 0, &format::dial},                                  \
    {"ch" #n "_volume", "Channel " #n " Volume", dVolume, 0, &format::volume},                                      \
    {"ch" #n "_gate", "Channel " #n " Gate", dGate, 0, &format::gate},                                              \
    {"ch" #n "_loop", "Channel " #n " Effects Loop", 1.0f, 2, &format::onOff},                                      \
    {"ch" #n "_cab_mix", "Channel " #n " Cabinet Mix", 0.5f, 0, &format::cabMix}

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"input", "Input", 0.0f, 2, &format::input},          // the interface's input 1 or 2
    {"channel", "Channel", 0.0f, kChannels, &format::channel},
    {"master", "Master", 0.5f, 0, &format::dial},
    {"sag", "Sag", 0.3f, 0, &format::dial},
    {"resonance", "Resonance", 0.4f, 0, &format::dial},
    {"level", "Level", 60.0f / 72.0f, 0, &format::level}, // -60..+12 dB, default 0 dB
    // Defaults: 1 clean, 2 crunch, 3 heavy (tight, mids down), 4 lead.
    KRUNCH_CHANNEL_PARAMS(1, 0.0f, 0.40f, 0.50f, 0.50f, 0.55f, 0.50f, 0.70f, 0.00f),
    KRUNCH_CHANNEL_PARAMS(2, 1.0f / 3.0f, 0.50f, 0.55f, 0.60f, 0.55f, 0.50f, 0.70f, 0.00f),
    KRUNCH_CHANNEL_PARAMS(3, 2.0f / 3.0f, 0.65f, 0.60f, 0.35f, 0.60f, 0.55f, 0.70f, 0.35f),
    KRUNCH_CHANNEL_PARAMS(4, 1.0f, 0.70f, 0.50f, 0.70f, 0.50f, 0.50f, 0.72f, 0.30f),
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

    // Pedal chains (setup, before processing): in front of the amp, in the loop.
    PedalChain &frontPedals() noexcept { return m_front; }
    PedalChain &loopPedals() noexcept { return m_loop; }

private:
    float channelValue(int channel, ChannelParam p) const noexcept { return m_params.get(channelParam(channel, p)); }
    Preamp::Settings preampSettings(int channel) const noexcept;
    // One channel's preamp and volume, from m_in into out.
    void runChannel(int channel, float *out, std::uint32_t frames) noexcept;
    void processBlock(const float *in, float *left, float *right, std::uint32_t frames) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    double m_sampleRate = 48000.0;

    PedalChain m_front, m_loop;
    Gate m_gate;
    std::array<Preamp, kChannels> m_preamps;
    std::array<float, kChannels> m_volume{}; // current (ramped) channel volumes; < 0 until first used
    PowerAmp m_powerAmp;
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
    std::array<float, kBlock> m_speakerWeight{}, m_speakerOut{};

    std::atomic<float> m_inputPeak{0.0f};
    std::atomic<float> m_outputPeak{0.0f};
    std::atomic<bool> m_gateClosed{false};
};

} // namespace krunch
