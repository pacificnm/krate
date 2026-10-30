#include <QtTest>

#include <QFile>
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
        QTest::newRow("k909") << QStringLiteral("k909") << QStringLiteral("label");
        QTest::newRow("k303") << QStringLiteral("k303") << QStringLiteral("name");
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
