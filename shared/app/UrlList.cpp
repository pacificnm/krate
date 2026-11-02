#include "app/UrlList.h"

#include <QList>
#include <QUrl>

namespace krate {

namespace {

void collect(QVariant v, QStringList *out)
{
    while (v.metaType() == QMetaType::fromType<QVariant>()) // a list<url> passes each url wrapped again
        v = v.value<QVariant>();
    if (v.metaType() == QMetaType::fromType<QList<QUrl>>()) {
        for (const QUrl &u : v.value<QList<QUrl>>())
            collect(QVariant(u), out);
    } else if (v.metaType() == QMetaType::fromType<QVariantList>()) {
        for (const QVariant &e : v.toList())
            collect(e, out);
    } else if (v.metaType() == QMetaType::fromType<QUrl>()) {
        const QUrl url = v.toUrl();
        if (url.isLocalFile())
            *out << url.toLocalFile();
    } else if (v.canConvert<QString>()) {
        const QString s = v.toString();
        const QUrl url(s);
        if (url.isLocalFile())
            *out << url.toLocalFile();
        else if (!s.isEmpty() && url.scheme().isEmpty())
            *out << s; // a plain path
    }
}

} // namespace

QStringList localPaths(const QVariantList &urls)
{
    QStringList out;
    for (const QVariant &u : urls)
        collect(u, &out);
    return out;
}

} // namespace krate
