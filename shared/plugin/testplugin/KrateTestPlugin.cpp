// krate-test.clap: two small CLAP plugins Krate's tests host (docs/DAW.md, D7).
// "Krate Test Synth": a sine per note (notes as CLAP events or MIDI), Level
// and Detune knobs, its knobs kept as state. "Krate Test Gain": a stereo
// effect, one Gain knob. The synth has a small X11 editor (a coloured
// window inside the one the host gives it) that ticks on a host timer and
// shows the count in its window's name, so the host's editor and timer
// support can be checked.

#include <clap/clap.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <string>

#include <X11/Xlib.h>

namespace {

struct ParamDef
{
    clap_id id;
    const char *name;
    double min, max, def;
    bool stepped;
};

constexpr ParamDef kSynthParams[] = {{1, "Level", 0.0, 1.0, 0.5, false}, {2, "Detune", -12.0, 12.0, 0.0, true}};
constexpr ParamDef kGainParams[] = {{7, "Gain", 0.0, 2.0, 1.0, false}};

struct Plugin
{
    clap_plugin plugin{};
    const clap_host *host = nullptr;
    bool synth = true;
    const ParamDef *defs = nullptr;
    std::uint32_t paramCount = 0;
    std::array<double, 2> values{};
    double rate = 48000.0;
    // The synth: one note at a time.
    int note = -1;
    double phase = 0.0;
    // Its editor.
    Display *display = nullptr;
    Window window = 0;
    clap_id timer = CLAP_INVALID_ID;
    int ticks = 0;

    int indexOf(clap_id id) const
    {
        for (std::uint32_t i = 0; i < paramCount; ++i)
            if (defs[i].id == id)
                return int(i);
        return -1;
    }
    void handle(const clap_event_header *e)
    {
        if (e->space_id != CLAP_CORE_EVENT_SPACE_ID)
            return;
        switch (e->type) {
        case CLAP_EVENT_NOTE_ON:
            note = reinterpret_cast<const clap_event_note *>(e)->key;
            break;
        case CLAP_EVENT_NOTE_OFF:
            if (reinterpret_cast<const clap_event_note *>(e)->key == note)
                note = -1;
            break;
        case CLAP_EVENT_MIDI: {
            const auto *m = reinterpret_cast<const clap_event_midi *>(e);
            const int type = m->data[0] & 0xF0;
            if (type == 0x90 && m->data[2] > 0)
                note = m->data[1];
            else if ((type == 0x80 || type == 0x90) && m->data[1] == note)
                note = -1;
            break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            const auto *p = reinterpret_cast<const clap_event_param_value *>(e);
            if (const int i = indexOf(p->param_id); i >= 0)
                values[std::size_t(i)] = std::clamp(p->value, defs[i].min, defs[i].max);
            break;
        }
        default:
            break;
        }
    }
};

Plugin *self(const clap_plugin *p) { return static_cast<Plugin *>(p->plugin_data); }

// --- params
std::uint32_t paramsCount(const clap_plugin *p) { return self(p)->paramCount; }
bool paramsInfo(const clap_plugin *p, std::uint32_t index, clap_param_info *info)
{
    const Plugin *s = self(p);
    if (index >= s->paramCount)
        return false;
    const ParamDef &d = s->defs[index];
    std::memset(info, 0, sizeof(*info));
    info->id = d.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | (d.stepped ? CLAP_PARAM_IS_STEPPED : 0);
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
    info->min_value = d.min;
    info->max_value = d.max;
    info->default_value = d.def;
    return true;
}
bool paramsValue(const clap_plugin *p, clap_id id, double *value)
{
    const Plugin *s = self(p);
    const int i = s->indexOf(id);
    if (i < 0)
        return false;
    *value = s->values[std::size_t(i)];
    return true;
}
bool paramsToText(const clap_plugin *p, clap_id id, double value, char *out, std::uint32_t size)
{
    if (self(p)->indexOf(id) < 0)
        return false;
    std::snprintf(out, size, id == 2 ? "%+.0f st" : "%.2f", value);
    return true;
}
bool paramsFromText(const clap_plugin *, clap_id, const char *text, double *value)
{
    *value = std::atof(text);
    return true;
}
void paramsFlush(const clap_plugin *p, const clap_input_events *in, const clap_output_events *)
{
    for (std::uint32_t i = 0; i < in->size(in); ++i)
        self(p)->handle(in->get(in, i));
}
const clap_plugin_params kParamsExt{paramsCount, paramsInfo, paramsValue, paramsToText, paramsFromText, paramsFlush};

// --- state: the knobs' values
bool stateSave(const clap_plugin *p, const clap_ostream *out)
{
    const Plugin *s = self(p);
    const char *bytes = reinterpret_cast<const char *>(s->values.data());
    std::int64_t left = std::int64_t(sizeof(double) * s->paramCount);
    while (left > 0) {
        const std::int64_t n = out->write(out, bytes, std::uint64_t(left));
        if (n <= 0)
            return false;
        bytes += n;
        left -= n;
    }
    return true;
}
bool stateLoad(const clap_plugin *p, const clap_istream *in)
{
    Plugin *s = self(p);
    std::array<double, 2> v{};
    char *bytes = reinterpret_cast<char *>(v.data());
    std::int64_t left = std::int64_t(sizeof(double) * s->paramCount);
    while (left > 0) {
        const std::int64_t n = in->read(in, bytes, std::uint64_t(left));
        if (n <= 0)
            return false;
        bytes += n;
        left -= n;
    }
    for (std::uint32_t i = 0; i < s->paramCount; ++i)
        s->values[i] = std::clamp(v[i], s->defs[i].min, s->defs[i].max);
    return true;
}
const clap_plugin_state kStateExt{stateSave, stateLoad};

// --- audio and note ports
std::uint32_t audioCount(const clap_plugin *p, bool isInput) { return isInput && self(p)->synth ? 0 : 1; }
bool audioGet(const clap_plugin *p, std::uint32_t index, bool isInput, clap_audio_port_info *info)
{
    if (index > 0 || (isInput && self(p)->synth))
        return false;
    std::memset(info, 0, sizeof(*info));
    info->id = isInput ? 10 : 20;
    std::snprintf(info->name, sizeof(info->name), "%s", isInput ? "In" : "Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}
const clap_plugin_audio_ports kAudioPortsExt{audioCount, audioGet};

std::uint32_t noteCount(const clap_plugin *p, bool isInput) { return isInput && self(p)->synth ? 1 : 0; }
bool noteGet(const clap_plugin *p, std::uint32_t index, bool isInput, clap_note_port_info *info)
{
    if (index > 0 || !isInput || !self(p)->synth)
        return false;
    std::memset(info, 0, sizeof(*info));
    info->id = 30;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    std::snprintf(info->name, sizeof(info->name), "Notes");
    return true;
}
const clap_plugin_note_ports kNotePortsExt{noteCount, noteGet};

// --- the synth's editor (X11)
constexpr std::uint32_t kGuiWidth = 320, kGuiHeight = 120;
const clap_host_timer_support *hostTimer(const Plugin *s)
{
    return static_cast<const clap_host_timer_support *>(s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT));
}
bool guiSupported(const clap_plugin *p, const char *api, bool floating)
{
    return self(p)->synth && !floating && !std::strcmp(api, CLAP_WINDOW_API_X11);
}
bool guiPreferred(const clap_plugin *, const char **api, bool *floating)
{
    *api = CLAP_WINDOW_API_X11;
    *floating = false;
    return true;
}
bool guiCreate(const clap_plugin *p, const char *api, bool floating)
{
    Plugin *s = self(p);
    if (!guiSupported(p, api, floating) || s->display)
        return false;
    s->display = XOpenDisplay(nullptr);
    if (!s->display)
        return false;
    if (const auto *timer = hostTimer(s))
        timer->register_timer(s->host, 20, &s->timer);
    return true;
}
void guiDestroy(const clap_plugin *p)
{
    Plugin *s = self(p);
    if (s->timer != CLAP_INVALID_ID)
        if (const auto *timer = hostTimer(s))
            timer->unregister_timer(s->host, s->timer);
    s->timer = CLAP_INVALID_ID;
    if (s->display) {
        if (s->window)
            XDestroyWindow(s->display, s->window);
        XCloseDisplay(s->display);
    }
    s->display = nullptr;
    s->window = 0;
}
bool guiScale(const clap_plugin *, double) { return true; }
bool guiSize(const clap_plugin *, std::uint32_t *w, std::uint32_t *h)
{
    *w = kGuiWidth;
    *h = kGuiHeight;
    return true;
}
bool guiCanResize(const clap_plugin *) { return false; }
bool guiHints(const clap_plugin *, clap_gui_resize_hints *) { return false; }
bool guiAdjust(const clap_plugin *, std::uint32_t *, std::uint32_t *) { return false; }
bool guiSetSize(const clap_plugin *, std::uint32_t w, std::uint32_t h) { return w == kGuiWidth && h == kGuiHeight; }
bool guiParent(const clap_plugin *p, const clap_window *parent)
{
    Plugin *s = self(p);
    if (!s->display || !parent || std::strcmp(parent->api, CLAP_WINDOW_API_X11))
        return false;
    s->window = XCreateSimpleWindow(s->display, Window(parent->x11), 0, 0, kGuiWidth, kGuiHeight, 0, 0, 0x3060A0);
    XStoreName(s->display, s->window, "krate-test-gui");
    XFlush(s->display);
    return true;
}
bool guiTransient(const clap_plugin *, const clap_window *) { return false; }
void guiTitle(const clap_plugin *, const char *) {}
bool guiShow(const clap_plugin *p)
{
    Plugin *s = self(p);
    if (!s->window)
        return false;
    XMapWindow(s->display, s->window);
    XFlush(s->display);
    return true;
}
bool guiHide(const clap_plugin *p)
{
    Plugin *s = self(p);
    if (!s->window)
        return false;
    XUnmapWindow(s->display, s->window);
    XFlush(s->display);
    return true;
}
const clap_plugin_gui kGuiExt{guiSupported, guiPreferred, guiCreate, guiDestroy, guiScale, guiSize, guiCanResize,
                              guiHints, guiAdjust, guiSetSize, guiParent, guiTransient, guiTitle, guiShow, guiHide};

void onTimer(const clap_plugin *p, clap_id id)
{
    Plugin *s = self(p);
    if (id != s->timer || !s->window)
        return;
    const std::string name = "krate-test-gui ticks=" + std::to_string(++s->ticks);
    XStoreName(s->display, s->window, name.c_str());
    XFlush(s->display);
}
const clap_plugin_timer_support kTimerExt{onTimer};

// --- the plugin
bool init(const clap_plugin *) { return true; }
void destroy(const clap_plugin *p)
{
    guiDestroy(p);
    delete self(p);
}
bool activate(const clap_plugin *p, double rate, std::uint32_t, std::uint32_t)
{
    self(p)->rate = rate;
    return true;
}
void deactivate(const clap_plugin *) {}
bool startProcessing(const clap_plugin *) { return true; }
void stopProcessing(const clap_plugin *) {}
void reset(const clap_plugin *p)
{
    self(p)->note = -1;
    self(p)->phase = 0.0;
}
clap_process_status process(const clap_plugin *p, const clap_process *process)
{
    Plugin *s = self(p);
    const std::uint32_t frames = process->frames_count;
    const std::uint32_t events = process->in_events->size(process->in_events);
    std::uint32_t next = 0;
    float **out = process->audio_outputs[0].data32;
    for (std::uint32_t i = 0; i < frames; ++i) {
        while (next < events) {
            const clap_event_header *e = process->in_events->get(process->in_events, next);
            if (e->time > i)
                break;
            s->handle(e);
            ++next;
        }
        if (s->synth) {
            float v = 0.0f;
            if (s->note >= 0) {
                const double hz = 440.0 * std::exp2((s->note + s->values[1] - 69.0) / 12.0);
                v = float(s->values[0] * std::sin(2.0 * std::numbers::pi * s->phase));
                s->phase += hz / s->rate;
                s->phase -= std::floor(s->phase);
            }
            out[0][i] = out[1][i] = v;
        } else {
            float **in = process->audio_inputs[0].data32;
            out[0][i] = float(in[0][i] * s->values[0]);
            out[1][i] = float(in[1][i] * s->values[0]);
        }
    }
    while (next < events)
        s->handle(process->in_events->get(process->in_events, next++));
    return CLAP_PROCESS_CONTINUE;
}
const void *extension(const clap_plugin *p, const char *id)
{
    if (self(p)->synth && !std::strcmp(id, CLAP_EXT_GUI))
        return &kGuiExt;
    if (self(p)->synth && !std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &kTimerExt;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &kParamsExt;
    if (!std::strcmp(id, CLAP_EXT_STATE))
        return &kStateExt;
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
        return &kAudioPortsExt;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &kNotePortsExt;
    return nullptr;
}
void onMainThread(const clap_plugin *) {}

const char *const kSynthFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, nullptr};
const char *const kGainFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};
const clap_plugin_descriptor kSynth{CLAP_VERSION_INIT, "com.krate.test.synth", "Krate Test Synth", "Krate", "", "", "",
                                    "1.0", "A sine per note, for tests", kSynthFeatures};
const clap_plugin_descriptor kGain{CLAP_VERSION_INIT, "com.krate.test.gain", "Krate Test Gain", "Krate", "", "", "",
                                   "1.0", "A gain, for tests", kGainFeatures};

const clap_plugin *create(const clap_plugin_factory *, const clap_host *host, const char *id)
{
    const bool synth = !std::strcmp(id, kSynth.id);
    if (!synth && std::strcmp(id, kGain.id))
        return nullptr;
    auto *s = new Plugin;
    s->host = host;
    s->synth = synth;
    s->defs = synth ? kSynthParams : kGainParams;
    s->paramCount = synth ? 2 : 1;
    for (std::uint32_t i = 0; i < s->paramCount; ++i)
        s->values[i] = s->defs[i].def;
    s->plugin = clap_plugin{synth ? &kSynth : &kGain, s, init, destroy, activate, deactivate, startProcessing,
                            stopProcessing, reset, process, extension, onMainThread};
    return &s->plugin;
}
std::uint32_t factoryCount(const clap_plugin_factory *) { return 2; }
const clap_plugin_descriptor *factoryDescriptor(const clap_plugin_factory *, std::uint32_t index)
{
    return index == 0 ? &kSynth : index == 1 ? &kGain : nullptr;
}
const clap_plugin_factory kFactory{factoryCount, factoryDescriptor, create};

bool entryInit(const char *) { return true; }
void entryDeinit() {}
const void *entryFactory(const char *id) { return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &kFactory : nullptr; }

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry clap_entry{CLAP_VERSION_INIT, entryInit, entryDeinit, entryFactory};
