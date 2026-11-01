#pragma once

#include <vector>

#include <QString>
#include <QStringList>
#include <QVariantList>

#include "SoundIndex.h"

// The sound library's database (docs/SOUNDS.md), ~/.local/share/krate/sounds.db:
// the folders you added and the sound files in them, indexed where they are
// (nothing copied), each with a category (guessed from its names until you
// set one), a favourite mark, its length. A folder scanned again keeps your
// categories and favourites; files gone from it go from the library.
class SoundLibrary
{
public:
    explicit SoundLibrary(const QString &file);
    ~SoundLibrary();
    SoundLibrary(const SoundLibrary &) = delete;
    SoundLibrary &operator=(const SoundLibrary &) = delete;

    bool isOpen() const { return m_open; }

    QStringList folders() const;
    bool addFolder(const QString &folder);      // false if it's there already (or inside one)
    void removeFolder(const QString &folder);   // and its sounds
    // A folder's files as found now (its scan): new ones added (their category
    // guessed), lengths updated, gone ones dropped. Returns how many it holds.
    int update(const QString &folder, const std::vector<sounds::FileInfo> &files);

    struct Query
    {
        int category = -1;       // a Category, -1: all
        bool favourites = false;
        QString search;          // in the name or its folders
        QString folder;          // "": all
        int limit = 5000;
    };
    // [{id, name, dir (its folder, relative), path, category, favorite, seconds}], by name.
    QVariantList sounds(const Query &query) const;
    QVariantMap sound(int id) const;
    // Per category (Category order), how many; the favourites; all.
    std::vector<int> categoryCounts() const;
    int favouriteCount() const;
    int total() const;
    void setCategory(int id, int category); // yours: kept from then on
    void setFavourite(int id, bool favourite);
    void setSeconds(int id, double seconds); // an MP3's, once known

private:
    QString m_connection;
    bool m_open = false;
};
