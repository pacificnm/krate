#include "plugin/Clap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include <dlfcn.h>

namespace krate::clap {

namespace {

std::string percentText(float v) { return std::to_string(int(std::lround(v * 100.0f))) + "%"; }

bool hasFeature(const clap_plugin_descriptor *d, const char *feature)
{
    if (!d->features)
        return false;
    for (const char *const *f = d->features; *f; ++f)
        if (!std::strcmp(*f, feature))
            return true;
    return false;
}

PluginInfo infoOf(const std::string &path, const clap_plugin_descriptor *d)
{
    PluginInfo info;
    info.path = path;
    info.id = d->id ? d->id : "";
    info.name = d->name ? d->name : info.id;
    info.vendor = d->vendor ? d->vendor : "";
    info.version = d->version ? d->version : "";
    info.description = d->description ? d->description : "";
    info.instrument = hasFeature(d, CLAP_PLUGIN_FEATURE_INSTRUMENT);
    info.effect = hasFeature(d, CLAP_PLUGIN_FEATURE_AUDIO_EFFECT);
    return info;
}

} // namespace

// --- Library --------------------------------------------------------------------------

std::shared_ptr<Library> Library::open(const std::string &path, std::string *error)
{
    const auto fail = [&](const std::string &why) -> std::shared_ptr<Library> {
        if (error)
            *error = path + ": " + why;
        return nullptr;
    };
    void *handle = dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW);
    if (!handle)
        return fail(dlerror());
    const auto *entry = static_cast<const clap_plugin_entry *>(dlsym(handle, "clap_entry"));
    if (!entry || !clap_version_is_compatible(entry->clap_version)) {
        dlclose(handle);
        return fail("not a CLAP plugin this host can load");
    }
    if (!entry->init(path.c_str())) {
        dlclose(handle);
        return fail("its entry wouldn't start");
    }
    std::shared_ptr<Library> library(new Library);
    library->m_path = path;
    library->m_handle = handle;
    library->m_entry = entry;
    library->m_factory = static_cast<const clap_plugin_factory *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!library->m_factory)
        return fail("no plugin factory");
    return library;
}

Library::~Library()
{
    if (m_entry)
        m_entry->deinit();
    if (m_handle)
        dlclose(m_handle);
}

std::vector<PluginInfo> Library::plugins() const
{
    std::vector<PluginInfo> list;
    if (!m_factory)
        return list;
    const std::uint32_t count = m_factory->get_plugin_count(m_factory);
    for (std::uint32_t i = 0; i < count; ++i)
        if (const clap_plugin_descriptor *d = m_factory->get_plugin_descriptor(m_factory, i); d && d->id)
            list.push_back(infoOf(m_path, d));
    return list;
}

std::vector<std::string> defaultSearchPaths()
{
    std::vector<std::string> dirs;
    if (const char *env = std::getenv("CLAP_PATH"))
        for (std::string rest = env; !rest.empty();) {
            const std::size_t colon = rest.find(':');
            if (const std::string dir = rest.substr(0, colon); !dir.empty())
                dirs.push_back(dir);
            rest = colon == std::string::npos ? "" : rest.substr(colon + 1);
        }
    if (const char *home = std::getenv("HOME"))
        dirs.push_back(std::string(home) + "/.clap");
    for (const char *dir : {"/usr/lib/clap", "/usr/local/lib/clap", "/usr/lib/x86_64-linux-gnu/clap", "/usr/lib64/clap"})
        dirs.push_back(dir);
    return dirs;
}

std::vector<PluginInfo> scan(const std::vector<std::string> &dirs)
{
    namespace fs = std::filesystem;
    std::vector<PluginInfo> found;
    std::vector<std::string> seen;
    for (const std::string &dir : dirs) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec))
            continue;
        for (fs::recursive_directory_iterator it(dir, fs::directory_options::follow_directory_symlink
                                                          | fs::directory_options::skip_permission_denied,
                                                 ec),
             end;
             it != end; it.increment(ec)) {
            if (ec || it->path().extension() != ".clap" || !it->is_regular_file(ec))
                continue;
            const std::string canonical = fs::weakly_canonical(it->path(), ec).string();
            if (std::find(seen.begin(), seen.end(), canonical) != seen.end())
                continue; // the same file by another way
            seen.push_back(canonical);
            if (const auto library = Library::open(it->path().string()))
                for (PluginInfo &info : library->plugins())
                    found.push_back(std::move(info));
        }
    }
    std::stable_sort(found.begin(), found.end(), [](const PluginInfo &a, const PluginInfo &b) { return a.name < b.name; });
    return found;
}

// --- The host's extensions ------------------------------------------------------------

namespace {

void hostLog(const clap_host *, clap_log_severity severity, const char *msg)
{
    if (severity >= CLAP_LOG_WARNING && msg)
        std::fprintf(stderr, "krate.plugin: %s\n", msg);
}
const clap_host_log kLog{hostLog};

} // namespace

const void *Instance::hostExtension(const clap_host *host, const char *id)
{
    static const clap_host_thread_check threadCheck{
        [](const clap_host *h) { return std::this_thread::get_id() == from(h)->m_mainThread; },
        [](const clap_host *h) { return std::this_thread::get_id() != from(h)->m_mainThread; }};
    static const clap_host_params params{
        [](const clap_host *h, clap_param_rescan_flags) { from(h)->m_services->paramsChanged(); },
        [](const clap_host *, clap_id, clap_param_clear_flags) {},
        [](const clap_host *h) { from(h)->m_services->requestCallback(); }};
    static const clap_host_state state{[](const clap_host *h) { from(h)->m_services->stateDirty(); }};
    static const clap_host_gui gui{
        [](const clap_host *) {},
        [](const clap_host *h, std::uint32_t w, std::uint32_t hgt) { return from(h)->m_services->guiRequestResize(w, hgt); },
        [](const clap_host *h) { return from(h)->m_services->guiRequestShow(); },
        [](const clap_host *h) { return from(h)->m_services->guiRequestHide(); },
        [](const clap_host *h, bool destroyed) { from(h)->m_services->guiClosed(destroyed); }};
    static const clap_host_timer_support timer{
        [](const clap_host *h, std::uint32_t ms, clap_id *tid) { return from(h)->m_services->registerTimer(ms, tid); },
        [](const clap_host *h, clap_id tid) { return from(h)->m_services->unregisterTimer(tid); }};
    static const clap_host_posix_fd_support fd{
        [](const clap_host *h, int f, clap_posix_fd_flags_t fl) { return from(h)->m_services->registerFd(f, fl); },
        [](const clap_host *h, int f, clap_posix_fd_flags_t fl) { return from(h)->m_services->modifyFd(f, fl); },
        [](const clap_host *h, int f) { return from(h)->m_services->unregisterFd(f); }};
    static const clap_host_latency latency{[](const clap_host *) {}};
    static const clap_host_audio_ports audioPorts{[](const clap_host *, std::uint32_t) { return false; },
                                                  [](const clap_host *h, std::uint32_t) { from(h)->m_services->requestRestart(); }};
    static const clap_host_note_ports notePorts{
        [](const clap_host *) -> std::uint32_t { return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI; },
        [](const clap_host *h, std::uint32_t) { from(h)->m_services->requestRestart(); }};

    if (!std::strcmp(id, CLAP_EXT_LOG))
        return &kLog;
    if (!std::strcmp(id, CLAP_EXT_THREAD_CHECK))
        return &threadCheck;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &params;
    if (!std::strcmp(id, CLAP_EXT_STATE))
        return &state;
    if (!std::strcmp(id, CLAP_EXT_GUI))
        return &gui;
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &timer;
    if (!std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT))
        return &fd;
    if (!std::strcmp(id, CLAP_EXT_LATENCY))
        return &latency;
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
        return &audioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &notePorts;
    (void)host;
    return nullptr;
}

void Instance::hostRequestRestart(const clap_host *host) { from(host)->m_services->requestRestart(); }
void Instance::hostRequestProcess(const clap_host *) {}
void Instance::hostRequestCallback(const clap_host *host) { from(host)->m_services->requestCallback(); }

// --- Instance -------------------------------------------------------------------------

std::unique_ptr<Instance> Instance::create(std::shared_ptr<Library> library, const std::string &pluginId,
                                           HostServices *services, std::string *error)
{
    const auto fail = [&](const std::string &why) -> std::unique_ptr<Instance> {
        if (error)
            *error = pluginId + ": " + why;
        return nullptr;
    };
    if (!library || !library->factory() || !services)
        return fail("no plugin library");
    std::unique_ptr<Instance> self(new Instance);
    self->m_library = std::move(library);
    self->m_services = services;
    self->m_mainThread = std::this_thread::get_id();
    self->m_host = clap_host{CLAP_VERSION_INIT, self.get(), "Krate", "Krate", "", "1.0",
                             &Instance::hostExtension, &Instance::hostRequestRestart, &Instance::hostRequestProcess,
                             &Instance::hostRequestCallback};
    const clap_plugin_factory *factory = self->m_library->factory();
    self->m_plugin = factory->create_plugin(factory, &self->m_host, pluginId.c_str());
    if (!self->m_plugin)
        return fail("the library has no such plugin");
    self->m_info = infoOf(self->m_library->path(), self->m_plugin->desc);
    if (!self->m_plugin->init(self->m_plugin)) {
        self->m_plugin->destroy(self->m_plugin);
        self->m_plugin = nullptr;
        return fail("it wouldn't start");
    }
    const auto ext = [&](const char *id) { return self->m_plugin->get_extension(self->m_plugin, id); };
    self->m_params = static_cast<const clap_plugin_params *>(ext(CLAP_EXT_PARAMS));
    self->m_state = static_cast<const clap_plugin_state *>(ext(CLAP_EXT_STATE));
    self->m_gui = static_cast<const clap_plugin_gui *>(ext(CLAP_EXT_GUI));
    self->m_timer = static_cast<const clap_plugin_timer_support *>(ext(CLAP_EXT_TIMER_SUPPORT));
    self->m_fd = static_cast<const clap_plugin_posix_fd_support *>(ext(CLAP_EXT_POSIX_FD_SUPPORT));

    // Its knobs: the parameters it shows (not hidden ones), keyed by id.
    if (self->m_params) {
        const std::uint32_t count = self->m_params->count(self->m_plugin);
        for (std::uint32_t i = 0; i < count; ++i) {
            clap_param_info info{};
            if (!self->m_params->get_info(self->m_plugin, i, &info) || (info.flags & CLAP_PARAM_IS_HIDDEN))
                continue;
            Param p;
            p.id = info.id;
            p.min = info.min_value;
            p.max = info.max_value > info.min_value ? info.max_value : info.min_value + 1.0;
            p.def = info.default_value;
            p.stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
            p.key = "p" + std::to_string(info.id);
            p.label = info.name;
            self->m_paramInfo.push_back(std::move(p));
        }
    }
    const std::size_t n = self->m_paramInfo.size();
    self->m_values = std::make_unique<std::atomic<float>[]>(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Param &p = self->m_paramInfo[i];
        const int range = int(std::lround(p.max - p.min));
        self->m_descriptors.push_back(ParamDescriptor{p.key, p.label, self->toNormalized(int(i), p.def),
                                                      p.stepped && range >= 1 && range <= 32 ? range + 1 : 0, &percentText});
    }
    self->rereadValues();
    self->m_changed.store(0);
    self->readPorts();
    return self;
}

Instance::~Instance()
{
    if (!m_plugin)
        return;
    deactivate();
    m_plugin->destroy(m_plugin);
}

int Instance::indexOf(clap_id id) const
{
    for (std::size_t i = 0; i < m_paramInfo.size(); ++i)
        if (m_paramInfo[i].id == id)
            return int(i);
    return -1;
}

double Instance::toValue(int index, float normalized) const
{
    const Param &p = m_paramInfo[std::size_t(index)];
    const double v = p.min + double(std::clamp(normalized, 0.0f, 1.0f)) * (p.max - p.min);
    return p.stepped ? std::round(v) : v;
}

float Instance::toNormalized(int index, double value) const
{
    const Param &p = m_paramInfo[std::size_t(index)];
    return float(std::clamp((value - p.min) / (p.max - p.min), 0.0, 1.0));
}

void Instance::rereadValues()
{
    if (!m_params)
        return;
    std::uint64_t changed = 0;
    for (std::size_t i = 0; i < m_paramInfo.size(); ++i) {
        double value = 0.0;
        if (!m_params->get_value(m_plugin, m_paramInfo[i].id, &value))
            continue;
        const float n = toNormalized(int(i), value);
        if (m_values[i].exchange(n, std::memory_order_relaxed) != n && i < 64)
            changed |= std::uint64_t(1) << i;
    }
    m_changed.fetch_or(changed);
}

void Instance::readPorts()
{
    m_inChannels.clear();
    m_outChannels.clear();
    const auto *audio = static_cast<const clap_plugin_audio_ports *>(m_plugin->get_extension(m_plugin, CLAP_EXT_AUDIO_PORTS));
    for (const bool input : {true, false}) {
        auto &channels = input ? m_inChannels : m_outChannels;
        const std::uint32_t count = audio ? audio->count(m_plugin, input) : 0;
        for (std::uint32_t i = 0; i < count; ++i) {
            clap_audio_port_info info{};
            channels.push_back(audio->get(m_plugin, i, input, &info) ? std::max<std::uint32_t>(info.channel_count, 1) : 2);
        }
    }
    m_inPorts = std::uint32_t(m_inChannels.size());
    m_outPorts = std::uint32_t(m_outChannels.size());
    // Notes: as CLAP note events if its first note port takes them, else MIDI.
    m_clapNotes = false;
    m_midi = false;
    const auto *notes = static_cast<const clap_plugin_note_ports *>(m_plugin->get_extension(m_plugin, CLAP_EXT_NOTE_PORTS));
    clap_note_port_info info{};
    if (notes && notes->count(m_plugin, true) > 0 && notes->get(m_plugin, 0, true, &info)) {
        m_clapNotes = (info.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0;
        m_midi = (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0;
    }
}

bool Instance::activate(double sampleRate, std::uint32_t maxFrames)
{
    if (m_active.load())
        return true;
    readPorts();
    // Every channel of every port gets a buffer; the main ones are pointed at the rack's when processing.
    std::uint32_t channels = 0;
    for (std::uint32_t c : m_inChannels)
        channels += c;
    for (std::uint32_t c : m_outChannels)
        channels += c;
    m_scratch.assign(channels + 1, std::vector<float>(maxFrames, 0.0f)); // + 1: silence
    m_inPtrs.assign(std::max<std::size_t>(1, channels), nullptr);
    m_outPtrs.assign(std::max<std::size_t>(1, channels), nullptr);
    m_inBuffers.assign(m_inPorts, clap_audio_buffer{});
    m_outBuffers.assign(m_outPorts, clap_audio_buffer{});
    std::uint32_t at = 0;
    for (std::uint32_t p = 0; p < m_inPorts; ++p) {
        m_inBuffers[p] = clap_audio_buffer{m_inPtrs.data() + at, nullptr, m_inChannels[p], 0, 0};
        for (std::uint32_t c = 0; c < m_inChannels[p]; ++c, ++at)
            m_inPtrs[at] = m_scratch[at].data();
    }
    std::uint32_t outAt = 0;
    for (std::uint32_t p = 0; p < m_outPorts; ++p) {
        m_outBuffers[p] = clap_audio_buffer{m_outPtrs.data() + outAt, nullptr, m_outChannels[p], 0, 0};
        for (std::uint32_t c = 0; c < m_outChannels[p]; ++c, ++outAt, ++at)
            m_outPtrs[outAt] = m_scratch[at].data();
    }
    if (!m_plugin->activate(m_plugin, sampleRate, 1, maxFrames))
        return false;
    m_rate = sampleRate;
    m_maxFrames = maxFrames;
    m_restartRate.store(0.0);
    m_stopRequested.store(false, std::memory_order_seq_cst);
    m_active.store(true, std::memory_order_release);
    return true;
}

void Instance::deactivate(int waitMs)
{
    if (!m_active.load())
        return;
    // The audio thread stops it (it may be in process()); without one, this thread does.
    m_stopRequested.store(true, std::memory_order_seq_cst);
    for (int waited = 0; m_processing.load(std::memory_order_seq_cst) && waited < waitMs; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (m_processing.exchange(false, std::memory_order_seq_cst))
        m_plugin->stop_processing(m_plugin);
    m_active.store(false, std::memory_order_release);
    m_plugin->deactivate(m_plugin);
}

bool Instance::saveState(std::vector<std::uint8_t> &out) const
{
    out.clear();
    if (!m_state)
        return false;
    const clap_ostream stream{&out, [](const clap_ostream *s, const void *buffer, std::uint64_t size) -> std::int64_t {
                                  auto *v = static_cast<std::vector<std::uint8_t> *>(s->ctx);
                                  const auto *b = static_cast<const std::uint8_t *>(buffer);
                                  v->insert(v->end(), b, b + size);
                                  return std::int64_t(size);
                              }};
    return m_state->save(m_plugin, &stream);
}

bool Instance::loadState(const std::vector<std::uint8_t> &in)
{
    if (!m_state)
        return false;
    struct Reader
    {
        const std::vector<std::uint8_t> *bytes;
        std::size_t at;
    } reader{&in, 0};
    const clap_istream stream{&reader, [](const clap_istream *s, void *buffer, std::uint64_t size) -> std::int64_t {
                                  auto *r = static_cast<Reader *>(s->ctx);
                                  const std::size_t n = std::min<std::size_t>(std::size_t(size), r->bytes->size() - r->at);
                                  std::memcpy(buffer, r->bytes->data() + r->at, n);
                                  r->at += n;
                                  return std::int64_t(n);
                              }};
    const bool ok = m_state->load(m_plugin, &stream);
    rereadValues();
    return ok;
}

void Instance::onMainThread()
{
    m_plugin->on_main_thread(m_plugin);
    // A flush asked for while it isn't processing: its knobs' changes now.
    if (m_params && !m_processing.load()) {
        static const clap_input_events none{nullptr, [](const clap_input_events *) -> std::uint32_t { return 0; },
                                            [](const clap_input_events *, std::uint32_t) -> const clap_event_header * {
                                                return nullptr;
                                            }};
        const clap_output_events out{this, [](const clap_output_events *list, const clap_event_header *e) {
                                         static_cast<Instance *>(list->ctx)->outputEvent(e);
                                         return true;
                                     }};
        m_params->flush(m_plugin, &none, &out);
    }
}

void Instance::onTimer(clap_id id)
{
    if (m_timer)
        m_timer->on_timer(m_plugin, id);
}

void Instance::onFd(int fd, clap_posix_fd_flags_t flags)
{
    if (m_fd)
        m_fd->on_fd(m_plugin, fd, flags);
}

float Instance::param(int index) const noexcept
{
    return index >= 0 && index < int(m_paramInfo.size()) ? m_values[std::size_t(index)].load(std::memory_order_relaxed) : 0.0f;
}

void Instance::setParam(int index, float normalized) noexcept
{
    if (index < 0 || index >= int(m_paramInfo.size()))
        return;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    m_values[std::size_t(index)].store(normalized, std::memory_order_relaxed);
    m_toAudio.push({std::uint32_t(index), toValue(index, normalized)});
}

void Instance::setParamFromHost(int index, float normalized) noexcept
{
    setParam(index, normalized);
    if (index >= 0 && index < 64)
        m_changed.fetch_or(std::uint64_t(1) << index);
}

void Instance::outputEvent(const clap_event_header *e) noexcept
{
    if (e->space_id != CLAP_CORE_EVENT_SPACE_ID || e->type != CLAP_EVENT_PARAM_VALUE)
        return;
    const auto *p = reinterpret_cast<const clap_event_param_value *>(e);
    const int index = indexOf(p->param_id);
    if (index < 0)
        return;
    m_values[std::size_t(index)].store(toNormalized(index, p->value), std::memory_order_relaxed);
    if (index < 64)
        m_changed.fetch_or(std::uint64_t(1) << index);
}

void Instance::prepare(double sampleRate, std::uint32_t maxFrames) noexcept
{
    if (!m_active.load(std::memory_order_acquire) || m_stopRequested.load())
        return;
    if (sampleRate != m_rate || maxFrames > m_maxFrames) { // the main thread activates it again
        m_restartRate.store(sampleRate);
        m_services->requestRestart();
        return;
    }
    m_plugin->reset(m_plugin); // silent, from scratch
}

void Instance::process(const AudioBlock &block) noexcept
{
    float *outL = block.outputs[0], *outR = block.outputs[1];
    const std::uint32_t frames = block.frames;
    const auto silence = [&] {
        std::fill_n(outL, frames, 0.0f);
        std::fill_n(outR, frames, 0.0f);
    };
    // Stopping: the main thread sets m_stopRequested, then waits for
    // m_processing to drop; starting sets m_processing before looking at the
    // request (both seq_cst), so neither can miss the other.
    if (!m_active.load(std::memory_order_acquire) || frames > m_maxFrames || m_restartRate.load() > 0.0) {
        silence();
        return;
    }
    if (m_stopRequested.load(std::memory_order_seq_cst)) {
        if (m_processing.exchange(false, std::memory_order_seq_cst))
            m_plugin->stop_processing(m_plugin);
        silence();
        return;
    }
    if (!m_processing.load(std::memory_order_seq_cst)) {
        m_processing.store(true, std::memory_order_seq_cst);
        if (m_stopRequested.load(std::memory_order_seq_cst)) { // asked to stop just now: don't start
            m_processing.store(false, std::memory_order_seq_cst);
            silence();
            return;
        }
        if (!m_plugin->start_processing(m_plugin)) {
            m_processing.store(false, std::memory_order_seq_cst);
            silence();
            return;
        }
    }

    // Events: knob changes first (at the block's start), then the notes, in order.
    m_eventCount = 0;
    while (const auto change = m_toAudio.pop()) {
        if (m_eventCount >= kMaxEvents)
            break;
        clap_event_param_value &e = m_events[m_eventCount++].param;
        e = clap_event_param_value{{sizeof(clap_event_param_value), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0},
                                   m_paramInfo[change->index].id, nullptr, -1, -1, -1, -1, change->value};
    }
    for (std::uint32_t i = 0; i < block.midiCount && m_eventCount < kMaxEvents; ++i) {
        const MidiEvent &m = block.midi[i];
        if (m.frame >= frames || m.size < 1)
            continue;
        const int type = m.data[0] & 0xF0, channel = m.data[0] & 0x0F;
        const bool on = type == 0x90 && m.size >= 3 && m.data[2] > 0;
        const bool off = type == 0x80 || (type == 0x90 && m.size >= 3 && m.data[2] == 0);
        if (m_clapNotes && (on || off)) {
            clap_event_note &e = m_events[m_eventCount++].note;
            e = clap_event_note{{sizeof(clap_event_note), m.frame, CLAP_CORE_EVENT_SPACE_ID,
                                 std::uint16_t(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0},
                                -1, 0, std::int16_t(channel), std::int16_t(m.data[1]), on ? m.data[2] / 127.0 : 0.0};
        } else if (m_midi) {
            clap_event_midi &e = m_events[m_eventCount++].midi;
            e = clap_event_midi{{sizeof(clap_event_midi), m.frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0}, 0,
                                {m.data[0], m.size > 1 ? m.data[1] : std::uint8_t(0), m.size > 2 ? m.data[2] : std::uint8_t(0)}};
        }
    }
    const clap_input_events in{this,
                               [](const clap_input_events *list) { return static_cast<const Instance *>(list->ctx)->m_eventCount; },
                               [](const clap_input_events *list, std::uint32_t i) -> const clap_event_header * {
                                   const auto *self = static_cast<const Instance *>(list->ctx);
                                   return i < self->m_eventCount ? &self->m_events[i].header : nullptr;
                               }};
    const clap_output_events out{this, [](const clap_output_events *list, const clap_event_header *e) {
                                     static_cast<Instance *>(list->ctx)->outputEvent(e);
                                     return true;
                                 }};

    // The transport: tempo, play, where it is (bars of 4/4).
    clap_event_transport transport{};
    transport.header = {sizeof(clap_event_transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    if (block.transport) {
        const TransportState &t = *block.transport;
        transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE
                          | (t.playing ? CLAP_TRANSPORT_IS_PLAYING : 0);
        transport.tempo = t.bpm;
        transport.song_pos_beats = clap_beattime(std::llround(t.beatAtBlockStart * double(CLAP_BEATTIME_FACTOR)));
        transport.bar_start = clap_beattime(std::llround(std::floor(t.beatAtBlockStart / 4.0) * 4.0 * double(CLAP_BEATTIME_FACTOR)));
        transport.bar_number = std::int32_t(t.beatAtBlockStart / 4.0);
        transport.tsig_num = 4;
        transport.tsig_denom = 4;
    }

    // Audio: the rack's input (copied: a plugin may work in place) into its
    // main input, its main output straight into the rack's.
    if (m_inPorts > 0) {
        float **main = m_inBuffers[0].data32;
        for (std::uint32_t c = 0; c < m_inChannels[0]; ++c) {
            const float *src = block.inputs && c < 2 ? block.inputs[c] : nullptr;
            if (src)
                std::copy_n(src, frames, main[c]);
            else
                std::fill_n(main[c], frames, 0.0f);
        }
    }
    std::array<float *, 2> mainOut{};
    if (m_outPorts > 0) {
        float **main = m_outBuffers[0].data32;
        mainOut = {main[0], m_outChannels[0] > 1 ? main[1] : nullptr};
        main[0] = outL;
        if (m_outChannels[0] > 1)
            main[1] = outR;
    }
    const clap_process process{std::int64_t(m_steadyTime), frames, &transport,
                               m_inBuffers.data(), m_outBuffers.data(), m_inPorts, m_outPorts, &in, &out};
    const clap_process_status status = m_plugin->process(m_plugin, &process);
    if (m_outPorts > 0) { // back to its own buffers for next time
        float **main = m_outBuffers[0].data32;
        main[0] = mainOut[0];
        if (m_outChannels[0] > 1)
            main[1] = mainOut[1];
    }
    m_steadyTime += frames;
    if (status == CLAP_PROCESS_ERROR || m_outPorts == 0)
        silence();
    else if (m_outChannels[0] == 1)
        std::copy_n(outL, frames, outR); // mono: on both sides
}

// --- PluginSlot -----------------------------------------------------------------------

void PluginSlot::prepare(double sampleRate, std::uint32_t maxFrames) noexcept
{
    m_rate.store(sampleRate, std::memory_order_relaxed);
    m_maxFrames = maxFrames;
    if (Instance *i = instance())
        i->prepare(sampleRate, maxFrames);
}

std::span<const ParamDescriptor> PluginSlot::parameters() const noexcept
{
    const Instance *i = instance();
    return i ? i->parameters() : std::span<const ParamDescriptor>{};
}

int PluginSlot::inputChannels() const noexcept
{
    const Instance *i = instance();
    return i ? i->inputChannels() : 0;
}

float PluginSlot::param(int index) const noexcept
{
    const Instance *i = instance();
    return i ? i->param(index) : 0.0f;
}

void PluginSlot::setParam(int index, float normalized) noexcept
{
    if (Instance *i = instance())
        i->setParam(index, normalized);
}

void PluginSlot::setParamFromHost(int index, float normalized) noexcept
{
    if (Instance *i = instance())
        i->setParamFromHost(index, normalized);
}

std::uint64_t PluginSlot::takeParamsChangedByAudio() noexcept
{
    Instance *i = instance();
    return i ? i->takeParamsChangedByAudio() : 0;
}

void PluginSlot::process(const AudioBlock &block) noexcept
{
    if (Instance *i = instance()) {
        i->process(block);
    } else {
        std::fill_n(block.outputs[0], block.frames, 0.0f);
        std::fill_n(block.outputs[1], block.frames, 0.0f);
    }
    m_blocks.fetch_add(1, std::memory_order_release);
}

} // namespace krate::clap
