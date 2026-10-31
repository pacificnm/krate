#pragma once

#include <vector>

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

namespace krate {

// A folder other apps drop presets into for an instrument (Konnekt's SEND TO,
// instruments/konnekt/docs/MANUAL.md#send-to-an-instrument): <data>/inbox,
// one JSON file per preset:
//   {"version": 1, "slot": 0..63, "name": "...", "params": {key: 0..1}, "from": "konnekt"}
// The instrument reads its bank once and rewrites it whole, so another app
// can't edit that file safely while it runs; it takes the inbox instead, when
// it starts and whenever something arrives (the folder is watched), stores
// each preset in its bank and deletes the file.
class PresetInbox : public QObject
{
    Q_OBJECT

public:
    struct Preset
    {
        int slot = -1;
        QString name;
        QHash<QString, qreal> values;
        QString from;
    };

    // `bankFile`: the instrument's bank (patches.json); the inbox is beside it.
    explicit PresetInbox(const QString &bankFile, QObject *parent = nullptr);

    QString dir() const { return m_dir; }
    // The presets waiting, oldest first; their files are removed. Files that
    // aren't presets are removed too (and reported in the log).
    std::vector<Preset> take();

    static QString dirFor(const QString &bankFile);
    // Writes a preset into the inbox beside `bankFile` (atomically); false on failure.
    static bool send(const QString &bankFile, const Preset &preset);

signals:
    void arrived(); // presets are waiting: take() them

private:
    QString m_dir;
    QFileSystemWatcher m_watcher;
    QTimer m_settle; // a little after the folder changes
};

} // namespace krate
