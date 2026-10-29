#pragma once

#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include "song/TabLayout.h"

class SongController;

// "My track" of the open song as scrolling tab (docs/SONG_PLAYER.md#ui).
// Sized to the viewport: it paints the systems between `scroll` and
// scroll + height, so long songs cost nothing extra. The cursor's place is
// published (cursorX, cursorY, cursorHeight, in content coordinates) for a
// QML overlay, so playback never repaints the tab.
class TabView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SongController *song READ song WRITE setSong NOTIFY songChanged)
    Q_PROPERTY(qreal scroll READ scroll WRITE setScroll NOTIFY scrollChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY layoutChanged)
    Q_PROPERTY(QString message READ message NOTIFY layoutChanged) // why there's no tab ("" when there is)
    Q_PROPERTY(bool cursorVisible READ cursorVisible NOTIFY cursorChanged)
    Q_PROPERTY(qreal cursorX READ cursorX NOTIFY cursorChanged)
    Q_PROPERTY(qreal cursorY READ cursorY NOTIFY cursorChanged)           // the top of the cursor's system
    Q_PROPERTY(qreal cursorHeight READ cursorHeight NOTIFY cursorChanged) // its height
    Q_PROPERTY(int cursorSystem READ cursorSystem NOTIFY cursorChanged)

public:
    explicit TabView(QQuickItem *parent = nullptr);

    SongController *song() const { return m_song; }
    void setSong(SongController *song);
    qreal scroll() const { return m_scroll; }
    void setScroll(qreal scroll);
    qreal contentHeight() const { return m_layout.height; }
    QString message() const { return m_message; }
    bool cursorVisible() const { return m_cursor.system >= 0; }
    qreal cursorX() const { return m_cursor.x; }
    qreal cursorY() const;
    qreal cursorHeight() const;
    int cursorSystem() const { return m_cursor.system; }

    // A click in content coordinates: jump to that bar.
    Q_INVOKABLE void clickAt(qreal x, qreal y);
    // The top of a system (for paging).
    Q_INVOKABLE qreal systemTop(int system) const;

    void paint(QPainter *painter) override;

signals:
    void songChanged();
    void scrollChanged();
    void layoutChanged();
    void cursorChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void relayout();
    void updateCursor();

    QPointer<SongController> m_song;
    qreal m_scroll = 0.0;
    krate::song::TabLayoutResult m_layout;
    krate::song::TabCursor m_cursor;
    QString m_message;
    int m_track = -2;
    qreal m_width = 0.0;
};
