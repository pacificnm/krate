#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rec/AudioTrack.h"
#include "rec/WavWriter.h"

namespace krate {

// The disk side of recording (docs/DAW.md, D2): a thread that, for every
// audio track, opens a WAV file when a take begins, writes what the audio
// thread puts in the track's ring buffer, and closes it once the take has
// ended and everything is written. The audio thread never waits on it.
class Recorder
{
public:
    struct Finished
    {
        int track = -1;
        std::string path;
        double startBeat = 0.0;
        std::uint64_t frames = 0;
        int rate = 48000;
        int channels = 2;
        bool ok = true; // the file was written whole
    };

    // `pathFor(track)`: where its next take goes (its folder exists).
    Recorder(std::vector<AudioTrack *> tracks, std::function<std::string(int track)> pathFor);
    ~Recorder();
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    void start();         // the disk thread, polling every 10 ms
    void stop();          // finishes what's in the buffers first
    void poll();          // one pass (the thread's; tests call it themselves)
    std::vector<Finished> takeFinished(); // takes written since the last call (any thread)

private:
    struct Slot
    {
        AudioTrack *track = nullptr;
        WavWriter writer;
        std::uint32_t id = 0;
        std::string path;
        double startBeat = 0.0;
        int channels = 2;
    };

    std::vector<Slot> m_slots;
    std::function<std::string(int)> m_pathFor;
    std::vector<float> m_left, m_right;
    std::mutex m_mutex; // the finished list
    std::vector<Finished> m_finished;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};

} // namespace krate
