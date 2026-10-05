// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfthumbnailslistview.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMetaObject>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>

#include <algorithm>

namespace pdfviewer
{

PDFThumbnailsListView::PDFThumbnailsListView(QWidget* parent) :
    QListView(parent),
    m_reorderEnabled(false),
    m_indicatorVisible(false)
{
}

void PDFThumbnailsListView::setReorderEnabled(bool enabled)
{
    m_reorderEnabled = enabled;
    setDragEnabled(enabled);

    // Drops are accepted only while a drag started by this view is running (see
    // startDrag). A view that always accepted drops would swallow file drops that
    // belong to the main window.
    setAcceptDrops(false);
    setDropIndicatorShown(false);
}

PDFPageReorder::InsertionPoint PDFThumbnailsListView::insertionPointAt(const QPoint& viewportPosition) const
{
    std::vector<QRect> itemRects;
    if (const QAbstractItemModel* itemModel = model())
    {
        const int rowCount = itemModel->rowCount(rootIndex());
        itemRects.reserve(size_t(rowCount));
        for (int row = 0; row < rowCount; ++row)
        {
            itemRects.push_back(visualRect(itemModel->index(row, 0, rootIndex())));
        }
    }

    return PDFPageReorder::computeInsertionPoint(itemRects, viewportPosition);
}

QString PDFThumbnailsListView::reorderMimeType()
{
    return QStringLiteral("application/x-familypdf-page-reorder");
}

QMimeData* PDFThumbnailsListView::createReorderMimeData(const std::vector<pdf::PDFInteger>& pages)
{
    QStringList texts;
    texts.reserve(int(pages.size()));
    for (const pdf::PDFInteger page : pages)
    {
        texts << QString::number(page);
    }

    QMimeData* mimeData = new QMimeData();
    mimeData->setData(reorderMimeType(), texts.join(QLatin1Char(',')).toLatin1());
    return mimeData;
}

std::vector<pdf::PDFInteger> PDFThumbnailsListView::pagesFromMimeData(const QMimeData* mimeData)
{
    std::vector<pdf::PDFInteger> pages;
    if (!mimeData || !mimeData->hasFormat(reorderMimeType()))
    {
        return pages;
    }

    const QList<QByteArray> parts = mimeData->data(reorderMimeType()).split(',');
    for (const QByteArray& part : parts)
    {
        bool ok = false;
        const qlonglong page = part.trimmed().toLongLong(&ok);
        if (ok && page >= 0)
        {
            pages.push_back(page);
        }
    }
    return pages;
}

bool PDFThumbnailsListView::canAccept(const QMimeData* mimeData) const
{
    return m_reorderEnabled && model() && mimeData && mimeData->hasFormat(reorderMimeType());
}

void PDFThumbnailsListView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);

    if (!m_reorderEnabled || !model() || !selectionModel())
    {
        return;
    }

    const QModelIndexList indexes = selectedIndexes();
    std::vector<pdf::PDFInteger> pages;
    pages.reserve(size_t(indexes.size()));
    for (const QModelIndex& index : indexes)
    {
        if (index.isValid())
        {
            pages.push_back(index.row());
        }
    }
    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    if (pages.empty())
    {
        return;
    }

    QDrag* drag = new QDrag(this);
    drag->setMimeData(createReorderMimeData(pages));
    const QPixmap pixmap = createDragPixmap(indexes);
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(int(pixmap.width() / pixmap.devicePixelRatio()) / 2, int(pixmap.height() / pixmap.devicePixelRatio()) / 2));

    setAcceptDrops(true);
    drag->exec(Qt::MoveAction, Qt::MoveAction);
    setAcceptDrops(false);

    setIndicator(false);
    stopAutoScroll();
}

void PDFThumbnailsListView::dragEnterEvent(QDragEnterEvent* event)
{
    if (canAccept(event->mimeData()))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void PDFThumbnailsListView::dragMoveEvent(QDragMoveEvent* event)
{
    if (!canAccept(event->mimeData()))
    {
        event->ignore();
        return;
    }

    const QPoint position = event->position().toPoint();
    setIndicator(true, insertionPointAt(position));
    event->setDropAction(Qt::MoveAction);
    event->accept();

    const int margin = autoScrollMargin();
    if (position.y() < margin || viewport()->height() - position.y() < margin)
    {
        startAutoScroll();
    }
    else
    {
        stopAutoScroll();
    }
}

void PDFThumbnailsListView::dragLeaveEvent(QDragLeaveEvent* event)
{
    setIndicator(false);
    stopAutoScroll();
    event->accept();
}

void PDFThumbnailsListView::dropEvent(QDropEvent* event)
{
    setIndicator(false);
    stopAutoScroll();

    if (!canAccept(event->mimeData()))
    {
        event->ignore();
        return;
    }

    const std::vector<pdf::PDFInteger> pages = pagesFromMimeData(event->mimeData());
    const int insertionRow = insertionPointAt(event->position().toPoint()).row;
    event->setDropAction(Qt::MoveAction);
    event->accept();

    if (pages.empty())
    {
        return;
    }

    // The document is replaced while the signal is handled. That must not happen
    // inside the drag-and-drop loop that is still delivering this event.
    QMetaObject::invokeMethod(this, [this, pages, insertionRow]()
    {
        Q_EMIT pagesDropped(pages, insertionRow);
    }, Qt::QueuedConnection);
}

void PDFThumbnailsListView::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);

    if (!m_indicatorVisible || m_indicator.anchor < 0 || !model())
    {
        return;
    }

    const QRect itemRect = visualRect(model()->index(m_indicator.anchor, 0, rootIndex()));
    if (!itemRect.isValid())
    {
        return;
    }

    constexpr int thickness = 3;
    QRect line;
    if (m_indicator.horizontalFlow)
    {
        const int x = m_indicator.after ? itemRect.right() + 1 : itemRect.left();
        line = QRect(std::max(0, x - thickness / 2), itemRect.top(), thickness, itemRect.height());
    }
    else
    {
        const int y = m_indicator.after ? itemRect.bottom() + 1 : itemRect.top();
        line = QRect(itemRect.left(), std::max(0, y - thickness / 2), itemRect.width(), thickness);
    }

    QPainter painter(viewport());
    painter.fillRect(line, palette().color(QPalette::Highlight));
}

void PDFThumbnailsListView::setIndicator(bool visible, const PDFPageReorder::InsertionPoint& point)
{
    const bool changed = m_indicatorVisible != visible
                      || (visible && (m_indicator.anchor != point.anchor || m_indicator.after != point.after
                                      || m_indicator.horizontalFlow != point.horizontalFlow));
    m_indicatorVisible = visible;
    if (visible)
    {
        m_indicator = point;
    }
    if (changed)
    {
        viewport()->update();
    }
}

QPixmap PDFThumbnailsListView::createDragPixmap(const QModelIndexList& indexes) const
{
    QPixmap thumbnail;
    if (!indexes.isEmpty())
    {
        const QModelIndex lead = currentIndex().isValid() && indexes.contains(currentIndex()) ? currentIndex() : indexes.front();
        thumbnail = qvariant_cast<QPixmap>(lead.data(Qt::DecorationRole));
    }

    const qreal ratio = devicePixelRatioF();
    if (thumbnail.isNull())
    {
        thumbnail = QPixmap(int(48 * ratio), int(64 * ratio));
        thumbnail.setDevicePixelRatio(ratio);
        thumbnail.fill(palette().color(QPalette::Mid));
    }

    QPixmap result(thumbnail.size());
    result.setDevicePixelRatio(thumbnail.devicePixelRatio());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setOpacity(0.75);
    painter.drawPixmap(0, 0, thumbnail);
    painter.setOpacity(1.0);

    if (indexes.size() > 1)
    {
        const qreal pixelRatio = result.devicePixelRatio();
        const QRect badge(int(result.width() / pixelRatio) - 22, 2, 20, 20);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawEllipse(badge);
        painter.setPen(palette().color(QPalette::HighlightedText));
        painter.drawText(badge, Qt::AlignCenter, QString::number(indexes.size()));
    }

    return result;
}

}   // namespace pdfviewer
