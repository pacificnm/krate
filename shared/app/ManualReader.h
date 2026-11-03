#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// QML singleton "Manual": an instrument's manual (a Markdown file, usually in
// the instrument's QML module), split into sections at its "## " headings for
// the manual view (ManualView.qml). The part before the first "## " (after the
// "# " title) is the first section, "Introduction".
class ManualReader : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Manual)
    QML_SINGLETON

public:
    struct Section
    {
        QString title;
        QString text; // Markdown, without its heading
    };

    using QObject::QObject;

    static QList<Section> split(const QString &markdown);

    // A section's text as the view shows it: Markdown, and pictures (a line
    // of its own, `![caption](path)`, the path relative to the manual).
    struct Block
    {
        QString text;    // Markdown ("" for a picture)
        QString image;   // the picture's path, as written
        QString caption;
    };
    static QList<Block> blocks(const QString &text);

    // [{title, text, blocks: [{text} or {image (a URL), caption}]}] from a
    // file (a path, "qrc:/..." or ":/..."); empty if it can't be read.
    Q_INVOKABLE QVariantList sections(const QString &source) const;
    // The section numbers whose title or text contain every word of `query` (case-insensitive).
    Q_INVOKABLE QVariantList search(const QVariantList &sections, const QString &query) const;
};
