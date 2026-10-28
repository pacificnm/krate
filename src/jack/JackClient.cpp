#include "jack/JackClient.h"

#include "dsp/Denormals.h"
#include "dsp/Engine.h"

namespace k303 {

namespace {

// libjack prints to stderr on every failed connect; the app reports status itself.
void silentJackMessage(const char *) {}

} // namespace

JackClient::JackClient(Engine &engine)
    : m_engine(engine)
{
    jack_set_error_function(silentJackMessage);
    jack_set_info_function(silentJackMessage);
}

JackClient::~JackClient()
{
    close();
}

bool JackClient::open(const char *clientName, std::string *error)
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

    m_outL = jack_port_register(m_client, "out_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    m_outR = jack_port_register(m_client, "out_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!m_outL || !m_outR) {
        if (error)
            *error = "could not register output ports";
        close();
        return false;
    }

    const jack_nframes_t rate = jack_get_sample_rate(m_client);
    m_sampleRate.store(rate, std::memory_order_relaxed);
    m_bufferSize.store(jack_get_buffer_size(m_client), std::memory_order_relaxed);
    m_engine.setSampleRate(rate);
    m_engineRate = rate;

    jack_set_process_callback(m_client, &JackClient::onProcess, this);
    jack_set_xrun_callback(m_client, &JackClient::onXrun, this);
    jack_set_sample_rate_callback(m_client, &JackClient::onSampleRate, this);
    jack_set_buffer_size_callback(m_client, &JackClient::onBufferSize, this);
    jack_on_shutdown(m_client, &JackClient::onShutdown, this);

    if (jack_activate(m_client) != 0) {
        if (error)
            *error = "jack_activate failed";
        close();
        return false;
    }
    return true;
}

void JackClient::close()
{
    if (!m_client)
        return;
    // After a server shutdown the client is a zombie: closing it is still the right cleanup.
    if (!m_serverGone.load(std::memory_order_relaxed))
        jack_deactivate(m_client);
    jack_client_close(m_client);
    m_client = nullptr;
    m_outL = nullptr;
    m_outR = nullptr;
}

std::vector<std::string> JackClient::connectToPlayback()
{
    std::vector<std::string> connected;
    if (!m_client)
        return connected;

    const char **ports = jack_get_ports(m_client, nullptr, JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsPhysical | JackPortIsInput);
    if (!ports)
        return connected;

    jack_port_t *outs[] = {m_outL, m_outR};
    for (int i = 0; i < 2 && ports[i]; ++i) {
        if (jack_connect(m_client, jack_port_name(outs[i]), ports[i]) == 0)
            connected.emplace_back(ports[i]);
    }
    // Mono playback device: send both channels to its only port.
    if (ports[0] && !ports[1]) {
        if (jack_connect(m_client, jack_port_name(m_outR), ports[0]) == 0)
            connected.emplace_back(ports[0]);
    }
    jack_free(ports);
    return connected;
}

int JackClient::onProcess(jack_nframes_t frames, void *arg)
{
    auto *self = static_cast<JackClient *>(arg);
    ScopedFlushDenormals ftz;

    const std::uint32_t rate = self->m_sampleRate.load(std::memory_order_relaxed);
    if (rate != self->m_engineRate) {
        self->m_engine.setSampleRate(rate);
        self->m_engineRate = rate;
    }

    auto *outL = static_cast<float *>(jack_port_get_buffer(self->m_outL, frames));
    auto *outR = static_cast<float *>(jack_port_get_buffer(self->m_outR, frames));
    self->m_engine.process(outL, outR, frames);
    return 0;
}

int JackClient::onXrun(void *arg)
{
    static_cast<JackClient *>(arg)->m_xruns.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int JackClient::onSampleRate(jack_nframes_t rate, void *arg)
{
    static_cast<JackClient *>(arg)->m_sampleRate.store(rate, std::memory_order_relaxed);
    return 0;
}

int JackClient::onBufferSize(jack_nframes_t frames, void *arg)
{
    static_cast<JackClient *>(arg)->m_bufferSize.store(frames, std::memory_order_relaxed);
    return 0;
}

void JackClient::onShutdown(void *arg)
{
    static_cast<JackClient *>(arg)->m_serverGone.store(true, std::memory_order_relaxed);
}

} // namespace k303
