#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Where Krate keeps things (docs/SETTINGS.md#folders). Each folder is the
// default unless ~/.config/krate/settings.json says otherwise (Krate's
// Settings screen writes it); the standalone apps read the instruments' folder
// from it too, so they and the rack keep sharing their sounds and patterns.
namespace krate::locations {

enum class Folder {
    Data,        // Krate's own: sequences, the sound index, crash reports, logs (~/.local/share/krate)
    Recordings,  // the audio tracks' takes (<data>/recordings)
    Library,     // the song library: songs' files and library.db (<data>/library)
    Exports,     // EXPORT's WAV files (~/Music/Krate)
    Instruments, // each instrument's sounds and patterns, in <it>/<id> (~/.local/share)
};
inline constexpr Folder kFolders[] = {Folder::Data, Folder::Recordings, Folder::Library, Folder::Exports, Folder::Instruments};

QString key(Folder folder); // "data", "recordings", ... (settings.json's)
QString label(Folder folder); // "Song library"
QString about(Folder folder); // what's in it, a sentence
// In the order the Settings screens show them.
inline constexpr Folder kShown[] = {Folder::Recordings, Folder::Library, Folder::Instruments, Folder::Exports, Folder::Data};

// ~/.config/krate/settings.json: fixed, as it says where the rest is.
QString settingsFile();
QJsonObject readSettings(const QString &path = settingsFile());

// Where the defaults are: this computer's (XDG_DATA_HOME, the Music folder),
// or another's (Krate Remote works them out for the player).
struct Roots
{
    QString dataHome; // ~/.local/share
    QString music;    // ~/Music
};
Roots localRoots();

// A folder as `settings` has it now (its "folders"), or its default.
QString folder(Folder folder, const QJsonObject &settings, const Roots &roots = localRoots());
QString folder(Folder folder); // as settings.json has it
QString defaultFolder(Folder folder, const QJsonObject &settings, const Roots &roots = localRoots());
// An instrument's sounds and patterns: <instruments>/<id>.
QString instrumentData(const QString &id);

// Moving a folder's files to a new place (what a moved folder does at
// Krate's start, before anything opens them): renamed when it can be, else
// copied, checked and the old one removed. Only into a folder that doesn't
// exist or is empty. Returns "" when done, or why not.
QString moveFolder(const QString &from, const QString &to);
bool isEmptyOrMissing(const QString &path);

} // namespace krate::locations
