#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class UpdateController;

namespace krate {

class HostController;

// A standalone app's own settings, what the rack keeps in rack.json and
// settings.json: which capture port feeds each input, where the outputs go,
// the buffer size, and whether to look for updates by itself. Kept in
// `path` (~/.config/<app>/app.json), saved as they change.
//
// A first start (no file yet) takes the audio choices from `seedPath` (Krate's
// rack.json) when there is one, so a guitar already set up in Krate plays at
// once; nothing chosen anywhere, the interface's inputs are wired in order.
class StandaloneSession : public QObject
{
    Q_OBJECT

public:
    StandaloneSession(QString path, HostController &host, UpdateController *updates, const QString &seedPath = {},
                      QObject *parent = nullptr);

    QStringList audioInputs() const { return m_audioInputs; }
    QString outputDevice() const { return m_outputDevice; }
    int bufferSize() const { return m_bufferSize; }
    bool autoCheckUpdates() const { return m_autoCheck; }
    bool seeded() const { return m_seeded; } // this start took Krate's choices

private:
    void read(const QJsonObject &json);
    void save();

    QString m_path;
    HostController &m_host;
    UpdateController *m_updates;
    QStringList m_audioInputs;
    QString m_outputDevice;
    int m_bufferSize = 0;
    bool m_autoCheck = true;
    bool m_seeded = false;
};

} // namespace krate
