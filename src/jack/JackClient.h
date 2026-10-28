#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <jack/jack.h>

namespace krate {
class IInstrument;
}

namespace k303 {

// Owns the JACK client and its ports and runs the instrument's process() in
// the JACK realtime callback. All public methods are for the GUI thread; the callbacks
// only touch atomics and the engine.
class JackClient
{
public:
    explicit JackClient(krate::IInstrument &instrument);
    ~JackClient();

    JackClient(const JackClient &) = delete;
    JackClient &operator=(const JackClient &) = delete;

    // Opens the client without starting a server, registers ports, activates.
    // On failure returns false and fills *error.
    bool open(const char *clientName, std::string *error);
    void close();

    bool isOpen() const { return m_client != nullptr; }
    // True once the server has shut the client down; call close() and retry later.
    bool serverGone() const { return m_serverGone.load(std::memory_order_relaxed); }

    std::uint32_t sampleRate() const { return m_sampleRate.load(std::memory_order_relaxed); }
    std::uint32_t bufferSize() const { return m_bufferSize.load(std::memory_order_relaxed); }
    std::uint32_t xrunCount() const { return m_xruns.load(std::memory_order_relaxed); }

    // Connects out_L/out_R to the first two physical playback ports.
    // Returns the names of the ports connected to.
    std::vector<std::string> connectToPlayback();

    // MIDI output ports of other clients (candidates to feed midi_in).
    std::vector<std::string> midiSourcePorts() const;
    bool connectMidiFrom(const std::string &port);
    bool midiInputConnected() const;
    // True once after ports appear or disappear (e.g. a controller is plugged in).
    bool takePortsChanged() { return m_portsChanged.exchange(false, std::memory_order_relaxed); }

private:
    static int onProcess(jack_nframes_t frames, void *arg);
    static int onXrun(void *arg);
    static int onSampleRate(jack_nframes_t rate, void *arg);
    static int onBufferSize(jack_nframes_t frames, void *arg);
    static void onShutdown(void *arg);
    static void onPortRegistration(jack_port_id_t port, int registered, void *arg);

    krate::IInstrument &m_instrument;
    jack_client_t *m_client = nullptr;
    jack_port_t *m_outL = nullptr;
    jack_port_t *m_outR = nullptr;
    jack_port_t *m_midiIn = nullptr;

    std::uint32_t m_engineRate = 0; // realtime thread only

    std::atomic<std::uint32_t> m_sampleRate{0};
    std::atomic<std::uint32_t> m_bufferSize{0};
    std::atomic<std::uint32_t> m_xruns{0};
    std::atomic<bool> m_serverGone{false};
    std::atomic<bool> m_portsChanged{false};
};

} // namespace k303
