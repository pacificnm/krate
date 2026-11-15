#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "dsp/Capture.h"
#include "dsp/Filters.h"
#include "dsp/Pedal.h"

namespace krunch {

// Capture: a pedal that plays a capture of a real pedal (docs/PEDALS.md#capture):
// a .nam file from the captures folder, the same format as the amps'
// (community capture sites share pedals too: drives, fuzzes, boosts, preamps).
// INPUT drives it harder or softer (the capture's knobs are as they were when
// it was made), TONE tilts it darker or brighter after, LEVEL sets how loud.
// Without a capture (or while one loads) it passes the guitar through.
//
// The model is handed over from the GUI thread (setModel) and kept alive by
// the pedal until the audio thread has moved two blocks on (collect); one the
// pedal still holds is freed with it. Pedal captures play as captured: not
// brought to the amps' common loudness.
class CapturePedal final : public Pedal
{
public:
    enum Knob : int { Input, Tone, Level };
    static const PedalInfo kInfo;

    CapturePedal() : Pedal(kInfo) {}
    ~CapturePedal() override;

    // GUI thread. `blocks`: the engine's block count now (Krunch::blocksProcessed()).
    void setModel(std::shared_ptr<CaptureModel> model, std::uint64_t blocks);
    void collect(std::uint64_t blocks);
    const CaptureModel *model() const noexcept { return m_live.get(); }

    // GUI thread: which capture it plays (by name, "" = none) and its loading
    // (PedalController's; kept here so they go where the pedal goes).
    std::string captureName;
    std::string liveName;   // the one it plays now
    std::string failedName; // couldn't be loaded (tried again when the folder changes)
    std::string loadingName;
    double loadingRate = 0.0;
    std::future<std::shared_ptr<CaptureModel>> loading;
    std::shared_ptr<std::string> loadError;

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    std::atomic<CaptureModel *> m_model{nullptr};
    std::shared_ptr<CaptureModel> m_live;
    struct Retired
    {
        std::shared_ptr<CaptureModel> model;
        std::uint64_t after;
    };
    std::vector<Retired> m_retired;

    double m_rate = 48000.0;
    OnePole m_tilt;          // the tone's split, at 800 Hz
    float m_input = -1.0f;   // ramped gains
    float m_level = -1.0f;
};

} // namespace krunch
