#include <QtTest>

#include <QTemporaryDir>

#include "KeyboardSettings.h"

using krate::KeyboardSettings;

// Your MIDI keyboard's settings: defaults, limits, SET FROM KEY, saved and read back.
class TestKeyboardSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreThe49KeyController()
    {
        QTemporaryDir dir;
        KeyboardSettings s(dir.filePath("keyboard.json"));
        QCOMPARE(s.keys(), 49);
        QCOMPARE(s.firstNote(), 35);
        QCOMPARE(s.octaveDown(), 3);
        QCOMPARE(s.octaveUp(), 4);
    }

    void savedAndReadBack()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("krate/keyboard.json");
        {
            KeyboardSettings s(path);
            s.setKeys(61);
            s.setFirstNote(36);
            s.setOctaveDown(2);
            s.setOctaveUp(3);
            s.setKeys(500);     // limited to 88
            s.setFirstNote(-4); // and 0
        }
        KeyboardSettings again(path);
        QCOMPARE(again.keys(), 88);
        QCOMPARE(again.firstNote(), 0);
        QCOMPARE(again.octaveDown(), 2);
        QCOMPARE(again.octaveUp(), 3);
        again.restoreDefaults();
        QCOMPARE(KeyboardSettings(path).firstNote(), 35);
    }

    void setFromKey()
    {
        QTemporaryDir dir;
        KeyboardSettings s(dir.filePath("keyboard.json"));
        s.captured(40); // not capturing: ignored
        QCOMPARE(s.firstNote(), 35);
        s.setCapturing(true);
        s.captured(47);
        QCOMPARE(s.firstNote(), 47);
        QVERIFY(!s.capturing());
    }
};

QTEST_GUILESS_MAIN(TestKeyboardSettings)
#include "tst_keyboardsettings.moc"
