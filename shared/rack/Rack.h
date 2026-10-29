#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/EventSource.h"
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
    // Events for particular devices (a song's tracks), sorted by frame, each
    // frame < frames. They skip routing, mappings and learn.
    const DeviceEvent *deviceMidi = nullptr;
    std::uint32_t deviceMidiCount = 0;
    const TransportState *transport = nullptr; // shared by every device
    // Rack::kInputs buffers, `frames` long (the audio interface's inputs), or nullptr.
    const float *const *inputs = nullptr;
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
    static constexpr int kInputs = 2;           // audio inputs, given to every device that has inputs
    static constexpr int kAnyChannel = 16;      // CC map row matching any channel
    static constexpr std::uint32_t kMaxEventsPerDevice = 256;

    // Focus actions (learnable). 0..kMaxDevices-1 focus that device.
    static constexpr int kFocusNext = 0xFFFE;
    static constexpr int kFocusPrevious = 0xFFFD;

    // The rack itself as an Action's device (e.g. play/stop).
    static constexpr int kRackDevice = 255;

    // What a mapped CC or note does.
    struct Mapping
    {
        enum class Kind : std::uint8_t {
            None,
            Param,       // device's parameter `target` follows the CC value
            FollowFocus, // the focused device's parameter with the same key as `device`'s `target`
            FocusAction, // focus: device `target`, kFocusNext or kFocusPrevious
            Action,      // a button: action `target` of `device` (or of the rack: kRackDevice)
            Mixer,       // mixer control `target` (MixerControl) of `device`'s strip (or the master: kRackDevice)
        };
        Kind kind = Kind::None;
        int device = -1;
        int target = -1;
    };

    // A mixer strip's controls (Mapping::Kind::Mixer targets). Fader and Pan
    // follow the CC; Mute and Solo are buttons (a press toggles).
    enum MixerControl : int { MixerFader, MixerPan, MixerMute, MixerSolo };
    static constexpr bool isMixerButton(int control) { return control == MixerMute || control == MixerSolo; }

    // Audio -> GUI: a mapped mixer control moved; the GUI's mixer applies it
    // (it owns the fader positions and the session).
    struct MixerMove
    {
        int device = -1; // device index, or kRackDevice for the master
        int control = MixerFader;
        std::uint8_t value = 0; // the CC value (buttons: a press)
    };

    // Audio -> GUI: an Action button was pressed (the GUI performs it).
    struct Fired
    {
        int device = -1; // device index, or kRackDevice
        int action = -1;
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
    // Notes as buttons: FocusAction or Action mappings only (the note never plays).
    void setNoteMapping(int channel, int note, Mapping mapping) noexcept;
    Mapping noteMapping(int channel, int note) const noexcept;
    void clearMappings() noexcept;

    // Learn: arm a target; the next CC (or, for buttons: focus actions, actions,
    // mixer mute/solo, CC or note) binds to it, recording its channel. Arming
    // Kind::None disarms.
    void arm(Mapping target) noexcept;
    Mapping armed() const noexcept { return unpack(m_armed.load(std::memory_order_relaxed)); }
    std::optional<Learned> popLearned() noexcept { return m_learned.pop(); }
    std::optional<Fired> popFired() noexcept { return m_fired.pop(); }
    std::optional<MixerMove> popMixerMove() noexcept { return m_mixerMoves.pop(); }

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

    void processChunk(const RackBlock &block, std::uint32_t offset, std::uint32_t frames, std::uint32_t &nextEvent,
                      std::uint32_t &nextDeviceEvent) noexcept;
    void route(MidiEvent event) noexcept;
    bool runAction(int action) noexcept;
    void forward(int device, const MidiEvent &event) noexcept;
    void applyCc(int channel, int cc, int value) noexcept;
    int resolveFollowFocus(const Mapping &m) const noexcept;

    std::uint32_t m_maxFrames;
    std::vector<float> m_silence; // a device input with no rack input behind it
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
    std::array<std::array<std::atomic<std::uint32_t>, 128>, kChannels> m_noteMap;
    std::atomic<std::uint32_t> m_armed{0};
    SpscQueue<Learned, 64> m_learned;
    SpscQueue<Fired, 64> m_fired;
    SpscQueue<MixerMove, 512> m_mixerMoves; // faders send many

    MasterStage m_master;
};

} // namespace krate
