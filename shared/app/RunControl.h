#pragma once

#include <QObject>
#include <QString>

namespace krate {

// Start and stop one instrument's own sequencer when it doesn't own the
// transport: in the Krate rack, each instrument's RUN / STOP runs just its
// sequencer on the rack's transport (the rack's PLAY / STOP runs them all).
// The rack implements it; instruments get it in their InstrumentContext.
class RunControl : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    // Whether instrument `id`'s sequencer is running now.
    virtual bool running(const QString &id) const = 0;
    virtual void setRunning(const QString &id, bool running) = 0;
    // An instrument with a sequencer (so "all stopped" knows whom to ask).
    virtual void addRunner(const QString &id) = 0;

signals:
    void changed(); // any instrument's running state, or the transport's
};

} // namespace krate
