// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfpagereorder.h"

#include <algorithm>
#include <numeric>

namespace pdfviewer
{

std::vector<pdf::PDFInteger> PDFPageReorder::normalizePages(const std::vector<pdf::PDFInteger>& pages, pdf::PDFInteger pageCount)
{
    std::vector<pdf::PDFInteger> result;
    result.reserve(pages.size());
    for (const pdf::PDFInteger page : pages)
    {
        if (page >= 0 && page < pageCount)
        {
            result.push_back(page);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<pdf::PDFInteger> PDFPageReorder::computeReversedPageOrder(pdf::PDFInteger pageCount,
                                                                  const std::vector<pdf::PDFInteger>& selectedPages)
{
    if (pageCount <= 1 || std::any_of(selectedPages.cbegin(), selectedPages.cend(), [pageCount](pdf::PDFInteger page)
        { return page < 0 || page >= pageCount; }))
    {
        return {};
    }

    const std::vector<pdf::PDFInteger> pages = normalizePages(selectedPages, pageCount);
    if (pages.size() >= 2 && pages.back() - pages.front() + 1 != pdf::PDFInteger(pages.size()))
    {
        return {};
    }

    std::vector<pdf::PDFInteger> order(size_t(pageCount), 0);
    std::iota(order.begin(), order.end(), pdf::PDFInteger(0));
    const pdf::PDFInteger first = pages.size() >= 2 ? pages.front() : 0;
    const pdf::PDFInteger end = pages.size() >= 2 ? pages.back() + 1 : pageCount;
    std::reverse(order.begin() + first, order.begin() + end);
    return order;
}

std::vector<pdf::PDFInteger> PDFPageReorder::computeNewPageOrder(pdf::PDFInteger pageCount,
                                                                 const std::vector<pdf::PDFInteger>& movedPages,
                                                                 pdf::PDFInteger insertionRow)
{
    std::vector<pdf::PDFInteger> order;
    if (pageCount <= 0)
    {
        return order;
    }

    const std::vector<pdf::PDFInteger> moved = normalizePages(movedPages, pageCount);
    insertionRow = std::clamp(insertionRow, pdf::PDFInteger(0), pageCount);

    // Pages that stay where they are, in their original order.
    std::vector<pdf::PDFInteger> remaining;
    remaining.reserve(size_t(pageCount) - moved.size());
    for (pdf::PDFInteger page = 0; page < pageCount; ++page)
    {
        if (!std::binary_search(moved.cbegin(), moved.cend(), page))
        {
            remaining.push_back(page);
        }
    }

    // The insertion row refers to the original sequence. Pages taken out in front
    // of it shift it towards the start of the remaining sequence.
    const pdf::PDFInteger movedBeforeRow = pdf::PDFInteger(std::count_if(moved.cbegin(), moved.cend(), [insertionRow](pdf::PDFInteger page)
    {
        return page < insertionRow;
    }));
    const pdf::PDFInteger insertAt = insertionRow - movedBeforeRow;

    order.reserve(size_t(pageCount));
    order.insert(order.end(), remaining.cbegin(), remaining.cbegin() + insertAt);
    order.insert(order.end(), moved.cbegin(), moved.cend());
    order.insert(order.end(), remaining.cbegin() + insertAt, remaining.cend());
    return order;
}

bool PDFPageReorder::isPermutation(const std::vector<pdf::PDFInteger>& order, pdf::PDFInteger pageCount)
{
    if (pageCount < 0 || pdf::PDFInteger(order.size()) != pageCount)
    {
        return false;
    }

    std::vector<bool> seen(order.size(), false);
    for (const pdf::PDFInteger page : order)
    {
        if (page < 0 || page >= pageCount || seen[size_t(page)])
        {
            return false;
        }
        seen[size_t(page)] = true;
    }
    return true;
}

bool PDFPageReorder::isIdentity(const std::vector<pdf::PDFInteger>& order)
{
    for (size_t i = 0; i < order.size(); ++i)
    {
        if (order[i] != pdf::PDFInteger(i))
        {
            return false;
        }
    }
    return true;
}

std::vector<pdf::PDFInteger> PDFPageReorder::mapOldToNew(const std::vector<pdf::PDFInteger>& newPageOrder,
                                                         const std::vector<pdf::PDFInteger>& oldPages)
{
    std::vector<pdf::PDFInteger> newPositions(newPageOrder.size(), -1);
    for (size_t position = 0; position < newPageOrder.size(); ++position)
    {
        const pdf::PDFInteger oldPage = newPageOrder[position];
        if (oldPage >= 0 && oldPage < pdf::PDFInteger(newPositions.size()))
        {
            newPositions[size_t(oldPage)] = pdf::PDFInteger(position);
        }
    }

    std::vector<pdf::PDFInteger> result;
    result.reserve(oldPages.size());
    for (const pdf::PDFInteger oldPage : oldPages)
    {
        if (oldPage >= 0 && oldPage < pdf::PDFInteger(newPositions.size()) && newPositions[size_t(oldPage)] >= 0)
        {
            result.push_back(newPositions[size_t(oldPage)]);
        }
    }
    return result;
}

PDFPageReorder::InsertionPoint PDFPageReorder::computeInsertionPoint(const std::vector<QRect>& itemRects, const QPoint& position)
{
    InsertionPoint result;
    const int itemCount = int(itemRects.size());
    if (itemCount == 0)
    {
        return result;
    }

    // Group the items into visual rows. Items of one row overlap vertically.
    struct Row
    {
        int first = 0;
        int last = 0;       // exclusive
        int top = 0;
        int bottom = 0;
    };
    std::vector<Row> rows;
    for (int i = 0; i < itemCount; ++i)
    {
        const QRect& rect = itemRects[size_t(i)];
        if (!rows.empty() && rect.top() <= rows.back().bottom && rect.bottom() >= rows.back().top)
        {
            Row& row = rows.back();
            row.last = i + 1;
            row.top = std::min(row.top, rect.top());
            row.bottom = std::max(row.bottom, rect.bottom());
        }
        else
        {
            rows.push_back({ i, i + 1, rect.top(), rect.bottom() });
        }
    }

    // A single column has one item per row. There the decision is made on the
    // vertical axis, otherwise on the horizontal axis.
    const bool horizontalFlow = int(rows.size()) < itemCount;
    result.horizontalFlow = horizontalFlow;

    const auto isBeforeCentre = [&](const QRect& rect)
    {
        return horizontalFlow ? 2 * (position.x() - rect.left()) < rect.width()
                              : 2 * (position.y() - rect.top()) < rect.height();
    };
    const auto before = [&](int item)
    {
        result.row = item;
        result.anchor = item;
        result.after = false;
        return result;
    };
    const auto after = [&](int item)
    {
        result.row = item + 1;
        result.anchor = item;
        result.after = true;
        return result;
    };

    for (int i = 0; i < itemCount; ++i)
    {
        if (itemRects[size_t(i)].contains(position))
        {
            return isBeforeCentre(itemRects[size_t(i)]) ? before(i) : after(i);
        }
    }

    if (position.y() < rows.front().top)
    {
        return before(0);
    }
    if (position.y() > rows.back().bottom)
    {
        return after(itemCount - 1);
    }

    for (size_t r = 0; r < rows.size(); ++r)
    {
        const Row& row = rows[r];
        if (position.y() >= row.top && position.y() <= row.bottom)
        {
            // Beside the items of a row.
            for (int i = row.first; i < row.last; ++i)
            {
                if (isBeforeCentre(itemRects[size_t(i)]))
                {
                    return before(i);
                }
            }
            return after(row.last - 1);
        }

        if (r + 1 < rows.size() && position.y() > row.bottom && position.y() < rows[r + 1].top)
        {
            // In the gap between two rows: goes in front of the lower row.
            return before(rows[r + 1].first);
        }
    }

    return after(itemCount - 1);
}

}   // namespace pdfviewer
