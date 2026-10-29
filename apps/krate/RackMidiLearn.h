#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <QJsonArray>
#include <QStringList>

#include "app/MidiLearnController.h"
#include "core/ParamDescriptor.h"
#include "rack/Rack.h"

namespace krate {
class InstrumentInstance;
}

// MIDI Learn for the Krate rack (docs/SHARED_FOUNDATIONS.MD#midi-routing):
// learns into the rack-wide map (krate::Rack), recording the channel.
//
// Keys from an instrument panel refer to the focused instrument (the panel on
// screen): a parameter key maps as fixed (that instrument's knob), or as
// follow-focus when followFocus is on; an action key (e.g. "pattern1") maps as
// that instrument's button. Rack keys: "rack:playstop", "rack:mixer", "rack:song" (the
// rack's own actions, kRackActions), "rack:focus:<n>", "rack:next",
// "rack:previous", and the mixer's "rack:mix:<n>:fader|pan|mute|solo" (strip
// n) and "rack:mix:master:fader". Notes can be learned for buttons (mute and
// solo too).
class RackMidiLearn final : public MidiLearnController
{
    Q_OBJECT

public:
    struct Device
    {
        QString id;
        std::span<const krate::ParamDescriptor> params;
        QStringList actions;
        krate::InstrumentInstance *instance = nullptr; // performs its actions; may be null in tests
    };

    // The rack's own learnable actions ("rack:<name>"); the index is the mapping's action.
    inline static const QStringList kRackActions{QStringLiteral("playstop"), QStringLiteral("mixer"), QStringLiteral("song")};
    // Mixer controls, indexed by krate::Rack::MixerControl.
    inline static const QStringList kMixerControls{QStringLiteral("fader"), QStringLiteral("pan"), QStringLiteral("mute"),
                                                   QStringLiteral("solo")};
    using ActionHandler = std::function<void(const QString &action)>; // one of kRackActions

    RackMidiLearn(krate::Rack &rack, std::vector<Device> devices, ActionHandler rackAction, QObject *parent);
    ~RackMidiLearn() override;

    bool canFollowFocus() const override { return true; }
    QString mappingFor(const QString &key) const override;
    QString badgeFor(const QString &key) const override;
    void clear(const QString &key) override;
    void clearAll() override;
    void drainEvents() override;

    // The focused instrument changed: panel keys now mean another instrument.
    void focusChanged() { mappingsEdited(); }

    // rack.json: every mapping by instrument id and key, so instrument order
    // doesn't matter. Loading skips entries it can't resolve.
    QJsonArray saveMappings() const;
    void loadMappings(const QJsonArray &mappings);

private:
    using Mapping = krate::Rack::Mapping;

    std::optional<Mapping> resolve(const QString &key) const;
    bool matches(const Mapping &m, const QString &key) const;
    QString paramKey(int device, int index) const;
    int deviceIndex(const QString &id) const;
    // Every (source, mapping) currently in the rack.
    struct Entry
    {
        bool isNote = false;
        int channel = 0; // 0..15, or krate::Rack::kAnyChannel (CCs only)
        int number = 0;
        Mapping mapping;
    };
    std::vector<Entry> entries() const;
    void setEntry(const Entry &e, Mapping m);

protected:
    bool armTarget(const QString &key) override;

private:
    krate::Rack &m_rack;
    std::vector<Device> m_devices;
    ActionHandler m_rackAction;
};
