// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfdocumentdecoration.h"

#include "pdfdocumentbuilder.h"

#include <QFont>
#include <QPainter>

#include <set>
#include <vector>

#include "pdfdbgheap.h"

namespace pdf
{

namespace
{

std::vector<PDFInteger> selectPageIndices(
        const PDFDocument* document,
        const PDFDocumentDecorationSettings& settings,
        QString* errorMessage)
{
    std::vector<PDFInteger> result;
    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();
    if (pageCount <= 0)
    {
        return result;
    }

    std::set<PDFInteger> rangeSelection;
    const QString rangeText = settings.pageRange.simplified();
    if (!rangeText.isEmpty())
    {
        QString parseError;
        const PDFClosedIntervalSet intervals =
            PDFClosedIntervalSet::parse(1, pageCount, rangeText, &parseError);
        if (!parseError.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = parseError;
            }
            return {};
        }

        for (const PDFInteger pageNumber : intervals.unfold())
        {
            rangeSelection.insert(pageNumber);
        }
    }

    result.reserve(pageCount);
    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFInteger pageNumber = pageIndex + 1;
        if (!rangeSelection.empty() && !rangeSelection.count(pageNumber))
        {
            continue;
        }

        switch (settings.pageSubset)
        {
            case PDFPageGeometrySettings::PageSubset::AllPages:
                break;

            case PDFPageGeometrySettings::PageSubset::OddPages:
                if ((pageNumber % 2) == 0)
                {
                    continue;
                }
                break;

            case PDFPageGeometrySettings::PageSubset::EvenPages:
                if ((pageNumber % 2) != 0)
                {
                    continue;
                }
                break;

            case PDFPageGeometrySettings::PageSubset::PortraitPages:
            case PDFPageGeometrySettings::PageSubset::LandscapePages:
            {
                const PDFPage* page = catalog->getPage(pageIndex);
                if (!page)
                {
                    continue;
                }
                const QSizeF size = page->getRotatedCropBox().size();
                const bool isLandscape = size.width() > size.height();
                if (settings.pageSubset ==
                        PDFPageGeometrySettings::PageSubset::PortraitPages &&
                    isLandscape)
                {
                    continue;
                }
                if (settings.pageSubset ==
                        PDFPageGeometrySettings::PageSubset::LandscapePages &&
                    !isLandscape)
                {
                    continue;
                }
                break;
            }
        }

        result.push_back(pageIndex);
    }
    return result;
}

} // namespace

PDFOperationResult PDFDocumentDecoration::apply(
        PDFDocument* document,
        const PDFDocumentDecorationSettings& settings,
        PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    if (modificationFlags)
    {
        *modificationFlags = PDFModifiedDocument::ModificationFlags();
    }
    if (!document)
    {
        return PDFTranslationContext::tr("Invalid document.");
    }
    if (settings.kind == PDFDocumentDecorationSettings::Kind::TextWatermark &&
        settings.text.trimmed().isEmpty())
    {
        return PDFTranslationContext::tr("Watermark text is empty.");
    }
    if (settings.kind == PDFDocumentDecorationSettings::Kind::TextWatermark &&
        settings.fontPointSize <= 0.0)
    {
        return PDFTranslationContext::tr("Watermark font size must be positive.");
    }
    if (settings.kind == PDFDocumentDecorationSettings::Kind::ImageBackground &&
        settings.image.isNull())
    {
        return PDFTranslationContext::tr("Background image is empty.");
    }

    QString selectionError;
    const std::vector<PDFInteger> pageIndices =
        selectPageIndices(document, settings, &selectionError);
    if (!selectionError.isEmpty())
    {
        return selectionError;
    }
    if (pageIndices.empty())
    {
        return true;
    }

    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    const auto mode = settings.foreground
        ? PDFPageContentStreamBuilder::Mode::PlaceAfter
        : PDFPageContentStreamBuilder::Mode::PlaceBefore;

    for (const PDFInteger pageIndex : pageIndices)
    {
        const PDFPage* page = document->getCatalog()->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        PDFPageContentStreamBuilder contentBuilder(
            builder,
            PDFContentStreamBuilder::CoordinateSystem::PDF,
            mode);
        QPainter* painter = contentBuilder.begin(page->getPageReference());
        if (!painter)
        {
            return PDFTranslationContext::tr(
                "Cannot create a content stream for page %1.")
                    .arg(pageIndex + 1);
        }

        const QRectF pageRect(
            QPointF(0.0, 0.0),
            page->getMediaBox().normalized().size());
        painter->save();
        painter->setOpacity(qBound<PDFReal>(0.0, settings.opacity, 1.0));

        if (settings.kind ==
            PDFDocumentDecorationSettings::Kind::ColorBackground)
        {
            painter->fillRect(pageRect, settings.color);
        }
        else if (settings.kind ==
                 PDFDocumentDecorationSettings::Kind::ImageBackground)
        {
            QRectF targetRect = pageRect;
            if (settings.imageScaleMode !=
                PDFDocumentDecorationSettings::ImageScaleMode::Stretch)
            {
                const Qt::AspectRatioMode aspectMode =
                    settings.imageScaleMode ==
                            PDFDocumentDecorationSettings::ImageScaleMode::Fit
                        ? Qt::KeepAspectRatio
                        : Qt::KeepAspectRatioByExpanding;
                const QSizeF scaledSize =
                    QSizeF(settings.image.size()).scaled(
                        pageRect.size(), aspectMode);
                targetRect = QRectF(QPointF(), scaledSize);
                targetRect.moveCenter(pageRect.center());
            }

            painter->setClipRect(pageRect);
            painter->drawImage(targetRect, settings.image);
        }
        else
        {
            QFont font(settings.fontFamily);
            font.setPointSizeF(settings.fontPointSize);
            painter->setFont(font);
            painter->setPen(settings.color);
            painter->translate(pageRect.center());
            painter->rotate(settings.angleDegrees);
            painter->drawText(
                QRectF(-pageRect.width() * 0.45,
                       -pageRect.height() * 0.20,
                       pageRect.width() * 0.90,
                       pageRect.height() * 0.40),
                Qt::AlignCenter | Qt::TextWordWrap,
                settings.text);
        }

        painter->restore();
        contentBuilder.end(painter);
    }

    modifier.markPageContentsChanged();
    if (modifier.finalize())
    {
        *document = *modifier.getDocument();
    }

    const PDFModifiedDocument::ModificationFlags flags(
        PDFModifiedDocument::PageContents |
        PDFModifiedDocument::PreserveUndoRedo);
    if (modificationFlags)
    {
        *modificationFlags = flags;
    }
    return true;
}

} // namespace pdf
