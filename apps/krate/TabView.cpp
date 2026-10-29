#include "TabView.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

#include "SongController.h"

namespace {
const QColor kPaper(0xF7, 0xF6, 0xF2);
const QColor kInk(0x1C, 0x1C, 0x1C);
const QColor kFaint(0x8A, 0x8C, 0x90);
const QColor kAccent(0xC0, 0x50, 0x10);

QColor toneColor(krate::song::TabItem::Tone tone)
{
    switch (tone) {
    case krate::song::TabItem::Tone::Faint: return kFaint;
    case krate::song::TabItem::Tone::Accent: return kAccent;
    case krate::song::TabItem::Tone::Ink: break;
    }
    return kInk;
}
} // namespace

TabView::TabView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setFillColor(kPaper);
}

void TabView::setSong(SongController *song)
{
    if (song == m_song)
        return;
    if (m_song)
        disconnect(m_song, nullptr, this, nullptr);
    m_song = song;
    if (m_song) {
        connect(m_song, &SongController::songChanged, this, [this] {
            m_track = -2;
            relayout();
        });
        connect(m_song, &SongController::tracksChanged, this, &TabView::relayout); // "my track" may have moved
        connect(m_song, &SongController::tickChanged, this, &TabView::updateCursor);
    }
    emit songChanged();
    m_track = -2;
    relayout();
}

void TabView::setScroll(qreal scroll)
{
    if (qFuzzyCompare(scroll, m_scroll))
        return;
    m_scroll = scroll;
    emit scrollChanged();
    update();
}

qreal TabView::cursorY() const
{
    return m_cursor.system >= 0 ? m_layout.systems[std::size_t(m_cursor.system)].top : 0.0;
}

qreal TabView::cursorHeight() const
{
    if (m_cursor.system < 0)
        return 0.0;
    const auto &s = m_layout.systems[std::size_t(m_cursor.system)];
    return s.staffBottom + 26.0 - s.top;
}

qreal TabView::systemTop(int system) const
{
    return system >= 0 && system < int(m_layout.systems.size()) ? m_layout.systems[std::size_t(system)].top : 0.0;
}

void TabView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), m_width))
        relayout();
}

void TabView::relayout()
{
    const krate::song::Score *score = m_song ? m_song->score() : nullptr;
    const int track = m_song ? m_song->myTrack() : -1;
    if (m_track == track && qFuzzyCompare(width(), m_width) && !(m_layout.systems.empty() && score))
        return; // a level or mute changed: the tab is the same
    m_track = track;
    m_width = width();
    m_layout = {};
    if (!m_song || !m_song->loaded())
        m_message.clear();
    else if (!score)
        m_message = tr("No tab: open the song's .gp file to see it");
    else if (track < 0)
        m_message = tr("Pick your track (MINE) to see its tab");
    else if (score->tracks[std::size_t(track)].drums)
        m_message = tr("Drum tracks have no tab view");
    else {
        m_layout = krate::song::layoutTab(*score, track, width());
        m_message = m_layout.systems.empty() ? tr("This track has no tab") : QString();
    }
    emit layoutChanged();
    updateCursor();
    update();
}

void TabView::updateCursor()
{
    const krate::song::Score *score = m_song ? m_song->score() : nullptr;
    const krate::song::TabCursor cursor = score ? krate::song::cursorAt(*score, m_layout, m_song->tick()) : krate::song::TabCursor{};
    if (cursor.system == m_cursor.system && qFuzzyCompare(cursor.x + 1.0, m_cursor.x + 1.0))
        return;
    m_cursor = cursor;
    emit cursorChanged();
}

void TabView::clickAt(qreal x, qreal y)
{
    if (const int bar = krate::song::barAt(m_layout, x, y); bar >= 0 && m_song)
        m_song->jumpToBar(bar);
}

void TabView::paint(QPainter *painter)
{
    using krate::song::TabItem;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    const qreal top = m_scroll, bottom = m_scroll + height();
    painter->translate(0.0, -m_scroll);
    QFont font(QStringLiteral("sans-serif"));
    for (const auto &system : m_layout.systems) {
        if (system.top + system.height < top || system.top > bottom)
            continue;
        for (const TabItem &item : system.items) {
            const QColor color = toneColor(item.tone);
            switch (item.kind) {
            case TabItem::Kind::Line:
                painter->setPen(QPen(color, item.width, Qt::SolidLine, Qt::FlatCap));
                painter->drawLine(QPointF(item.x1, item.y1), QPointF(item.x2, item.y2));
                break;
            case TabItem::Kind::Dot:
                painter->setPen(Qt::NoPen);
                painter->setBrush(color);
                painter->drawEllipse(QPointF(item.x1, item.y1), item.x2, item.x2);
                break;
            case TabItem::Kind::Text: {
                font.setPixelSize(int(item.size));
                font.setBold(item.bold);
                painter->setFont(font);
                const QString text = QString::fromStdString(item.text);
                const QFontMetricsF metrics(font);
                const qreal w = metrics.horizontalAdvance(text);
                const QRectF box(item.x1 - w / 2.0 - 1.5, item.y1 - metrics.height() / 2.0, w + 3.0, metrics.height());
                if (item.knockout)
                    painter->fillRect(box.adjusted(0, 2, 0, -2), kPaper);
                painter->setPen(color);
                painter->drawText(box, Qt::AlignCenter, text);
                break;
            }
            }
        }
    }
}
