#include "dsp/Pedal.h"

#include <algorithm>
#include <cmath>

#include "dsp/GreenDrive.h"
#include "dsp/TimePedals.h"

namespace krunch {

Pedal::Pedal(const PedalInfo &info)
    : m_info(info)
{
    for (std::size_t i = 0; i < info.knobs.size() && i < kMaxKnobs; ++i)
        m_knobs[i].store(info.knobs[i].defaultValue, std::memory_order_relaxed);
}

void Pedal::setKnob(int index, float value) noexcept
{
    if (index >= 0 && index < kMaxKnobs)
        m_knobs[std::size_t(index)].store(std::clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
}

void Pedal::prepare(double sampleRate) noexcept
{
    m_mixStep = 1.0 / (0.010 * sampleRate);
    m_mix = 0.0;
    prepareEffect(sampleRate);
    resetEffect();
}

void Pedal::process(float *buffer, std::uint32_t frames) noexcept
{
    const double target = on() ? 1.0 : 0.0;
    if (m_mix == 0.0 && target == 0.0)
        return; // off: the signal passes untouched
    if (m_mix == 1.0 && target == 1.0) {
        processEffect(buffer, frames);
        return;
    }
    // Switching: from fully off, start the effect fresh; crossfade dry and wet.
    if (m_mix == 0.0)
        resetEffect();
    std::copy_n(buffer, frames, m_dry.data());
    processEffect(buffer, frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_mix = target > m_mix ? std::min(target, m_mix + m_mixStep) : std::max(target, m_mix - m_mixStep);
        buffer[n] = float(buffer[n] * m_mix + m_dry[n] * (1.0 - m_mix));
    }
}

std::span<const PedalInfo *const> pedalTypes()
{
    static const PedalInfo *const kTypes[] = {&GreenDrive::kInfo, &Chorus::kInfo, &Delay::kInfo, &Reverb::kInfo};
    return kTypes;
}

std::unique_ptr<Pedal> makePedal(std::string_view type)
{
    if (type == GreenDrive::kInfo.type)
        return std::make_unique<GreenDrive>();
    if (type == Chorus::kInfo.type)
        return std::make_unique<Chorus>();
    if (type == Delay::kInfo.type)
        return std::make_unique<Delay>();
    if (type == Reverb::kInfo.type)
        return std::make_unique<Reverb>();
    return nullptr;
}

void PedalChain::prepare(double sampleRate) noexcept
{
    if (const PedalList *l = list())
        for (int i = 0; i < l->count; ++i)
            l->pedals[std::size_t(i)]->prepare(sampleRate);
}

void PedalChain::process(float *buffer, std::uint32_t frames) noexcept
{
    const PedalList *l = list();
    if (!l)
        return;
    for (std::uint32_t at = 0; at < frames; at += Pedal::kMaxFrames) {
        const std::uint32_t n = std::min(Pedal::kMaxFrames, frames - at);
        for (int i = 0; i < l->count; ++i)
            l->pedals[std::size_t(i)]->process(buffer + at, n);
    }
}

Pedalboard::~Pedalboard()
{
    m_front.set(nullptr); // whoever owns the chains stops processing first
    m_loop.set(nullptr);
}

bool Pedalboard::add(std::string_view type, double sampleRate, std::uint64_t blocks, int position, int loop)
{
    if (count() >= PedalList::kMaxPedals)
        return false;
    auto pedal = makePedal(type);
    if (!pedal)
        return false;
    pedal->prepare(sampleRate); // before the audio thread can see it
    if (position < 0 || position > count())
        position = count();
    const bool inLoop = loop < 0 ? pedal->info().loopByDefault : loop > 0;
    m_pedals.insert(m_pedals.begin() + position, Entry{std::move(pedal), inLoop});
    publish(blocks);
    return true;
}

void Pedalboard::remove(int position, std::uint64_t blocks)
{
    if (position < 0 || position >= count())
        return;
    auto pedal = std::move(m_pedals[std::size_t(position)].pedal);
    m_pedals.erase(m_pedals.begin() + position);
    publish(blocks);
    m_retired.push_back({std::move(pedal), nullptr, blocks});
}

void Pedalboard::move(int from, int to, std::uint64_t blocks)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to)
        return;
    Entry entry = std::move(m_pedals[std::size_t(from)]);
    m_pedals.erase(m_pedals.begin() + from);
    m_pedals.insert(m_pedals.begin() + to, std::move(entry));
    publish(blocks);
}

void Pedalboard::setInLoop(int position, bool loop, std::uint64_t blocks)
{
    if (position < 0 || position >= count() || m_pedals[std::size_t(position)].loop == loop)
        return;
    m_pedals[std::size_t(position)].loop = loop;
    publish(blocks);
}

void Pedalboard::clear(std::uint64_t blocks)
{
    while (count() > 0)
        remove(count() - 1, blocks);
}

void Pedalboard::publish(std::uint64_t blocks)
{
    auto front = std::make_unique<PedalList>(), loop = std::make_unique<PedalList>();
    for (const Entry &e : m_pedals) {
        PedalList &list = e.loop ? *loop : *front;
        list.pedals[std::size_t(list.count++)] = e.pedal.get();
    }
    m_front.set(front.get());
    m_loop.set(loop.get());
    if (m_frontList)
        m_retired.push_back({nullptr, std::move(m_frontList), blocks});
    if (m_loopList)
        m_retired.push_back({nullptr, std::move(m_loopList), blocks});
    m_frontList = std::move(front);
    m_loopList = std::move(loop);
}

void Pedalboard::collect(std::uint64_t blocks)
{
    std::erase_if(m_retired, [blocks](const Retired &r) { return blocks >= r.after + 2; });
}

} // namespace krunch
