#pragma once

#include <deque>
#include <memory>

#include <QFile>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "Shell.h"

class QProcess;
class QQmlEngine;
class QJSEngine;

// QML singleton "Files" (docs/REMOTE.md#files): the player's files, a folder
// at a time, and sending files there and back. Files go one at a time, each
// streamed through its own ssh (the connection is shared): an upload lands
// beside its place and is renamed into it once whole, a download likewise
// here, so a cut-off transfer never leaves half a file under the real name.
class RemoteFiles : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Files)
    QML_SINGLETON

    Q_PROPERTY(QString path READ path NOTIFY listed)
    // [{name, dir, size, modified (ms since the epoch), link}]: folders first, then by name
    Q_PROPERTY(QVariantList entries READ entries NOTIFY listed)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // [{name, upload, done, total, state ("waiting", "sending", "done", "failed", "cancelled"), error}]
    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)
    Q_PROPERTY(bool transferring READ transferring NOTIFY transfersChanged)
    Q_PROPERTY(QString downloadFolder READ downloadFolder WRITE setDownloadFolder NOTIFY downloadFolderChanged)

public:
    explicit RemoteFiles(Shell &shell, QObject *parent = nullptr);
    ~RemoteFiles() override;
    static RemoteFiles *create(QQmlEngine *, QJSEngine *);

    QString path() const { return m_path; }
    QVariantList entries() const { return m_entries; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QVariantList transfers() const;
    bool transferring() const;
    QString downloadFolder() const { return m_downloadFolder; }
    void setDownloadFolder(const QString &folder);

    Q_INVOKABLE void open(const QString &path);
    Q_INVOKABLE void openEntry(const QString &name) { open(child(name)); }
    Q_INVOKABLE void up();
    Q_INVOKABLE void refresh() { open(m_path); }
    Q_INVOKABLE void makeFolder(const QString &name);
    Q_INVOKABLE void rename(const QString &from, const QString &to);
    Q_INVOKABLE void remove(const QStringList &names); // in this folder, folders with all they hold
    Q_INVOKABLE bool exists(const QString &name) const;

    // Local files or folders (from a file dialog, or dropped) into this folder.
    Q_INVOKABLE void upload(const QList<QUrl> &urls);
    // Entries of this folder into downloadFolder (folders with all they hold).
    Q_INVOKABLE void download(const QStringList &names);
    Q_INVOKABLE void cancelTransfers();
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void reset(); // disconnected: nothing to show

signals:
    void listed();
    void loadingChanged();
    void errorChanged();
    void transfersChanged();
    void downloadFolderChanged();
    void transfersDone(); // the queue is empty again

private:
    struct Transfer
    {
        bool upload = true;
        QString local, remote, name;
        qint64 total = 0, done = 0;
        QString state = QStringLiteral("waiting");
        QString error{};
    };

    QString child(const QString &name) const;
    void setError(const QString &error);
    void queue(Transfer t);
    void next();
    void startUpload(Transfer &t);
    void startDownload(Transfer &t);
    void feed();
    void finish(const QString &error);
    void changedSoon();

    Shell &m_shell;
    QString m_path;
    QVariantList m_entries;
    bool m_loading = false;
    int m_listing = 0; // the latest listing's number: earlier answers are dropped
    QString m_error;
    QString m_downloadFolder;

    std::deque<Transfer> m_transfers;
    int m_current = -1; // index of the transfer going
    QPointer<QProcess> m_process;
    std::unique_ptr<QFile> m_file;
    bool m_uploadedHere = false; // refresh when the queue empties
    QTimer m_tick;               // progress, a few times a second
    bool m_dirty = false;
};
