#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// QML singleton "Crashes": Krate's crash reports (docs/CRASHES.md), to read
// on the screen: the list, a report's text, the one this start made (a banner
// until dismissed), and whether the viewer is open.
class CrashReportsController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Crashes)
    QML_SINGLETON

    Q_PROPERTY(QVariantList reports READ reports NOTIFY changed) // [{path, when, summary}], newest first
    Q_PROPERTY(QString newReport READ newReport NOTIFY changed)  // made at this start; "" once dismissed
    Q_PROPERTY(QString newSummary READ newSummary NOTIFY changed)
    Q_PROPERTY(bool viewing READ viewing WRITE setViewing NOTIFY viewingChanged)
    Q_PROPERTY(QString folder READ folder CONSTANT)

public:
    // `dir`: where crash::install() keeps them (the app's data folder).
    explicit CrashReportsController(QString dir, QObject *parent = nullptr);
    ~CrashReportsController() override;
    static CrashReportsController *create(QQmlEngine *, QJSEngine *);

    QVariantList reports() const;
    QString newReport() const { return m_new; }
    QString newSummary() const;
    bool viewing() const { return m_viewing; }
    void setViewing(bool on);
    QString folder() const;

    Q_INVOKABLE QString text(const QString &path) const;
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void remove(const QString &path);
    Q_INVOKABLE void removeAll();

signals:
    void changed();
    void viewingChanged();

private:
    static inline CrashReportsController *s_instance = nullptr;
    QString m_dir;
    QString m_new;
    bool m_viewing = false;
};
