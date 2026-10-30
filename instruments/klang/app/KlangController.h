#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class RunControl;
class Transport;
}
namespace klang {
class Engine;
}

// QML singleton "Klang": the panel's controls (krate::ParamController by key;
// the pad controls show the edited pad), the edited pad, hitting pads from the
// screen, their lights, and Tempo and run/stop (standalone they drive the
// transport it is given; in the Krate rack, the rack's run control).
class KlangController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Klang)
    QML_SINGLETON

    Q_PROPERTY(int editedPad READ editedPad WRITE setEditedPad NOTIFY editedPadChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    Q_PROPERTY(bool canRun READ canRun NOTIFY playingChanged)
    // Bumped when the pads' lights change; bind `Klang.lights >= 0 && Klang.sounding(n)`.
    Q_PROPERTY(int lights READ lights NOTIFY lightsChanged)

public:
    KlangController(klang::Engine &engine, const QString &panelFilePath, krate::Transport *transport, QObject *parent);
    ~KlangController() override;

    static KlangController *create(QQmlEngine *, QJSEngine *);

    int editedPad() const;
    void setEditedPad(int pad);
    bool ownsTransport() const { return m_transport != nullptr; }
    bool canRun() const { return m_transport != nullptr || m_run != nullptr; }
    void setRunControl(krate::RunControl *run, const QString &id);
    bool playing() const;
    void setPlaying(bool playing);
    int lights() const { return m_lights; }

    // A pad on the screen: hit it (and edit it), and let it go.
    Q_INVOKABLE void pressPad(int pad);
    Q_INVOKABLE void releasePad(int pad);
    Q_INVOKABLE bool sounding(int pad) const;

signals:
    void editedPadChanged();
    void playingChanged();
    void lightsChanged();

private:
    void syncTempo();
    void poll();

    static inline KlangController *s_instance = nullptr;

    klang::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    krate::RunControl *m_run = nullptr;
    QString m_runId;
    QTimer m_pollTimer;
    bool m_playing = false;
    int m_lights = 0;
    std::uint32_t m_lightsSeen = 0;
};
