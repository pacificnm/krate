#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/Instrument.h"
#include "core/MasterStage.h"
#include "core/SpscQueue.h"

namespace krate {

// One block of work for the rack (from its host).
struct RackBlock
{
    std::uint32_t frames = 0;
    const MidiEvent *midi = nullptr;           // sorted by frame, each frame < frames
    std::uint32_t midiCount = 0;
    const TransportState *transport = nullptr; // shared by every device
    float *masterL = nullptr;
    float *masterR = nullptr;
    // Optional per-device outputs: 2 buffers per device (L, R) in device order,
    // each `frames` long; each device's own output, before the mixer.
    float *const *deviceOutputs = nullptr;
};

// The Krate rack's realtime core (docs/SHARED_FOUNDATIONS.MD#the-rack-layer):
// hosts several instruments on one transport, routes MIDI to them (per channel,
// otherwise the focused device), resolves rack-mapped CCs to device parameters,
// switches focus from learned messages, and mixes into the master stage.
// No Qt, no JACK: the host (RackHost) or a test feeds it blocks.
//
// Threads: addDevice() before processing; process() on the audio thread;
// everything else any thread (atomics). Devices are stereo.
class Rack
{
public:
    static constexpr int kMaxDevices = 8;
    static constexpr int kChannels = 16;
    static constexpr int kAnyChannel = 16;      // CC map row matching any channel
    static constexpr std::uint32_t kMaxEventsPerDevice = 256;

    // Focus actions (learnable). 0..kMaxDevices-1 focus that device.
    static constexpr int kFocusNext = 0xFFFE;
    static constexpr int kFocusPrevious = 0xFFFD;

    // What a mapped CC or note does.
    struct Mapping
    {
        enum class Kind : std::uint8_t { None, Param, FollowFocus, FocusAction };
        Kind kind = Kind::None;
        int device = -1;  // Param: target device; FollowFocus: device the key was learned from
        int target = -1;  // Param/FollowFocus: parameter index on `device`; FocusAction: action
    };

    // Audio -> GUI: something was learned.
    struct Learned
    {
        bool isNote = false;      // a note (focus action) rather than a CC
        std::uint8_t channel = 0;
        std::uint8_t number = 0;  // CC or note number
        Mapping mapping;
    };

    explicit Rack(std::uint32_t maxFrames = 4096);

    Rack(const Rack &) = delete;
    Rack &operator=(const Rack &) = delete;

    // Setup, before processing. Returns the device index, or -1 if full.
    // midiChannel: 0..15 to route that channel's notes to the device, -1 for none.
    int addDevice(IInstrument &instrument, int midiChannel = -1);
    int deviceCount() const noexcept { return m_count; }
    IInstrument &device(int index) noexcept { return *m_devices[std::size_t(index)].instrument; }

    // From the processing thread, between blocks (same contract as IInstrument::prepare).
    void prepare(double sampleRate) noexcept;

    // --- Any thread ------------------------------------------------------------
    void setFocus(int device) noexcept;
    int focus() const noexcept { return m_focus.load(std::memory_order_relaxed); }

    void setChannelDevice(int channel, int device) noexcept; // device -1 = none (use focus)
    int channelDevice(int channel) const noexcept;
    void setProgramChangeFocus(bool on) noexcept { m_programFocus.store(on, std::memory_order_relaxed); }

    // Mixer. Level is linear gain (1 = unity); pan -1 (left) .. +1 (right), balance law.
    void setLevel(int device, float gain) noexcept;
    void setPan(int device, float pan) noexcept;
    void setMute(int device, bool mute) noexcept;
    void setSolo(int device, bool solo) noexcept;
    void setMasterLevel(float gain) noexcept { m_masterLevel.store(gain, std::memory_order_relaxed); }
    float takeDevicePeak(int device) noexcept; // post-fader, since last call
    float takeMasterPeak() noexcept { return m_master.takePeak(); }

    // Rack MIDI map. channel 0..15 or kAnyChannel.
    void setCcMapping(int channel, int cc, Mapping mapping) noexcept;
    Mapping ccMapping(int channel, int cc) const noexcept;
    void setNoteAction(int channel, int note, int action) noexcept; // action -1 = none
    int noteAction(int channel, int note) const noexcept;
    void clearMappings() noexcept;

    // Learn: arm a target; the next CC (or, for focus actions, CC or note) binds
    // to it, recording its channel. Arming Kind::None disarms.
    void arm(Mapping target) noexcept;
    Mapping armed() const noexcept { return unpack(m_armed.load(std::memory_order_relaxed)); }
    std::optional<Learned> popLearned() noexcept { return m_learned.pop(); }

    // --- Audio thread ----------------------------------------------------------
    void process(const RackBlock &block) noexcept;

private:
    struct Device
    {
        IInstrument *instrument = nullptr;
        std::vector<float> left;
        std::vector<float> right;
        std::array<MidiEvent, kMaxEventsPerDevice> midi{};
        std::uint32_t midiCount = 0;

        std::atomic<float> level{1.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool> mute{false};
        std::atomic<bool> solo{false};
        std::atomic<float> peak{0.0f};
        float gainL = 1.0f; // current (ramped) gains, audio thread only
        float gainR = 1.0f;
    };

    static std::uint32_t pack(Mapping m) noexcept;
    static Mapping unpack(std::uint32_t packed) noexcept;

    void processChunk(const RackBlock &block, std::uint32_t offset, std::uint32_t frames,
                      std::uint32_t &nextEvent) noexcept;
    void route(MidiEvent event) noexcept;
    bool runAction(int action) noexcept;
    void forward(int device, const MidiEvent &event) noexcept;
    void applyCc(int channel, int cc, int value) noexcept;
    int resolveFollowFocus(const Mapping &m) const noexcept;

    std::uint32_t m_maxFrames;
    double m_sampleRate = 48000.0;
    std::array<Device, kMaxDevices> m_devices;
    int m_count = 0;

    std::atomic<int> m_focus{0};
    std::array<std::atomic<std::int8_t>, kChannels> m_channelDevice;
    std::atomic<bool> m_programFocus{true};
    std::atomic<float> m_masterLevel{1.0f};
    float m_masterGain = 1.0f; // ramped, audio thread only
    bool m_primed = false;     // first block starts at the fader settings instead of ramping to them

    std::array<std::array<std::atomic<std::uint32_t>, 128>, kChannels + 1> m_ccMap;
    std::array<std::array<std::atomic<std::int32_t>, 128>, kChannels> m_noteActions;
    std::atomic<std::uint32_t> m_armed{0};
    SpscQueue<Learned, 64> m_learned;

    MasterStage m_master;
};

} // namespace krate
