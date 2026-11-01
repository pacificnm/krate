#include "rec/Bounce.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Transport.h"
#include "host/JackProcessor.h"

namespace krate {

std::int64_t bounce(JackProcessor &processor, Transport &transport, double sampleRate, const BounceRange &range,
                    const std::function<bool(const float *, const float *, std::uint32_t)> &write,
                    const std::function<bool(double)> &progress)
{
    constexpr std::uint32_t kBlock = 512;
    const int channels = std::max(2, processor.outputCount());
    std::vector<std::vector<float>> buffers(std::size_t(channels), std::vector<float>(kBlock, 0.0f));
    std::vector<float *> outputs;
    for (auto &b : buffers)
        outputs.push_back(b.data());

    // Krate's own time from the start (not a DAW's), playing from `fromBeat`, no loop.
    transport.setExternal(nullptr);
    transport.setPlaying(false);
    transport.advance(0, sampleRate);
    transport.setLoop(false, 0, 0);
    transport.setStartBeat(range.fromBeat);
    transport.setPlaying(true);

    const double framesPerBeat = 60.0 / std::max(transport.bpm(), 1.0) * sampleRate;
    const auto total = std::int64_t(std::ceil(std::max(0.0, range.toBeat - range.fromBeat) * framesPerBeat - 1e-6));
    const double expected = double(total) + 0.5 * sampleRate; // for progress: some tail
    std::int64_t done = 0;
    const auto run = [&](std::uint32_t n) {
        processor.process(nullptr, outputs.data(), n, nullptr, 0, sampleRate);
        done += n;
        return write(outputs[0], outputs[1], n) && (!progress || progress(std::min(1.0, double(done) / expected)));
    };
    while (done < total)
        if (!run(std::uint32_t(std::min<std::int64_t>(kBlock, total - done)))) {
            transport.setPlaying(false);
            return -1;
        }

    // The tail: stopped, until it's quiet.
    transport.setPlaying(false);
    const auto tailMost = std::int64_t(range.tailSeconds * sampleRate), quietEnough = std::int64_t(0.25 * sampleRate);
    std::int64_t tail = 0, quiet = 0;
    while (tail < tailMost && quiet < quietEnough) {
        if (!run(kBlock)) {
            transport.advance(0, sampleRate);
            return -1;
        }
        float peak = 0.0f;
        for (std::uint32_t i = 0; i < kBlock; ++i)
            peak = std::max({peak, std::fabs(outputs[0][i]), std::fabs(outputs[1][i])});
        quiet = peak < range.silence ? quiet + kBlock : 0;
        tail += kBlock;
    }
    transport.advance(0, sampleRate); // stopped, for whoever runs it next
    if (progress)
        progress(1.0);
    return done;
}

} // namespace krate
