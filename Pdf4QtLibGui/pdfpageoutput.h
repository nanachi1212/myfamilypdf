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

#ifndef PDFPAGEOUTPUT_H
#define PDFPAGEOUTPUT_H

#include "pdf4qtlibgui_export.h"

#include "pdfdocument.h"
#include "pdfrenderer.h"
#include "pdfoptionalcontent.h"

#include <QCoreApplication>
#include <QImage>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <vector>

class QPrinter;

namespace pdf
{
class PDFCMSManager;
class PDFDrawWidgetProxy;
class PDFFontCache;
class PDFRasterizer;
class PDFRasterizerPool;
}

namespace pdfviewer
{

/// Everything needed to render pages for printing or image export without the
/// draw widget. The pointers are not owned and must outlive the object using them.
struct PDF4QTLIBGUILIBSHARED_EXPORT PDFPageOutputContext
{
    const pdf::PDFDocument* document = nullptr;
    pdf::PDFFontCache* fontCache = nullptr;
    const pdf::PDFCMSManager* cmsManager = nullptr;
    pdf::PDFRenderer::Features features = pdf::PDFRenderer::getDefaultFeatures();
    pdf::PDFMeshQualitySettings meshQualitySettings;
    pdf::RendererEngine rendererEngine = pdf::RendererEngine::Blend2D_SingleThread;

    /// Takes the shared rendering resources from the viewer. The on-screen
    /// view modes (color adjustments, debug overlays) are not part of the output.
    static PDFPageOutputContext fromProxy(const pdf::PDFDocument* document, pdf::PDFDrawWidgetProxy* proxy);

    /// Removes the view-only features from \p viewFeatures.
    static pdf::PDFRenderer::Features getOutputFeatures(pdf::PDFRenderer::Features viewFeatures);
};

enum class PDFPrintOrientation
{
    Auto,       ///< Each sheet follows the orientation of the printed page
    Portrait,
    Landscape
};

enum class PDFPrintScaling
{
    FitToPrintableArea,     ///< Scale the page up or down to fill the printable area
    ActualSize              ///< Print at 100%, centered on the sheet
};

struct PDFPrintOptions
{
    std::vector<pdf::PDFInteger> pageIndices;   ///< Zero based indices, printed in the given order
    PDFPrintOrientation orientation = PDFPrintOrientation::Auto;
    PDFPrintScaling scaling = PDFPrintScaling::FitToPrintableArea;
};

struct PDFPrintResult
{
    bool completed = false;     ///< True, if every requested page was sent to the printer
    bool cancelled = false;
    int pagesPrinted = 0;       ///< Sheets sent to the printer (including repeated copies)
    QString errorMessage;
    QStringList renderWarnings;
};

/// Called before each sheet with the number of sheets already done and the total.
/// Returning false cancels the job.
using PDFPrintProgressCallback = std::function<bool(int done, int total)>;

class PDF4QTLIBGUILIBSHARED_EXPORT PDFPageOutput
{
    Q_DECLARE_TR_FUNCTIONS(pdfviewer::PDFPageOutput)

public:
    /// Returns the area (in device pixels) of a sheet occupied by a page of the given size.
    /// The page is always centered in \p printableArea.
    static QRectF calculatePagePlacement(const QSizeF& pageSizePoints,
                                         const QRectF& printableArea,
                                         qreal resolutionDpi,
                                         PDFPrintScaling scaling);

    /// Returns true, if the page is displayed wider than it is high.
    static bool isLandscapePage(const pdf::PDFPage* page);

    /// Prints the pages to the printer using the vector print pipeline of the viewer;
    /// pages are rendered one by one while printing, never in advance. If the printer
    /// can't produce multiple copies by itself, the copies are repeated here.
    /// The printer must be configured (paper, duplex, copies, ...) except for orientation,
    /// which is set according to \p options. Must be called from the GUI thread.
    static PDFPrintResult print(QPrinter* printer,
                                const PDFPageOutputContext& context,
                                const PDFPrintOptions& options,
                                const PDFPrintProgressCallback& progress = PDFPrintProgressCallback());
};

enum class PDFImageFormat
{
    Png,
    Jpeg
};

struct PDFImageExportTarget
{
    pdf::PDFInteger pageIndex = 0;
    QRectF region;      ///< Region in page user space, null for the whole page
    QString fileName;   ///< Absolute file name
};

struct PDFImageExportItemResult
{
    enum class Status
    {
        Succeeded,
        Failed,
        NotProcessed    ///< Skipped because the export was cancelled
    };

    PDFImageExportTarget target;
    Status status = Status::NotProcessed;
    QSize imageSize;
    QString errorMessage;
};

struct PDF4QTLIBGUILIBSHARED_EXPORT PDFImageExportResult
{
    std::vector<PDFImageExportItemResult> items;
    bool cancelled = false;

    int getCount(PDFImageExportItemResult::Status status) const;
    int getSucceededCount() const { return getCount(PDFImageExportItemResult::Status::Succeeded); }
    int getFailedCount() const { return getCount(PDFImageExportItemResult::Status::Failed); }
    int getNotProcessedCount() const { return getCount(PDFImageExportItemResult::Status::NotProcessed); }

    /// True only if every requested file was written.
    bool isComplete() const { return !cancelled && getSucceededCount() == int(items.size()); }
};

/// Renders pages to images using the rasterizer of the viewer and writes PNG/JPEG
/// files. Only the requested pages are touched. Construct in the GUI thread;
/// the export itself can run in any thread.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFPageImageExporter
{
    Q_DECLARE_TR_FUNCTIONS(pdfviewer::PDFPageImageExporter)

public:
    explicit PDFPageImageExporter(const PDFPageOutputContext& context,
                                  pdf::OCUsage optionalContentUsage,
                                  int rasterizerCount);
    ~PDFPageImageExporter();

    PDFPageImageExporter(const PDFPageImageExporter&) = delete;
    PDFPageImageExporter& operator=(const PDFPageImageExporter&) = delete;

    /// Renders the whole page into an opaque image of the given size. The page is flattened
    /// onto white paper. Returns a null image and sets \p errorMessage on failure.
    QImage renderPage(pdf::PDFInteger pageIndex, QSize size, QString* errorMessage = nullptr) const;

    /// Renders the pages (or regions of them) and writes the files. Every file is written
    /// atomically, so a failure never leaves a partially written image behind. When \p cancelled
    /// becomes true, pages which are not started yet are reported as not processed.
    /// \p processed (optional) counts the finished targets.
    PDFImageExportResult exportTargets(const std::vector<PDFImageExportTarget>& targets,
                                       PDFImageFormat format,
                                       int dpi,
                                       int jpegQuality,
                                       const std::atomic_bool* cancelled = nullptr,
                                       std::atomic_int* processed = nullptr) const;

    static constexpr int getMinDpi() { return 36; }
    static constexpr int getMaxDpi() { return 1200; }
    static constexpr int getDefaultDpi() { return 150; }
    static constexpr int getDefaultJpegQuality() { return 90; }

    /// Largest accepted image, in pixels.
    static constexpr qint64 getMaxImagePixels() { return 120000000; }

    /// Returns the size (pixels) of the whole page rendered with the given resolution.
    static QSize getImageSize(const pdf::PDFPage* page, int dpi);

    /// Returns the number of parallel rasterizers which keeps memory use reasonable
    /// when rendering images of the given size.
    static int getRecommendedRasterizerCount(QSize largestImageSize);

    static QString getFileExtension(PDFImageFormat format);

    /// Returns a predictable, collision free file name (without directory) such as "report_p007.png".
    /// The page number is padded to the width of the page count, so files sort in page order.
    static QString getFileName(const QString& baseName,
                               pdf::PDFInteger pageIndex,
                               pdf::PDFInteger pageCount,
                               PDFImageFormat format,
                               bool isRegion);

    /// Makes a document name usable as a file name prefix.
    static QString sanitizeBaseName(const QString& name);

private:
    PDFImageExportItemResult exportTarget(const PDFImageExportTarget& target,
                                          PDFImageFormat format,
                                          int dpi,
                                          int jpegQuality) const;

    QImage renderPageWith(pdf::PDFRasterizer* rasterizer,
                          pdf::PDFInteger pageIndex,
                          QSize size,
                          QString* errorMessage) const;

    PDFPageOutputContext m_context;
    pdf::PDFOptionalContentActivity* m_optionalContentActivity;
    pdf::PDFRasterizerPool* m_rasterizerPool;
};

}   // namespace pdfviewer

#endif // PDFPAGEOUTPUT_H
