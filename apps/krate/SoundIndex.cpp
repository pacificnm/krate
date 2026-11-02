#include "SoundIndex.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <optional>
#include <vector>

namespace sounds {

const QStringList &categoryNames()
{
    static const QStringList names{QStringLiteral("Kick"),  QStringLiteral("Snare"),      QStringLiteral("Clap"),
                                   QStringLiteral("Hi-hat"), QStringLiteral("Cymbal"),    QStringLiteral("Tom"),
                                   QStringLiteral("Percussion"), QStringLiteral("Bass"),  QStringLiteral("Synth"),
                                   QStringLiteral("Keys"),  QStringLiteral("Vocal"),      QStringLiteral("FX"),
                                   QStringLiteral("Loop"),  QStringLiteral("Other")};
    return names;
}

namespace {

// Words: camelCase and letters/digits split apart, lower case ("OpenHat02" -> open hat 02).
QStringList words(QString text)
{
    static const QRegularExpression camel(QStringLiteral("([a-z])([A-Z])"));
    static const QRegularExpression letterDigit(QStringLiteral("([A-Za-z])([0-9])|([0-9])([A-Za-z])"));
    static const QRegularExpression separators(QStringLiteral("[^a-z0-9]+"));
    text.replace(camel, QStringLiteral("\\1 \\2"));
    text.replace(letterDigit, QStringLiteral("\\1\\3 \\2\\4"));
    return text.toLower().split(separators, Qt::SkipEmptyParts);
}

struct Rule
{
    Category category;
    QStringList words;
};

// Two-word names first (a "bass drum" is a kick), then single words.
const std::vector<Rule> &pairRules()
{
    static const std::vector<Rule> rules{
        {Category::Kick, {QStringLiteral("bass drum"), QStringLiteral("kick drum")}},
        {Category::HiHat, {QStringLiteral("hi hat"), QStringLiteral("open hat"), QStringLiteral("closed hat"),
                           QStringLiteral("high hat"), QStringLiteral("pedal hat")}},
        {Category::Tom, {QStringLiteral("floor tom")}},
        {Category::Percussion, {QStringLiteral("rim shot"), QStringLiteral("wood block")}},
    };
    return rules;
}

const std::vector<Rule> &wordRules()
{
    static const std::vector<Rule> rules{
        {Category::Kick, {"kick", "kicks", "kik", "kck", "bd", "bassdrum", "kickdrum"}},
        {Category::Snare, {"snare", "snares", "snr", "sd"}},
        {Category::Clap, {"clap", "claps", "clp", "snap", "snaps"}},
        {Category::HiHat, {"hat", "hats", "hihat", "hihats", "hh", "hhc", "hho", "chh", "ohh", "ch", "oh"}},
        {Category::Cymbal, {"crash", "crashes", "ride", "rides", "cymbal", "cymbals", "cym", "splash", "china"}},
        {Category::Tom, {"tom", "toms"}},
        {Category::Percussion, {"perc", "percs", "percussion", "shaker", "shakers", "tamb", "tambourine", "conga", "congas",
                                "bongo", "bongos", "cowbell", "clave", "claves", "rim", "rims", "rimshot", "woodblock",
                                "cabasa", "guiro", "triangle", "timbale", "timbales"}},
        {Category::Bass, {"bass", "basses", "sub", "subs"}},
        {Category::Synth, {"synth", "synths", "lead", "leads", "pad", "pads", "pluck", "plucks", "stab", "stabs", "arp",
                           "arps", "saw"}},
        {Category::Keys, {"piano", "pianos", "keys", "organ", "organs", "epiano", "chord", "chords", "keyboard"}},
        {Category::Vocal, {"vocal", "vocals", "vox", "voice", "voices", "chant", "chants", "acapella", "spoken", "phrase",
                           "adlib", "adlibs"}},
        {Category::FX, {"fx", "sfx", "riser", "risers", "rise", "sweep", "sweeps", "impact", "impacts", "downlifter",
                        "uplifter", "whoosh", "glitch", "reverse", "transition", "foley", "noise"}},
    };
    return rules;
}

bool isLoopWord(const QString &w)
{
    static const QRegularExpression bpm(QStringLiteral("^\\d{2,3}bpm$"));
    return w == u"loop" || w == u"loops" || w == u"bpm" || w == u"groove" || w == u"grooves" || bpm.match(w).hasMatch();
}

// The category these words name, if any: pairs first, then the first word that names one.
std::optional<Category> named(const QStringList &w)
{
    for (const Rule &rule : pairRules())
        for (qsizetype i = 0; i + 1 < w.size(); ++i)
            if (rule.words.contains(w[i] + u' ' + w[i + 1]))
                return rule.category;
    for (const QString &word : w)
        for (const Rule &rule : wordRules())
            if (rule.words.contains(word))
                return rule.category;
    return std::nullopt;
}

} // namespace

Category guessCategory(const QString &relativePath)
{
    QStringList parts = relativePath.split(u'/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return Category::Other;
    parts.last() = QFileInfo(parts.last()).completeBaseName();
    // A loop anywhere: a loop, whatever it's a loop of.
    for (const QString &part : parts)
        for (const QString &w : words(part))
            if (isLoopWord(w))
                return Category::Loop;
    for (qsizetype i = parts.size() - 1; i >= 0; --i) // the name, then its folders from the nearest
        if (const auto category = named(words(parts[i])))
            return *category;
    return Category::Other;
}

QStringList findSoundFiles(const QString &folder)
{
    QStringList files;
    QDirIterator it(folder, {QStringLiteral("*.wav"), QStringLiteral("*.WAV"), QStringLiteral("*.Wav"), QStringLiteral("*.mp3"),
                             QStringLiteral("*.MP3")},
                    QDir::Files | QDir::Readable, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext())
        files << it.next();
    files.sort(Qt::CaseInsensitive);
    return files;
}

QString fingerprint(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    constexpr qint64 kPart = 64 * 1024;
    const qint64 size = file.size();
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(file.read(kPart));
    if (size > kPart) {
        file.seek(std::max(kPart, size - kPart));
        hash.addData(file.read(kPart));
    }
    return QString::number(size) + u':' + QString::fromLatin1(hash.result().toHex());
}

FileInfo soundInfo(const QString &path)
{
    FileInfo info;
    info.path = path;
    info.fingerprint = fingerprint(path);
    if (!path.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive))
        return info;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return info;
    const QByteArray head = file.read(12);
    if (head.size() < 12 || !head.startsWith("RIFF") || head.mid(8, 4) != "WAVE")
        return info;
    const auto u16 = [](const QByteArray &b, int at) { return quint16(quint8(b[at]) | quint8(b[at + 1]) << 8); };
    const auto u32 = [](const QByteArray &b, int at) {
        return quint32(quint8(b[at])) | quint32(quint8(b[at + 1])) << 8 | quint32(quint8(b[at + 2])) << 16
               | quint32(quint8(b[at + 3])) << 24;
    };
    int blockAlign = 0;
    for (int guard = 0; guard < 64 && !file.atEnd(); ++guard) { // its chunks: fmt, then data
        const QByteArray chunk = file.read(8);
        if (chunk.size() < 8)
            break;
        const quint32 size = u32(chunk, 4);
        if (chunk.startsWith("fmt ")) {
            const QByteArray fmt = file.read(std::min<quint32>(size, 64));
            if (fmt.size() < 16)
                break;
            info.channels = u16(fmt, 2);
            info.rate = int(u32(fmt, 4));
            blockAlign = u16(fmt, 12);
            file.seek(file.pos() - fmt.size() + size + (size & 1));
        } else if (chunk.startsWith("data")) {
            if (blockAlign > 0 && info.rate > 0)
                info.seconds = double(size / quint32(blockAlign)) / info.rate;
            break;
        } else {
            file.seek(file.pos() + size + (size & 1));
        }
    }
    return info;
}

} // namespace sounds
