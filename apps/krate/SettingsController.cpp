#include "SettingsController.h"

#include <optional>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>

#include "plugin/Clap.h"

Q_LOGGING_CATEGORY(lcSettings, "krate.settings")

using krate::locations::Folder;
namespace loc = krate::locations;

namespace {

constexpr int kFileVersion = 1;

struct FolderText
{
    Folder folder;
    const char *label;
    const char *about;
};
// In the order the screen shows them.
constexpr FolderText kTexts[] = {
    {Folder::Recordings, "Recordings", "The audio tracks' takes (WAV files)."},
    {Folder::Library, "Song library", "The songs you've added (tabs, MIDI files, backing tracks) and the library's database."},
    {Folder::Instruments, "Instruments' sounds",
     "Each instrument's patterns, patches, kits, samples and cabinets, in a folder of its own (the instruments' own apps use it too)."},
    {Folder::Exports, "Exports", "Where EXPORT writes the song and its stems."},
    {Folder::Data, "Krate's data", "Sequences, the sound library's index, crash reports and logs (and recordings and songs, unless they're elsewhere)."},
};

std::optional<Folder> folderOf(const QString &key)
{
    for (Folder f : loc::kFolders)
        if (loc::key(f) == key)
            return f;
    return std::nullopt;
}

QStringList strings(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &s : v.toArray())
        out << s.toString();
    return out;
}

} // namespace

QJsonObject SettingsController::applyPending(QJsonObject settings, const QStringList &instrumentIds, QStringList *notes)
{
    const QJsonObject pending = settings.value(u"pending").toObject();
    if (pending.isEmpty())
        return settings;
    // Where everything goes: the folders as they'll be (a default follows the data folder).
    QJsonObject after = settings;
    QJsonObject folders = after.value(u"folders").toObject();
    for (auto it = pending.begin(); it != pending.end(); ++it)
        folders[it.key()] = it.value().toObject().value(u"path").toString();
    after[QStringLiteral("folders")] = folders;

    QJsonObject done = settings.value(u"folders").toObject();
    // The data folder last: one moved inside it (by default) is moved out of it first.
    for (Folder f : {Folder::Recordings, Folder::Library, Folder::Instruments, Folder::Exports, Folder::Data}) {
        const QString key = loc::key(f);
        if (!pending.contains(key))
            continue;
        const QJsonObject p = pending.value(key).toObject();
        const QString from = loc::folder(f, settings), to = loc::folder(f, after);
        QString why;
        if (p.value(u"move").toBool() && f != Folder::Exports) {
            if (f == Folder::Instruments) { // just the instruments' own folders
                for (const QString &id : instrumentIds)
                    if (QFileInfo(QDir(from).filePath(id)).isDir() && !loc::isEmptyOrMissing(QDir(to).filePath(id)))
                        why = QStringLiteral("%1 has a %2 folder already").arg(to, id);
                QStringList moved;
                for (const QString &id : instrumentIds) {
                    if (!why.isEmpty())
                        break;
                    if (const QString err = loc::moveFolder(QDir(from).filePath(id), QDir(to).filePath(id)); err.isEmpty())
                        moved << id;
                    else
                        why = err;
                }
                if (!why.isEmpty()) // all or nothing: back where they were
                    for (const QString &id : std::as_const(moved))
                        loc::moveFolder(QDir(to).filePath(id), QDir(from).filePath(id));
            } else {
                why = loc::moveFolder(from, to);
            }
        }
        if (why.isEmpty()) {
            done[key] = p.value(u"path").toString();
            *notes << (p.value(u"move").toBool() && f != Folder::Exports ? QStringLiteral("%1: moved to %2") : QStringLiteral("%1: now %2"))
                          .arg(key, to);
        } else {
            *notes << QStringLiteral("%1: not moved (%2), still %3").arg(key, why, from);
        }
    }
    settings[QStringLiteral("folders")] = done;
    settings.remove(QStringLiteral("pending"));
    return settings;
}

SettingsController::SettingsController(QString path, QStringList instrumentIds, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
    , m_ids(std::move(instrumentIds))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    QJsonObject json = loc::readSettings(m_path);
    m_pluginFolders = strings(json.value(u"pluginFolders"));
    m_midiInputs = json.value(u"midiInputs").toString();
    m_programChangeFocus = json.value(u"programChangeFocus").toBool(true);
    m_autoCheckUpdates = json.value(u"autoCheckUpdates").toBool(true);
    const bool waiting = json.contains(u"pending");
    if (waiting) // before anything opens the folders
        json = applyPending(json, m_ids, &m_notes);
    m_current = json;
    if (waiting)
        save();
    for (const QString &note : std::as_const(m_notes))
        qCInfo(lcSettings) << note;
}

SettingsController::~SettingsController()
{
    s_instance = nullptr;
}

SettingsController *SettingsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList SettingsController::folders() const
{
    QVariantList list;
    for (const FolderText &t : kTexts) {
        const QString key = loc::key(t.folder);
        const QJsonObject p = m_pending.value(key).toObject();
        const bool waiting = m_pending.contains(key);
        QJsonObject next = m_current;
        QJsonObject nextFolders = next.value(u"folders").toObject();
        if (waiting)
            nextFolders[key] = p.value(u"path").toString();
        next[QStringLiteral("folders")] = nextFolders;
        list << QVariantMap{
            {QStringLiteral("key"), key},
            {QStringLiteral("label"), QString::fromLatin1(t.label)},
            {QStringLiteral("about"), QString::fromLatin1(t.about)},
            {QStringLiteral("path"), folder(t.folder)},
            {QStringLiteral("defaultPath"), loc::defaultFolder(t.folder, m_current)},
            {QStringLiteral("isDefault"), m_current.value(u"folders").toObject().value(key).toString().isEmpty()},
            {QStringLiteral("pending"), waiting ? loc::folder(t.folder, next) : QString()},
            {QStringLiteral("pendingMove"), p.value(u"move").toBool()},
            {QStringLiteral("atOnce"), t.folder == Folder::Exports},
        };
    }
    return list;
}

void SettingsController::setFolder(const QString &key, const QString &path, bool move)
{
    const auto f = folderOf(key);
    if (!f)
        return;
    const QString clean = path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (*f == Folder::Exports) { // nothing holds it open: at once
        QJsonObject folders = m_current.value(u"folders").toObject();
        folders[key] = clean;
        m_current[QStringLiteral("folders")] = folders;
        save();
        if (onExportsChanged)
            onExportsChanged(folder(Folder::Exports));
        emit changed();
        return;
    }
    QJsonObject next = m_current;
    QJsonObject folders = next.value(u"folders").toObject();
    folders[key] = clean;
    next[QStringLiteral("folders")] = folders;
    if (loc::folder(*f, next) == folder(*f)) { // where it is already
        cancel(key);
        return;
    }
    m_pending[key] = QJsonObject{{QStringLiteral("path"), clean}, {QStringLiteral("move"), move}};
    qCInfo(lcSettings) << key << "->" << loc::folder(*f, next) << (move ? "(moving its files)" : "") << "at the next start";
    save();
    emit changed();
}

void SettingsController::resetFolder(const QString &key, bool move) { setFolder(key, QString(), move); }

void SettingsController::cancel(const QString &key)
{
    if (!m_pending.contains(key))
        return;
    m_pending.remove(key);
    save();
    emit changed();
}

void SettingsController::setPluginFolders(const QStringList &folders)
{
    QStringList clean;
    for (const QString &f : folders)
        if (!f.trimmed().isEmpty() && !clean.contains(QDir::cleanPath(f.trimmed())))
            clean << QDir::cleanPath(f.trimmed());
    if (clean == m_pluginFolders)
        return;
    m_pluginFolders = clean;
    save();
    if (onPluginFoldersChanged)
        onPluginFoldersChanged(m_pluginFolders);
    emit changed();
}

QStringList SettingsController::standardPluginFolders() const
{
    QStringList list;
    for (const std::string &dir : krate::clap::defaultSearchPaths())
        list << QString::fromStdString(dir);
    return list;
}

void SettingsController::setMidiInputs(const QString &pattern)
{
    if (pattern == m_midiInputs)
        return;
    m_midiInputs = pattern;
    save();
    if (onMidiInputsChanged)
        onMidiInputsChanged(m_midiInputs);
    emit changed();
}

void SettingsController::setProgramChangeFocus(bool on)
{
    if (on == m_programChangeFocus)
        return;
    m_programChangeFocus = on;
    save();
    if (onProgramChangeFocusChanged)
        onProgramChangeFocusChanged(on);
    emit changed();
}

void SettingsController::setAutoCheckUpdates(bool on)
{
    if (on == m_autoCheckUpdates)
        return;
    m_autoCheckUpdates = on;
    save();
    if (onAutoCheckChanged)
        onAutoCheckChanged(on);
    emit changed();
}

QString SettingsController::configFolder() const { return QFileInfo(m_path).absolutePath(); }

bool SettingsController::kiosk() const { return qEnvironmentVariableIsSet("KRATE_KIOSK"); }

void SettingsController::restart()
{
    qCInfo(lcSettings) << "restarting to apply settings";
    if (!kiosk()) { // a desktop: start again once this one has let go of the audio (its JACK name)
        QStringList args{QStringLiteral("-c"), QStringLiteral("sleep 2; exec \"$0\" \"$@\""),
                         QCoreApplication::applicationFilePath()};
        args += QCoreApplication::arguments().mid(1);
        QProcess::startDetached(QStringLiteral("/bin/sh"), args);
    }
    QCoreApplication::quit(); // the kiosk's service starts it again
}

void SettingsController::save()
{
    QJsonObject json = m_current;
    json[QStringLiteral("version")] = kFileVersion;
    json[QStringLiteral("pluginFolders")] = QJsonArray::fromStringList(m_pluginFolders);
    json[QStringLiteral("midiInputs")] = m_midiInputs;
    json[QStringLiteral("programChangeFocus")] = m_programChangeFocus;
    json[QStringLiteral("autoCheckUpdates")] = m_autoCheckUpdates;
    if (!m_pending.isEmpty())
        json[QStringLiteral("pending")] = m_pending;
    else
        json.remove(QStringLiteral("pending"));
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(json).toJson());
        f.commit();
    }
}
