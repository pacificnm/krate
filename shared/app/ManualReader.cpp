#include "ManualReader.h"

#include <QFile>
#include <QRegularExpression>
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

QList<ManualReader::Block> ManualReader::blocks(const QString &text)
{
    static const QRegularExpression picture(QStringLiteral(R"(^\s*!\[([^\]]*)\]\(([^)\s]+)\)\s*$)"));
    QList<Block> result;
    QString pending;
    bool inCode = false;
    const auto flush = [&] {
        if (!pending.trimmed().isEmpty())
            result << Block{pending.trimmed(), {}, {}};
        pending.clear();
    };
    for (const QString &line : text.split(u'\n')) {
        if (line.startsWith(QStringLiteral("```")))
            inCode = !inCode;
        if (const auto m = picture.match(line); !inCode && m.hasMatch()) {
            flush();
            result << Block{{}, m.captured(2), m.captured(1)};
            continue;
        }
        pending += line + u'\n';
    }
    flush();
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
    // Pictures are found next to the manual.
    const QUrl base = source.startsWith(u"qrc:") || source.startsWith(u"file:") ? QUrl(source)
                      : source.startsWith(u':')                                 ? QUrl(QStringLiteral("qrc") + source)
                                                                                : QUrl::fromLocalFile(source);
    for (const Section &s : split(QString::fromUtf8(file.readAll()))) {
        QVariantList shown;
        for (const Block &b : blocks(s.text))
            shown << (b.image.isEmpty() ? QVariantMap{{QStringLiteral("text"), b.text}}
                                        : QVariantMap{{QStringLiteral("image"), base.resolved(QUrl(b.image)).toString()},
                                                      {QStringLiteral("caption"), b.caption}});
        result << QVariantMap{{QStringLiteral("title"), s.title}, {QStringLiteral("text"), s.text}, {QStringLiteral("blocks"), shown}};
    }
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
