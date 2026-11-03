#include <QtTest>

#include <QFile>
#include <QUrl>
#include <QTemporaryDir>
#include <QRegularExpression>

#include "app/ManualReader.h"

// The manual reader, and each instrument's manual covering every control on its panel.
class TestManual : public QObject
{
    Q_OBJECT

    static QString read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

private slots:
    void splitsAtSecondLevelHeadings()
    {
        const auto sections = ManualReader::split(QStringLiteral(
            "# Title\n\nIntro text.\n\n## One\n\nFirst.\n\n```\n## not a heading in code\n```\n\n## Two\n\nSecond.\n"));
        QCOMPARE(sections.size(), 3);
        QCOMPARE(sections[0].title, QStringLiteral("Introduction"));
        QCOMPARE(sections[0].text, QStringLiteral("Intro text."));
        QCOMPARE(sections[1].title, QStringLiteral("One"));
        QVERIFY(sections[1].text.contains(QStringLiteral("## not a heading in code")));
        QCOMPARE(sections[2].text, QStringLiteral("Second."));
    }

    // A picture on a line of its own splits the text around it (not in code);
    // its path is found next to the manual.
    void picturesBetweenTheText()
    {
        const auto blocks = ManualReader::blocks(QStringLiteral(
            "Before.\n\n![The panel](images/panel.png)\n\nAfter, ![inline](x.png) stays.\n```\n![code](c.png)\n```"));
        QCOMPARE(blocks.size(), 3);
        QCOMPARE(blocks[0].text, QStringLiteral("Before."));
        QCOMPARE(blocks[1].image, QStringLiteral("images/panel.png"));
        QCOMPARE(blocks[1].caption, QStringLiteral("The panel"));
        QVERIFY(blocks[2].text.startsWith(QStringLiteral("After, ![inline](x.png) stays.")));
        QVERIFY(blocks[2].text.contains(QStringLiteral("![code](c.png)")));

        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("MANUAL.md")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# T\n\nIntro.\n\n![Shot](images/a.png)\n");
        f.close();
        const QVariantList shown = ManualReader().sections(f.fileName()).value(0).toMap().value(QStringLiteral("blocks")).toList();
        QCOMPARE(shown.size(), 2);
        QCOMPARE(shown[1].toMap().value(QStringLiteral("image")).toString(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("images/a.png"))).toString());
    }

    // Every picture a manual shows is there (in the repo, and so in the app's resources).
    void picturesExist()
    {
        for (const QString &manual : {QStringLiteral("/apps/krate"), QStringLiteral("/instruments/korrode"), QStringLiteral("/instruments/kessel"),
                                      QStringLiteral("/instruments/kalliope"), QStringLiteral("/instruments/krimson"), QStringLiteral("/instruments/kobalt"),
                                      QStringLiteral("/instruments/klang"), QStringLiteral("/instruments/konnekt"), QStringLiteral("/instruments/krunch"),
                                      QStringLiteral("/instruments/klick")}) {
            const QString dir = QStringLiteral(KRATE_SOURCE_DIR) + manual + QStringLiteral("/docs/");
            const QString text = read(dir + QStringLiteral("MANUAL.md"));
            QVERIFY2(!text.isEmpty(), qPrintable(manual));
            for (const auto &s : ManualReader::split(text))
                for (const auto &b : ManualReader::blocks(s.text))
                    if (!b.image.isEmpty())
                        QVERIFY2(QFile::exists(dir + b.image), qPrintable(manual + QStringLiteral(": ") + b.image));
        }
    }

    void searches()
    {
        ManualReader reader;
        const QVariantList sections{QVariantMap{{"title", "Oscillators"}, {"text", "Hard sync tears"}},
                                    QVariantMap{{"title", "Filter"}, {"text", "Resonance sings"}}};
        QCOMPARE(reader.search(sections, "SYNC"), (QVariantList{0}));
        QCOMPARE(reader.search(sections, "filter sings"), (QVariantList{1}));
        QCOMPARE(reader.search(sections, "nothing"), QVariantList{});
    }

    // Every label on an instrument's panel is explained in its manual (a
    // two-line label, "CUTOFF\nFREQ", as its words: "CUTOFF FREQ").
    void manualCoversThePanel_data()
    {
        QTest::addColumn<QString>("instrument");
        QTest::addColumn<QString>("field"); // the QML property that names a control
        QTest::newRow("krimson") << QStringLiteral("krimson") << QStringLiteral("label");
        QTest::newRow("kalliope") << QStringLiteral("kalliope") << QStringLiteral("label");
        QTest::newRow("kobalt") << QStringLiteral("kobalt") << QStringLiteral("label");
        QTest::newRow("klang") << QStringLiteral("klang") << QStringLiteral("label");
        QTest::newRow("kessel") << QStringLiteral("kessel") << QStringLiteral("label");
        QTest::newRow("korrode") << QStringLiteral("korrode") << QStringLiteral("name");
        QTest::newRow("krunch") << QStringLiteral("krunch") << QStringLiteral("label");
        QTest::newRow("klick") << QStringLiteral("klick") << QStringLiteral("label");
    }
    void manualCoversThePanel()
    {
        QFETCH(QString, instrument);
        QFETCH(QString, field);
        const QString dir = QStringLiteral(KRATE_SOURCE_DIR "/instruments/") + instrument;
        const QString manual = read(dir + QStringLiteral("/docs/MANUAL.md"));
        const QString panel = read(dir + QStringLiteral("/qml/Panel.qml"));
        QVERIFY(!manual.isEmpty() && !panel.isEmpty());
        QVERIFY(ManualReader::split(manual).size() >= 10);
        const QRegularExpression label(field + QStringLiteral(R"(: \"([^\"]{2,})\")"));
        QStringList missing;
        for (auto it = label.globalMatch(panel); it.hasNext();) {
            const QString l = it.next().captured(1).replace(QStringLiteral("\\n"), QStringLiteral(" ")).trimmed();
            if (!manual.contains(l, Qt::CaseInsensitive))
                missing << l;
        }
        missing.removeDuplicates();
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("not in the manual: ") + missing.join(QStringLiteral(", "))));
    }
};

QTEST_GUILESS_MAIN(TestManual)
#include "tst_manual.moc"
