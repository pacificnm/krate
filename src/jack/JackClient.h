#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <jack/jack.h>

namespace k303 {

class Engine;

// Owns the JACK client and its ports and runs Engine::process in the JACK
// realtime callback. All public methods are for the GUI thread; the callbacks
// only touch atomics and the engine.
class JackClient
{
public:
    explicit JackClient(Engine &engine);
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

private:
    static int onProcess(jack_nframes_t frames, void *arg);
    static int onXrun(void *arg);
    static int onSampleRate(jack_nframes_t rate, void *arg);
    static int onBufferSize(jack_nframes_t frames, void *arg);
    static void onShutdown(void *arg);

    Engine &m_engine;
    jack_client_t *m_client = nullptr;
    jack_port_t *m_outL = nullptr;
    jack_port_t *m_outR = nullptr;

    std::uint32_t m_engineRate = 0; // realtime thread only

    std::atomic<std::uint32_t> m_sampleRate{0};
    std::atomic<std::uint32_t> m_bufferSize{0};
    std::atomic<std::uint32_t> m_xruns{0};
    std::atomic<bool> m_serverGone{false};
};

} // namespace k303
