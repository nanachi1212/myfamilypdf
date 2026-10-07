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

#include "pdfconstants.h"
#include "pdfdocumentbuilder.h"
#include "pdfimage.h"
#include "pdfimageconversion.h"
#include "pdfimageoptimizer.h"
#include "pdfjpegimage.h"
#include "pdfcatalog.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfconstants.h"

#include <QtTest>
#include <QColor>
#include <QImage>
#include <QBuffer>

#include <memory>
#include <random>

class ImageOptimizerTest : public QObject
{
    Q_OBJECT

private slots:
    void test_bitonal_conversion_otsu();
    void test_bitonal_conversion_manual();
    void test_image_analysis_classification();
    void test_optimizer_keeps_original_if_larger();
    void test_optimizer_skips_disabled_override();
    void test_optimizer_preserve_keeps_bitonal_encoding();
    void test_optimizer_preserves_smask();
    void test_optimizer_reduces_size_for_photo();
    void test_jpeg_passthrough_roundtrip_data();
    void test_jpeg_passthrough_roundtrip();
    void test_jpeg_header_rejections();
    void test_jpeg_icc_profile_rejected();
    void test_jpeg_header_metadata();

private:
    static QImage createLineArtImage(int size);
    static QImage createTextScanImage(int size);
    static QImage createPhotoImage(int size);
    static QImage createAlphaImage(int size);
    static QImage extractAlphaMask(const QImage& image);
    static pdf::PDFDocument createDocumentWithImage(const QImage& image,
                                                    bool addSoftMask,
                                                    pdf::PDFImage::ImageCompression compression);
};

QImage ImageOptimizerTest::createLineArtImage(int size)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::white);

    for (int x = 4; x < size - 4; ++x)
    {
        image.setPixel(x, 4, qRgb(0, 0, 0));
        image.setPixel(x, size - 5, qRgb(0, 0, 0));
    }

    for (int y = 4; y < size - 4; ++y)
    {
        image.setPixel(4, y, qRgb(0, 0, 0));
        image.setPixel(size - 5, y, qRgb(0, 0, 0));
    }

    for (int i = 8; i < size - 8; i += 4)
    {
        image.setPixel(i, i, qRgb(0, 0, 0));
        image.setPixel(size - 1 - i, i, qRgb(0, 0, 0));
    }

    for (int x = 8; x < size - 8; x += 6)
    {
        image.setPixel(x, size / 2, qRgb(200, 0, 0));
    }

    return image;
}

QImage ImageOptimizerTest::createTextScanImage(int size)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::white);

    for (int y = 10; y < size - 10; y += 8)
    {
        for (int x = 8; x < size - 8; ++x)
        {
            image.setPixel(x, y, qRgb(0, 0, 0));
        }
    }

    return image;
}

QImage ImageOptimizerTest::createPhotoImage(int size)
{
    QImage image(size, size, QImage::Format_ARGB32);

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 255);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const int r = dist(rng);
            const int g = dist(rng);
            const int b = dist(rng);
            image.setPixel(x, y, qRgb(r, g, b));
        }
    }

    return image;
}

QImage ImageOptimizerTest::createAlphaImage(int size)
{
    QImage image(size, size, QImage::Format_ARGB32);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const int alpha = static_cast<int>((255.0 * x) / qMax(1, size - 1));
            image.setPixel(x, y, qRgba(30, 120, 200, alpha));
        }
    }

    return image;
}

QImage ImageOptimizerTest::extractAlphaMask(const QImage& image)
{
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    QImage mask(rgba.size(), QImage::Format_Grayscale8);

    for (int y = 0; y < rgba.height(); ++y)
    {
        const uchar* src = rgba.constScanLine(y);
        uchar* dst = mask.scanLine(y);
        for (int x = 0; x < rgba.width(); ++x)
        {
            dst[x] = src[3];
            src += 4;
        }
    }

    return mask;
}

pdf::PDFDocument ImageOptimizerTest::createDocumentWithImage(const QImage& image,
                                                             bool addSoftMask,
                                                             pdf::PDFImage::ImageCompression compression)
{
    pdf::PDFDocumentBuilder builder;
    pdf::PDFObjectReference pageRef = builder.appendPage(QRectF(0, 0, 200, 200));

    pdf::PDFImage::ImageEncodeOptions options;
    options.compression = compression;
    options.colorMode = pdf::PDFImage::ImageColorMode::Preserve;
    options.enablePngPredictor = true;
    options.alphaHandling = addSoftMask ? pdf::PDFImage::AlphaHandling::DropAlphaPreserveColors
                                        : pdf::PDFImage::AlphaHandling::FlattenToWhite;

    pdf::PDFStream imageStream = pdf::PDFImage::createStreamFromImage(image, options);

    if (addSoftMask)
    {
        QImage maskImage = extractAlphaMask(image);
        pdf::PDFImage::ImageEncodeOptions maskOptions;
        maskOptions.compression = pdf::PDFImage::ImageCompression::Flate;
        maskOptions.colorMode = pdf::PDFImage::ImageColorMode::Grayscale;
        maskOptions.enablePngPredictor = true;
        maskOptions.alphaHandling = pdf::PDFImage::AlphaHandling::FlattenToWhite;

        pdf::PDFStream maskStream = pdf::PDFImage::createStreamFromImage(maskImage, maskOptions);
        pdf::PDFObjectReference maskRef = builder.addObject(
            pdf::PDFObject::createStream(std::make_shared<pdf::PDFStream>(maskStream)));

        pdf::PDFDictionary dict = *imageStream.getDictionary();
        dict.setEntry(pdf::PDFInplaceOrMemoryString("SMask"), pdf::PDFObject::createReference(maskRef));
        const QByteArray* content = imageStream.getContent();
        QByteArray contentDereferenced = content ? *content : QByteArray();
        imageStream = pdf::PDFStream(std::move(dict), std::move(contentDereferenced));
    }

    pdf::PDFObjectReference imageRef = builder.addObject(
        pdf::PDFObject::createStream(std::make_shared<pdf::PDFStream>(imageStream)));

    QByteArray content("q 200 0 0 200 0 0 cm /Im1 Do Q");
    pdf::PDFDictionary contentDict;
    contentDict.addEntry(pdf::PDFInplaceOrMemoryString(pdf::PDF_STREAM_DICT_LENGTH),
                         pdf::PDFObject::createInteger(content.size()));
    pdf::PDFStream contentStream(std::move(contentDict), std::move(content));
    pdf::PDFObjectReference contentRef = builder.addObject(
        pdf::PDFObject::createStream(std::make_shared<pdf::PDFStream>(contentStream)));

    pdf::PDFDictionary xObject;
    xObject.addEntry(pdf::PDFInplaceOrMemoryString("Im1"), pdf::PDFObject::createReference(imageRef));

    pdf::PDFDictionary resources;
    resources.addEntry(pdf::PDFInplaceOrMemoryString("XObject"),
                       pdf::PDFObject::createDictionary(std::make_shared<pdf::PDFDictionary>(std::move(xObject))));

    pdf::PDFDictionary pageUpdate;
    pageUpdate.addEntry(pdf::PDFInplaceOrMemoryString("Resources"),
                        pdf::PDFObject::createDictionary(std::make_shared<pdf::PDFDictionary>(std::move(resources))));
    pageUpdate.addEntry(pdf::PDFInplaceOrMemoryString("Contents"), pdf::PDFObject::createReference(contentRef));

    builder.mergeTo(pageRef, pdf::PDFObject::createDictionary(std::make_shared<pdf::PDFDictionary>(std::move(pageUpdate))));

    return builder.build();
}

void ImageOptimizerTest::test_bitonal_conversion_otsu()
{
    QImage image(64, 64, QImage::Format_Grayscale8);
    for (int y = 0; y < image.height(); ++y)
    {
        uchar* row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x)
        {
            row[x] = static_cast<uchar>((255 * x) / qMax(1, image.width() - 1));
        }
    }

    pdf::PDFImageConversion conversion;
    conversion.setImage(image);
    conversion.setConversionMethod(pdf::PDFImageConversion::ConversionMethod::Automatic);

    QVERIFY(conversion.convert());
    QImage result = conversion.getConvertedImage();
    QVERIFY(!result.isNull());
    QCOMPARE(result.format(), QImage::Format_Mono);

    const int threshold = conversion.getThreshold();
    QVERIFY(threshold >= 0 && threshold <= 255);
}

void ImageOptimizerTest::test_bitonal_conversion_manual()
{
    QImage image(2, 1, QImage::Format_Grayscale8);
    image.setPixelColor(0, 0, QColor(50, 50, 50));
    image.setPixelColor(1, 0, QColor(250, 250, 250));

    pdf::PDFImageConversion conversion;
    conversion.setImage(image);
    conversion.setConversionMethod(pdf::PDFImageConversion::ConversionMethod::Manual);
    conversion.setThreshold(200);

    QVERIFY(conversion.convert());
    QImage result = conversion.getConvertedImage();
    QCOMPARE(result.format(), QImage::Format_Mono);
    QCOMPARE(result.pixelIndex(0, 0), 0u);
    QCOMPARE(result.pixelIndex(1, 0), 1u);
}

void ImageOptimizerTest::test_image_analysis_classification()
{
    QImage lineArt = createLineArtImage(64);
    QImage textScan = createTextScanImage(64);
    QImage photo = createPhotoImage(64);

    pdf::PDFImageOptimizer::ImageAnalysis lineAnalysis = pdf::PDFImageOptimizer::analyzeImage(lineArt);
    pdf::PDFImageOptimizer::ImageAnalysis textAnalysis = pdf::PDFImageOptimizer::analyzeImage(textScan);
    pdf::PDFImageOptimizer::ImageAnalysis photoAnalysis = pdf::PDFImageOptimizer::analyzeImage(photo);

    QCOMPARE(lineAnalysis.kind, pdf::PDFImageOptimizer::ImageAnalysis::Kind::LineArt);
    QCOMPARE(textAnalysis.kind, pdf::PDFImageOptimizer::ImageAnalysis::Kind::TextScan);
    QCOMPARE(photoAnalysis.kind, pdf::PDFImageOptimizer::ImageAnalysis::Kind::Photo);
}

void ImageOptimizerTest::test_optimizer_keeps_original_if_larger()
{
    QImage lineArt = createLineArtImage(256);
    pdf::PDFDocument document = createDocumentWithImage(lineArt, false, pdf::PDFImage::ImageCompression::RunLength);

    std::vector<pdf::PDFImageOptimizer::ImageInfo> infos = pdf::PDFImageOptimizer::collectImageInfos(&document);
    QCOMPARE(infos.size(), 1u);

    pdf::PDFImageOptimizer::Settings settings = pdf::PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    settings.autoMode = false;
    settings.colorMode = pdf::PDFImageOptimizer::ColorMode::Preserve;
    settings.goal = pdf::PDFImageOptimizer::OptimizationGoal::PreferQuality;
    settings.keepOriginalIfLarger = true;
    settings.preserveTransparency = true;
    settings.colorProfile.algorithm = pdf::PDFImageOptimizer::CompressionAlgorithm::JPEG;
    settings.colorProfile.targetDpi = 0;
    settings.colorProfile.jpegQuality = 100;

    pdf::PDFImageOptimizer optimizer;
    pdf::PDFDocument optimized = optimizer.optimize(&document, settings);

    const pdf::PDFObject& imageObject = optimized.getObjectByReference(infos[0].reference);
    QVERIFY(imageObject.isStream());

    const pdf::PDFStream* stream = imageObject.getStream();
    const pdf::PDFDictionary* dictionary = stream->getDictionary();
    QVERIFY(dictionary);

    const pdf::PDFObject& filterObject = optimized.getObject(dictionary->get(pdf::PDF_STREAM_DICT_FILTER));
    QVERIFY(filterObject.isName());
    QCOMPARE(QString::fromLatin1(filterObject.getString()), QString("RunLengthDecode"));
}

void ImageOptimizerTest::test_optimizer_skips_disabled_override()
{
    QImage textScan = createTextScanImage(128);
    pdf::PDFDocument document = createDocumentWithImage(textScan, false, pdf::PDFImage::ImageCompression::Flate);

    std::vector<pdf::PDFImageOptimizer::ImageInfo> infos = pdf::PDFImageOptimizer::collectImageInfos(&document);
    QCOMPARE(infos.size(), 1u);

    const pdf::PDFObject& originalImageObject = document.getObjectByReference(infos[0].reference);
    QVERIFY(originalImageObject.isStream());
    const pdf::PDFStream* originalStream = originalImageObject.getStream();
    QVERIFY(originalStream);

    pdf::PDFImageOptimizer::Settings settings = pdf::PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    settings.autoMode = true;
    settings.colorMode = pdf::PDFImageOptimizer::ColorMode::Auto;
    settings.keepOriginalIfLarger = false;

    pdf::PDFImageOptimizer::ImageOverrides overrides;
    pdf::PDFImageOptimizer::ImageOverride imageOverride;
    imageOverride.enabled = false;
    overrides.emplace(infos[0].reference, imageOverride);

    pdf::PDFImageOptimizer optimizer;
    std::vector<pdf::PDFImageOptimizer::ImageResult> results;
    pdf::PDFDocument optimized = optimizer.optimize(&document, settings, overrides, nullptr, nullptr, &results);

    QCOMPARE(results.size(), 1u);
    QVERIFY(results[0].keptOriginal);

    const pdf::PDFObject& optimizedImageObject = optimized.getObjectByReference(infos[0].reference);
    QVERIFY(optimizedImageObject.isStream());
    const pdf::PDFStream* optimizedStream = optimizedImageObject.getStream();
    QVERIFY(optimizedStream);

    QCOMPARE(*optimizedStream->getContent(), *originalStream->getContent());
}

void ImageOptimizerTest::test_optimizer_preserve_keeps_bitonal_encoding()
{
    QImage bitonal(64, 64, QImage::Format_Mono);
    bitonal.fill(1);

    for (int y = 8; y < 56; ++y)
    {
        for (int x = 8; x < 56; ++x)
        {
            if (((x / 8) + (y / 8)) % 2 == 0)
            {
                bitonal.setPixel(x, y, 0);
            }
        }
    }

    pdf::PDFDocument document = createDocumentWithImage(bitonal, false, pdf::PDFImage::ImageCompression::Flate);

    std::vector<pdf::PDFImageOptimizer::ImageInfo> infos = pdf::PDFImageOptimizer::collectImageInfos(&document);
    QCOMPARE(infos.size(), 1u);
    QCOMPARE(infos[0].bitsPerComponent, 1);

    pdf::PDFImageOptimizer::Settings settings = pdf::PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    settings.autoMode = false;
    settings.colorMode = pdf::PDFImageOptimizer::ColorMode::Preserve;
    settings.goal = pdf::PDFImageOptimizer::OptimizationGoal::PreferQuality;
    settings.keepOriginalIfLarger = false;
    settings.preserveTransparency = true;
    settings.colorProfile.algorithm = pdf::PDFImageOptimizer::CompressionAlgorithm::Flate;
    settings.colorProfile.targetDpi = 0;

    pdf::PDFImageOptimizer optimizer;
    pdf::PDFDocument optimized = optimizer.optimize(&document, settings);

    const pdf::PDFObject& imageObject = optimized.getObjectByReference(infos[0].reference);
    QVERIFY(imageObject.isStream());

    const pdf::PDFStream* stream = imageObject.getStream();
    const pdf::PDFDictionary* dictionary = stream->getDictionary();
    QVERIFY(dictionary);

    QCOMPARE(optimized.getObject(dictionary->get("BitsPerComponent")).getInteger(), pdf::PDFInteger(1));

    const pdf::PDFObject& colorSpaceObject = optimized.getObject(dictionary->get("ColorSpace"));
    QVERIFY(colorSpaceObject.isName());
    QCOMPARE(QString::fromLatin1(colorSpaceObject.getString()), QString("DeviceGray"));
}

void ImageOptimizerTest::test_optimizer_preserves_smask()
{
    QImage alphaImage = createAlphaImage(64);
    pdf::PDFDocument document = createDocumentWithImage(alphaImage, true, pdf::PDFImage::ImageCompression::Flate);

    std::vector<pdf::PDFImageOptimizer::ImageInfo> infos = pdf::PDFImageOptimizer::collectImageInfos(&document);
    QCOMPARE(infos.size(), 1u);

    pdf::PDFImageOptimizer::Settings settings = pdf::PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    settings.autoMode = false;
    settings.colorMode = pdf::PDFImageOptimizer::ColorMode::Preserve;
    settings.goal = pdf::PDFImageOptimizer::OptimizationGoal::PreferQuality;
    settings.keepOriginalIfLarger = false;
    settings.preserveTransparency = true;
    settings.colorProfile.algorithm = pdf::PDFImageOptimizer::CompressionAlgorithm::JPEG;
    settings.colorProfile.targetDpi = 0;
    settings.colorProfile.jpegQuality = 70;

    pdf::PDFImageOptimizer optimizer;
    pdf::PDFDocument optimized = optimizer.optimize(&document, settings);

    const pdf::PDFObject& imageObject = optimized.getObjectByReference(infos[0].reference);
    QVERIFY(imageObject.isStream());

    const pdf::PDFStream* stream = imageObject.getStream();
    const pdf::PDFDictionary* dictionary = stream->getDictionary();
    QVERIFY(dictionary);
    QVERIFY(dictionary->hasKey("SMask"));

    const pdf::PDFObject& maskObject = optimized.getObject(dictionary->get("SMask"));
    QVERIFY(maskObject.isStream());
}

void ImageOptimizerTest::test_optimizer_reduces_size_for_photo()
{
    QImage photo = createPhotoImage(128);
    pdf::PDFDocument document = createDocumentWithImage(photo, false, pdf::PDFImage::ImageCompression::Flate);

    std::vector<pdf::PDFImageOptimizer::ImageInfo> infos = pdf::PDFImageOptimizer::collectImageInfos(&document);
    QCOMPARE(infos.size(), 1u);

    pdf::PDFImageOptimizer::Settings settings = pdf::PDFImageOptimizer::Settings::createDefault();
    settings.enabled = true;
    settings.autoMode = false;
    settings.colorMode = pdf::PDFImageOptimizer::ColorMode::Preserve;
    settings.goal = pdf::PDFImageOptimizer::OptimizationGoal::MinimumSize;
    settings.keepOriginalIfLarger = true;
    settings.preserveTransparency = true;
    settings.colorProfile.algorithm = pdf::PDFImageOptimizer::CompressionAlgorithm::JPEG;
    settings.colorProfile.targetDpi = 0;
    settings.colorProfile.jpegQuality = 35;

    pdf::PDFImageOptimizer optimizer;
    std::vector<pdf::PDFImageOptimizer::ImageResult> results;
    pdf::PDFDocument optimized = optimizer.optimize(&document, settings, {}, nullptr, nullptr, &results);

    QCOMPARE(results.size(), 1u);
    QVERIFY(!results[0].keptOriginal);
    QVERIFY(results[0].newBytes > 0);
    QVERIFY(results[0].originalBytes > 0);
    QVERIFY(results[0].newBytes < results[0].originalBytes);

    const pdf::PDFObject& imageObject = optimized.getObjectByReference(infos[0].reference);
    QVERIFY(imageObject.isStream());
}

namespace
{

QByteArray smallJpeg(bool gray = false)
{
    QImage image(40, 20, gray ? QImage::Format_Grayscale8 : QImage::Format_RGB888);
    image.fill(gray ? QColor(100, 100, 100) : QColor(30, 120, 200));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPG"))
        return {};
    return bytes;
}

QByteArray exifSegment(int orientation, bool littleEndian)
{
    QByteArray payload("Exif\0\0", 6);
    payload += littleEndian ? QByteArray::fromHex("49492a0008000000010012010300010000000000000000000000")
                            : QByteArray::fromHex("4d4d002a00000008000101120003000000010000000000000000");
    payload[6 + 18 + (littleEndian ? 0 : 1)] = char(orientation);
    const int length = int(payload.size()) + 2;
    return QByteArray::fromHex("ffe1") + char(length >> 8) + char(length & 255) + payload;
}

// A header-only fixture for unsupported formats and exact limit boundaries.
QByteArray jpegHeader(int sof = 0xC0, int bits = 8, int components = 3, int width = 40, int height = 20)
{
    QByteArray bytes = QByteArray::fromHex("ffd8ff");
    bytes += char(sof);
    const int length = 8 + 3 * components;
    bytes += char(length >> 8);
    bytes += char(length & 255);
    bytes += char(bits);
    bytes += char(height >> 8);
    bytes += char(height & 255);
    bytes += char(width >> 8);
    bytes += char(width & 255);
    bytes += char(components);
    for (int index = 0; index < components; ++index)
    {
        bytes += char(index + 1);
        bytes += char(0x11);
        bytes += char(0);
    }
    return bytes + QByteArray::fromHex("ffd9");
}

} // namespace

void ImageOptimizerTest::test_jpeg_passthrough_roundtrip_data()
{
    QTest::addColumn<bool>("gray");
    QTest::addColumn<int>("orientation");
    QTest::addColumn<bool>("littleEndian");
    QTest::newRow("rgb-no-exif") << false << 0 << true;
    QTest::newRow("gray-no-exif") << true << 0 << true;
    for (const int orientation : { 1, 3, 6, 8 })
    {
        for (const bool littleEndian : { true, false })
        {
            const QByteArray row = QByteArray::number(orientation) + (littleEndian ? "-II" : "-MM");
            QTest::newRow(row.constData()) << false << orientation << littleEndian;
        }
    }
}

void ImageOptimizerTest::test_jpeg_passthrough_roundtrip()
{
    QFETCH(bool, gray);
    QFETCH(int, orientation);
    QFETCH(bool, littleEndian);
    QByteArray jpeg = smallJpeg(gray);
    QVERIFY2(!jpeg.isEmpty(), "Qt JPEG writer must be available.");
    if (orientation)
        jpeg.insert(2, exifSegment(orientation, littleEndian));
    const pdf::PDFJpegImageInfo info = pdf::PDFJpegImage::parseHeader(jpeg);
    QVERIFY2(info.canWriteDirectly, qPrintable(info.errorMessage));
    QVERIFY(info.valid);
    QCOMPARE(info.exifOrientation, orientation ? orientation : 1);
    QCOMPARE(info.bitsPerComponent, 8);
    QCOMPARE(info.componentCount, gray ? 1 : 3);
    QVERIFY(!info.isProgressive);

    pdf::PDFObject directImage;
    QString error;
    QVERIFY2(pdf::PDFJpegImage::createImageXObject(jpeg, &directImage, &error), qPrintable(error));
    QVERIFY(directImage.isStream());
    QCOMPARE(*directImage.getStream()->getContent(), jpeg);

    pdf::PDFDocument document;
    QVERIFY2(pdf::PDFJpegImage::createDocument(jpeg, &document, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QByteArray pdfBytes;
    QBuffer output(&pdfBytes);
    QVERIFY(output.open(QIODevice::WriteOnly));
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(&output, &document));
    output.close();
    pdf::PDFDocumentReader reader(nullptr, [](bool*) { return QString(); }, false, false);
    const pdf::PDFDocument reopened = reader.readFromBuffer(pdfBytes);
    QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
    QCOMPARE(reopened.getCatalog()->getPageCount(), size_t(1));
    const pdf::PDFPage* page = reopened.getCatalog()->getPage(0);
    QVERIFY(page);
    const bool landscape = orientation == 6 || orientation == 8;
    QCOMPARE(page->getMediaBox(), QRectF(0, 0, landscape ? 842 : 595, landscape ? 595 : 842));
    const int degrees = orientation == 3 ? 180 : orientation == 6 ? 90 : orientation == 8 ? 270 : 0;
    const auto& storage = reopened.getStorage();
    const pdf::PDFDictionary* pageDictionary = storage.getDictionaryFromObject(
        pdf::PDFObject::createReference(page->getPageReference()));
    QVERIFY(pageDictionary);
    QCOMPARE(pageDictionary->get("Rotate").getInteger(), pdf::PDFInteger(degrees));
    QCOMPARE(page->getPageRotation(), orientation == 3 ? pdf::PageRotation::Rotate180
                                   : orientation == 6 ? pdf::PageRotation::Rotate90
                                   : orientation == 8 ? pdf::PageRotation::Rotate270 : pdf::PageRotation::None);
    const pdf::PDFDictionary* resources = storage.getDictionaryFromObject(page->getResources());
    QVERIFY(resources);
    const pdf::PDFDictionary* xObjects = storage.getDictionaryFromObject(resources->get("XObject"));
    QVERIFY(xObjects);
    const pdf::PDFObject& image = reopened.getObject(xObjects->get("Im1"));
    QVERIFY(image.isStream());
    const pdf::PDFStream* stream = image.getStream();
    QCOMPARE(*stream->getContent(), jpeg); // Raw bytes, never getDecodedStream().
    const pdf::PDFDictionary* dictionary = stream->getDictionary();
    QCOMPARE(dictionary->get("Type").getString(), QByteArray("XObject"));
    QCOMPARE(dictionary->get("Subtype").getString(), QByteArray("Image"));
    QCOMPARE(dictionary->get("Filter").getString(), QByteArray("DCTDecode"));
    QCOMPARE(dictionary->get("ColorSpace").getString(), QByteArray(gray ? "DeviceGray" : "DeviceRGB"));
    QCOMPARE(dictionary->get("Width").getInteger(), pdf::PDFInteger(40));
    QCOMPARE(dictionary->get("Height").getInteger(), pdf::PDFInteger(20));
    QCOMPARE(dictionary->get("BitsPerComponent").getInteger(), pdf::PDFInteger(8));
    QCOMPARE(dictionary->get("Length").getInteger(), pdf::PDFInteger(jpeg.size()));

    const pdf::PDFObject& content = reopened.getObject(page->getContents());
    QVERIFY(content.isStream());
    const QList<QByteArray> tokens = content.getStream()->getContent()->trimmed().split(' ');
    QCOMPARE(tokens.size(), qsizetype(11));
    QCOMPARE(tokens[0], QByteArray("q"));
    QCOMPARE(tokens[2], QByteArray("0"));
    QCOMPARE(tokens[3], QByteArray("0"));
    QCOMPARE(tokens[7], QByteArray("cm"));
    QCOMPARE(tokens[8], QByteArray("/Im1"));
    QCOMPARE(tokens[9], QByteArray("Do"));
    QCOMPARE(tokens[10], QByteArray("Q"));
    const double width = tokens[1].toDouble();
    const double height = tokens[4].toDouble();
    const double x = tokens[5].toDouble();
    const double y = tokens[6].toDouble();
    QVERIFY(qAbs(width / height - 2.0) < 1e-8);
    QVERIFY(width <= page->getMediaBox().width() && height <= page->getMediaBox().height());
    QVERIFY(qAbs(x - (page->getMediaBox().width() - width) / 2) < 1e-8);
    QVERIFY(qAbs(y - (page->getMediaBox().height() - height) / 2) < 1e-8);
}

void ImageOptimizerTest::test_jpeg_header_rejections()
{
    using Reason = pdf::PDFJpegImageReason;
    auto checkRejected = [](const QByteArray& bytes, Reason expected)
    {
        const pdf::PDFJpegImageInfo info = pdf::PDFJpegImage::parseHeader(bytes);
        QVERIFY(!info.canWriteDirectly);
        QCOMPARE(info.reason, expected);
        QVERIFY(!info.errorMessage.isEmpty());
        pdf::PDFObject image = pdf::PDFObject::createInteger(1);
        QString error;
        QVERIFY(!pdf::PDFJpegImage::createImageXObject(bytes, &image, &error));
        QVERIFY(image.isNull());
        QCOMPARE(error, info.errorMessage);
        pdf::PDFDocument document;
        QVERIFY(!pdf::PDFJpegImage::createDocument(bytes, &document, &error));
        QCOMPARE(document.getCatalog()->getPageCount(), size_t(0));
        QCOMPARE(error, info.errorMessage);
    };
    checkRejected({}, Reason::EmptyInput);
    checkRejected("garbage", Reason::InvalidSOI);
    checkRejected(QByteArray::fromHex("ffd8ffe10001"), Reason::InvalidSegment);
    checkRejected(QByteArray::fromHex("ffd8ffe100200000"), Reason::InvalidSegment);
    checkRejected(QByteArray::fromHex("ffd8ff"), Reason::InvalidSegment);
    checkRejected(QByteArray::fromHex("ffd8ffd9"), Reason::MissingSOF);
    checkRejected(jpegHeader(0xC0, 8, 3, 0, 20), Reason::InvalidDimensions);
    checkRejected(jpegHeader(0xC0, 8, 3, 40, 0), Reason::InvalidDimensions);
    checkRejected(jpegHeader(0xC2), Reason::UnsupportedSOF);
    checkRejected(jpegHeader(0xC3), Reason::UnsupportedSOF);
    checkRejected(jpegHeader(0xC0, 12), Reason::UnsupportedPrecision);
    checkRejected(jpegHeader(0xC0, 8, 4), Reason::UnsupportedComponents);
    checkRejected(jpegHeader(0xC0, 8, 2), Reason::UnsupportedComponents);
    checkRejected(jpegHeader(0xC0, 8, 3, 8001, 6000), Reason::PixelLimitExceeded);
    for (const int orientation : { 2, 4, 5, 7 })
    {
        QByteArray bytes = jpegHeader();
        bytes.insert(2, exifSegment(orientation, orientation % 2 == 0));
        checkRejected(bytes, Reason::MirroredOrientation);
    }
    QByteArray large = jpegHeader();
    large.resize(64 * 1024 * 1024, '\0');
    QVERIFY(pdf::PDFJpegImage::parseHeader(large).canWriteDirectly);
    large.append('\0');
    checkRejected(large, Reason::FileSizeLimitExceeded);

    const QByteArray jpeg = smallJpeg();
    QVERIFY(!jpeg.isEmpty());
    const qsizetype sos = jpeg.indexOf(QByteArray::fromHex("ffda"));
    QVERIFY(sos > 0);
    const int scanLength = (static_cast<unsigned char>(jpeg[sos + 2]) << 8)
                         | static_cast<unsigned char>(jpeg[sos + 3]);
    for (qsizetype size = 0; size < sos + 2 + scanLength; ++size)
    {
        const auto info = pdf::PDFJpegImage::parseHeader(jpeg.left(size));
        QVERIFY(!info.valid);
        QVERIFY(!info.canWriteDirectly);
        QVERIFY(!info.errorMessage.isEmpty());
    }
}

void ImageOptimizerTest::test_jpeg_icc_profile_rejected()
{
    const QByteArray plainJpeg = jpegHeader();
    const auto plainInfo = pdf::PDFJpegImage::parseHeader(plainJpeg);
    QVERIFY2(plainInfo.canWriteDirectly, qPrintable(plainInfo.errorMessage));

    QByteArray iccPayload("ICC_PROFILE\0", 12);
    iccPayload += char(1); // ICC chunk sequence number.
    iccPayload += char(1); // Total ICC chunk count.
    iccPayload += char(0x42);
    const int segmentLength = int(iccPayload.size()) + 2;
    QByteArray iccSegment = QByteArray::fromHex("ffe2");
    iccSegment += char(segmentLength >> 8);
    iccSegment += char(segmentLength & 255);
    iccSegment += iccPayload;
    const QByteArray iccJpeg = QByteArray::fromHex("ffd8") + iccSegment + plainJpeg.mid(2);

    const pdf::PDFJpegImageInfo info = pdf::PDFJpegImage::parseHeader(iccJpeg);
    QVERIFY(!info.canWriteDirectly);
    QCOMPARE(info.reason, pdf::PDFJpegImageReason::UnsupportedICCProfile);
    QCOMPARE(info.errorMessage, QStringLiteral("JPEGs with embedded ICC profiles are not supported for direct insertion."));
}

void ImageOptimizerTest::test_jpeg_header_metadata()
{
    QVERIFY(pdf::PDFJpegImage::parseHeader(jpegHeader(0xC1)).canWriteDirectly);
    QVERIFY(pdf::PDFJpegImage::parseHeader(jpegHeader(0xC0, 8, 3, 8000, 6000)).canWriteDirectly);
    for (const int sof : { 0xC2, 0xC6, 0xCA, 0xCE })
    {
        const auto info = pdf::PDFJpegImage::parseHeader(jpegHeader(sof));
        QVERIFY(info.valid);
        QVERIFY(info.isProgressive);
        QCOMPARE(info.reason, pdf::PDFJpegImageReason::UnsupportedSOF);
    }
    QByteArray jpeg = smallJpeg();
    QVERIFY(!jpeg.isEmpty());
    jpeg.insert(2, QByteArray::fromHex("ffee000e41646f626500640000000001"));
    auto info = pdf::PDFJpegImage::parseHeader(jpeg);
    QVERIFY(info.canWriteDirectly);
    QVERIFY(info.hasAdobeMarker);
    QCOMPARE(info.adobeTransform, 1);

    // An unrelated APP1 must not overwrite an earlier Exif orientation.
    jpeg.insert(2, exifSegment(6, true) + QByteArray::fromHex("ffe10005616263"));
    QCOMPARE(pdf::PDFJpegImage::parseHeader(jpeg).exifOrientation, 6);
    for (const bool littleEndian : { true, false })
    {
        QByteArray exif = exifSegment(3, littleEndian);
        exif.replace(4 + 6 + 4, 4, QByteArray::fromHex("ffffffff")); // Invalid IFD0 offset.
        QByteArray malformed = smallJpeg();
        malformed.insert(2, exif);
        info = pdf::PDFJpegImage::parseHeader(malformed);
        QVERIFY(info.canWriteDirectly);
        QCOMPARE(info.exifOrientation, 1);
    }
}

QTEST_GUILESS_MAIN(ImageOptimizerTest)

#include "tst_imageoptimizertest.moc"
