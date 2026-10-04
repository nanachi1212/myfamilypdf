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

#include "pdfpageoutput.h"

#include "pdfannotation.h"
#include "pdfcms.h"
#include "pdfdrawspacecontroller.h"
#include "pdfexecutionpolicy.h"
#include "pdffont.h"
#include "pdfpainter.h"
#include "pdfutils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageWriter>
#include <QPageLayout>
#include <QPainter>
#include <QPrinter>
#include <QSaveFile>
#include <QtMath>

#include <algorithm>
#include <numeric>

#include "pdfdbgheap.h"

namespace pdfviewer
{

namespace
{

constexpr int MAX_REPORTED_RENDER_WARNINGS = 20;

}   // namespace

PDFPageOutputContext PDFPageOutputContext::fromProxy(const pdf::PDFDocument* document, pdf::PDFDrawWidgetProxy* proxy)
{
    PDFPageOutputContext context;
    context.document = document;
    context.fontCache = proxy->getFontCache();
    context.cmsManager = proxy->getCMSManager();
    context.features = getOutputFeatures(proxy->getFeatures());
    context.meshQualitySettings = proxy->getMeshQualitySettings();
    context.rendererEngine = proxy->getRendererEngine();
    return context;
}

pdf::PDFRenderer::Features PDFPageOutputContext::getOutputFeatures(pdf::PDFRenderer::Features viewFeatures)
{
    // Color adjustments, debug overlays and render times are modes of the on-screen view.
    // A page printed or exported while the viewer shows inverted colors must not be inverted.
    constexpr pdf::PDFRenderer::Features viewOnlyFeatures = pdf::PDFRenderer::Features(
        pdf::PDFRenderer::DisplayTimes |
        pdf::PDFRenderer::DebugTextBlocks |
        pdf::PDFRenderer::DebugTextLines |
        pdf::PDFRenderer::DenyExtraGraphics |
        pdf::PDFRenderer::LogicalSizeZooming |
        pdf::PDFRenderer::ColorAdjust_Invert |
        pdf::PDFRenderer::ColorAdjust_Grayscale |
        pdf::PDFRenderer::ColorAdjust_HighContrast |
        pdf::PDFRenderer::ColorAdjust_Bitonal |
        pdf::PDFRenderer::ColorAdjust_CustomColors);
    return viewFeatures & ~viewOnlyFeatures;
}

QRectF PDFPageOutput::calculatePagePlacement(const QSizeF& pageSizePoints,
                                             const QRectF& printableArea,
                                             qreal resolutionDpi,
                                             PDFPrintScaling scaling)
{
    const QSizeF pageSize = pageSizePoints * pdf::PDF_POINT_TO_INCH * resolutionDpi;
    if (pageSize.isEmpty() || printableArea.isEmpty())
    {
        return printableArea;
    }

    qreal scale = 1.0;
    if (scaling == PDFPrintScaling::FitToPrintableArea)
    {
        scale = qMin(printableArea.width() / pageSize.width(), printableArea.height() / pageSize.height());
    }

    QRectF placement(QPointF(0.0, 0.0), pageSize * scale);
    placement.moveCenter(printableArea.center());
    return placement;
}

bool PDFPageOutput::isLandscapePage(const pdf::PDFPage* page)
{
    const QSizeF size = page->getRotatedMediaBox().size();
    return size.width() > size.height();
}

PDFPrintResult PDFPageOutput::print(QPrinter* printer,
                                    const PDFPageOutputContext& context,
                                    const PDFPrintOptions& options,
                                    const PDFPrintProgressCallback& progress)
{
    PDFPrintResult result;

    const pdf::PDFCatalog* catalog = context.document->getCatalog();
    const pdf::PDFInteger pageCount = pdf::PDFInteger(catalog->getPageCount());
    std::vector<pdf::PDFInteger> pages;
    pages.reserve(options.pageIndices.size());
    for (const pdf::PDFInteger pageIndex : options.pageIndices)
    {
        if (pageIndex >= 0 && pageIndex < pageCount && catalog->getPage(pageIndex))
        {
            pages.push_back(pageIndex);
        }
    }

    if (!printer || pages.empty())
    {
        result.errorMessage = tr("There are no pages to print.");
        return result;
    }

    // Engines without native multiple copies (for example PDF output) get the copies from us.
    std::vector<pdf::PDFInteger> sheets;
    if (!printer->supportsMultipleCopies() && printer->copyCount() > 1)
    {
        const int copies = printer->copyCount();
        const bool collate = printer->collateCopies();
        printer->setCopyCount(1);
        sheets.reserve(pages.size() * size_t(copies));
        if (collate)
        {
            for (int copy = 0; copy < copies; ++copy)
            {
                sheets.insert(sheets.end(), pages.cbegin(), pages.cend());
            }
        }
        else
        {
            for (const pdf::PDFInteger pageIndex : pages)
            {
                sheets.insert(sheets.end(), size_t(copies), pageIndex);
            }
        }
    }
    else
    {
        sheets = pages;
    }

    auto getSheetOrientation = [&](pdf::PDFInteger pageIndex)
    {
        switch (options.orientation)
        {
            case PDFPrintOrientation::Portrait:
                return QPageLayout::Portrait;

            case PDFPrintOrientation::Landscape:
                return QPageLayout::Landscape;

            default:
                break;
        }
        return isLandscapePage(catalog->getPage(pageIndex)) ? QPageLayout::Landscape : QPageLayout::Portrait;
    };

    // The painter coordinates start in the corner of the printable area.
    printer->setFullPage(false);
    printer->setPageOrientation(getSheetOrientation(sheets.front()));

    QPainter painter;
    if (!painter.begin(printer))
    {
        result.errorMessage = tr("Printing could not be started. The printer is not available, or no output file was chosen.");
        return result;
    }

    pdf::PDFOptionalContentActivity optionalContentActivity(context.document, pdf::OCUsage::Print, nullptr);
    const pdf::PDFCMSPointer cms = context.cmsManager->getCurrentCMS();
    pdf::PDFRenderer renderer(context.document, context.fontCache, cms.data(), &optionalContentActivity, context.features, context.meshQualitySettings);
    pdf::PDFModifiedDocument modifiedDocument(const_cast<pdf::PDFDocument*>(context.document), &optionalContentActivity);
    pdf::PDFAnnotationManager annotationManager(context.fontCache, context.cmsManager, &optionalContentActivity, context.meshQualitySettings, context.features, pdf::PDFAnnotationManager::Target::Print, nullptr);
    annotationManager.setDocument(modifiedDocument);
    pdf::PDFColorConvertor colorConvertor = cms->getColorConvertor();
    pdf::PDFRenderer::applyFeaturesToColorConvertor(context.features, colorConvertor);

    auto addWarnings = [&](pdf::PDFInteger pageIndex, const QList<pdf::PDFRenderError>& errors)
    {
        for (const pdf::PDFRenderError& error : errors)
        {
            if (error.type == pdf::RenderErrorType::Error && result.renderWarnings.size() < MAX_REPORTED_RENDER_WARNINGS)
            {
                result.renderWarnings << tr("Page %1: %2").arg(pageIndex + 1).arg(error.message);
            }
        }
    };

    const int total = int(sheets.size());
    bool failed = false;
    for (int sheetIndex = 0; sheetIndex < total; ++sheetIndex)
    {
        if (progress && !progress(sheetIndex, total))
        {
            result.cancelled = true;
            break;
        }

        const pdf::PDFInteger pageIndex = sheets[size_t(sheetIndex)];
        const pdf::PDFPage* page = catalog->getPage(pageIndex);

        if (sheetIndex > 0)
        {
            const QPageLayout::Orientation orientation = getSheetOrientation(pageIndex);
            if (printer->pageLayout().orientation() != orientation)
            {
                printer->setPageOrientation(orientation);
            }

            if (!printer->newPage())
            {
                result.errorMessage = tr("The printer did not accept page %1.").arg(pageIndex + 1);
                failed = true;
                break;
            }
        }

        // The sheet size comes from the page layout. Printer metrics can lag
        // behind a page that has just changed its orientation.
        const int resolution = printer->resolution();
        const QRectF printableArea(QPointF(0.0, 0.0), QSizeF(printer->pageLayout().paintRectPixels(resolution).size()));
        const QRectF placement = calculatePagePlacement(page->getRotatedMediaBox().size(), printableArea, resolution, options.scaling);
        const QTransform matrix = pdf::PDFRenderer::createPagePointToDevicePointMatrix(page, placement);

        painter.save();
        painter.setClipRect(printableArea);
        addWarnings(pageIndex, renderer.render(&painter, matrix, size_t(pageIndex)));

        QList<pdf::PDFRenderError> annotationErrors;
        pdf::PDFTextLayoutGetter textLayoutGetter(nullptr, pageIndex);
        annotationManager.drawPage(&painter, pageIndex, nullptr, textLayoutGetter, matrix, colorConvertor, annotationErrors);
        addWarnings(pageIndex, annotationErrors);
        painter.restore();

        ++result.pagesPrinted;
    }

    if (result.cancelled || failed)
    {
        printer->abort();
    }
    painter.end();

    if (result.cancelled && !printer->outputFileName().isEmpty())
    {
        // Do not leave a half written output file behind.
        QFile::remove(printer->outputFileName());
    }

    result.completed = !result.cancelled && !failed;
    return result;
}

int PDFImageExportResult::getCount(PDFImageExportItemResult::Status status) const
{
    return int(std::count_if(items.cbegin(), items.cend(), [status](const PDFImageExportItemResult& item) { return item.status == status; }));
}

PDFPageImageExporter::PDFPageImageExporter(const PDFPageOutputContext& context,
                                           pdf::OCUsage optionalContentUsage,
                                           int rasterizerCount) :
    m_context(context),
    m_optionalContentActivity(new pdf::PDFOptionalContentActivity(context.document, optionalContentUsage, nullptr)),
    m_rasterizerPool(nullptr)
{
    // The pool keeps a reference to the mesh settings, so it uses our own copy.
    m_rasterizerPool = new pdf::PDFRasterizerPool(m_context.document,
                                                  m_context.fontCache,
                                                  m_context.cmsManager,
                                                  m_optionalContentActivity,
                                                  m_context.features,
                                                  m_context.meshQualitySettings,
                                                  pdf::PDFRasterizerPool::getCorrectedRasterizerCount(rasterizerCount),
                                                  m_context.rendererEngine,
                                                  nullptr);
}

PDFPageImageExporter::~PDFPageImageExporter()
{
    delete m_rasterizerPool;
    delete m_optionalContentActivity;
}

QSize PDFPageImageExporter::getImageSize(const pdf::PDFPage* page, int dpi)
{
    const QSizeF size = page->getRotatedMediaBox().size() * pdf::PDF_POINT_TO_INCH * dpi;
    return QSize(qMax(1, qRound(size.width())), qMax(1, qRound(size.height())));
}

int PDFPageImageExporter::getRecommendedRasterizerCount(QSize largestImageSize)
{
    // The image is held while it is rendered, flattened, cropped and encoded.
    constexpr qint64 memoryBudget = 1200ll * 1024 * 1024;
    const qint64 bytesPerImage = qMax<qint64>(1, qint64(largestImageSize.width()) * largestImageSize.height() * 4 * 2);
    const int byMemory = int(std::clamp<qint64>(memoryBudget / bytesPerImage, 1, 256));
    return qMin(byMemory, pdf::PDFRasterizerPool::getDefaultRasterizerCount());
}

QString PDFPageImageExporter::getFileExtension(PDFImageFormat format)
{
    return format == PDFImageFormat::Png ? QStringLiteral("png") : QStringLiteral("jpg");
}

QString PDFPageImageExporter::getFileName(const QString& baseName,
                                          pdf::PDFInteger pageIndex,
                                          pdf::PDFInteger pageCount,
                                          PDFImageFormat format,
                                          bool isRegion)
{
    const int width = QString::number(qMax<pdf::PDFInteger>(1, pageCount)).size();
    return QStringLiteral("%1_p%2%3.%4").arg(baseName,
                                             QString::number(pageIndex + 1).rightJustified(width, QLatin1Char('0')),
                                             isRegion ? QStringLiteral("_selection") : QString(),
                                             getFileExtension(format));
}

QString PDFPageImageExporter::sanitizeBaseName(const QString& name)
{
    QString result = name;
    for (QChar& ch : result)
    {
        if (ch.unicode() < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(ch))
        {
            ch = QLatin1Char('_');
        }
    }

    result = result.trimmed();
    while (result.endsWith(QLatin1Char('.')))
    {
        result.chop(1);
    }
    result.truncate(80);

    return result.isEmpty() ? QStringLiteral("document") : result;
}

QImage PDFPageImageExporter::renderPageWith(pdf::PDFRasterizer* rasterizer,
                                            pdf::PDFInteger pageIndex,
                                            QSize size,
                                            QString* errorMessage) const
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return QImage();
    };

    const pdf::PDFCatalog* catalog = m_context.document->getCatalog();
    const pdf::PDFPage* page = (pageIndex >= 0 && pageIndex < pdf::PDFInteger(catalog->getPageCount())) ? catalog->getPage(pageIndex) : nullptr;
    if (!page)
    {
        return fail(tr("Page %1 does not exist.").arg(pageIndex + 1));
    }

    if (size.isEmpty() || qint64(size.width()) * size.height() > getMaxImagePixels())
    {
        return fail(tr("The image of page %1 would be too large (%2 x %3 pixels). Use a lower resolution.").arg(pageIndex + 1).arg(size.width()).arg(size.height()));
    }

    // Only this page is compiled; the rest of the document is not touched.
    pdf::PDFPrecompiledPage precompiledPage;
    const pdf::PDFCMSPointer cms = m_context.cmsManager->getCurrentCMS();
    pdf::PDFRenderer renderer(m_context.document, m_context.fontCache, cms.data(), m_optionalContentActivity, m_context.features, m_context.meshQualitySettings);
    renderer.compile(&precompiledPage, size_t(pageIndex));

    // Annotations are only drawn into the image, so the document is not modified.
    pdf::PDFModifiedDocument modifiedDocument(const_cast<pdf::PDFDocument*>(m_context.document), const_cast<pdf::PDFOptionalContentActivity*>(m_optionalContentActivity));
    pdf::PDFAnnotationManager annotationManager(m_context.fontCache, m_context.cmsManager, m_optionalContentActivity, m_context.meshQualitySettings, m_context.features, pdf::PDFAnnotationManager::Target::Print, nullptr);
    annotationManager.setDocument(modifiedDocument);

    QImage image = rasterizer->render(pageIndex, page, &precompiledPage, size, m_context.features, &annotationManager, cms.data(), pdf::PageRotation::None);
    if (image.isNull() || image.size() != size)
    {
        return fail(tr("Not enough memory to render page %1 at %2 x %3 pixels.").arg(pageIndex + 1).arg(size.width()).arg(size.height()));
    }

    // The rasterizer leaves the parts of the page which are not painted transparent.
    // Output images show the page on white paper (JPEG has no transparency at all).
    {
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        painter.fillRect(image.rect(), Qt::white);
    }
    image.reinterpretAsFormat(QImage::Format_RGB32);
    return image;
}

QImage PDFPageImageExporter::renderPage(pdf::PDFInteger pageIndex, QSize size, QString* errorMessage) const
{
    pdf::PDFRasterizer* rasterizer = m_rasterizerPool->acquire();
    QImage image = renderPageWith(rasterizer, pageIndex, size, errorMessage);
    m_rasterizerPool->release(rasterizer);
    return image;
}

PDFImageExportItemResult PDFPageImageExporter::exportTarget(const PDFImageExportTarget& target,
                                                            PDFImageFormat format,
                                                            int dpi,
                                                            int jpegQuality) const
{
    PDFImageExportItemResult result;
    result.target = target;
    result.status = PDFImageExportItemResult::Status::Failed;

    const pdf::PDFCatalog* catalog = m_context.document->getCatalog();
    const pdf::PDFPage* page = (target.pageIndex >= 0 && target.pageIndex < pdf::PDFInteger(catalog->getPageCount())) ? catalog->getPage(target.pageIndex) : nullptr;
    if (!page)
    {
        result.errorMessage = tr("Page %1 does not exist.").arg(target.pageIndex + 1);
        return result;
    }

    const QSize fullSize = getImageSize(page, dpi);

    // The rasterizer is held until the file is written, which bounds memory use
    // by the number of rasterizers.
    pdf::PDFRasterizer* rasterizer = m_rasterizerPool->acquire();
    struct Lease
    {
        const PDFPageImageExporter* exporter;
        pdf::PDFRasterizer* rasterizer;
        ~Lease() { exporter->m_rasterizerPool->release(rasterizer); }
    } lease{ this, rasterizer };

    QImage image = renderPageWith(rasterizer, target.pageIndex, fullSize, &result.errorMessage);
    if (image.isNull())
    {
        return result;
    }

    if (!target.region.isNull())
    {
        const QTransform matrix = pdf::PDFRenderer::createPagePointToDevicePointMatrix(page, QRectF(QPointF(0.0, 0.0), QSizeF(fullSize)));
        const QRect cropRect = matrix.mapRect(target.region.normalized()).toAlignedRect().intersected(image.rect());
        if (cropRect.isEmpty())
        {
            result.errorMessage = tr("The selection on page %1 is outside of the page.").arg(target.pageIndex + 1);
            return result;
        }

        const int dotsPerMeterX = image.dotsPerMeterX();
        const int dotsPerMeterY = image.dotsPerMeterY();
        image = image.copy(cropRect);
        image.setDotsPerMeterX(dotsPerMeterX);
        image.setDotsPerMeterY(dotsPerMeterY);
    }

    // Commit only complete files: a failed write never replaces or leaves a partial image.
    QSaveFile file(target.fileName);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        result.errorMessage = tr("Cannot write '%1': %2").arg(QDir::toNativeSeparators(target.fileName), file.errorString());
        return result;
    }

    QImageWriter writer(&file, format == PDFImageFormat::Png ? QByteArrayLiteral("png") : QByteArrayLiteral("jpeg"));
    if (format == PDFImageFormat::Jpeg)
    {
        writer.setQuality(qBound(1, jpegQuality, 100));
    }

    if (!writer.write(image))
    {
        file.cancelWriting();
        result.errorMessage = tr("Cannot write '%1': %2").arg(QDir::toNativeSeparators(target.fileName), writer.errorString());
        return result;
    }

    if (!file.commit())
    {
        result.errorMessage = tr("Cannot write '%1': %2").arg(QDir::toNativeSeparators(target.fileName), file.errorString());
        return result;
    }

    result.status = PDFImageExportItemResult::Status::Succeeded;
    result.imageSize = image.size();
    result.errorMessage.clear();
    return result;
}

PDFImageExportResult PDFPageImageExporter::exportTargets(const std::vector<PDFImageExportTarget>& targets,
                                                         PDFImageFormat format,
                                                         int dpi,
                                                         int jpegQuality,
                                                         const std::atomic_bool* cancelled,
                                                         std::atomic_int* processed) const
{
    PDFImageExportResult result;
    result.items.resize(targets.size());
    for (size_t i = 0; i < targets.size(); ++i)
    {
        result.items[i].target = targets[i];
    }

    std::vector<size_t> indices(targets.size());
    std::iota(indices.begin(), indices.end(), size_t(0));

    auto processTarget = [&](size_t index)
    {
        if (cancelled && cancelled->load())
        {
            return;
        }

        result.items[index] = exportTarget(targets[index], format, dpi, jpegQuality);
        if (processed)
        {
            ++(*processed);
        }
    };
    pdf::PDFExecutionPolicy::execute(pdf::PDFExecutionPolicy::Scope::Page, indices.cbegin(), indices.cend(), processTarget);

    result.cancelled = cancelled && cancelled->load() && result.getNotProcessedCount() > 0;
    return result;
}

}   // namespace pdfviewer
