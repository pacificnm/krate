#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

namespace krate {
class Rack;
}
class RackController;
class RackMidiLearn;

// rack.json (~/.config/krate): tempo, focus, MIDI device, mixer, channel
// routing and the rack's MIDI mappings, keyed by instrument id. Loaded at
// startup; saved 2 s after any change and on destruction. Each instrument's
// own state (knobs, patterns) stays in its own folder.
class RackSession : public QObject
{
    Q_OBJECT

public:
    static constexpr int kVersion = 1;

    RackSession(const QString &path, RackController &controller, krate::Rack &rack, RackMidiLearn &learn,
                const QStringList &deviceIds, QObject *parent);
    ~RackSession() override;

    // Applies the saved session (a missing or unreadable file leaves defaults).
    void load();
    void save();
    QString midiInput() const { return m_midiInput; }

private:
    void changed() { m_saveTimer.start(); }

    QString m_path;
    RackController &m_controller;
    krate::Rack &m_rack;
    RackMidiLearn &m_learn;
    QStringList m_ids;
    QString m_midiInput = QStringLiteral("Impact LX49");
    QTimer m_saveTimer;
};
