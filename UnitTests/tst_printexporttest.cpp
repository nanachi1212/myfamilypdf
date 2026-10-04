// Print and image export workflow tests (FamilyPDF v7).
//
// Pages are checked by rendering them with the product renderer, so every
// assertion compares what a user would see: sizes, orientation, colors.

#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>

#include "pdfannotation.h"
#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocument.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdffont.h"
#include "pdfform.h"
#include "pdfoptionalcontent.h"
#include "pdfpageoutput.h"
#include "pdfpainter.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

using namespace pdfviewer;

namespace
{

/// A document with the rendering resources needed to print or export it.
class LoadedDocument
{
public:
    bool load(const QString& path)
    {
        pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
        pdf::PDFDocument document = reader.readFromFile(path);
        if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
        {
            qWarning() << "Cannot read" << path << reader.getErrorMessage();
            return false;
        }
        adopt(std::move(document));
        return true;
    }

    void adopt(pdf::PDFDocument document)
    {
        m_document = std::make_unique<pdf::PDFDocument>(std::move(document));
        m_optionalContent = std::make_unique<pdf::PDFOptionalContentActivity>(m_document.get(), pdf::OCUsage::Export, nullptr);
        m_cmsManager = std::make_unique<pdf::PDFCMSManager>(nullptr);
        m_cmsManager->setDocument(m_document.get());
        m_fontCache = std::make_unique<pdf::PDFFontCache>(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
        m_fontCache->setDocument(pdf::PDFModifiedDocument(m_document.get(), m_optionalContent.get()));
        m_fontCache->setCacheShrinkEnabled(nullptr, false);
    }

    PDFPageOutputContext context() const
    {
        PDFPageOutputContext context;
        context.document = m_document.get();
        context.fontCache = m_fontCache.get();
        context.cmsManager = m_cmsManager.get();
        context.features = PDFPageOutputContext::getOutputFeatures(pdf::PDFRenderer::getDefaultFeatures());
        context.rendererEngine = pdf::RendererEngine::Blend2D_SingleThread;
        return context;
    }

    const pdf::PDFDocument* document() const { return m_document.get(); }
    int pageCount() const { return int(m_document->getCatalog()->getPageCount()); }
    const pdf::PDFPage* page(int index) const { return m_document->getCatalog()->getPage(index); }

    QImage render(int pageIndex, int dpi, QString* error = nullptr) const
    {
        PDFPageImageExporter exporter(context(), pdf::OCUsage::Export, 1);
        return exporter.renderPage(pageIndex, PDFPageImageExporter::getImageSize(page(pageIndex), dpi), error);
    }

private:
    std::unique_ptr<pdf::PDFDocument> m_document;
    std::unique_ptr<pdf::PDFOptionalContentActivity> m_optionalContent;
    std::unique_ptr<pdf::PDFCMSManager> m_cmsManager;
    std::unique_ptr<pdf::PDFFontCache> m_fontCache;
};

bool near(const QColor& actual, const QColor& expected, int tolerance = 12)
{
    return qAbs(actual.red() - expected.red()) <= tolerance &&
           qAbs(actual.green() - expected.green()) <= tolerance &&
           qAbs(actual.blue() - expected.blue()) <= tolerance;
}

QString describe(const QColor& color)
{
    return QStringLiteral("(%1,%2,%3)").arg(color.red()).arg(color.green()).arg(color.blue());
}

/// Bounding rectangle of everything which is not white paper.
QRect nonWhiteBounds(const QImage& image)
{
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            if (qRed(line[x]) < 240 || qGreen(line[x]) < 240 || qBlue(line[x]) < 240)
            {
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }
    }
    return right < 0 ? QRect() : QRect(QPoint(left, top), QPoint(right, bottom));
}

qint64 countNonWhite(const QImage& image)
{
    qint64 count = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            if (qRed(line[x]) < 200 || qGreen(line[x]) < 200 || qBlue(line[x]) < 200)
            {
                ++count;
            }
        }
    }
    return count;
}

const QColor PAGE_COLORS[] = {
    QColor(200, 30, 30), QColor(30, 150, 60), QColor(30, 60, 200), QColor(220, 160, 20),
    QColor(150, 40, 170), QColor(20, 160, 170), QColor(110, 110, 110), QColor(240, 100, 150)
};

/// Page index -> distinguishing color of the page contents.
QColor colorOfPage(int index) { return PAGE_COLORS[index % 8]; }

struct PageSpec
{
    QSizeF size = QSizeF(595, 842);
    pdf::PageRotation rotation = pdf::PageRotation::None;
    std::optional<QRectF> cropBox;
    bool marker = true;
};

/// Fills the page with its identity color (inset by 20 points) and puts a black
/// marker square into the top-left corner of the unrotated page.
void paintPage(pdf::PDFDocumentBuilder* builder, pdf::PDFObjectReference page, const PageSpec& spec, int index, bool transparent = false)
{
    pdf::PDFPageContentStreamBuilder contentBuilder(builder,
                                                    pdf::PDFContentStreamBuilder::CoordinateSystem::PDF,
                                                    pdf::PDFPageContentStreamBuilder::Mode::Replace);
    QPainter* painter = contentBuilder.begin(page);
    Q_ASSERT(painter);
    QColor color = colorOfPage(index);
    if (transparent)
    {
        painter->setOpacity(0.5);
    }
    painter->fillRect(QRectF(20, 20, spec.size.width() - 40, spec.size.height() - 40), color);
    painter->setOpacity(1.0);
    if (spec.marker)
    {
        painter->fillRect(QRectF(0, spec.size.height() - 30, 30, 30), Qt::black);
    }
    contentBuilder.end(painter);
}

/// Builds a document from the page specs. Every page gets its identity color.
pdf::PDFDocument buildDocument(const std::vector<PageSpec>& specs, int transparentPage = -1)
{
    pdf::PDFDocumentBuilder builder;
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const pdf::PDFObjectReference page = builder.appendPage(QRectF(QPointF(0, 0), specs[i].size));
        paintPage(&builder, page, specs[i], int(i), int(i) == transparentPage);
        if (specs[i].rotation != pdf::PageRotation::None)
        {
            builder.setPageRotation(page, specs[i].rotation);
        }
        if (specs[i].cropBox)
        {
            builder.setPageCropBox(page, specs[i].cropBox.value());
        }
    }
    return builder.build();
}

bool writeDocument(pdf::PDFDocument document, const QString& path)
{
    pdf::PDFDocumentWriter writer(nullptr);
    return bool(writer.write(path, &document, true));
}

PDFPrintResult printToPdf(const LoadedDocument& source,
                               const PDFPrintOptions& options,
                               const QString& outputPath,
                               int copies = 1,
                               bool collate = true,
                               const PDFPrintProgressCallback& progress = PDFPrintProgressCallback())
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setCopyCount(copies);
    printer.setCollateCopies(collate);
    return PDFPageOutput::print(&printer, source.context(), options, progress);
}

PDFPrintOptions makeOptions(std::vector<pdf::PDFInteger> pages,
                            PDFPrintOrientation orientation = PDFPrintOrientation::Auto,
                            PDFPrintScaling scaling = PDFPrintScaling::FitToPrintableArea)
{
    PDFPrintOptions options;
    options.pageIndices = std::move(pages);
    options.orientation = orientation;
    options.scaling = scaling;
    return options;
}

}   // namespace

class PrintExportTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void outputFeaturesDropViewOnlyModes();
    void pagePlacement_data();
    void pagePlacement();
    void fileNames();
    void sanitizedBaseNames();

    void printAllPagesFollowsPageOrientation();
    void printRangeKeepsPageOrderAndIdentity();
    void printForcedOrientation();
    void printFitAndActualSize();
    void printCopiesAndCollation();
    void printCancelLeavesNoOutput();
    void printWithoutPages();
    void printRotatedPageAndCropBox();
    void printAnnotationsAndTransparency();
    void printJpeg2000Scan();
    void printChineseText();
    void printOnlyRequestedPagesOfLargeDocument();
    void printWithWindowsPrinterDriver();

    void exportPngSizesAndFlattening();
    void exportJpegQuality();
    void exportRotatedAndMixedPageSizes();
    void exportBatchNamesAndOrder();
    void exportReportsPartialFailure();
    void exportCancel();
    void exportRegion();
    void exportTransparencyAndAnnotation();
    void exportJpeg2000Scan();
    void exportOnlyRequestedPagesOfLargeDocument();
    void exportHighDpiAndLimits();

private:
    QString path(const QString& name) const { return m_temp.filePath(name); }

    QTemporaryDir m_temp;
};

void PrintExportTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_temp.filePath(QStringLiteral("settings")));
}

void PrintExportTest::outputFeaturesDropViewOnlyModes()
{
    const pdf::PDFRenderer::Features view = pdf::PDFRenderer::getDefaultFeatures() |
                                            pdf::PDFRenderer::ColorAdjust_Invert |
                                            pdf::PDFRenderer::ColorAdjust_Grayscale |
                                            pdf::PDFRenderer::DisplayTimes |
                                            pdf::PDFRenderer::DebugTextBlocks |
                                            pdf::PDFRenderer::DenyExtraGraphics;
    const pdf::PDFRenderer::Features output = PDFPageOutputContext::getOutputFeatures(view);
    QVERIFY(!output.testFlag(pdf::PDFRenderer::ColorAdjust_Invert));
    QVERIFY(!output.testFlag(pdf::PDFRenderer::ColorAdjust_Grayscale));
    QVERIFY(!output.testFlag(pdf::PDFRenderer::DisplayTimes));
    QVERIFY(!output.testFlag(pdf::PDFRenderer::DebugTextBlocks));
    QVERIFY(!output.testFlag(pdf::PDFRenderer::DenyExtraGraphics));
    // Content related features survive.
    QVERIFY(output.testFlag(pdf::PDFRenderer::Antialiasing));
    QVERIFY(output.testFlag(pdf::PDFRenderer::ClipToCropBox));
    QVERIFY(output.testFlag(pdf::PDFRenderer::DisplayAnnotations));
}

void PrintExportTest::pagePlacement_data()
{
    QTest::addColumn<QSizeF>("pagePoints");
    QTest::addColumn<QRectF>("area");
    QTest::addColumn<int>("scaling");
    QTest::addColumn<QRectF>("expected");

    // 72 dpi: one pixel is one point.
    QTest::newRow("fit A4 page to equal area") << QSizeF(595, 842) << QRectF(0, 0, 595, 842) << int(PDFPrintScaling::FitToPrintableArea) << QRectF(0, 0, 595, 842);
    QTest::newRow("fit shrinks wide page") << QSizeF(1000, 500) << QRectF(0, 0, 500, 700) << int(PDFPrintScaling::FitToPrintableArea) << QRectF(0, 225, 500, 250);
    QTest::newRow("fit enlarges small page") << QSizeF(100, 200) << QRectF(0, 0, 400, 400) << int(PDFPrintScaling::FitToPrintableArea) << QRectF(100, 0, 200, 400);
    QTest::newRow("fit honors area origin") << QSizeF(100, 100) << QRectF(10, 20, 200, 100) << int(PDFPrintScaling::FitToPrintableArea) << QRectF(60, 20, 100, 100);
    QTest::newRow("actual size is centered") << QSizeF(100, 200) << QRectF(0, 0, 400, 400) << int(PDFPrintScaling::ActualSize) << QRectF(150, 100, 100, 200);
    QTest::newRow("actual size larger than sheet is centered and clipped") << QSizeF(600, 800) << QRectF(0, 0, 500, 700) << int(PDFPrintScaling::ActualSize) << QRectF(-50, -50, 600, 800);
}

void PrintExportTest::pagePlacement()
{
    QFETCH(QSizeF, pagePoints);
    QFETCH(QRectF, area);
    QFETCH(int, scaling);
    QFETCH(QRectF, expected);

    const QRectF placement = PDFPageOutput::calculatePagePlacement(pagePoints, area, 72.0, PDFPrintScaling(scaling));
    QVERIFY2(qAbs(placement.left() - expected.left()) < 0.01 && qAbs(placement.top() - expected.top()) < 0.01 &&
             qAbs(placement.width() - expected.width()) < 0.01 && qAbs(placement.height() - expected.height()) < 0.01,
             qPrintable(QStringLiteral("%1,%2 %3x%4").arg(placement.left()).arg(placement.top()).arg(placement.width()).arg(placement.height())));

    // The printer resolution scales the placement, not the proportions.
    const QRectF doubled = PDFPageOutput::calculatePagePlacement(pagePoints, QRectF(area.topLeft() * 2.0, area.size() * 2.0), 144.0, PDFPrintScaling(scaling));
    QVERIFY(qAbs(doubled.width() - (scaling == int(PDFPrintScaling::ActualSize) ? expected.width() * 2.0 : expected.width() * 2.0)) < 0.01);
}

void PrintExportTest::fileNames()
{
    QCOMPARE(PDFPageImageExporter::getFileName(QStringLiteral("report"), 6, 10, PDFImageFormat::Png, false), QStringLiteral("report_p07.png"));
    QCOMPARE(PDFPageImageExporter::getFileName(QStringLiteral("report"), 9, 10, PDFImageFormat::Jpeg, false), QStringLiteral("report_p10.jpg"));
    QCOMPARE(PDFPageImageExporter::getFileName(QStringLiteral("report"), 0, 1, PDFImageFormat::Png, false), QStringLiteral("report_p1.png"));
    QCOMPARE(PDFPageImageExporter::getFileName(QStringLiteral("book"), 4, 1160, PDFImageFormat::Png, false), QStringLiteral("book_p0005.png"));
    QCOMPARE(PDFPageImageExporter::getFileName(QStringLiteral("report"), 2, 100, PDFImageFormat::Png, true), QStringLiteral("report_p003_selection.png"));

    // Names sort in page order and never collide for different pages.
    QStringList names;
    for (int i = 0; i < 120; ++i)
    {
        names << PDFPageImageExporter::getFileName(QStringLiteral("d"), i, 120, PDFImageFormat::Png, false);
    }
    QStringList sorted = names;
    sorted.sort();
    QCOMPARE(sorted, names);
    QCOMPARE(QSet<QString>(names.cbegin(), names.cend()).size(), names.size());
}

void PrintExportTest::sanitizedBaseNames()
{
    QCOMPARE(PDFPageImageExporter::sanitizeBaseName(QStringLiteral("a:b/c\\d*e?")), QStringLiteral("a_b_c_d_e_"));
    QCOMPARE(PDFPageImageExporter::sanitizeBaseName(QString()), QStringLiteral("document"));
    QCOMPARE(PDFPageImageExporter::sanitizeBaseName(QStringLiteral("name. ")), QStringLiteral("name"));
    QCOMPARE(PDFPageImageExporter::sanitizeBaseName(QString::fromUtf16(u"家庭 文件")), QString::fromUtf16(u"家庭 文件"));
    QVERIFY(PDFPageImageExporter::sanitizeBaseName(QString(300, QLatin1Char('x'))).size() <= 80);
}

void PrintExportTest::printAllPagesFollowsPageOrientation()
{
    // A4 portrait, Letter landscape, rotated page (displayed portrait), A5 portrait, square
    PageSpec rotated;
    rotated.size = QSizeF(400, 300);
    rotated.rotation = pdf::PageRotation::Rotate90;
    PageSpec landscape;
    landscape.size = QSizeF(792, 612);
    PageSpec a5;
    a5.size = QSizeF(420, 595);
    PageSpec square;
    square.size = QSizeF(500, 500);
    const QString sourcePath = path(QStringLiteral("orientation-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ PageSpec(), landscape, rotated, a5, square }), sourcePath));

    LoadedDocument source;
    QVERIFY(source.load(sourcePath));
    const QString outputPath = path(QStringLiteral("orientation-printed.pdf"));
    const PDFPrintResult result = printToPdf(source, makeOptions({ 0, 1, 2, 3, 4 }), outputPath);
    QVERIFY2(result.completed, qPrintable(result.errorMessage));
    QCOMPARE(result.pagesPrinted, 5);
    QVERIFY(result.renderWarnings.isEmpty());

    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    QCOMPARE(printed.pageCount(), 5);
    const bool expectedLandscape[] = { false, true, false, false, false };
    for (int i = 0; i < 5; ++i)
    {
        const QSizeF size = printed.page(i)->getRotatedMediaBox().size();
        QVERIFY2(PDFPageOutput::isLandscapePage(printed.page(i)) == expectedLandscape[i],
                 qPrintable(QStringLiteral("page %1 is %2x%3").arg(i + 1).arg(size.width()).arg(size.height())));
        // A4 sheet in either orientation
        QVERIFY(qAbs(qMax(size.width(), size.height()) - 842.0) < 1.5);
        QVERIFY(qAbs(qMin(size.width(), size.height()) - 595.0) < 1.5);
    }
}

void PrintExportTest::printRangeKeepsPageOrderAndIdentity()
{
    std::vector<PageSpec> specs(10);
    for (PageSpec& spec : specs)
    {
        spec.marker = false;
    }
    const QString sourcePath = path(QStringLiteral("range-source.pdf"));
    QVERIFY(writeDocument(buildDocument(specs), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    // "1-3,8,10": discontinuous range, as the dialog produces it.
    QString parseError;
    const pdf::PDFClosedIntervalSet numbers = pdf::PDFClosedIntervalSet::parsePageSelection(10, QStringLiteral("1-3,8,10"), &parseError);
    QVERIFY(parseError.isEmpty());
    std::vector<pdf::PDFInteger> pages;
    for (const pdf::PDFInteger number : numbers.unfold())
    {
        pages.push_back(number - 1);
    }
    QCOMPARE(pages, (std::vector<pdf::PDFInteger>{ 0, 1, 2, 7, 9 }));

    const QString outputPath = path(QStringLiteral("range-printed.pdf"));
    const PDFPrintResult result = printToPdf(source, makeOptions(pages), outputPath);
    QVERIFY2(result.completed, qPrintable(result.errorMessage));
    QCOMPARE(result.pagesPrinted, 5);

    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    QCOMPARE(printed.pageCount(), 5);
    for (size_t i = 0; i < pages.size(); ++i)
    {
        const QImage image = printed.render(int(i), 36);
        QVERIFY(!image.isNull());
        const QColor center = image.pixelColor(image.width() / 2, image.height() / 2);
        QVERIFY2(near(center, colorOfPage(int(pages[i]))),
                 qPrintable(QStringLiteral("sheet %1 should show page %2 %3 but is %4").arg(i + 1).arg(pages[i] + 1).arg(describe(colorOfPage(int(pages[i])))).arg(describe(center))));
    }

    // A single page ("current page") prints exactly that page.
    const QString singlePath = path(QStringLiteral("range-single.pdf"));
    QVERIFY(printToPdf(source, makeOptions({ 5 }), singlePath).completed);
    LoadedDocument single;
    QVERIFY(single.load(singlePath));
    QCOMPARE(single.pageCount(), 1);
    const QImage singleImage = single.render(0, 36);
    QVERIFY(near(singleImage.pixelColor(singleImage.width() / 2, singleImage.height() / 2), colorOfPage(5)));
}

void PrintExportTest::printForcedOrientation()
{
    PageSpec landscape;
    landscape.size = QSizeF(792, 612);
    const QString sourcePath = path(QStringLiteral("forced-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ PageSpec(), landscape }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    for (const PDFPrintOrientation orientation : { PDFPrintOrientation::Portrait, PDFPrintOrientation::Landscape })
    {
        const QString outputPath = path(QStringLiteral("forced-%1.pdf").arg(int(orientation)));
        QVERIFY(printToPdf(source, makeOptions({ 0, 1 }, orientation), outputPath).completed);
        LoadedDocument printed;
        QVERIFY(printed.load(outputPath));
        QCOMPARE(printed.pageCount(), 2);
        for (int i = 0; i < 2; ++i)
        {
            QCOMPARE(PDFPageOutput::isLandscapePage(printed.page(i)), orientation == PDFPrintOrientation::Landscape);
        }
    }
}

void PrintExportTest::printFitAndActualSize()
{
    // A 300 x 400 point page filled with color. On A4 (595 x 842) "fit" enlarges it
    // to the printable area, "actual size" keeps it at 300 x 400 points.
    PageSpec spec;
    spec.size = QSizeF(300, 400);
    spec.marker = false;
    const QString sourcePath = path(QStringLiteral("fit-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ spec }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    auto printedBounds = [&](PDFPrintScaling scaling, QSizeF* sheetSize)
    {
        const QString outputPath = path(QStringLiteral("fit-%1.pdf").arg(int(scaling)));
        const PDFPrintResult result = printToPdf(source, makeOptions({ 0 }, PDFPrintOrientation::Portrait, scaling), outputPath);
        LoadedDocument printed;
        if (!result.completed || !printed.load(outputPath))
        {
            return QRect();
        }
        *sheetSize = printed.page(0)->getRotatedMediaBox().size();
        return nonWhiteBounds(printed.render(0, 72));
    };

    QSizeF sheet;
    const QRect fitBounds = printedBounds(PDFPrintScaling::FitToPrintableArea, &sheet);
    QVERIFY(qAbs(sheet.width() - 595.0) < 1.5 && qAbs(sheet.height() - 842.0) < 1.5);
    // The color spans 260 x 360 points at 100%. On the printable area of the sheet (about
    // 575 x 822 points) "fit" scales the page by about 1.92: 498 x 690 points.
    QVERIFY2(fitBounds.width() > 480 && fitBounds.width() < 520 && fitBounds.height() > 670 && fitBounds.height() < 710,
             qPrintable(QStringLiteral("fit size %1x%2").arg(fitBounds.width()).arg(fitBounds.height())));
    // centered horizontally
    QVERIFY(qAbs(fitBounds.center().x() - 297) < 6);

    const QRect actualBounds = printedBounds(PDFPrintScaling::ActualSize, &sheet);
    QVERIFY2(qAbs(actualBounds.width() - 260) <= 3 && qAbs(actualBounds.height() - 360) <= 3,
             qPrintable(QStringLiteral("actual size %1x%2").arg(actualBounds.width()).arg(actualBounds.height())));
    QVERIFY(qAbs(actualBounds.center().x() - 297) < 4);
    QVERIFY(qAbs(actualBounds.center().y() - 421) < 4);
    QVERIFY(fitBounds.height() > actualBounds.height() * 1.8);
}

void PrintExportTest::printCopiesAndCollation()
{
    std::vector<PageSpec> specs(3);
    for (PageSpec& spec : specs)
    {
        spec.marker = false;
    }
    const QString sourcePath = path(QStringLiteral("copies-source.pdf"));
    QVERIFY(writeDocument(buildDocument(specs), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    auto sequence = [&](bool collate)
    {
        const QString outputPath = path(QStringLiteral("copies-%1.pdf").arg(collate));
        const PDFPrintResult result = printToPdf(source, makeOptions({ 0, 2 }), outputPath, 2, collate);
        QList<int> pages;
        LoadedDocument printed;
        if (!result.completed || !printed.load(outputPath))
        {
            return pages;
        }
        for (int i = 0; i < printed.pageCount(); ++i)
        {
            const QImage image = printed.render(i, 36);
            for (int p : { 0, 2 })
            {
                if (near(image.pixelColor(image.width() / 2, image.height() / 2), colorOfPage(p)))
                {
                    pages << p + 1;
                }
            }
        }
        return pages;
    };

    // The PDF engine has no native copies, so the copies are produced here.
    QCOMPARE(sequence(true), (QList<int>{ 1, 3, 1, 3 }));
    QCOMPARE(sequence(false), (QList<int>{ 1, 1, 3, 3 }));
}

void PrintExportTest::printCancelLeavesNoOutput()
{
    std::vector<PageSpec> specs(6);
    const QString sourcePath = path(QStringLiteral("cancel-source.pdf"));
    QVERIFY(writeDocument(buildDocument(specs), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    const QString outputPath = path(QStringLiteral("cancel-printed.pdf"));
    int callbacks = 0;
    const PDFPrintResult result = printToPdf(source, makeOptions({ 0, 1, 2, 3, 4, 5 }), outputPath, 1, true, [&callbacks](int done, int total)
    {
        ++callbacks;
        return done < 2;    // cancel before the third sheet
    });
    QVERIFY(result.cancelled);
    QVERIFY(!result.completed);
    QCOMPARE(result.pagesPrinted, 2);
    QCOMPARE(callbacks, 3);
    QVERIFY2(!QFileInfo::exists(outputPath), "a cancelled job must not leave an output file");
}

void PrintExportTest::printWithoutPages()
{
    const QString sourcePath = path(QStringLiteral("nopages-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ PageSpec() }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    const QString outputPath = path(QStringLiteral("nopages-printed.pdf"));
    PDFPrintResult result = printToPdf(source, makeOptions({}), outputPath);
    QVERIFY(!result.completed);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(!QFileInfo::exists(outputPath));

    // Page numbers outside of the document are ignored, never rendered.
    result = printToPdf(source, makeOptions({ -1, 5, 99 }), outputPath);
    QVERIFY(!result.completed);
    QVERIFY(!QFileInfo::exists(outputPath));

    result = printToPdf(source, makeOptions({ 7, 0, -3 }), outputPath);
    QVERIFY(result.completed);
    QCOMPARE(result.pagesPrinted, 1);
}

void PrintExportTest::printRotatedPageAndCropBox()
{
    // MediaBox 400 x 300 rotated by 90 degrees is displayed 300 x 400. The black marker
    // is in the top-left corner of the unrotated page, which becomes the top-right.
    PageSpec rotated;
    rotated.size = QSizeF(400, 300);
    rotated.rotation = pdf::PageRotation::Rotate90;
    const QString sourcePath = path(QStringLiteral("rotated-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ rotated }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    const QImage original = source.render(0, 72);
    QCOMPARE(original.size(), QSize(300, 400));
    QVERIFY(near(original.pixelColor(original.width() - 10, 10), Qt::black, 20));
    QVERIFY(near(original.pixelColor(10, 10), Qt::white, 8));

    const QString outputPath = path(QStringLiteral("rotated-printed.pdf"));
    QVERIFY(printToPdf(source, makeOptions({ 0 }, PDFPrintOrientation::Portrait, PDFPrintScaling::ActualSize), outputPath).completed);
    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    const QImage image = printed.render(0, 72);
    // 300 x 400 page centered on the 595 x 842 sheet: the marker is at the page's top-right.
    const int pageLeft = (image.width() - 300) / 2;
    const int pageTop = (image.height() - 400) / 2;
    QVERIFY(near(image.pixelColor(pageLeft + 300 - 10, pageTop + 10), Qt::black, 40));
    QVERIFY(near(image.pixelColor(pageLeft + 10, pageTop + 10), Qt::white, 12));

    // The crop box clips the content; the page keeps the media box size, as it is shown on screen.
    PageSpec cropped;
    cropped.size = QSizeF(400, 400);
    cropped.marker = false;
    cropped.cropBox = QRectF(100, 100, 200, 200);
    const QString croppedPath = path(QStringLiteral("cropped-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ cropped }), croppedPath));
    LoadedDocument croppedSource;
    QVERIFY(croppedSource.load(croppedPath));
    const QImage croppedImage = croppedSource.render(0, 72);
    QCOMPARE(croppedImage.size(), QSize(400, 400));
    const QRect croppedBounds = nonWhiteBounds(croppedImage);
    QVERIFY2(qAbs(croppedBounds.left() - 100) <= 2 && qAbs(croppedBounds.right() - 299) <= 2 &&
             qAbs(croppedBounds.top() - 100) <= 2 && qAbs(croppedBounds.bottom() - 299) <= 2,
             qPrintable(QStringLiteral("crop bounds %1,%2 %3x%4").arg(croppedBounds.left()).arg(croppedBounds.top()).arg(croppedBounds.width()).arg(croppedBounds.height())));
}

void PrintExportTest::printAnnotationsAndTransparency()
{
    // Page 1: 50% transparent fill. Page 2: red square annotation.
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference first = builder.appendPage(QRectF(0, 0, 400, 400));
    paintPage(&builder, first, PageSpec{ QSizeF(400, 400) }, 0, true);
    const pdf::PDFObjectReference second = builder.appendPage(QRectF(0, 0, 400, 400));
    PageSpec plain;
    plain.size = QSizeF(400, 400);
    plain.marker = false;
    {
        pdf::PDFPageContentStreamBuilder content(&builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(second);
        painter->fillRect(QRectF(0, 0, 400, 400), Qt::white);
        content.end(painter);
    }
    const pdf::PDFObjectReference square = builder.createAnnotationSquare(second, QRectF(100, 100, 120, 120), 3.0, QColor(220, 20, 20), QColor(Qt::black), QStringLiteral("t"), QStringLiteral("s"), QStringLiteral("c"));
    builder.updateAnnotationAppearanceStreams(square);
    const QString sourcePath = path(QStringLiteral("annotation-source.pdf"));
    QVERIFY(writeDocument(builder.build(), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    // Transparency: the red page color blends with the white paper.
    const QColor blended = source.render(0, 72).pixelColor(200, 200);
    const QColor expectedBlend((255 + colorOfPage(0).red()) / 2, (255 + colorOfPage(0).green()) / 2, (255 + colorOfPage(0).blue()) / 2);
    QVERIFY2(near(blended, expectedBlend, 14), qPrintable(describe(blended) + " vs " + describe(expectedBlend)));

    // The annotation is part of the rendered page (it is printable by default).
    const QImage annotated = source.render(1, 72);
    QVERIFY2(near(annotated.pixelColor(160, 400 - 160), QColor(220, 20, 20), 20), qPrintable(describe(annotated.pixelColor(160, 240))));

    // Both survive printing.
    const QString outputPath = path(QStringLiteral("annotation-printed.pdf"));
    QVERIFY(printToPdf(source, makeOptions({ 0, 1 }, PDFPrintOrientation::Portrait, PDFPrintScaling::ActualSize), outputPath).completed);
    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    const QImage printedBlend = printed.render(0, 72);
    QVERIFY2(near(printedBlend.pixelColor(printedBlend.width() / 2, printedBlend.height() / 2), expectedBlend, 14), qPrintable(describe(printedBlend.pixelColor(printedBlend.width() / 2, printedBlend.height() / 2))));
    const QImage printedAnnotation = printed.render(1, 72);
    const int left = (printedAnnotation.width() - 400) / 2;
    const int top = (printedAnnotation.height() - 400) / 2;
    QVERIFY2(near(printedAnnotation.pixelColor(left + 160, top + 240), QColor(220, 20, 20), 25), qPrintable(describe(printedAnnotation.pixelColor(left + 160, top + 240))));

    // Annotations marked as hidden for printing are not printed: switch the display off.
    PDFPageOutputContext context = source.context();
    context.features = context.features & ~pdf::PDFRenderer::Features(pdf::PDFRenderer::DisplayAnnotations);
    PDFPageImageExporter exporter(context, pdf::OCUsage::Export, 1);
    const QImage hidden = exporter.renderPage(1, QSize(400, 400));
    QVERIFY(near(hidden.pixelColor(160, 240), Qt::white, 8));
}

void PrintExportTest::printJpeg2000Scan()
{
    const QString scanPath = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    QVERIFY2(!scanPath.isEmpty(), "missing JPEG2000 fixture");
    LoadedDocument scan;
    QVERIFY(scan.load(scanPath));

    const QString outputPath = path(QStringLiteral("jpx-printed.pdf"));
    const PDFPrintResult result = printToPdf(scan, makeOptions({ 0 }), outputPath);
    QVERIFY2(result.completed, qPrintable(result.errorMessage));
    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    const QImage image = printed.render(0, 72);
    // The scan is fitted to the A4 sheet; the red/blue markers stay in the top corners.
    const QRect bounds = nonWhiteBounds(image);
    QVERIFY2(bounds.height() > 600, qPrintable(QStringLiteral("scan bounds %1x%2").arg(bounds.width()).arg(bounds.height())));
    const QPoint redPoint(bounds.left() + bounds.width() * 60 / 300, bounds.top() + bounds.height() * 32 / 400);
    const QPoint bluePoint(bounds.left() + bounds.width() * 240 / 300, bounds.top() + bounds.height() * 32 / 400);
    QVERIFY2(near(image.pixelColor(redPoint), QColor(200, 30, 30), 40), qPrintable(describe(image.pixelColor(redPoint))));
    QVERIFY2(near(image.pixelColor(bluePoint), QColor(30, 60, 200), 40), qPrintable(describe(image.pixelColor(bluePoint))));

}

void PrintExportTest::printChineseText()
{
    // Chinese text, drawn with a font installed on the system (the offscreen CI platform may have none).
    if (!QFontDatabase::hasFamily(QStringLiteral("Microsoft JhengHei UI")))
    {
        QSKIP("The Microsoft JhengHei UI font is not installed.");
    }

    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 200));
    {
        pdf::PDFPageContentStreamBuilder content(&builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->fillRect(QRectF(0, 0, 400, 200), Qt::white);
        QFont font(QStringLiteral("Microsoft JhengHei UI"));
        font.setPixelSize(48);
        painter->setFont(font);
        painter->setPen(Qt::black);
        painter->translate(0, 200);
        painter->scale(1.0, -1.0);
        painter->drawText(QPointF(20, 100), QString::fromUtf16(u"家庭測試文字"));
        content.end(painter);
    }
    const QString chinesePath = path(QStringLiteral("chinese-source.pdf"));
    QVERIFY(writeDocument(builder.build(), chinesePath));
    LoadedDocument chinese;
    QVERIFY(chinese.load(chinesePath));
    const QImage original = chinese.render(0, 144);
    QVERIFY2(countNonWhite(original) > 1500, qPrintable(QStringLiteral("text pixels %1").arg(countNonWhite(original))));

    const QString chinesePrinted = path(QStringLiteral("chinese-printed.pdf"));
    QVERIFY(printToPdf(chinese, makeOptions({ 0 }, PDFPrintOrientation::Auto, PDFPrintScaling::ActualSize), chinesePrinted).completed);
    LoadedDocument printedChinese;
    QVERIFY(printedChinese.load(chinesePrinted));
    // Same glyphs, same size: the printed text covers the same area as the original page text.
    const qint64 printedPixels = countNonWhite(printedChinese.render(0, 144));
    QVERIFY2(printedPixels > 1500 && qAbs(double(printedPixels) / double(countNonWhite(original)) - 1.0) < 0.15,
             qPrintable(QStringLiteral("printed %1 original %2").arg(printedPixels).arg(countNonWhite(original))));
}

void PrintExportTest::printOnlyRequestedPagesOfLargeDocument()
{
    // Pages other than the printed one are broken on purpose: printing page 250 of
    // a 300 page document must not touch any of them.
    pdf::PDFDocumentBuilder builder;
    std::vector<pdf::PDFObjectReference> pages;
    for (int i = 0; i < 300; ++i)
    {
        pages.push_back(builder.appendPage(QRectF(0, 0, 300, 400)));
    }
    PageSpec spec;
    spec.size = QSizeF(300, 400);
    spec.marker = false;
    paintPage(&builder, pages[250], spec, 2);
    QVERIFY(writeDocument(builder.build(), path(QStringLiteral("large-source.pdf"))));

    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("large-source.pdf"))));
    QCOMPARE(source.pageCount(), 300);

    QElapsedTimer timer;
    timer.start();
    int sheets = 0;
    const PDFPrintResult result = printToPdf(source, makeOptions({ 250 }), path(QStringLiteral("large-printed.pdf")), 1, true, [&sheets](int, int total)
    {
        sheets = total;
        return true;
    });
    const qint64 elapsed = timer.elapsed();
    qInfo() << "PERF print 1 page (#251) of 300 ms:" << elapsed;
    QVERIFY(result.completed);
    QCOMPARE(sheets, 1);
    LoadedDocument printed;
    QVERIFY(printed.load(path(QStringLiteral("large-printed.pdf"))));
    QCOMPARE(printed.pageCount(), 1);
    const QImage image = printed.render(0, 36);
    QVERIFY(near(image.pixelColor(image.width() / 2, image.height() / 2), colorOfPage(2)));
    QVERIFY2(elapsed < 8000, "printing one page must not depend on the document size");
}

void PrintExportTest::printWithWindowsPrinterDriver()
{
    // Runs the real Windows backend (Microsoft Print to PDF) when it is available.
    if (QGuiApplication::platformName() != QLatin1String("windows"))
    {
        QSKIP("The Windows printer backend needs the windows platform plugin (run with QT_QPA_PLATFORM=windows).");
    }
    const QString printerName = QStringLiteral("Microsoft Print to PDF");
    if (!QPrinterInfo::availablePrinterNames().contains(printerName))
    {
        QSKIP("Microsoft Print to PDF is not installed.");
    }

    PageSpec landscape;
    landscape.size = QSizeF(792, 612);
    const QString sourcePath = path(QStringLiteral("driver-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ PageSpec(), landscape, PageSpec() }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    // A file name which does not end with .pdf makes Qt use the driver instead of its own PDF writer.
    const QString outputPath = path(QStringLiteral("driver-output.out"));
    QPrinter printer(QPrinterInfo::printerInfo(printerName), QPrinter::HighResolution);
    printer.setOutputFileName(outputPath);
    const PDFPrintResult result = PDFPageOutput::print(&printer, source.context(), makeOptions({ 0, 1, 2 }));
    QVERIFY2(result.completed, qPrintable(result.errorMessage));

    LoadedDocument printed;
    QVERIFY(printed.load(outputPath));
    QCOMPARE(printed.pageCount(), 3);
    QVERIFY(!PDFPageOutput::isLandscapePage(printed.page(0)));
    QVERIFY(PDFPageOutput::isLandscapePage(printed.page(1)));
    QVERIFY(!PDFPageOutput::isLandscapePage(printed.page(2)));
    const QImage second = printed.render(1, 36);
    QVERIFY(near(second.pixelColor(second.width() / 2, second.height() / 2), colorOfPage(1)));
}

void PrintExportTest::exportPngSizesAndFlattening()
{
    PageSpec spec;
    spec.marker = false;
    const QString sourcePath = path(QStringLiteral("png-source.pdf"));
    QVERIFY(writeDocument(buildDocument({ spec }), sourcePath));
    LoadedDocument source;
    QVERIFY(source.load(sourcePath));

    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
    for (const int dpi : { 72, 150, 300 })
    {
        PDFImageExportTarget target;
        target.pageIndex = 0;
        target.fileName = path(QStringLiteral("page-%1.png").arg(dpi));
        const PDFImageExportResult result = exporter.exportTargets({ target }, PDFImageFormat::Png, dpi, 90);
        QVERIFY(result.isComplete());

        QImageReader reader(target.fileName);
        const QImage image = reader.read();
        QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
        QCOMPARE(image.size(), QSize(qRound(595.0 * dpi / 72.0), qRound(842.0 * dpi / 72.0)));
        QCOMPARE(result.items.front().imageSize, image.size());

        // Page content is colored, the margin is white paper with no transparency.
        QVERIFY(near(image.pixelColor(image.width() / 2, image.height() / 2), colorOfPage(0)));
        const QColor corner = image.pixelColor(2, 2);
        QCOMPARE(corner.alpha(), 255);
        QVERIFY(near(corner, Qt::white, 2));
        QVERIFY(!image.hasAlphaChannel() || image.pixelColor(0, 0).alpha() == 255);

        // The resolution is stored in the file.
        QVERIFY2(qAbs(image.dotsPerMeterX() - int(dpi / 0.0254)) < 40, qPrintable(QString::number(image.dotsPerMeterX())));
    }
}

void PrintExportTest::exportJpegQuality()
{
    PageSpec spec;
    spec.marker = false;
    // Detailed content so quality matters: stripes of several colors
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 400));
    {
        pdf::PDFPageContentStreamBuilder content(&builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        for (int i = 0; i < 80; ++i)
        {
            painter->fillRect(QRectF(i * 5, 0, 3, 400), QColor((i * 37) % 256, (i * 91) % 256, (i * 53) % 256));
        }
        content.end(painter);
    }
    QVERIFY(writeDocument(builder.build(), path(QStringLiteral("jpeg-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("jpeg-source.pdf"))));

    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 1);
    qint64 sizes[2] = {};
    int index = 0;
    for (const int quality : { 20, 95 })
    {
        PDFImageExportTarget target;
        target.pageIndex = 0;
        target.fileName = path(QStringLiteral("quality-%1.jpg").arg(quality));
        QVERIFY(exporter.exportTargets({ target }, PDFImageFormat::Jpeg, 100, quality).isComplete());
        sizes[index++] = QFileInfo(target.fileName).size();

        QImageReader reader(target.fileName);
        QCOMPARE(QString::fromLatin1(reader.format()), QStringLiteral("jpeg"));
        const QImage image = reader.read();
        QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
        QCOMPARE(image.size(), QSize(556, 556));
    }
    QVERIFY2(sizes[0] < sizes[1], qPrintable(QStringLiteral("q20=%1 q95=%2").arg(sizes[0]).arg(sizes[1])));

    // Pages without any painted content are white in JPEG, never black.
    pdf::PDFDocumentBuilder blankBuilder;
    blankBuilder.appendPage(QRectF(0, 0, 200, 200));
    QVERIFY(writeDocument(blankBuilder.build(), path(QStringLiteral("blank-source.pdf"))));
    LoadedDocument blank;
    QVERIFY(blank.load(path(QStringLiteral("blank-source.pdf"))));
    PDFPageImageExporter blankExporter(blank.context(), pdf::OCUsage::Export, 1);
    PDFImageExportTarget blankTarget;
    blankTarget.pageIndex = 0;
    blankTarget.fileName = path(QStringLiteral("blank.jpg"));
    QVERIFY(blankExporter.exportTargets({ blankTarget }, PDFImageFormat::Jpeg, 72, 90).isComplete());
    const QImage blankImage(blankTarget.fileName);
    QVERIFY(near(blankImage.pixelColor(100, 100), Qt::white, 3));
}

void PrintExportTest::exportRotatedAndMixedPageSizes()
{
    PageSpec rotated;
    rotated.size = QSizeF(400, 300);
    rotated.rotation = pdf::PageRotation::Rotate90;
    PageSpec rotated180;
    rotated180.size = QSizeF(200, 300);
    rotated180.rotation = pdf::PageRotation::Rotate180;
    PageSpec rotated270;
    rotated270.size = QSizeF(400, 300);
    rotated270.rotation = pdf::PageRotation::Rotate270;
    PageSpec letter;
    letter.size = QSizeF(792, 612);
    PageSpec a5;
    a5.size = QSizeF(420, 595);
    QVERIFY(writeDocument(buildDocument({ PageSpec(), rotated, rotated180, rotated270, letter, a5 }), path(QStringLiteral("mixed-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("mixed-source.pdf"))));

    const QSize expected[] = { QSize(595, 842), QSize(300, 400), QSize(200, 300), QSize(300, 400), QSize(792, 612), QSize(420, 595) };
    for (int i = 0; i < 6; ++i)
    {
        const QImage image = source.render(i, 72);
        QCOMPARE(image.size(), expected[i]);
        QVERIFY(near(image.pixelColor(image.width() / 2, image.height() / 2), colorOfPage(i)));
    }

    // The black marker marks the top-left corner of the unrotated page.
    const QImage cw = source.render(1, 72);          // 90 degrees clockwise -> top-right
    QVERIFY(near(cw.pixelColor(cw.width() - 10, 10), Qt::black, 25));
    const QImage upsideDown = source.render(2, 72);  // 180 degrees -> bottom-right
    QVERIFY(near(upsideDown.pixelColor(upsideDown.width() - 10, upsideDown.height() - 10), Qt::black, 25));
    const QImage ccw = source.render(3, 72);         // 270 degrees -> bottom-left
    QVERIFY(near(ccw.pixelColor(10, ccw.height() - 10), Qt::black, 25));
    const QImage upright = source.render(0, 72);
    QVERIFY(near(upright.pixelColor(10, 10), Qt::black, 25));

    // Exported files keep those sizes.
    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
    std::vector<PDFImageExportTarget> targets;
    for (int i = 0; i < 6; ++i)
    {
        PDFImageExportTarget target;
        target.pageIndex = i;
        target.fileName = path(QStringLiteral("mixed-%1.png").arg(i));
        targets.push_back(target);
    }
    const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 144, 90);
    QVERIFY(result.isComplete());
    for (int i = 0; i < 6; ++i)
    {
        QCOMPARE(QImage(targets[size_t(i)].fileName).size(), expected[i] * 2);
    }
}

void PrintExportTest::exportBatchNamesAndOrder()
{
    std::vector<PageSpec> specs(12);
    for (PageSpec& spec : specs)
    {
        spec.size = QSizeF(200, 200);
        spec.marker = false;
    }
    QVERIFY(writeDocument(buildDocument(specs), path(QStringLiteral("batch-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("batch-source.pdf"))));

    QDir directory(path(QStringLiteral("batch")));
    QVERIFY(directory.mkpath(QStringLiteral(".")));

    // "1-3,8,10-12" as the dialog does it
    const std::vector<pdf::PDFInteger> pages = { 0, 1, 2, 7, 9, 10, 11 };
    std::vector<PDFImageExportTarget> targets;
    for (const pdf::PDFInteger pageIndex : pages)
    {
        PDFImageExportTarget target;
        target.pageIndex = pageIndex;
        target.fileName = directory.absoluteFilePath(PDFPageImageExporter::getFileName(QStringLiteral("batch"), pageIndex, 12, PDFImageFormat::Png, false));
        targets.push_back(target);
    }

    std::atomic_int processed { 0 };
    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 3);
    QElapsedTimer timer;
    timer.start();
    const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 100, 90, nullptr, &processed);
    qInfo() << "PERF export 7 pages @100 dpi ms:" << timer.elapsed();
    QVERIFY(result.isComplete());
    QCOMPARE(processed.load(), 7);
    QCOMPARE(result.getSucceededCount(), 7);

    // One file per page, named by page number, nothing else is created.
    QStringList files = directory.entryList(QDir::Files, QDir::Name);
    QCOMPARE(files, (QStringList{ "batch_p01.png", "batch_p02.png", "batch_p03.png", "batch_p08.png", "batch_p10.png", "batch_p11.png", "batch_p12.png" }));
    QVERIFY(directory.entryList({ "*.tmp", "*.~*" }, QDir::Files | QDir::Hidden).isEmpty());

    // Each file shows the right page.
    for (size_t i = 0; i < pages.size(); ++i)
    {
        const QImage image(targets[i].fileName);
        QCOMPARE(image.size(), QSize(278, 278));
        QVERIFY2(near(image.pixelColor(139, 139), colorOfPage(int(pages[i]))),
                 qPrintable(QStringLiteral("%1 shows %2").arg(files[int(i)], describe(image.pixelColor(139, 139)))));
        // The result keeps the requested order.
        QCOMPARE(result.items[i].target.pageIndex, pages[i]);
    }
}

void PrintExportTest::exportReportsPartialFailure()
{
    std::vector<PageSpec> specs(5);
    QVERIFY(writeDocument(buildDocument(specs), path(QStringLiteral("partial-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("partial-source.pdf"))));

    QDir directory(path(QStringLiteral("partial")));
    QVERIFY(directory.mkpath(QStringLiteral(".")));

    std::vector<PDFImageExportTarget> targets;
    for (int i = 0; i < 5; ++i)
    {
        PDFImageExportTarget target;
        target.pageIndex = i;
        target.fileName = directory.absoluteFilePath(QStringLiteral("p%1.png").arg(i + 1));
        targets.push_back(target);
    }
    // Page 3 cannot be written: its folder does not exist. Page 5 does not exist in a 4 page request.
    targets[2].fileName = directory.absoluteFilePath(QStringLiteral("missing-folder/p3.png"));
    targets[4].pageIndex = 99;

    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
    const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 50, 90);
    QVERIFY2(!result.isComplete(), "partial success must not be reported as complete");
    QVERIFY(!result.cancelled);
    QCOMPARE(result.getSucceededCount(), 3);
    QCOMPARE(result.getFailedCount(), 2);
    QCOMPARE(result.items[2].status, PDFImageExportItemResult::Status::Failed);
    QVERIFY(!result.items[2].errorMessage.isEmpty());
    QCOMPARE(result.items[4].status, PDFImageExportItemResult::Status::Failed);
    QVERIFY(!result.items[4].errorMessage.isEmpty());
    QCOMPARE(directory.entryList(QDir::Files, QDir::Name), (QStringList{ "p1.png", "p2.png", "p4.png" }));

    // A write which fails must not destroy an existing file or leave a partial one.
    const QString existing = directory.absoluteFilePath(QStringLiteral("p1.png"));
    const qint64 sizeBefore = QFileInfo(existing).size();
    QVERIFY(QDir().mkpath(directory.absoluteFilePath(QStringLiteral("blocked.png"))));   // a folder named like the target
    PDFImageExportTarget blocked;
    blocked.pageIndex = 0;
    blocked.fileName = directory.absoluteFilePath(QStringLiteral("blocked.png"));
    const PDFImageExportResult blockedResult = exporter.exportTargets({ blocked }, PDFImageFormat::Png, 50, 90);
    QVERIFY(!blockedResult.isComplete());
    QCOMPARE(QFileInfo(existing).size(), sizeBefore);
}

void PrintExportTest::exportCancel()
{
    constexpr int PAGE_COUNT = 120;
    std::vector<PageSpec> specs(PAGE_COUNT);
    for (PageSpec& spec : specs)
    {
        spec.size = QSizeF(300, 400);
    }
    QVERIFY(writeDocument(buildDocument(specs), path(QStringLiteral("cancel-export-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("cancel-export-source.pdf"))));

    QDir directory(path(QStringLiteral("cancel-export")));
    QVERIFY(directory.mkpath(QStringLiteral(".")));
    std::vector<PDFImageExportTarget> targets;
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        PDFImageExportTarget target;
        target.pageIndex = i;
        target.fileName = directory.absoluteFilePath(PDFPageImageExporter::getFileName(QStringLiteral("c"), i, PAGE_COUNT, PDFImageFormat::Png, false));
        targets.push_back(target);
    }

    // Cancel before anything started: nothing is rendered or written.
    {
        std::atomic_bool cancelled { true };
        PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
        const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 72, 90, &cancelled);
        QVERIFY(result.cancelled);
        QVERIFY(!result.isComplete());
        QCOMPARE(result.getNotProcessedCount(), PAGE_COUNT);
        QVERIFY(directory.entryList(QDir::Files).isEmpty());
    }

    // Cancel while running: finished files are reported as succeeded, the rest as not processed.
    {
        std::atomic_bool cancelled { false };
        std::atomic_int processed { 0 };
        PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
        std::thread canceller([&]()
        {
            while (processed.load() < 3)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            cancelled = true;
        });
        const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 72, 90, &cancelled, &processed);
        canceller.join();
        QVERIFY(result.cancelled);
        QVERIFY(!result.isComplete());
        const int succeeded = result.getSucceededCount();
        QVERIFY(succeeded >= 3 && succeeded < PAGE_COUNT);
        QCOMPARE(result.getFailedCount(), 0);
        QCOMPARE(succeeded + result.getNotProcessedCount(), PAGE_COUNT);
        QCOMPARE(directory.entryList(QDir::Files).size(), succeeded);
        for (const PDFImageExportItemResult& item : result.items)
        {
            QCOMPARE(QFileInfo::exists(item.target.fileName), item.status == PDFImageExportItemResult::Status::Succeeded);
        }
    }
}

void PrintExportTest::exportRegion()
{
    // A blue square inside the page; the region around it is exported.
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 300));
    {
        pdf::PDFPageContentStreamBuilder content(&builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->fillRect(QRectF(0, 0, 400, 300), Qt::white);
        painter->fillRect(QRectF(150, 100, 100, 50), QColor(30, 60, 200));   // PDF coordinates (origin bottom-left)
        content.end(painter);
    }
    builder.setPageRotation(page, pdf::PageRotation::None);
    QVERIFY(writeDocument(builder.build(), path(QStringLiteral("region-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("region-source.pdf"))));

    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 1);
    PDFImageExportTarget target;
    target.pageIndex = 0;
    target.region = QRectF(140, 90, 120, 70);       // 10 point margin around the square
    target.fileName = path(QStringLiteral("region.png"));
    const PDFImageExportResult result = exporter.exportTargets({ target }, PDFImageFormat::Png, 144, 90);
    QVERIFY2(result.isComplete(), qPrintable(result.items.front().errorMessage));
    const QImage image(target.fileName);
    QCOMPARE(image.size(), QSize(240, 140));
    QVERIFY(near(image.pixelColor(120, 70), QColor(30, 60, 200)));
    QVERIFY(near(image.pixelColor(5, 5), Qt::white, 4));
    QVERIFY(near(image.pixelColor(234, 134), Qt::white, 4));
    // The square starts 10 points (20 pixels) inside the region.
    QVERIFY(near(image.pixelColor(25, 25), QColor(30, 60, 200)));
    QVERIFY(near(image.pixelColor(15, 70), Qt::white, 4));

    // A region partly outside of the page is clipped, a region outside of the page fails.
    PDFImageExportTarget clipped = target;
    clipped.region = QRectF(300, 200, 300, 300);
    clipped.fileName = path(QStringLiteral("region-clipped.png"));
    QVERIFY(exporter.exportTargets({ clipped }, PDFImageFormat::Png, 72, 90).isComplete());
    QCOMPARE(QImage(clipped.fileName).size(), QSize(100, 100));
    PDFImageExportTarget outside = target;
    outside.region = QRectF(1000, 1000, 50, 50);
    outside.fileName = path(QStringLiteral("region-outside.png"));
    const PDFImageExportResult outsideResult = exporter.exportTargets({ outside }, PDFImageFormat::Png, 72, 90);
    QVERIFY(!outsideResult.isComplete());
    QVERIFY(!QFileInfo::exists(outside.fileName));

    // On a rotated page the same page-space region is found in the rotated image.
    pdf::PDFDocumentBuilder rotatedBuilder;
    const pdf::PDFObjectReference rotatedPage = rotatedBuilder.appendPage(QRectF(0, 0, 400, 300));
    {
        pdf::PDFPageContentStreamBuilder content(&rotatedBuilder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(rotatedPage);
        painter->fillRect(QRectF(0, 0, 400, 300), Qt::white);
        painter->fillRect(QRectF(150, 100, 100, 50), QColor(30, 60, 200));
        content.end(painter);
    }
    rotatedBuilder.setPageRotation(rotatedPage, pdf::PageRotation::Rotate90);
    QVERIFY(writeDocument(rotatedBuilder.build(), path(QStringLiteral("region-rotated-source.pdf"))));
    LoadedDocument rotated;
    QVERIFY(rotated.load(path(QStringLiteral("region-rotated-source.pdf"))));
    PDFPageImageExporter rotatedExporter(rotated.context(), pdf::OCUsage::Export, 1);
    PDFImageExportTarget rotatedTarget = target;
    rotatedTarget.fileName = path(QStringLiteral("region-rotated.png"));
    QVERIFY(rotatedExporter.exportTargets({ rotatedTarget }, PDFImageFormat::Png, 144, 90).isComplete());
    const QImage rotatedImage(rotatedTarget.fileName);
    QCOMPARE(rotatedImage.size(), QSize(140, 240));    // width and height swapped by the rotation
    QVERIFY(near(rotatedImage.pixelColor(70, 120), QColor(30, 60, 200)));
}

void PrintExportTest::exportTransparencyAndAnnotation()
{
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 300));
    {
        pdf::PDFPageContentStreamBuilder content(&builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = content.begin(page);
        painter->fillRect(QRectF(0, 0, 400, 300), Qt::white);
        painter->setOpacity(0.5);
        painter->fillRect(QRectF(20, 150, 120, 100), QColor(0, 0, 255));     // 50% blue over white
        painter->setOpacity(1.0);
        content.end(painter);
    }
    const pdf::PDFObjectReference square = builder.createAnnotationSquare(page, QRectF(200, 150, 100, 100), 2.0, QColor(220, 20, 20), QColor(Qt::black), QStringLiteral("t"), QStringLiteral("s"), QStringLiteral("c"));
    builder.updateAnnotationAppearanceStreams(square);

    QVERIFY(writeDocument(builder.build(), path(QStringLiteral("annotations-source.pdf"))));

    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("annotations-source.pdf"))));
    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 1);
    PDFImageExportTarget target;
    target.pageIndex = 0;
    target.fileName = path(QStringLiteral("annotations.png"));
    QVERIFY(exporter.exportTargets({ target }, PDFImageFormat::Png, 72, 90).isComplete());
    const QImage image(target.fileName);
    QCOMPARE(image.size(), QSize(400, 300));

    // 50% blue over the white paper
    QVERIFY2(near(image.pixelColor(80, 300 - 200), QColor(127, 127, 255), 14), qPrintable(describe(image.pixelColor(80, 100))));
    // Square annotation appearance
    QVERIFY2(near(image.pixelColor(250, 300 - 200), QColor(220, 20, 20), 20), qPrintable(describe(image.pixelColor(250, 100))));
}

void PrintExportTest::exportJpeg2000Scan()
{
    const QString scanPath = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    QVERIFY2(!scanPath.isEmpty(), "missing JPEG2000 fixture");
    LoadedDocument scan;
    QVERIFY(scan.load(scanPath));
    QCOMPARE(scan.pageCount(), 1);

    PDFPageImageExporter exporter(scan.context(), pdf::OCUsage::Export, 1);
    for (const PDFImageFormat format : { PDFImageFormat::Png, PDFImageFormat::Jpeg })
    {
        PDFImageExportTarget target;
        target.pageIndex = 0;
        target.fileName = path(QStringLiteral("scan.%1").arg(PDFPageImageExporter::getFileExtension(format)));
        QElapsedTimer timer;
        timer.start();
        QVERIFY(exporter.exportTargets({ target }, format, 72, 90).isComplete());
        qInfo() << "PERF JPEG2000 300x400 page export ms:" << timer.elapsed();
        const QImage image(target.fileName);
        QCOMPARE(image.size(), QSize(300, 400));
        QVERIFY2(near(image.pixelColor(60, 32), QColor(200, 30, 30), 40), qPrintable(describe(image.pixelColor(60, 32))));
        QVERIFY2(near(image.pixelColor(240, 32), QColor(30, 60, 200), 40), qPrintable(describe(image.pixelColor(240, 32))));
        QVERIFY2(near(image.pixelColor(150, 369), QColor(30, 150, 70), 40), qPrintable(describe(image.pixelColor(150, 369))));
        QVERIFY2(near(image.pixelColor(10, 200), QColor(236, 232, 220), 24), qPrintable(describe(image.pixelColor(10, 200))));
    }
}

void PrintExportTest::exportOnlyRequestedPagesOfLargeDocument()
{
    pdf::PDFDocumentBuilder builder;
    std::vector<pdf::PDFObjectReference> pages;
    for (int i = 0; i < 400; ++i)
    {
        pages.push_back(builder.appendPage(QRectF(0, 0, 595, 842)));
    }
    PageSpec spec;
    spec.marker = false;
    paintPage(&builder, pages[320], spec, 1);
    paintPage(&builder, pages[321], spec, 3);
    QVERIFY(writeDocument(builder.build(), path(QStringLiteral("large-export-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("large-export-source.pdf"))));

    QElapsedTimer timer;
    timer.start();
    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 2);
    std::vector<PDFImageExportTarget> targets;
    for (const int pageIndex : { 320, 321 })
    {
        PDFImageExportTarget target;
        target.pageIndex = pageIndex;
        target.fileName = path(QStringLiteral("large-%1.png").arg(pageIndex + 1));
        targets.push_back(target);
    }
    const PDFImageExportResult result = exporter.exportTargets(targets, PDFImageFormat::Png, 100, 90);
    const qint64 elapsed = timer.elapsed();
    qInfo() << "PERF export 2 pages (#321, #322) of 400 ms:" << elapsed;
    QVERIFY(result.isComplete());
    QVERIFY(near(QImage(targets[0].fileName).pixelColor(300, 400), colorOfPage(1)));
    QVERIFY(near(QImage(targets[1].fileName).pixelColor(300, 400), colorOfPage(3)));
    QVERIFY2(elapsed < 8000, "exporting two pages must not depend on the document size");
}

void PrintExportTest::exportHighDpiAndLimits()
{
    PageSpec spec;
    spec.marker = false;
    QVERIFY(writeDocument(buildDocument({ spec }), path(QStringLiteral("dpi-source.pdf"))));
    LoadedDocument source;
    QVERIFY(source.load(path(QStringLiteral("dpi-source.pdf"))));

    PDFPageImageExporter exporter(source.context(), pdf::OCUsage::Export, 1);
    PDFImageExportTarget target;
    target.pageIndex = 0;
    target.fileName = path(QStringLiteral("dpi-600.png"));
    QElapsedTimer timer;
    timer.start();
    QVERIFY(exporter.exportTargets({ target }, PDFImageFormat::Png, 600, 90).isComplete());
    qInfo() << "PERF 1 page A4 PNG @600 dpi ms:" << timer.elapsed();
    QImageReader reader(target.fileName);
    QCOMPARE(reader.size(), QSize(4958, 7017));

    // Too large images are refused before any memory is allocated.
    QString error;
    const QImage tooLarge = exporter.renderPage(0, QSize(20000, 20000), &error);
    QVERIFY(tooLarge.isNull());
    QVERIFY(!error.isEmpty());
    PDFImageExportTarget huge;
    huge.pageIndex = 0;
    huge.fileName = path(QStringLiteral("huge.png"));
    const PDFImageExportResult hugeResult = exporter.exportTargets({ huge }, PDFImageFormat::Png, PDFPageImageExporter::getMaxDpi() * 4, 90);
    QVERIFY(!hugeResult.isComplete());
    QVERIFY(!QFileInfo::exists(huge.fileName));

    QVERIFY(PDFPageImageExporter::getRecommendedRasterizerCount(QSize(4958, 7017)) >= 1);
    QVERIFY(PDFPageImageExporter::getRecommendedRasterizerCount(QSize(10000, 10000)) <= PDFPageImageExporter::getRecommendedRasterizerCount(QSize(100, 100)));
}

QTEST_MAIN(PrintExportTest)
#include "tst_printexporttest.moc"
