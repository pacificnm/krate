#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace krate {

// What the GUI side needs from an audio host, whichever kind it is: the
// standalone JackHost (one instrument) or the rack's host (all of them).
// krate::HostController (the QML "Host" singleton) works through this.
// All methods are for the GUI thread.
class AudioHost
{
public:
    virtual ~AudioHost() = default;

    virtual bool open(const char *clientName, std::string *error) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool serverGone() const = 0;

    virtual std::uint32_t sampleRate() const = 0;
    virtual std::uint32_t bufferSize() const = 0;
    virtual std::uint32_t xrunCount() const = 0;
    virtual float takePeak() noexcept = 0; // master output peak since last call

    virtual std::vector<std::string> connectToPlayback() = 0;
    virtual std::vector<std::string> midiSourcePorts() const = 0;
    virtual bool connectMidiFrom(const std::string &port) = 0;
    virtual bool midiInputConnected() const = 0;
    virtual bool takePortsChanged() = 0;

    // Audio inputs (in_1, ...), each fed by one capture port of the audio
    // interface (a guitar). Hosts without inputs keep the defaults.
    virtual int inputCount() const { return 0; }
    virtual std::vector<std::string> captureSourcePorts() const { return {}; } // physical capture ports
    // Connects input `input` to `port` alone ("" disconnects it).
    virtual bool connectInput(int input, const std::string &port) { (void)input; (void)port; return false; }
    virtual bool inputConnected(int input) const { (void)input; return false; }
    virtual float takeInputPeak(int input) noexcept { (void)input; return 0.0f; } // since last call

    // Asks the server for this many frames per block (smaller = less delay, more CPU).
    virtual bool setBufferSize(std::uint32_t frames) { (void)frames; return false; }
    // Frames from an input to the speakers: capture + playback latency as the server reports them.
    virtual std::uint32_t roundTripFrames() const { return 0; }
};

} // namespace krate
