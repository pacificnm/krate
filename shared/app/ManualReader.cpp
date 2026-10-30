#include "ManualReader.h"

#include <QFile>
#include <QUrl>

QList<ManualReader::Section> ManualReader::split(const QString &markdown)
{
    QList<Section> result;
    Section current{QStringLiteral("Introduction"), {}};
    bool inCode = false;
    for (const QString &line : markdown.split(u'\n')) {
        if (line.startsWith(QStringLiteral("```")))
            inCode = !inCode;
        if (!inCode && line.startsWith(QStringLiteral("## "))) {
            if (!current.text.trimmed().isEmpty())
                result << current;
            current = {line.mid(3).trimmed(), {}};
            continue;
        }
        if (!inCode && line.startsWith(QStringLiteral("# ")))
            continue; // the title: the view shows it
        current.text += line + u'\n';
    }
    if (!current.text.trimmed().isEmpty())
        result << current;
    for (Section &s : result)
        s.text = s.text.trimmed();
    return result;
}

QVariantList ManualReader::sections(const QString &source) const
{
    QString path = source;
    if (source.startsWith(u"qrc:"))
        path = u':' + QUrl(source).path();
    else if (source.startsWith(u"file:"))
        path = QUrl(source).toLocalFile();
    QFile file(path);
    QVariantList result;
    if (!file.open(QIODevice::ReadOnly))
        return result;
    for (const Section &s : split(QString::fromUtf8(file.readAll())))
        result << QVariantMap{{QStringLiteral("title"), s.title}, {QStringLiteral("text"), s.text}};
    return result;
}

QVariantList ManualReader::search(const QVariantList &sections, const QString &query) const
{
    const QStringList words = query.split(u' ', Qt::SkipEmptyParts);
    QVariantList result;
    for (qsizetype i = 0; i < sections.size(); ++i) {
        const QVariantMap s = sections[i].toMap();
        const QString text = s.value(QStringLiteral("title")).toString() + u' ' + s.value(QStringLiteral("text")).toString();
        if (std::all_of(words.begin(), words.end(), [&text](const QString &w) { return text.contains(w, Qt::CaseInsensitive); }))
            result << int(i);
    }
    return result;
}
