// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfpngimage.h"
#include "pdfdocumentbuilder.h"

#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QtMath>
#include <algorithm>

namespace pdf
{
namespace
{

void setEntry(PDFDictionary& dictionary, const char* key, PDFObject value)
{
    dictionary.addEntry(PDFInplaceOrMemoryString(key), std::move(value));
}

PDFObject dictionaryObject(PDFDictionary dictionary)
{
    return PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(dictionary)));
}

PDFObject imageStream(QByteArray data, int width, int height, bool mask, PDFObject softMask = PDFObject())
{
    PDFDictionary dictionary;
    setEntry(dictionary, "Type", PDFObject::createName("XObject"));
    setEntry(dictionary, "Subtype", PDFObject::createName("Image"));
    setEntry(dictionary, "Width", PDFObject::createInteger(width));
    setEntry(dictionary, "Height", PDFObject::createInteger(height));
    setEntry(dictionary, "BitsPerComponent", PDFObject::createInteger(8));
    setEntry(dictionary, "ColorSpace", PDFObject::createName(mask ? "DeviceGray" : "DeviceRGB"));
    setEntry(dictionary, "Filter", PDFObject::createName("FlateDecode"));
    setEntry(dictionary, "Length", PDFObject::createInteger(data.size()));
    if (!softMask.isNull())
        setEntry(dictionary, "SMask", std::move(softMask));
    return PDFObject::createStream(std::make_shared<PDFStream>(std::move(dictionary), std::move(data)));
}

} // namespace

bool PDFPngImage::createDocument(const QByteArray& bytes, PDFDocument* document, QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (!document)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("A document output is required.");
        return false;
    }
    *document = PDFDocument();
    auto reject = [&](const QString& message)
    {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (bytes.isEmpty() || bytes.size() > 64LL * 1024 * 1024)
        return reject(PDFTranslationContext::tr("PNG must be nonempty and no larger than 64 MiB."));

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly))
        return reject(PDFTranslationContext::tr("The PNG file is invalid or damaged."));
    QImageReader reader(&buffer, "PNG");
    const QSize size = reader.size();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0 || qint64(size.width()) * size.height() > 48000000)
        return reject(PDFTranslationContext::tr("PNG exceeds the 48 megapixel limit."));
    QImage image = reader.read();
    if (image.isNull())
        return reject(PDFTranslationContext::tr("The PNG file is invalid or damaged."));

    const bool hasAlpha = image.hasAlphaChannel();
    image = image.convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull())
        return reject(PDFTranslationContext::tr("The PNG image could not be decoded."));
    const int width = image.width();
    const int height = image.height();
    QByteArray rgb;
    QByteArray alpha;
    rgb.resize(qsizetype(width) * height * 3);
    if (hasAlpha)
        alpha.resize(qsizetype(width) * height);
    qsizetype rgbOffset = 0;
    qsizetype alphaOffset = 0;
    for (int y = 0; y < height; ++y)
    {
        const uchar* row = image.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            const uchar* pixel = row + x * 4;
            rgb[rgbOffset++] = char(pixel[0]);
            rgb[rgbOffset++] = char(pixel[1]);
            rgb[rgbOffset++] = char(pixel[2]);
            if (hasAlpha)
                alpha[alphaOffset++] = char(pixel[3]);
        }
    }
    // qCompress prepends an uncompressed length; PDF FlateDecode expects only zlib data.
    rgb = qCompress(rgb, 6).mid(4);
    if (hasAlpha)
        alpha = qCompress(alpha, 6).mid(4);

    const QRectF mediaBox(0, 0, 595, 842);
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(mediaBox);
    PDFObject softMask;
    if (hasAlpha)
    {
        const PDFObjectReference mask = builder.addObject(imageStream(std::move(alpha), width, height, true));
        softMask = PDFObject::createReference(mask);
    }
    const PDFObjectReference imageReference = builder.addObject(imageStream(std::move(rgb), width, height, false, std::move(softMask)));
    const double scale = std::min(mediaBox.width() / width, mediaBox.height() / height);
    const double drawWidth = scale * width;
    const double drawHeight = scale * height;
    QByteArray content = "q " + QByteArray::number(drawWidth, 'f', 8) + " 0 0 " + QByteArray::number(drawHeight, 'f', 8)
                             + " " + QByteArray::number((mediaBox.width() - drawWidth) / 2, 'f', 8)
                             + " " + QByteArray::number((mediaBox.height() - drawHeight) / 2, 'f', 8) + " cm /Im1 Do Q\n";
    PDFDictionary xObjects;
    setEntry(xObjects, "Im1", PDFObject::createReference(imageReference));
    PDFDictionary resources;
    setEntry(resources, "XObject", dictionaryObject(std::move(xObjects)));
    PDFDictionary pageUpdate;
    setEntry(pageUpdate, "Resources", dictionaryObject(std::move(resources)));
    const PDFObjectReference contents = builder.addObject(PDFObject::createStream(std::make_shared<PDFStream>(PDFDictionary(), std::move(content))));
    setEntry(pageUpdate, "Contents", PDFObject::createReference(contents));
    builder.mergeTo(page, dictionaryObject(std::move(pageUpdate)));
    *document = builder.build();
    return true;
}

} // namespace pdf
