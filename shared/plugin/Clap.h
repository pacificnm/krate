#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <clap/clap.h>

#include "core/Instrument.h"
#include "core/ParamDescriptor.h"
#include "core/SpscQueue.h"

// Hosting CLAP plugins in the Krate rack (docs/DAW.md, D7). No Qt: what a
// plugin asks of its host's main thread (timers, file descriptors, a call
// back, a restart, its window) goes through HostServices, which the app
// provides.
namespace krate::clap {

// A plugin found in a .clap file.
struct PluginInfo
{
    std::string path; // the .clap file
    std::string id, name, vendor, version, description;
    bool instrument = false; // its features say so
    bool effect = false;
};

// A loaded .clap file: its entry initialised, its factory. Shared by the
// plugins made from it; unloaded with the last.
class Library
{
public:
    static std::shared_ptr<Library> open(const std::string &path, std::string *error = nullptr);
    ~Library();
    Library(const Library &) = delete;
    Library &operator=(const Library &) = delete;

    const std::string &path() const { return m_path; }
    const clap_plugin_factory *factory() const { return m_factory; }
    std::vector<PluginInfo> plugins() const;

private:
    Library() = default;
    std::string m_path;
    void *m_handle = nullptr;
    const clap_plugin_entry *m_entry = nullptr;
    const clap_plugin_factory *m_factory = nullptr;
};

// Where plugins are looked for: $CLAP_PATH, ~/.clap, then the system's.
std::vector<std::string> defaultSearchPaths();
// Every plugin in the .clap files under `dirs`, by name.
std::vector<PluginInfo> scan(const std::vector<std::string> &dirs);

// What a plugin asks of the host's main thread. Main thread unless said.
class HostServices
{
public:
    virtual ~HostServices() = default;
    virtual void requestCallback() = 0; // any thread: call Instance::onMainThread() soon
    virtual void requestRestart() = 0;  // any thread: deactivate and activate it again
    virtual bool registerTimer(std::uint32_t periodMs, clap_id *id) = 0;
    virtual bool unregisterTimer(clap_id id) = 0;
    virtual bool registerFd(int fd, clap_posix_fd_flags_t flags) = 0;
    virtual bool modifyFd(int fd, clap_posix_fd_flags_t flags) = 0;
    virtual bool unregisterFd(int fd) = 0;
    virtual bool guiRequestResize(std::uint32_t width, std::uint32_t height) = 0;
    virtual bool guiRequestShow() = 0;
    virtual bool guiRequestHide() = 0;
    virtual void guiClosed(bool destroyed) = 0;
    virtual void paramsChanged() {} // the knobs' values changed from the plugin's side (any thread)
    virtual void stateDirty() {}
};

// One plugin, as a rack instrument. Made, activated, saved, restored and
// destroyed on the main thread; processed on the audio thread. Its knobs are
// the plugin's parameters (keys "p<id>", values normalized over their
// ranges); knobs turned in its own window come back to the panel and to
// automation.
class Instance final : public IInstrument
{
public:
    static std::unique_ptr<Instance> create(std::shared_ptr<Library> library, const std::string &pluginId,
                                            HostServices *services, std::string *error = nullptr);
    ~Instance() override;
    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;

    const PluginInfo &info() const { return m_info; }

    // --- Main thread
    bool activate(double sampleRate, std::uint32_t maxFrames);
    // Stops processing (on the audio thread if one runs, within `waitMs`), then deactivates.
    void deactivate(int waitMs = 200);
    bool active() const { return m_active.load(std::memory_order_acquire); }
    // The audio thread asked to run at a rate (or size) it isn't active for.
    bool wantsRestart() const { return m_restartRate.load(std::memory_order_acquire) > 0.0; }
    double restartRate() const { return m_restartRate.load(std::memory_order_acquire); }
    bool saveState(std::vector<std::uint8_t> &out) const;
    bool loadState(const std::vector<std::uint8_t> &in);
    void onMainThread();                // after requestCallback()
    void onTimer(clap_id id);           // a timer it registered fired
    void onFd(int fd, clap_posix_fd_flags_t flags);
    void rereadValues();                // its knobs' values, as it has them now
    const clap_plugin *plugin() const { return m_plugin; }
    const clap_plugin_gui *gui() const { return m_gui; }
    bool hasAudioInput() const { return m_inPorts > 0; }

    // --- IInstrument
    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const ParamDescriptor> parameters() const noexcept override { return m_descriptors; }
    int inputChannels() const noexcept override { return m_inPorts > 0 ? 2 : 0; }
    bool stereoInput() const noexcept override { return true; }
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_changed.exchange(0); }
    void process(const AudioBlock &block) noexcept override;

private:
    struct Param
    {
        clap_id id = 0;
        double min = 0.0, max = 1.0, def = 0.0;
        bool stepped = false;
        std::string key, label;
    };
    union Event
    {
        clap_event_header header;
        clap_event_note note;
        clap_event_midi midi;
        clap_event_param_value param;
    };
    static constexpr std::size_t kMaxEvents = 1024;

    Instance() = default;
    void readPorts();
    double toValue(int index, float normalized) const;
    float toNormalized(int index, double value) const;
    int indexOf(clap_id id) const;
    void outputEvent(const clap_event_header *event) noexcept;

    // The host struct and its extensions (static trampolines).
    static Instance *from(const clap_host *host) { return static_cast<Instance *>(host->host_data); }
    static const void *hostExtension(const clap_host *host, const char *id);
    static void hostRequestRestart(const clap_host *host);
    static void hostRequestProcess(const clap_host *host);
    static void hostRequestCallback(const clap_host *host);

    clap_host m_host{};
    std::shared_ptr<Library> m_library;
    HostServices *m_services = nullptr;
    PluginInfo m_info;
    const clap_plugin *m_plugin = nullptr;
    const clap_plugin_params *m_params = nullptr;
    const clap_plugin_state *m_state = nullptr;
    const clap_plugin_gui *m_gui = nullptr;
    const clap_plugin_timer_support *m_timer = nullptr;
    const clap_plugin_posix_fd_support *m_fd = nullptr;
    std::thread::id m_mainThread;

    std::vector<Param> m_paramInfo;
    std::vector<ParamDescriptor> m_descriptors;
    std::unique_ptr<std::atomic<float>[]> m_values; // normalized
    std::atomic<std::uint64_t> m_changed{0};
    struct ParamChange
    {
        std::uint32_t index;
        double value;
    };
    SpscQueue<ParamChange, 1024> m_toAudio;

    // Ports, as activated.
    std::uint32_t m_inPorts = 0, m_outPorts = 0;
    std::vector<std::uint32_t> m_inChannels, m_outChannels;
    bool m_clapNotes = true, m_midi = true;

    std::atomic<bool> m_active{false};
    double m_rate = 0.0;
    std::uint32_t m_maxFrames = 0;
    std::atomic<double> m_restartRate{0.0};

    // Audio thread.
    std::atomic<bool> m_processing{false};
    std::atomic<bool> m_stopRequested{false};
    std::uint64_t m_steadyTime = 0;
    std::array<Event, kMaxEvents> m_events{};
    std::uint32_t m_eventCount = 0;
    std::vector<std::vector<float>> m_scratch; // ports beyond the main ones, and silence
    std::vector<float *> m_inPtrs, m_outPtrs;
    std::vector<clap_audio_buffer> m_inBuffers, m_outBuffers;
};

// A rack device that a plugin is loaded into (or none: silence). The plugin
// can change while the audio runs: the one before is handed back to be freed
// once the audio thread has moved on (blocks()).
class PluginSlot final : public IInstrument
{
public:
    // Main thread: the plugin to play (owned by the caller); returns the one before.
    Instance *setInstance(Instance *instance) noexcept
    {
        return m_instance.exchange(instance, std::memory_order_acq_rel);
    }
    Instance *instance() const noexcept { return m_instance.load(std::memory_order_acquire); }
    std::uint64_t blocks() const noexcept { return m_blocks.load(std::memory_order_acquire); }
    double sampleRate() const noexcept { return m_rate.load(std::memory_order_relaxed); }

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const ParamDescriptor> parameters() const noexcept override;
    int inputChannels() const noexcept override;
    bool stereoInput() const noexcept override { return true; }
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override;
    void process(const AudioBlock &block) noexcept override;

private:
    std::atomic<Instance *> m_instance{nullptr};
    std::atomic<std::uint64_t> m_blocks{0};
    std::atomic<double> m_rate{0.0};
    std::uint32_t m_maxFrames = 0;
};

} // namespace krate::clap
