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
};

} // namespace krate
