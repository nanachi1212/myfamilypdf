// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfdocumentbuilder.h"
#include "pdfdocumentdecoration.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QFont>
#include <QGuiApplication>
#include <QtTest>

class DocumentEditTest : public QObject
{
    Q_OBJECT

private slots:
    void decorationsRespectPageSelectionAndLayerOrder();
    void imageBackgroundRoundTrips();
    void pageGeometryAndRotationRoundTrip();
};

namespace
{

std::vector<pdf::PDFObjectReference> getPageContentReferences(
        const pdf::PDFDocument& document,
        pdf::PDFInteger pageIndex)
{
    const pdf::PDFPage* page = document.getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return {};
    }

    const pdf::PDFObjectStorage* storage = &document.getStorage();
    const pdf::PDFDictionary* pageDictionary =
        storage->getDictionaryFromObject(
            storage->getObjectByReference(page->getPageReference()));
    if (!pageDictionary)
    {
        return {};
    }

    const pdf::PDFObject contents = pageDictionary->get("Contents");
    const pdf::PDFObject resolvedContents = storage->getObject(contents);
    if (resolvedContents.isStream() && contents.isReference())
    {
        return {contents.getReference()};
    }

    std::vector<pdf::PDFObjectReference> references;
    if (resolvedContents.isArray())
    {
        const pdf::PDFArray* array = resolvedContents.getArray();
        references.reserve(array->getCount());
        for (const pdf::PDFObject& item : *array)
        {
            if (item.isReference())
            {
                references.push_back(item.getReference());
            }
        }
    }
    return references;
}

} // namespace

void DocumentEditTest::decorationsRespectPageSelectionAndLayerOrder()
{
    QGuiApplication::setFont(QFont(QStringLiteral("Arial")));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    for (int pageIndex = 0; pageIndex < 3; ++pageIndex)
    {
        const pdf::PDFObjectReference page =
            builder.appendPage(QRectF(0.0, 0.0, 300.0, 400.0));
        pdf::PDFPageContentStreamBuilder contentBuilder(
            &builder,
            pdf::PDFContentStreamBuilder::CoordinateSystem::PDF,
            pdf::PDFPageContentStreamBuilder::Mode::Replace);
        QPainter* painter = contentBuilder.begin(page);
        QVERIFY(painter);
        painter->fillRect(QRectF(20.0, 20.0, 20.0, 20.0), Qt::black);
        contentBuilder.end(painter);
    }

    pdf::PDFDocument document = builder.build();
    std::array<pdf::PDFObjectReference, 3> originalContents;
    for (pdf::PDFInteger pageIndex = 0; pageIndex < 3; ++pageIndex)
    {
        const auto references = getPageContentReferences(document, pageIndex);
        QCOMPARE(references.size(), std::size_t(1));
        originalContents[pageIndex] = references.front();
    }

    pdf::PDFDocumentDecorationSettings background;
    background.kind =
        pdf::PDFDocumentDecorationSettings::Kind::ColorBackground;
    background.pageRange = QStringLiteral("-");
    background.pageSubset =
        pdf::PDFPageGeometrySettings::PageSubset::OddPages;
    background.color = QColor(245, 240, 220);
    background.opacity = 1.0;
    background.foreground = false;

    pdf::PDFModifiedDocument::ModificationFlags flags;
    pdf::PDFOperationResult result =
        pdf::PDFDocumentDecoration::apply(&document, background, &flags);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));
    QVERIFY(flags.testFlag(pdf::PDFModifiedDocument::PageContents));

    for (pdf::PDFInteger pageIndex = 0; pageIndex < 3; ++pageIndex)
    {
        const auto references = getPageContentReferences(document, pageIndex);
        if ((pageIndex % 2) == 0)
        {
            QCOMPARE(references.size(), std::size_t(2));
            QCOMPARE(references.back(), originalContents[pageIndex]);
        }
        else
        {
            QCOMPARE(references.size(), std::size_t(1));
            QCOMPARE(references.front(), originalContents[pageIndex]);
        }
    }

    pdf::PDFDocumentDecorationSettings watermark;
    watermark.kind =
        pdf::PDFDocumentDecorationSettings::Kind::TextWatermark;
    watermark.pageRange = QStringLiteral("2");
    watermark.text = QString::fromUtf16(u"\u5BB6\u5EAD\u6E2C\u8A66");
    watermark.fontFamily = QStringLiteral("Microsoft JhengHei UI");
    watermark.color = QColor(180, 0, 0);
    watermark.opacity = 0.35;
    watermark.angleDegrees = -35.0;
    watermark.fontPointSize = 36.0;
    watermark.foreground = true;

    result = pdf::PDFDocumentDecoration::apply(&document, watermark, &flags);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));
    QVERIFY(flags.testFlag(pdf::PDFModifiedDocument::PageContents));

    const auto secondPageReferences = getPageContentReferences(document, 1);
    QCOMPARE(secondPageReferences.size(), std::size_t(2));
    QCOMPARE(secondPageReferences.front(), originalContents[1]);

    QString outputPath =
        qEnvironmentVariable("FAMILYPDF_DOCUMENT_EDIT_FIXTURE");
    if (outputPath.isEmpty())
    {
        outputPath = directory.filePath(QStringLiteral("decorated.pdf"));
    }
    else
    {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    }
    pdf::PDFDocumentWriter writer(nullptr);
    result = writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    const pdf::PDFDocument restored = reader.readFromFile(outputPath);
    QVERIFY2(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK,
             qPrintable(reader.getErrorMessage()));
    QCOMPARE(getPageContentReferences(restored, 0).size(), std::size_t(2));
    QCOMPARE(getPageContentReferences(restored, 1).size(), std::size_t(2));
    QCOMPARE(getPageContentReferences(restored, 2).size(), std::size_t(2));
}

void DocumentEditTest::imageBackgroundRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0.0, 0.0, 300.0, 400.0));
    pdf::PDFDocument document = builder.build();

    QImage image(80, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(20, 90, 180));

    pdf::PDFDocumentDecorationSettings background;
    background.kind =
        pdf::PDFDocumentDecorationSettings::Kind::ImageBackground;
    background.image = image;
    background.imageScaleMode =
        pdf::PDFDocumentDecorationSettings::ImageScaleMode::Fill;
    background.opacity = 0.8;
    background.foreground = false;

    pdf::PDFModifiedDocument::ModificationFlags flags;
    pdf::PDFOperationResult result =
        pdf::PDFDocumentDecoration::apply(&document, background, &flags);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));
    QVERIFY(flags.testFlag(pdf::PDFModifiedDocument::PageContents));
    QCOMPARE(getPageContentReferences(document, 0).size(), std::size_t(1));

    const QString outputPath =
        directory.filePath(QStringLiteral("image-background.pdf"));
    pdf::PDFDocumentWriter writer(nullptr);
    result = writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    const pdf::PDFDocument restored = reader.readFromFile(outputPath);
    QVERIFY2(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK,
             qPrintable(reader.getErrorMessage()));
    QCOMPARE(getPageContentReferences(restored, 0).size(), std::size_t(1));
}

void DocumentEditTest::pageGeometryAndRotationRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0.0, 0.0, 300.0, 400.0));
    builder.appendPage(QRectF(0.0, 0.0, 300.0, 400.0));
    pdf::PDFDocument document = builder.build();

    pdf::PDFPageGeometrySettings settings;
    settings.pageRange = QStringLiteral("2");
    settings.useTargetPageSize = true;
    settings.targetPageSizeMM = QSizeF(148.0, 210.0);
    settings.applyMediaBox = true;
    settings.applyCropBox = true;
    settings.rotationQuarterTurns = 1;

    pdf::PDFModifiedDocument::ModificationFlags flags;
    pdf::PDFOperationResult result =
        pdf::PDFPageGeometry::apply(&document, settings, &flags);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));

    QString outputPath =
        qEnvironmentVariable("FAMILYPDF_PAGE_GEOMETRY_FIXTURE");
    if (outputPath.isEmpty())
    {
        outputPath =
            directory.filePath(QStringLiteral("page-geometry.pdf"));
    }
    else
    {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    }
    pdf::PDFDocumentWriter writer(nullptr);
    result = writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(result), qPrintable(result.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    const pdf::PDFDocument restored = reader.readFromFile(outputPath);
    QVERIFY2(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK,
             qPrintable(reader.getErrorMessage()));

    const pdf::PDFPage* firstPage = restored.getCatalog()->getPage(0);
    const pdf::PDFPage* secondPage = restored.getCatalog()->getPage(1);
    QVERIFY(firstPage);
    QVERIFY(secondPage);
    QCOMPARE(firstPage->getPageRotation(), pdf::PageRotation::None);
    QCOMPARE(secondPage->getPageRotation(), pdf::PageRotation::Rotate90);
    QVERIFY(qAbs(secondPage->getMediaBox().size().width() -
                 148.0 * pdf::PDF_MM_TO_POINT) < 0.0001);
    QVERIFY(qAbs(secondPage->getMediaBox().size().height() -
                 210.0 * pdf::PDF_MM_TO_POINT) < 0.0001);
    QCOMPARE(secondPage->getCropBox(), secondPage->getMediaBox());
}

QTEST_MAIN(DocumentEditTest)

#include "tst_documentedittest.moc"
