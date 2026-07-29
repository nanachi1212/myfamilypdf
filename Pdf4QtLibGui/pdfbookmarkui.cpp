// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfbookmarkui.h"
#include "pdfwidgetutils.h"
#include "pdfpainterutils.h"

#include <QPainter>
#include <QPainterPath>

namespace pdfviewer
{

PDFBookmarkItemModel::PDFBookmarkItemModel(PDFBookmarkManager* bookmarkManager, QObject* parent) :
    BaseClass(parent),
    m_bookmarkManager(bookmarkManager)
{
    connect(m_bookmarkManager, &PDFBookmarkManager::bookmarksAboutToBeChanged, this, &PDFBookmarkItemModel::beginResetModel);
    connect(m_bookmarkManager, &PDFBookmarkManager::bookmarksChanged, this, &PDFBookmarkItemModel::endResetModel);
}

QModelIndex PDFBookmarkItemModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!m_bookmarkManager || row < 0 || column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        const int folderCount = m_bookmarkManager->getFolderCount();
        if (row < folderCount)
        {
            return createIndex(row, column, getNodeId(FOLDER_NODE, row));
        }

        const int bookmarkIndex = getRootBookmarkIndex(row - folderCount);
        return bookmarkIndex >= 0 ?
                   createIndex(row, column, getNodeId(BOOKMARK_NODE, bookmarkIndex)) : QModelIndex();
    }

    const int folderIndex = getFolderIndex(parent);
    const int bookmarkIndex = getFolderBookmarkIndex(folderIndex, row);
    return bookmarkIndex >= 0 ?
               createIndex(row, column, getNodeId(BOOKMARK_NODE, bookmarkIndex)) : QModelIndex();
}

QModelIndex PDFBookmarkItemModel::parent(const QModelIndex& child) const
{
    const int bookmarkIndex = getBookmarkIndex(child);
    if (bookmarkIndex < 0)
    {
        return QModelIndex();
    }

    const QString folderId = m_bookmarkManager->getBookmark(bookmarkIndex).folderId;
    for (int i = 0; i < m_bookmarkManager->getFolderCount(); ++i)
    {
        if (!folderId.isEmpty() && m_bookmarkManager->getFolder(i).id == folderId)
        {
            return createIndex(i, 0, getNodeId(FOLDER_NODE, i));
        }
    }
    return QModelIndex();
}

int PDFBookmarkItemModel::rowCount(const QModelIndex& parent) const
{
    if (!m_bookmarkManager)
    {
        return 0;
    }

    if (!parent.isValid())
    {
        int rootBookmarkCount = 0;
        for (int i = 0; i < m_bookmarkManager->getBookmarkCount(); ++i)
        {
            rootBookmarkCount += m_bookmarkManager->getBookmark(i).folderId.isEmpty() ? 1 : 0;
        }
        return m_bookmarkManager->getFolderCount() + rootBookmarkCount;
    }

    const int folderIndex = getFolderIndex(parent);
    if (folderIndex < 0)
    {
        return 0;
    }

    const QString folderId = m_bookmarkManager->getFolder(folderIndex).id;
    int childCount = 0;
    for (int i = 0; i < m_bookmarkManager->getBookmarkCount(); ++i)
    {
        childCount += m_bookmarkManager->getBookmark(i).folderId == folderId ? 1 : 0;
    }
    return childCount;
}

int PDFBookmarkItemModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant PDFBookmarkItemModel::data(const QModelIndex& index, int role) const
{
    const int folderIndex = getFolderIndex(index);
    if (folderIndex >= 0)
    {
        const PDFBookmarkManager::BookmarkFolder folder = m_bookmarkManager->getFolder(folderIndex);
        if (role == Qt::DisplayRole)
        {
            return folder.name;
        }
        if (role == Qt::ForegroundRole)
        {
            return folder.color;
        }
    }

    const int bookmarkIndex = getBookmarkIndex(index);
    if (bookmarkIndex >= 0)
    {
        const PDFBookmarkManager::Bookmark bookmark = m_bookmarkManager->getBookmark(bookmarkIndex);
        if (role == Qt::DisplayRole)
        {
            return bookmark.name;
        }
        if (role == Qt::ForegroundRole)
        {
            return bookmark.color;
        }
    }

    return QVariant();
}

bool PDFBookmarkItemModel::isFolder(const QModelIndex& index) const
{
    return getFolderIndex(index) >= 0;
}

int PDFBookmarkItemModel::getFolderIndex(const QModelIndex& index) const
{
    const int folderIndex = getNodeSourceIndex(index, FOLDER_NODE);
    return folderIndex >= 0 && folderIndex < m_bookmarkManager->getFolderCount() ? folderIndex : -1;
}

int PDFBookmarkItemModel::getBookmarkIndex(const QModelIndex& index) const
{
    const int bookmarkIndex = getNodeSourceIndex(index, BOOKMARK_NODE);
    return bookmarkIndex >= 0 && bookmarkIndex < m_bookmarkManager->getBookmarkCount() ? bookmarkIndex : -1;
}

QModelIndex PDFBookmarkItemModel::getBookmarkModelIndex(int bookmarkIndex) const
{
    if (!m_bookmarkManager || bookmarkIndex < 0 || bookmarkIndex >= m_bookmarkManager->getBookmarkCount())
    {
        return QModelIndex();
    }

    const QString folderId = m_bookmarkManager->getBookmark(bookmarkIndex).folderId;
    if (folderId.isEmpty())
    {
        const int row = getRootRowForBookmark(bookmarkIndex);
        return row >= 0 ? createIndex(row, 0, getNodeId(BOOKMARK_NODE, bookmarkIndex)) : QModelIndex();
    }

    for (int folderIndex = 0; folderIndex < m_bookmarkManager->getFolderCount(); ++folderIndex)
    {
        if (m_bookmarkManager->getFolder(folderIndex).id != folderId)
        {
            continue;
        }

        int childRow = 0;
        for (int i = 0; i < m_bookmarkManager->getBookmarkCount(); ++i)
        {
            if (m_bookmarkManager->getBookmark(i).folderId == folderId)
            {
                if (i == bookmarkIndex)
                {
                    return createIndex(childRow, 0, getNodeId(BOOKMARK_NODE, bookmarkIndex));
                }
                ++childRow;
            }
        }
    }
    return QModelIndex();
}

quintptr PDFBookmarkItemModel::getNodeId(quintptr type, int sourceIndex)
{
    return (static_cast<quintptr>(sourceIndex + 1) << 2) | type;
}

int PDFBookmarkItemModel::getNodeSourceIndex(const QModelIndex& index, quintptr type)
{
    if (!index.isValid() || (index.internalId() & 3) != type)
    {
        return -1;
    }
    return static_cast<int>(index.internalId() >> 2) - 1;
}

int PDFBookmarkItemModel::getRootBookmarkIndex(int rootBookmarkRow) const
{
    if (!m_bookmarkManager || rootBookmarkRow < 0)
    {
        return -1;
    }

    int currentRow = 0;
    for (int i = 0; i < m_bookmarkManager->getBookmarkCount(); ++i)
    {
        if (m_bookmarkManager->getBookmark(i).folderId.isEmpty())
        {
            if (currentRow == rootBookmarkRow)
            {
                return i;
            }
            ++currentRow;
        }
    }
    return -1;
}

int PDFBookmarkItemModel::getFolderBookmarkIndex(int folderIndex, int childRow) const
{
    if (!m_bookmarkManager || folderIndex < 0 ||
        folderIndex >= m_bookmarkManager->getFolderCount() || childRow < 0)
    {
        return -1;
    }

    const QString folderId = m_bookmarkManager->getFolder(folderIndex).id;
    int currentRow = 0;
    for (int i = 0; i < m_bookmarkManager->getBookmarkCount(); ++i)
    {
        if (m_bookmarkManager->getBookmark(i).folderId == folderId)
        {
            if (currentRow == childRow)
            {
                return i;
            }
            ++currentRow;
        }
    }
    return -1;
}

int PDFBookmarkItemModel::getRootRowForBookmark(int bookmarkIndex) const
{
    int rootRow = m_bookmarkManager ? m_bookmarkManager->getFolderCount() : 0;
    for (int i = 0; m_bookmarkManager && i < m_bookmarkManager->getBookmarkCount(); ++i)
    {
        if (m_bookmarkManager->getBookmark(i).folderId.isEmpty())
        {
            if (i == bookmarkIndex)
            {
                return rootRow;
            }
            ++rootRow;
        }
    }
    return -1;
}

PDFBookmarkItemDelegate::PDFBookmarkItemDelegate(PDFBookmarkManager* bookmarkManager, QObject* parent) :
    BaseClass(parent),
    m_bookmarkManager(bookmarkManager)
{

}

void PDFBookmarkItemDelegate::paint(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);

    const PDFBookmarkItemModel* model = dynamic_cast<const PDFBookmarkItemModel*>(index.model());
    const int bookmarkIndex = model ? model->getBookmarkIndex(index) : -1;
    if (bookmarkIndex < 0)
    {
        BaseClass::paint(painter, option, index);
        return;
    }
    PDFBookmarkManager::Bookmark bookmark = m_bookmarkManager->getBookmark(bookmarkIndex);

    options.text = QString();
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    const int margin = pdf::PDFWidgetUtils::scaleDPI_x(option.widget, MARGIN);
    const int iconSize = pdf::PDFWidgetUtils::scaleDPI_x(option.widget, ICON_SIZE);

    QRect rect = options.rect;
    rect.marginsRemoved(QMargins(margin, margin, margin, margin));

    QColor color = bookmark.color.isValid() ? bookmark.color :
                   (bookmark.isAuto ? QColor(0, 123, 255) : QColor(255, 159, 0));

    if (options.state.testFlag(QStyle::State_Selected))
    {
        color = Qt::yellow;
    }

    QRect iconRect = rect;
    iconRect.setWidth(iconSize);
    iconRect.setHeight(iconSize);
    iconRect.moveCenter(QPoint(rect.left() + iconSize / 2, rect.center().y()));
    drawStar(*painter, iconRect.center(), iconRect.width() * 0.5, color);

    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + margin);
    textRect.moveTop(rect.top() + (rect.height() - 2 * options.fontMetrics.lineSpacing()) / 2);

    textRect.setHeight(options.fontMetrics.lineSpacing());

    QFont font = options.font;
    font.setBold(true);

    painter->setFont(font);
    painter->setPen(options.state.testFlag(QStyle::State_Selected) ?
                        options.palette.highlightedText().color() : color);
    painter->drawText(textRect, getPageText(bookmark));

    textRect.translate(0, textRect.height());

    painter->setFont(options.font);
    painter->drawText(textRect, bookmark.name);
}

QSize PDFBookmarkItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const PDFBookmarkItemModel* model = dynamic_cast<const PDFBookmarkItemModel*>(index.model());
    const int bookmarkIndex = model ? model->getBookmarkIndex(index) : -1;
    if (bookmarkIndex < 0)
    {
        return BaseClass::sizeHint(option, index);
    }
    PDFBookmarkManager::Bookmark bookmark = m_bookmarkManager->getBookmark(bookmarkIndex);

    const int textWidthLine1 = option.fontMetrics.horizontalAdvance(getPageText(bookmark));
    const int textWidthLine2 = option.fontMetrics.horizontalAdvance(option.text);
    const int textWidth = qMax(textWidthLine1, textWidthLine2);
    const int textHeight = option.fontMetrics.lineSpacing() * 2;

    const int margin = pdf::PDFWidgetUtils::scaleDPI_x(option.widget, MARGIN);
    const int iconSize = pdf::PDFWidgetUtils::scaleDPI_x(option.widget, ICON_SIZE);

    const int requiredWidth = 3 * margin + iconSize + textWidth;
    const int requiredHeight = 2 * margin + qMax(iconSize, textHeight);

    return QSize(requiredWidth, requiredHeight);
}

void PDFBookmarkItemDelegate::drawStar(QPainter& painter, const QPointF& center, double size, const QColor& color) const
{
    pdf::PDFPainterStateGuard guard(&painter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    QPainterPath path;
    double angle = M_PI / 5;
    double phase = -M_PI / 10;

    for (int i = 0; i < 10; ++i)
    {
        double radius = (i % 2 == 0) ? size : size / 2.5;
        QPointF point(radius * cos(i * angle + phase), radius * sin(i * angle + phase));
        point += center;

        if (i == 0)
        {
            path.moveTo(point);
        }
        else
        {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    painter.drawPath(path);
}

QString PDFBookmarkItemDelegate::getPageText(const PDFBookmarkManager::Bookmark& bookmark) const
{
    if (bookmark.isAuto)
    {
        return tr("Page %1 | Generated").arg(bookmark.pageIndex + 1);
    }
    else
    {
        return tr("Page %1").arg(bookmark.pageIndex + 1);
    }
}

}   // namespace pdfviewer
