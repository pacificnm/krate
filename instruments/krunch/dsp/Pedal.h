#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace krunch {

// A pedal's knob: what the pedalboard shows and saves.
struct KnobInfo
{
    std::string_view key;   // saved in pedals.json and songs: never rename
    std::string_view label; // on the pedal
    float defaultValue;     // 0..1
    std::string (*format)(float);
};

// A kind of pedal (docs/PEDALS.md).
struct PedalInfo
{
    std::string_view type; // saved: never rename
    std::string_view name; // on the pedal
    std::string_view colour; // its enclosure, "#RRGGBB"
    std::span<const KnobInfo> knobs;
};

// One pedal: mono, in place, at the base rate (docs/PEDALS.md). Knobs and the
// footswitch are atomics, set from any thread; switching on or off crossfades
// over 10 ms. process() and prepare() are for the audio thread (prepare also
// before the pedal is handed to it) and don't allocate.
class Pedal
{
public:
    static constexpr int kMaxKnobs = 6;
    static constexpr std::uint32_t kMaxFrames = 256; // per process() call

    explicit Pedal(const PedalInfo &info);
    virtual ~Pedal() = default;

    const PedalInfo &info() const noexcept { return m_info; }
    float knob(int index) const noexcept { return m_knobs[std::size_t(index)].load(std::memory_order_relaxed); }
    void setKnob(int index, float value) noexcept;
    bool on() const noexcept { return m_on.load(std::memory_order_relaxed); }
    void setOn(bool on) noexcept { m_on.store(on, std::memory_order_relaxed); }

    void prepare(double sampleRate) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept; // frames <= kMaxFrames

protected:
    virtual void prepareEffect(double sampleRate) noexcept = 0;
    virtual void resetEffect() noexcept = 0;
    // The pedal switched on: wet only, in place.
    virtual void processEffect(float *buffer, std::uint32_t frames) noexcept = 0;

private:
    const PedalInfo &m_info;
    std::array<std::atomic<float>, kMaxKnobs> m_knobs{};
    std::atomic<bool> m_on{true};
    double m_mix = 0.0; // 0 off .. 1 on, ramping (a new pedal fades in)
    double m_mixStep = 0.0;
    std::array<float, kMaxFrames> m_dry{};
};

// Every kind of pedal, for the "add" menu.
std::span<const PedalInfo *const> pedalTypes();
// A new pedal of that type, at its default settings (nullptr if there's no such type).
std::unique_ptr<Pedal> makePedal(std::string_view type);

// The pedals in one place, in order, as the audio thread sees them.
struct PedalList
{
    static constexpr int kMaxPedals = 8;
    int count = 0;
    std::array<Pedal *, kMaxPedals> pedals{};
};

// Where a pedal list plays (in front of the amp, or in its effects loop).
// The list is handed over from any thread; the caller keeps it (and its
// pedals) alive until the audio thread can't see it any more (Pedalboard).
class PedalChain
{
public:
    void set(const PedalList *list) noexcept { m_list.store(list, std::memory_order_release); }
    const PedalList *list() const noexcept { return m_list.load(std::memory_order_acquire); }

    // Audio thread.
    void prepare(double sampleRate) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    std::atomic<const PedalList *> m_list{nullptr};
};

// Owns a chain's pedals on the GUI side: adds, removes and reorders them,
// hands each new order to the chain, and frees what the audio thread has
// finished with (collect(), once its block count has moved two on).
class Pedalboard
{
public:
    explicit Pedalboard(PedalChain &chain) : m_chain(chain) {}
    ~Pedalboard();

    Pedalboard(const Pedalboard &) = delete;
    Pedalboard &operator=(const Pedalboard &) = delete;

    int count() const noexcept { return int(m_pedals.size()); }
    Pedal &pedal(int position) noexcept { return *m_pedals[std::size_t(position)]; }

    // blocks: the audio thread's block count now (Krunch::blocksProcessed()).
    // Returns false if the board is full or there's no such type.
    bool add(std::string_view type, double sampleRate, std::uint64_t blocks, int position = -1);
    void remove(int position, std::uint64_t blocks);
    void move(int from, int to, std::uint64_t blocks);
    void clear(std::uint64_t blocks);
    void collect(std::uint64_t blocks);

private:
    void publish(std::uint64_t blocks);

    PedalChain &m_chain;
    std::vector<std::unique_ptr<Pedal>> m_pedals;
    std::unique_ptr<PedalList> m_list;
    struct Retired
    {
        std::unique_ptr<Pedal> pedal;
        std::unique_ptr<PedalList> list;
        std::uint64_t after;
    };
    std::vector<Retired> m_retired;
};

} // namespace krunch
