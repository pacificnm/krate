#pragma once

#include <QString>
#include <QStringList>

// The sound library's file side (docs/SOUNDS.md): finding sound files in a
// folder, reading a WAV's length from its header (no decoding), and guessing
// a sound's category from its name and its folders'.
namespace sounds {

enum class Category { Kick, Snare, Clap, HiHat, Cymbal, Tom, Percussion, Bass, Synth, Keys, Vocal, FX, Loop, Other, Count };

// "Kick", "Snare", ... in Category order.
const QStringList &categoryNames();

// From a path relative to the folder added ("Drum Kits/Dusty/BD_02.wav" and
// the like): the file's name first, then its folders from the nearest; a
// loop anywhere makes it a Loop.
Category guessCategory(const QString &relativePath);

struct FileInfo
{
    QString path;
    double seconds = 0.0; // 0: not known (MP3s, until played)
    int rate = 0, channels = 0;
    QString fingerprint; // the same for the same contents (copies of one sound in several folders)
};

// WAV and MP3 files under `folder`, at any depth.
QStringList findSoundFiles(const QString &folder);
// The contents in short: its size and a hash of its start and end (no need
// to read a long loop whole). "" if it can't be read.
QString fingerprint(const QString &path);
// Its length, rate and channels from a WAV header; for an MP3 only the path
// (and both, its fingerprint).
FileInfo soundInfo(const QString &path);

} // namespace sounds
