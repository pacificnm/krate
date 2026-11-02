#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <QList>
#include <QString>

#include "app/InstrumentRegistry.h"
#include "core/Instrument.h"

// Copies of an instrument in the rack (docs/DAW.md#copies): "Korrode 2" ..
// "Korrode 16", each a device of its own from the start (its mixer strip, its
// sequencer targets, its MIDI links), but the instrument itself is only built
// the first time it's added to the rack, so sixteen of everything cost nothing
// until played. Its files are its own (~/.config/korrode2,
// ~/.local/share/korrode2), starting as a copy of the first one's.

// The entries for copies 2..maxCopies of `base`, in order.
QList<krate::InstrumentEntry> copyEntries(const krate::InstrumentEntry &base);

// A copy's files, the first time: a copy of `from`'s folder (with what's in
// it), unless `to` is there already.
void seedFolder(const QString &from, const QString &to);

// The rack's device for a copy: silent and knob-less... except that it has
// the first one's knobs (every copy of an instrument has the same), so MIDI
// Learn and automation know them before it's built. Once built, it is the
// copy's instrument. Threads: setTarget() on the GUI thread while the device
// isn't in the rack; the rest as IInstrument.
class CopyInstrument final : public krate::IInstrument
{
public:
    explicit CopyInstrument(krate::IInstrument &first) : m_first(first) {}

    void setTarget(krate::IInstrument *target) noexcept;
    krate::IInstrument *target() const noexcept { return m_target.load(std::memory_order_acquire); }

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override;
    int outputChannels() const noexcept override;
    int inputChannels() const noexcept override;
    bool stereoInput() const noexcept override;
    float param(int index) const noexcept override;
    void setParam(int index, float normalized) noexcept override;
    void setParamFromHost(int index, float normalized) noexcept override;
    std::uint64_t takeParamsChangedByAudio() noexcept override;
    krate::MidiLearnMap *midiLearnMap() noexcept override;
    // Kept for when it's built: its RUN / STOP, a song holding it.
    void setSequencerEnabled(bool on) noexcept override;
    void setSequencerSuspended(bool on) noexcept override;
    void process(const krate::AudioBlock &block) noexcept override;

private:
    krate::IInstrument &m_first;
    std::atomic<krate::IInstrument *> m_target{nullptr};
    std::atomic<bool> m_enabled{true}, m_suspended{false};
};

// A copy as the rack's instance: built by build() (the rack, before it goes
// into the rack; loading a sound or a sample into it), nothing until then.
class CopyInstance final : public krate::InstrumentInstance
{
public:
    // `firstConfig`, `firstData`: the first one's folders, its files copied
    // into this one's (context.configDir, context.dataDir) when it's built.
    CopyInstance(const krate::InstrumentEntry &entry, krate::InstrumentContext context, krate::InstrumentInstance &first,
                 QString firstConfig, QString firstData);
    ~CopyInstance() override;

    bool built() const { return m_instance != nullptr; }
    void build();

    krate::IInstrument &instrument() override { return m_device; }
    void triggerAction(const QString &action) override;
    QVariantMap panelProperties() override;
    QJsonObject saveSound() override;
    void loadSound(const QJsonObject &sound) override;
    QStringList sampleTargets() const override;
    QStringList sampleTargetContents() const override;
    QVariantList noteLanes() const override;
    bool assignSample(int target, const QString &path, QString *error) override;
    void clearSample(int target) override;

private:
    std::function<std::unique_ptr<krate::InstrumentInstance>(const krate::InstrumentContext &)> m_create;
    krate::InstrumentContext m_context;
    QString m_firstConfig, m_firstData;
    CopyInstrument m_device;
    std::unique_ptr<krate::InstrumentInstance> m_instance;
};
