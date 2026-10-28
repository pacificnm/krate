#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <jack/jack.h>

#include "core/MasterStage.h"
#include "host/AudioHost.h"

namespace krate {

class IInstrument;
class Transport;

// Standalone host: one JACK client running one instrument
// (docs/SHARED_FOUNDATIONS.MD#threads-standalone-and-rack). Knows nothing
// about any particular instrument. Per block, in the JACK realtime callback:
// read MIDI, advance the transport, run the instrument, then the master stage
// (soft clip + meter) into the output ports. Public methods are for the GUI
// thread; the callbacks only touch atomics, the instrument and the transport.
class JackHost final : public AudioHost
{
public:
    static constexpr int kMaxChannels = 8;

    JackHost(IInstrument &instrument, Transport &transport);
    ~JackHost();

    JackHost(const JackHost &) = delete;
    JackHost &operator=(const JackHost &) = delete;

    // Opens the client without starting a server, registers one output port per
    // instrument channel (out_L/out_R for stereo) and midi_in, then activates.
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
    float takePeak() noexcept override { return m_master.takePeak(); }

    // Connects the outputs to the first physical playback ports (a mono device
    // gets every channel). Returns the names of the ports connected to.
    std::vector<std::string> connectToPlayback() override;

    // MIDI output ports of other clients (candidates to feed midi_in).
    std::vector<std::string> midiSourcePorts() const override;
    bool connectMidiFrom(const std::string &port) override;
    bool midiInputConnected() const override;
    // True once after ports appear or disappear (e.g. a controller is plugged in).
    bool takePortsChanged() override { return m_portsChanged.exchange(false, std::memory_order_relaxed); }

private:
    static int onProcess(jack_nframes_t frames, void *arg);
    static int onXrun(void *arg);
    static int onSampleRate(jack_nframes_t rate, void *arg);
    static int onBufferSize(jack_nframes_t frames, void *arg);
    static void onShutdown(void *arg);
    static void onPortRegistration(jack_port_id_t port, int registered, void *arg);

    IInstrument &m_instrument;
    Transport &m_transport;
    const int m_channels;
    jack_client_t *m_client = nullptr;
    std::array<jack_port_t *, kMaxChannels> m_outs{};
    jack_port_t *m_midiIn = nullptr;
    MasterStage m_master;

    std::uint32_t m_preparedRate = 0; // realtime thread only

    std::atomic<std::uint32_t> m_sampleRate{0};
    std::atomic<std::uint32_t> m_bufferSize{0};
    std::atomic<std::uint32_t> m_xruns{0};
    std::atomic<bool> m_serverGone{false};
    std::atomic<bool> m_portsChanged{false};
};

} // namespace krate
