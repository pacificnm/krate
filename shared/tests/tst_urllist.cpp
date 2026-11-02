// Files from QML into C++ (krate::localPaths): a file dialog's list<url>
// arrives with each url wrapped in a QVariant of its own (an import that took
// them as plain urls got nothing: Krunch's IMPORT of IRs and captures);
// a [folder], a drop's urls and plain paths come through too.

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtTest>

#include "app/UrlList.h"

class Sink : public QObject
{
    Q_OBJECT

public:
    QList<QStringList> got;
    Q_INVOKABLE void take(const QVariantList &urls) { got << krate::localPaths(urls); }
};

class TestUrlList : public QObject
{
    Q_OBJECT

private slots:
    void fromQml()
    {
        QQmlEngine engine;
        Sink sink;
        engine.rootContext()->setContextProperty(QStringLiteral("sink"), &sink);
        QQmlComponent c(&engine);
        c.setData("import QtQml\n"
                  "QtObject {\n"
                  "  property list<url> files: [\"file:///tmp/a b.wav\", \"file:///tmp/c.nam\"]\n" // as FileDialog.selectedFiles
                  "  property url folder: \"file:///tmp/irs\"\n"                                     // as FolderDialog.selectedFolder
                  "  Component.onCompleted: { sink.take(files); sink.take([folder]); sink.take([Qt.resolvedUrl(\"file:///tmp/d.wav\")]) }\n"
                  "}",
                  QUrl());
        std::unique_ptr<QObject> o(c.create());
        QVERIFY2(o, qPrintable(c.errorString()));
        QCOMPARE(sink.got.size(), 3);
        QCOMPARE(sink.got[0], (QStringList{QStringLiteral("/tmp/a b.wav"), QStringLiteral("/tmp/c.nam")}));
        QCOMPARE(sink.got[1], QStringList{QStringLiteral("/tmp/irs")});
        QCOMPARE(sink.got[2], QStringList{QStringLiteral("/tmp/d.wav")});
    }

    void fromCpp()
    {
        const QVariantList mixed{QVariant::fromValue(QVariant(QUrl(QStringLiteral("file:///x/1.wav")))),
                                 QUrl(QStringLiteral("file:///x/2.wav")), QStringLiteral("/x/3.wav"),
                                 QStringLiteral("file:///x/4.wav"), QUrl(QStringLiteral("https://example.com/5.wav")),
                                 QVariant::fromValue(QList<QUrl>{QUrl(QStringLiteral("file:///x/6.wav"))}), QString()};
        QCOMPARE(krate::localPaths(mixed), (QStringList{QStringLiteral("/x/1.wav"), QStringLiteral("/x/2.wav"), QStringLiteral("/x/3.wav"),
                                                        QStringLiteral("/x/4.wav"), QStringLiteral("/x/6.wav")}));
    }
};

QTEST_GUILESS_MAIN(TestUrlList)
#include "tst_urllist.moc"
