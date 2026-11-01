#include "rec/Recorder.h"

#include <chrono>
#include <utility>

namespace krate {

namespace {
constexpr std::size_t kChunk = 4096;
}

Recorder::Recorder(std::vector<AudioTrack *> tracks, std::function<std::string(int)> pathFor)
    : m_slots(tracks.size()) // built in place: a slot's writer holds an open file, it doesn't move
    , m_pathFor(std::move(pathFor))
    , m_left(kChunk)
    , m_right(kChunk)
{
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        m_slots[i].track = tracks[i];
        m_slots[i].id = tracks[i]->takeId();
    }
}

Recorder::~Recorder()
{
    stop();
}

void Recorder::start()
{
    if (m_running.exchange(true))
        return;
    m_thread = std::thread([this] {
        while (m_running.load(std::memory_order_relaxed)) {
            poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        poll(); // what's left
    });
}

void Recorder::stop()
{
    if (m_running.exchange(false) && m_thread.joinable())
        m_thread.join();
    for (Slot &s : m_slots) // a take still open (quitting while recording): finished as it is
        if (s.writer.isOpen()) {
            const std::uint64_t frames = s.writer.frames();
            const bool ok = s.writer.close();
            std::lock_guard lock(m_mutex);
            m_finished.push_back({int(&s - m_slots.data()), s.path, s.startBeat, frames, int(s.track->sampleRate()),
                                  s.channels, ok});
        }
}

void Recorder::poll()
{
    for (std::size_t t = 0; t < m_slots.size(); ++t) {
        Slot &s = m_slots[t];
        AudioTrack &track = *s.track;
        // A take has begun: its file.
        if (!s.writer.isOpen() && track.takeId() != s.id) {
            s.id = track.takeId();
            s.startBeat = track.takeStartBeat();
            s.channels = track.takeChannels();
            s.path = m_pathFor(int(t));
            if (!s.writer.open(s.path, int(track.sampleRate()), s.channels)) {
                // Can't write: drop this take's audio, let the next one try.
                while (track.ring().read(m_left.data(), m_right.data(), kChunk) > 0) {
                }
                track.writerDone();
                std::lock_guard lock(m_mutex);
                m_finished.push_back({int(t), s.path, s.startBeat, 0, int(track.sampleRate()), s.channels, false});
                continue;
            }
        }
        if (!s.writer.isOpen())
            continue;
        // Ended? Asked before draining, so everything it wrote is in the buffer.
        const bool ended = !track.recording();
        while (const std::size_t n = track.ring().read(m_left.data(), m_right.data(), kChunk))
            s.writer.write(m_left.data(), m_right.data(), n);
        if (ended && track.ring().available() == 0) {
            const std::uint64_t frames = s.writer.frames();
            const bool ok = s.writer.close();
            track.writerDone();
            std::lock_guard lock(m_mutex);
            m_finished.push_back({int(t), s.path, s.startBeat, frames, int(track.sampleRate()), s.channels, ok});
        }
    }
}

std::vector<Recorder::Finished> Recorder::takeFinished()
{
    std::lock_guard lock(m_mutex);
    return std::exchange(m_finished, {});
}

} // namespace krate
