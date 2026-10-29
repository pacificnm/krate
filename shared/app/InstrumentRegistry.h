#pragma once

#include <functional>
#include <memory>

#include <QString>
#include <QStringList>

namespace krate {

class IInstrument;
class Transport;

// What a host gives an instrument it creates (docs/SHARED_FOUNDATIONS.MD#adding-instruments).
struct InstrumentContext
{
    // The transport the instrument's own controls (RUN/STOP, Tempo) drive.
    // Standalone: the app's transport. In the rack: nullptr, because the rack's
    // transport bar owns play and tempo.
    Transport *transport = nullptr;
    QString configDir; // e.g. ~/.config/<id>: panel.json, midimap.json
    QString dataDir;   // e.g. ~/.local/share/<id>: patterns and other user data
};

// One live instrument: its realtime core plus the Qt controllers its panel binds
// to (created by the factory, destroyed with this object).
class InstrumentInstance
{
public:
    virtual ~InstrumentInstance() = default;
    virtual IInstrument &instrument() = 0;
    // A learned MIDI button for one of the entry's actions was pressed.
    virtual void triggerAction(const QString &action) { Q_UNUSED(action); }
};

// How a host (standalone app or the Krate rack) finds and builds an instrument.
// Each instrument library exports one; the rack lists them in one place.
struct InstrumentEntry
{
    QString id;                  // "k303": client name, config folder, session key
    QString name;                // "k303" as shown to the user
    int defaultMidiChannel = -1; // 0..15 for the rack's channel routing, -1 = focus only
    QList<int> extraMidiChannels; // more channels (0..15) the rack routes to it (Krimson's slots B-D)
    QString qmlModule;           // QML module holding the panel, e.g. "K303"
    QString panelComponent;      // the panel inside it, e.g. "Panel"
    QStringList actions;         // learnable buttons, e.g. "pattern1", "runstop"
    std::function<std::unique_ptr<InstrumentInstance>(const InstrumentContext &)> create;
};

} // namespace krate
