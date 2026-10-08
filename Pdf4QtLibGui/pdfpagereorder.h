// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFPAGEREORDER_H
#define PDFPAGEREORDER_H

#include "pdf4qtlibgui_export.h"
#include "pdfglobal.h"

#include <QPoint>
#include <QRect>

#include <vector>

namespace pdfviewer
{

/// Pure page-order arithmetic for thumbnail drag and drop. A "page order" is a
/// vector where element i is the old (zero based) index of the page that ends
/// up at position i. Nothing here touches a document.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFPageReorder
{
public:
    /// Where a drop lands, expressed against the thumbnail layout.
    struct InsertionPoint
    {
        int row = 0;                    ///< Insertion index in the original sequence [0, itemCount]
        int anchor = -1;                ///< Item next to the insertion line, -1 when there are no items
        bool after = false;             ///< The line is drawn after (right/below) the anchor
        bool horizontalFlow = true;     ///< Items sit side by side (line is vertical); false = one column
    };

    /// Returns sorted, unique page indices that are valid for \p pageCount.
    static std::vector<pdf::PDFInteger> normalizePages(const std::vector<pdf::PDFInteger>& pages, pdf::PDFInteger pageCount);

    /// Reverses the whole document for zero or one selected page, otherwise only
    /// the contiguous selected range. Returns empty for fewer than two document
    /// pages, invalid indices or a non-contiguous selection (nothing to apply).
    static std::vector<pdf::PDFInteger> computeReversedPageOrder(pdf::PDFInteger pageCount,
                                                              const std::vector<pdf::PDFInteger>& selectedPages);

    /// Moves \p movedPages (keeping their relative order) so that they are inserted
    /// before original index \p insertionRow (0..pageCount). The insertion index is
    /// translated after the moved pages are taken out, so it works for both
    /// directions. Returns the identity order when nothing valid was moved.
    static std::vector<pdf::PDFInteger> computeNewPageOrder(pdf::PDFInteger pageCount,
                                                            const std::vector<pdf::PDFInteger>& movedPages,
                                                            pdf::PDFInteger insertionRow);

    /// Zero based indices of the odd numbered pages (1, 3, 5, ...: indices 0, 2, 4) or of the even numbered pages.
    static std::vector<pdf::PDFInteger> selectByParity(pdf::PDFInteger pageCount, bool oddNumbered);

    /// Every page of 0..pageCount-1 that is not in \p selectedPages.
    static std::vector<pdf::PDFInteger> invertSelection(pdf::PDFInteger pageCount, const std::vector<pdf::PDFInteger>& selectedPages);

    /// True if \p order is a complete permutation of 0..pageCount-1.
    static bool isPermutation(const std::vector<pdf::PDFInteger>& order, pdf::PDFInteger pageCount);

    /// True if \p order is 0, 1, 2, ...
    static bool isIdentity(const std::vector<pdf::PDFInteger>& order);

    /// Returns the new index of each old page index in \p oldPages (same order).
    static std::vector<pdf::PDFInteger> mapOldToNew(const std::vector<pdf::PDFInteger>& newPageOrder,
                                                    const std::vector<pdf::PDFInteger>& oldPages);

    /// Finds the insertion point for a drop at \p position. \p itemRects are the
    /// item rectangles in model order, in the same coordinates as \p position.
    /// Over an item: first half before, second half after. Beside items of a
    /// row: before the first item whose centre is right of the cursor. Between
    /// rows: before the first item of the row below. Above everything: 0.
    /// Below everything: end of the document.
    static InsertionPoint computeInsertionPoint(const std::vector<QRect>& itemRects, const QPoint& position);
};

}   // namespace pdfviewer

#endif // PDFPAGEREORDER_H
