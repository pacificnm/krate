#include "host/JackHost.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>

#include <jack/midiport.h>

#include "core/Denormals.h"

namespace krate {

namespace {

// libjack prints to stderr on every failed connect; the app reports status itself.
void silentJackMessage(const char *) {}

} // namespace

JackHost::JackHost(JackProcessor &processor)
    : m_processor(processor)
    , m_channels(std::clamp(processor.outputCount(), 1, kMaxChannels))
    , m_inputs(std::clamp(processor.inputCount(), 0, kMaxInputs))
{
    jack_set_error_function(silentJackMessage);
    jack_set_info_function(silentJackMessage);
}

JackHost::~JackHost()
{
    close();
}

bool JackHost::open(const char *clientName, std::string *error)
{
    if (m_client)
        return true;

    jack_status_t status{};
    m_client = jack_client_open(clientName, JackNoStartServer, &status);
    if (!m_client) {
        if (error)
            *error = (status & JackServerFailed) ? "no JACK/PipeWire server" : "jack_client_open failed";
        return false;
    }

    m_serverGone.store(false, std::memory_order_relaxed);
    m_xruns.store(0, std::memory_order_relaxed);

    bool portsOk = true;
    for (int c = 0; c < m_channels; ++c) {
        if (!m_processor.outputWanted(c))
            continue;
        const std::string name = m_processor.outputName(c);
        m_outs[std::size_t(c)] = jack_port_register(m_client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        portsOk = portsOk && m_outs[std::size_t(c)];
    }
    for (int c = 0; c < m_inputs; ++c) {
        const std::string name = m_processor.inputName(c);
        m_ins[std::size_t(c)] = jack_port_register(m_client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        portsOk = portsOk && m_ins[std::size_t(c)];
    }
    m_midiIn = jack_port_register(m_client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    if (!portsOk || !m_midiIn) {
        if (error)
            *error = "could not register ports";
        close();
        return false;
    }

    const jack_nframes_t rate = jack_get_sample_rate(m_client);
    m_sampleRate.store(rate, std::memory_order_relaxed);
    m_bufferSize.store(jack_get_buffer_size(m_client), std::memory_order_relaxed);
    m_processor.prepare(rate, jack_get_buffer_size(m_client));
    m_preparedRate = rate;

    jack_set_process_callback(m_client, &JackHost::onProcess, this);
    jack_set_xrun_callback(m_client, &JackHost::onXrun, this);
    jack_set_sample_rate_callback(m_client, &JackHost::onSampleRate, this);
    jack_set_buffer_size_callback(m_client, &JackHost::onBufferSize, this);
    jack_on_shutdown(m_client, &JackHost::onShutdown, this);
    jack_set_port_registration_callback(m_client, &JackHost::onPortRegistration, this);
    m_portsChanged.store(true, std::memory_order_relaxed); // look for MIDI sources once at start

    if (jack_activate(m_client) != 0) {
        if (error)
            *error = "jack_activate failed";
        close();
        return false;
    }
    return true;
}

void JackHost::close()
{
    if (!m_client)
        return;
    // After a server shutdown the client is a zombie: closing it is still the right cleanup.
    if (!m_serverGone.load(std::memory_order_relaxed))
        jack_deactivate(m_client);
    jack_client_close(m_client);
    m_client = nullptr;
    for (auto &out : m_outs)
        out.store(nullptr);
    m_ins.fill(nullptr);
    m_midiIn = nullptr;
}

namespace {

std::vector<std::string> portList(jack_client_t *client, unsigned long flags)
{
    std::vector<std::string> result;
    if (const char **ports = jack_get_ports(client, nullptr, JACK_DEFAULT_AUDIO_TYPE, flags)) {
        for (int i = 0; ports[i]; ++i)
            result.emplace_back(ports[i]);
        jack_free(ports);
    }
    return result;
}

std::string clientOf(const std::string &port) { return port.substr(0, port.find(':')); }

} // namespace

std::vector<std::string> JackHost::connectToPlayback(const std::string &device)
{
    std::vector<std::string> connected;
    if (!m_client)
        return connected;

    std::vector<std::string> ports;
    for (const auto &port : portList(m_client, JackPortIsPhysical | JackPortIsInput))
        if (device.empty() || clientOf(port) == device)
            ports.push_back(port);
    if (ports.empty())
        return connected;

    // Output c goes to the playback port the processor names; with fewer playback
    // ports, wrap around (a mono device gets every channel).
    for (int c = 0; c < m_channels; ++c) {
        const int channel = m_processor.playbackChannel(c);
        if (channel < 0)
            continue;
        jack_port_t *out = m_outs[std::size_t(c)];
        if (!out)
            continue;
        if (const char **old = jack_port_get_connections(out)) { // a different device before
            for (int i = 0; old[i]; ++i)
                jack_disconnect(m_client, jack_port_name(out), old[i]);
            jack_free(old);
        }
        const std::string &target = ports[std::size_t(channel) % ports.size()];
        if (jack_connect(m_client, jack_port_name(out), target.c_str()) == 0)
            connected.push_back(target);
    }
    return connected;
}

std::vector<std::string> JackHost::playbackDevices() const
{
    std::vector<std::string> devices;
    if (!m_client)
        return devices;
    for (const auto &port : portList(m_client, JackPortIsPhysical | JackPortIsInput))
        if (const std::string client = clientOf(port); std::find(devices.begin(), devices.end(), client) == devices.end())
            devices.push_back(client);
    return devices;
}

std::vector<std::string> JackHost::midiSourcePorts() const
{
    std::vector<std::string> result;
    if (!m_client)
        return result;
    const char **ports = jack_get_ports(m_client, nullptr, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
    if (!ports)
        return result;
    for (int i = 0; ports[i]; ++i)
        result.emplace_back(ports[i]);
    jack_free(ports);
    return result;
}

bool JackHost::connectMidiFrom(const std::string &port)
{
    return m_client && jack_connect(m_client, port.c_str(), jack_port_name(m_midiIn)) == 0;
}

bool JackHost::midiInputConnected() const
{
    return m_midiIn && jack_port_connected(m_midiIn) > 0;
}

std::vector<std::string> JackHost::captureSourcePorts() const
{
    // Hardware capture ports, and PipeWire's per-input sources (a USB interface's
    // "Input 1 Mic/Inst/Line:capture_MONO"), which are not marked physical.
    std::vector<std::string> result;
    if (!m_client)
        return result;
    for (const auto &port : portList(m_client, JackPortIsOutput)) {
        const std::string name = port.substr(port.find(':') + 1);
        const jack_port_t *p = jack_port_by_name(m_client, port.c_str());
        if (name.starts_with("capture_") || (p && (jack_port_flags(p) & JackPortIsPhysical)))
            result.push_back(port);
    }
    std::stable_partition(result.begin(), result.end(), // the named per-input sources first
                          [](const std::string &port) { return port.ends_with(":capture_MONO"); });
    return result;
}

bool JackHost::connectInput(int input, const std::string &port)
{
    if (!m_client || input < 0 || input >= m_inputs)
        return false;
    jack_port_t *in = m_ins[std::size_t(input)];
    if (const char **connected = jack_port_get_connections(in)) {
        bool already = false;
        for (int i = 0; connected[i]; ++i) {
            if (port == connected[i])
                already = true;
            else
                jack_disconnect(m_client, connected[i], jack_port_name(in));
        }
        jack_free(connected);
        if (already)
            return true;
    }
    return !port.empty() && jack_connect(m_client, port.c_str(), jack_port_name(in)) == 0;
}

bool JackHost::inputConnected(int input) const
{
    return m_client && input >= 0 && input < m_inputs && jack_port_connected(m_ins[std::size_t(input)]) > 0;
}

float JackHost::takeInputPeak(int input) noexcept
{
    if (input < 0 || input >= m_inputs)
        return 0.0f;
    return m_inputPeaks[std::size_t(input)].exchange(0.0f, std::memory_order_relaxed);
}

bool JackHost::setBufferSize(std::uint32_t frames)
{
    return m_client && frames > 0 && jack_set_buffer_size(m_client, frames) == 0;
}

std::uint32_t JackHost::roundTripFrames() const
{
    if (!m_client || m_channels < 1)
        return 0;
    jack_latency_range_t capture{0, 0}, playback{0, 0};
    if (m_inputs > 0)
        jack_port_get_latency_range(m_ins[0], JackCaptureLatency, &capture);
    jack_port_get_latency_range(m_outs[0], JackPlaybackLatency, &playback);
    return capture.max + playback.max;
}

void JackHost::updateOutputs()
{
    if (!m_client || m_serverGone.load(std::memory_order_relaxed))
        return;
    for (int c = 0; c < m_channels; ++c) {
        if (m_outs[std::size_t(c)].load(std::memory_order_relaxed) || !m_processor.outputWanted(c))
            continue;
        const std::string name = m_processor.outputName(c);
        // Published after it's made: the next cycle writes to it.
        if (jack_port_t *port = jack_port_register(m_client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0))
            m_outs[std::size_t(c)].store(port, std::memory_order_release);
    }
}

void JackHost::suspend()
{
    m_suspended.store(true, std::memory_order_seq_cst);
    if (!m_client)
        return; // no callbacks running (one starting later sees the flag)
    // Callbacks run one after another: once one has begun since the flag was
    // set (it sees it), the one before has finished.
    const std::uint64_t seen = m_cycles.load(std::memory_order_seq_cst);
    for (int waited = 0; m_cycles.load(std::memory_order_seq_cst) == seen && waited < 500; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

int JackHost::onProcess(jack_nframes_t frames, void *arg)
{
    auto *self = static_cast<JackHost *>(arg);
    ScopedFlushDenormals ftz;
    self->m_cycles.fetch_add(1, std::memory_order_seq_cst);
    if (self->m_suspended.load(std::memory_order_seq_cst)) { // another thread has the processor (exporting)
        for (int c = 0; c < self->m_channels; ++c)
            if (jack_port_t *port = self->m_outs[std::size_t(c)].load(std::memory_order_acquire))
                std::fill_n(static_cast<float *>(jack_port_get_buffer(port, frames)), frames, 0.0f);
        return 0;
    }

    const std::uint32_t rate = self->m_sampleRate.load(std::memory_order_relaxed);
    if (rate != self->m_preparedRate) {
        self->m_processor.prepare(rate, frames);
        self->m_preparedRate = rate;
    }

    float *outputs[kMaxChannels];
    for (int c = 0; c < self->m_channels; ++c) {
        jack_port_t *port = self->m_outs[std::size_t(c)].load(std::memory_order_acquire);
        outputs[c] = port ? static_cast<float *>(jack_port_get_buffer(port, frames)) : nullptr;
    }

    const float *inputs[kMaxInputs];
    for (int c = 0; c < self->m_inputs; ++c) {
        const auto *in = static_cast<const float *>(jack_port_get_buffer(self->m_ins[std::size_t(c)], frames));
        inputs[c] = in;
        float peak = 0.0f;
        for (jack_nframes_t s = 0; s < frames; ++s)
            peak = std::max(peak, std::fabs(in[s]));
        std::atomic<float> &held = self->m_inputPeaks[std::size_t(c)];
        if (peak > held.load(std::memory_order_relaxed))
            held.store(peak, std::memory_order_relaxed);
    }

    // Copy this block's MIDI into a fixed array; the engine applies each at its frame.
    constexpr std::uint32_t kMaxEvents = 256;
    MidiEvent events[kMaxEvents];
    std::uint32_t count = 0;
    void *midi = jack_port_get_buffer(self->m_midiIn, frames);
    const std::uint32_t available = jack_midi_get_event_count(midi);
    for (std::uint32_t i = 0; i < available && count < kMaxEvents; ++i) {
        jack_midi_event_t ev;
        if (jack_midi_event_get(&ev, midi, i) != 0 || ev.size < 1 || ev.size > 3 || ev.time >= frames)
            continue; // SysEx and malformed events are ignored
        MidiEvent &out = events[count++];
        out.frame = ev.time;
        out.size = std::uint8_t(ev.size);
        for (std::size_t b = 0; b < ev.size; ++b)
            out.data[b] = ev.buffer[b];
    }

    // JACK's transport: when a timebase master (a DAW) publishes tempo and
    // position, the processor's transport follows it (docs/SHARED_FOUNDATIONS.MD#transport).
    Transport *transport = self->m_processor.transport();
    if (transport) {
        jack_position_t pos;
        const jack_transport_state_t state = jack_transport_query(self->m_client, &pos);
        if ((pos.valid & JackPositionBBT) && pos.beats_per_minute > 0.0 && pos.beat_type > 0.0f && pos.ticks_per_beat > 0.0) {
            const Transport::External external =
                Transport::fromBbt(state == JackTransportRolling, pos.bar, pos.beat, double(pos.tick), pos.ticks_per_beat,
                                   double(pos.beats_per_bar), double(pos.beat_type), pos.beats_per_minute);
            transport->setExternal(&external);
            self->m_syncBpm.store(external.bpm, std::memory_order_relaxed);
        } else {
            transport->setExternal(nullptr);
            self->m_syncBpm.store(0.0, std::memory_order_relaxed);
        }
    }

    self->m_processor.process(inputs, outputs, frames, events, count, double(rate));

    if (transport) // PLAY / STOP pressed while following: start or stop JACK's transport
        switch (transport->takeRequest()) {
        case Transport::Start: jack_transport_start(self->m_client); break;
        case Transport::Stop: jack_transport_stop(self->m_client); break;
        case Transport::None: break;
        }
    return 0;
}

int JackHost::onXrun(void *arg)
{
    static_cast<JackHost *>(arg)->m_xruns.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int JackHost::onSampleRate(jack_nframes_t rate, void *arg)
{
    static_cast<JackHost *>(arg)->m_sampleRate.store(rate, std::memory_order_relaxed);
    return 0;
}

int JackHost::onBufferSize(jack_nframes_t frames, void *arg)
{
    static_cast<JackHost *>(arg)->m_bufferSize.store(frames, std::memory_order_relaxed);
    return 0;
}

void JackHost::onPortRegistration(jack_port_id_t, int, void *arg)
{
    static_cast<JackHost *>(arg)->m_portsChanged.store(true, std::memory_order_relaxed);
}

void JackHost::onShutdown(void *arg)
{
    static_cast<JackHost *>(arg)->m_serverGone.store(true, std::memory_order_relaxed);
}

} // namespace krate
