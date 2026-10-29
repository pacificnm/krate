#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <jack/jack.h>

#include "host/AudioHost.h"
#include "host/JackProcessor.h"

namespace krate {

// A JACK client running a JackProcessor: one instrument (InstrumentProcessor,
// standalone apps) or the whole rack (RackProcessor, the krate app)
// (docs/SHARED_FOUNDATIONS.MD#threads-standalone-and-rack). Per block, in the
// JACK realtime callback: read MIDI, hand it with the output buffers to the
// processor, with the input buffers. Public methods are for the GUI thread; the callbacks only touch
// atomics and the processor.
class JackHost final : public AudioHost
{
public:
    static constexpr int kMaxChannels = 32;
    static constexpr int kMaxInputs = 8;

    explicit JackHost(JackProcessor &processor);
    ~JackHost();

    JackHost(const JackHost &) = delete;
    JackHost &operator=(const JackHost &) = delete;

    // Opens the client without starting a server, registers the processor's
    // output and input ports and midi_in, then activates.
    // On failure returns false and fills *error.
    bool open(const char *clientName, std::string *error) override;
    void close() override;

    bool isOpen() const override { return m_client != nullptr; }
    // True once the server has shut the client down; call close() and retry later.
    bool serverGone() const override { return m_serverGone.load(std::memory_order_relaxed); }

    std::uint32_t sampleRate() const override { return m_sampleRate.load(std::memory_order_relaxed); }
    std::uint32_t bufferSize() const override { return m_bufferSize.load(std::memory_order_relaxed); }
    std::uint32_t xrunCount() const override { return m_xruns.load(std::memory_order_relaxed); }

    // Master output peak since the last call (0..1), for the level meter.
    float takePeak() noexcept override { return m_processor.takePeak(); }

    // Connects each output the processor routes to the speakers to that playback
    // port of `device` (or of the hardware, ""); a mono device gets every channel.
    std::vector<std::string> connectToPlayback(const std::string &device = {}) override;
    std::vector<std::string> playbackDevices() const override;

    // MIDI output ports of other clients (candidates to feed midi_in).
    std::vector<std::string> midiSourcePorts() const override;
    bool connectMidiFrom(const std::string &port) override;
    bool midiInputConnected() const override;
    // True once after ports appear or disappear (e.g. a controller is plugged in).
    bool takePortsChanged() override { return m_portsChanged.exchange(false, std::memory_order_relaxed); }

    int inputCount() const override { return m_inputs; }
    std::vector<std::string> captureSourcePorts() const override;
    bool connectInput(int input, const std::string &port) override;
    bool inputConnected(int input) const override;
    float takeInputPeak(int input) noexcept override;
    bool setBufferSize(std::uint32_t frames) override;
    std::uint32_t roundTripFrames() const override;

private:
    static int onProcess(jack_nframes_t frames, void *arg);
    static int onXrun(void *arg);
    static int onSampleRate(jack_nframes_t rate, void *arg);
    static int onBufferSize(jack_nframes_t frames, void *arg);
    static void onShutdown(void *arg);
    static void onPortRegistration(jack_port_id_t port, int registered, void *arg);

    JackProcessor &m_processor;
    const int m_channels;
    const int m_inputs;
    jack_client_t *m_client = nullptr;
    std::array<jack_port_t *, kMaxChannels> m_outs{};
    std::array<jack_port_t *, kMaxInputs> m_ins{};
    std::array<std::atomic<float>, kMaxInputs> m_inputPeaks{};
    jack_port_t *m_midiIn = nullptr;

    std::uint32_t m_preparedRate = 0; // realtime thread only

    std::atomic<std::uint32_t> m_sampleRate{0};
    std::atomic<std::uint32_t> m_bufferSize{0};
    std::atomic<std::uint32_t> m_xruns{0};
    std::atomic<bool> m_serverGone{false};
    std::atomic<bool> m_portsChanged{false};
};

} // namespace krate
