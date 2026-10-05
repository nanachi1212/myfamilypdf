// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFTHUMBNAILSLISTVIEW_H
#define PDFTHUMBNAILSLISTVIEW_H

#include "pdf4qtlibgui_export.h"
#include "pdfglobal.h"
#include "pdfpagereorder.h"

#include <QListView>

#include <vector>

class QMimeData;

namespace pdfviewer
{

/// Thumbnail list of the sidebar. Besides the standard selection behaviour it can
/// start a drag of the selected thumbnails and tell where they were dropped.
///
/// The standard model drag and drop of a QListView in IconMode with static movement
/// did not hand a usable insertion row to QAbstractItemModel::dropMimeData in a
/// probe (see docs/page-reorder-v8.md), so the drop position is resolved here from
/// the item rectangles. The view never changes the model or the document; it only
/// emits pagesDropped().
class PDF4QTLIBGUILIBSHARED_EXPORT PDFThumbnailsListView : public QListView
{
    Q_OBJECT

public:
    explicit PDFThumbnailsListView(QWidget* parent = nullptr);

    /// Enables dragging of selected thumbnails (Editor). Viewer keeps this off.
    void setReorderEnabled(bool enabled);
    bool isReorderEnabled() const { return m_reorderEnabled; }

    /// Resolves a position (viewport coordinates) to an insertion point.
    PDFPageReorder::InsertionPoint insertionPointAt(const QPoint& viewportPosition) const;

    static QString reorderMimeType();
    static QMimeData* createReorderMimeData(const std::vector<pdf::PDFInteger>& pages);
    static std::vector<pdf::PDFInteger> pagesFromMimeData(const QMimeData* mimeData);

signals:
    /// Emitted (queued) after a drop. \p insertionRow is an index in the sequence
    /// before the move, in the range [0, rowCount].
    void pagesDropped(std::vector<pdf::PDFInteger> movedPages, int insertionRow);

protected:
    virtual void startDrag(Qt::DropActions supportedActions) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

private:
    bool canAccept(const QMimeData* mimeData) const;
    void setIndicator(bool visible, const PDFPageReorder::InsertionPoint& point = PDFPageReorder::InsertionPoint());
    QPixmap createDragPixmap(const QModelIndexList& indexes) const;

    bool m_reorderEnabled;
    bool m_indicatorVisible;
    PDFPageReorder::InsertionPoint m_indicator;
};

}   // namespace pdfviewer

#endif // PDFTHUMBNAILSLISTVIEW_H
