// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfjpegimage.h"
#include "pdfdocumentbuilder.h"

#include <algorithm>

namespace pdf
{
namespace
{

int byteAt(const QByteArray& bytes, qsizetype offset)
{
    return static_cast<unsigned char>(bytes[offset]);
}

int bigEndian16(const QByteArray& bytes, qsizetype offset)
{
    return (byteAt(bytes, offset) << 8) | byteAt(bytes, offset + 1);
}

int readExifOrientation(const QByteArray& bytes, qsizetype start, qsizetype size)
{
    if (size < 14 || bytes.mid(start, 6) != QByteArray("Exif\0\0", 6))
        return 1;

    const qsizetype tiff = start + 6;
    const qsizetype tiffSize = size - 6;
    const bool littleEndian = bytes.mid(tiff, 2) == "II";
    if (!littleEndian && bytes.mid(tiff, 2) != "MM")
        return 1;

    auto read16 = [&](qsizetype offset) -> quint32
    {
        return littleEndian ? byteAt(bytes, tiff + offset) | (byteAt(bytes, tiff + offset + 1) << 8)
                            : bigEndian16(bytes, tiff + offset);
    };
    auto read32 = [&](qsizetype offset) -> quint32
    {
        const quint32 first = read16(offset);
        const quint32 second = read16(offset + 2);
        return littleEndian ? first | (second << 16) : (first << 16) | second;
    };

    if (read16(2) != 42)
        return 1;
    const quint32 ifd = read32(4);
    if (ifd < 8 || ifd > quint64(tiffSize - 2))
        return 1;
    const quint32 count = read16(ifd);
    const qsizetype entries = qsizetype(ifd) + 2;
    if (count > quint64((tiffSize - entries) / 12))
        return 1;

    for (quint32 index = 0; index < count; ++index)
    {
        const qsizetype entry = entries + qsizetype(index) * 12;
        if (read16(entry) == 0x0112 && read16(entry + 2) == 3 && read32(entry + 4) == 1)
        {
            const int orientation = int(read16(entry + 8));
            return orientation >= 1 && orientation <= 8 ? orientation : 1;
        }
    }
    return 1;
}

PDFObject dictionaryObject(PDFDictionary dictionary)
{
    return PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(dictionary)));
}

void setEntry(PDFDictionary& dictionary, const char* key, PDFObject value)
{
    dictionary.addEntry(PDFInplaceOrMemoryString(key), std::move(value));
}

PDFObject streamObject(PDFDictionary dictionary, QByteArray bytes)
{
    setEntry(dictionary, "Length", PDFObject::createInteger(bytes.size()));
    return PDFObject::createStream(std::make_shared<PDFStream>(std::move(dictionary), std::move(bytes)));
}

PDFObject createJpegStream(const QByteArray& bytes, const PDFJpegImageInfo& info)
{
    PDFDictionary dictionary;
    setEntry(dictionary, "Type", PDFObject::createName("XObject"));
    setEntry(dictionary, "Subtype", PDFObject::createName("Image"));
    setEntry(dictionary, "Width", PDFObject::createInteger(info.width));
    setEntry(dictionary, "Height", PDFObject::createInteger(info.height));
    setEntry(dictionary, "BitsPerComponent", PDFObject::createInteger(8));
    setEntry(dictionary, "ColorSpace", PDFObject::createName(info.componentCount == 1 ? "DeviceGray" : "DeviceRGB"));
    setEntry(dictionary, "Filter", PDFObject::createName("DCTDecode"));
    return streamObject(std::move(dictionary), bytes);
}

} // namespace

PDFJpegImageInfo PDFJpegImage::parseHeader(const QByteArray& bytes)
{
    PDFJpegImageInfo info;
    auto reject = [&](PDFJpegImageReason reason, const char* message)
    {
        info.reason = reason;
        info.errorMessage = PDFTranslationContext::tr(message);
        return info;
    };

    if (bytes.isEmpty())
        return reject(PDFJpegImageReason::EmptyInput, "JPEG data is empty.");
    if (bytes.size() < 2 || byteAt(bytes, 0) != 0xFF || byteAt(bytes, 1) != 0xD8)
        return reject(PDFJpegImageReason::InvalidSOI, "JPEG must start with the SOI marker.");

    int sof = -1;
    bool headerEnded = false;
    qsizetype offset = 2;
    while (offset < bytes.size())
    {
        if (byteAt(bytes, offset++) != 0xFF)
            return reject(PDFJpegImageReason::InvalidSegment, "Invalid JPEG marker prefix.");
        while (offset < bytes.size() && byteAt(bytes, offset) == 0xFF)
            ++offset;
        if (offset == bytes.size())
            return reject(PDFJpegImageReason::InvalidSegment, "Truncated JPEG marker.");
        const int marker = byteAt(bytes, offset++);
        if (marker == 0xD9)
        {
            headerEnded = true;
            break;
        }
        if (marker == 0 || marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7))
            return reject(PDFJpegImageReason::InvalidSegment, "Unexpected marker in JPEG header.");
        if (marker == 0x01) // TEM is the only standalone header marker.
            continue;
        if (bytes.size() - offset < 2)
            return reject(PDFJpegImageReason::InvalidSegment, "Truncated JPEG segment length.");
        const int length = bigEndian16(bytes, offset);
        if (length < 2 || length > bytes.size() - offset)
            return reject(PDFJpegImageReason::InvalidSegment, "JPEG segment length is invalid or truncated.");

        const qsizetype payload = offset + 2;
        const int payloadSize = length - 2;
        const bool isSOF = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isSOF)
        {
            if (sof != -1 || payloadSize < 6 || payloadSize != 6 + 3 * byteAt(bytes, payload + 5))
                return reject(PDFJpegImageReason::InvalidSegment, "Invalid JPEG frame header.");
            sof = marker;
            info.bitsPerComponent = byteAt(bytes, payload);
            info.height = bigEndian16(bytes, payload + 1);
            info.width = bigEndian16(bytes, payload + 3);
            info.componentCount = byteAt(bytes, payload + 5);
            info.isProgressive = marker == 0xC2 || marker == 0xC6 || marker == 0xCA || marker == 0xCE;
        }
        else if (marker == 0xE1 && payloadSize >= 6 && bytes.mid(payload, 6) == QByteArray("Exif\0\0", 6))
            info.exifOrientation = readExifOrientation(bytes, payload, payloadSize);
        else if (marker == 0xEE && payloadSize >= 12 && bytes.mid(payload, 5) == "Adobe")
        {
            info.hasAdobeMarker = true;
            info.adobeTransform = byteAt(bytes, payload + 11);
        }
        else if (marker == 0xDA)
        {
            if (payloadSize < 4 || payloadSize != 4 + 2 * byteAt(bytes, payload))
                return reject(PDFJpegImageReason::InvalidSegment, "Invalid JPEG scan header.");
            headerEnded = true;
            break; // Do not inspect entropy-coded pixels.
        }
        offset += length;
    }

    if (sof == -1)
        return reject(PDFJpegImageReason::MissingSOF, "JPEG frame header (SOF) was not found.");
    if (!headerEnded)
        return reject(PDFJpegImageReason::InvalidSegment, "Truncated JPEG header before SOS or EOI.");
    if (info.width == 0 || info.height == 0)
        return reject(PDFJpegImageReason::InvalidDimensions, "JPEG width and height must be nonzero.");

    info.valid = true;
    if (bytes.size() > 64LL * 1024 * 1024)
        return reject(PDFJpegImageReason::FileSizeLimitExceeded, "JPEG exceeds the 64 MiB passthrough limit.");
    if (qint64(info.width) * info.height > 48000000)
        return reject(PDFJpegImageReason::PixelLimitExceeded, "JPEG exceeds the 48 megapixel passthrough limit.");
    if (info.componentCount != 1 && info.componentCount != 3)
        return reject(PDFJpegImageReason::UnsupportedComponents, "Only 1 or 3 JPEG components are supported; CMYK/YCCK requires decoding.");
    if (sof != 0xC0 && sof != 0xC1)
        return reject(PDFJpegImageReason::UnsupportedSOF, "Only baseline or extended sequential Huffman JPEG is supported; other SOF types require decoding.");
    if (info.bitsPerComponent != 8)
        return reject(PDFJpegImageReason::UnsupportedPrecision, "Only 8-bit JPEG components are supported.");
    if (info.exifOrientation == 2 || info.exifOrientation == 4 || info.exifOrientation == 5 || info.exifOrientation == 7)
        return reject(PDFJpegImageReason::MirroredOrientation, "Mirrored Exif orientations (2, 4, 5, 7) require decoding.");

    info.canWriteDirectly = true;
    return info;
}

bool PDFJpegImage::createImageXObject(const QByteArray& bytes, PDFObject* image, QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (!image)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("An image output is required.");
        return false;
    }
    *image = PDFObject();
    const PDFJpegImageInfo info = parseHeader(bytes);
    if (!info.canWriteDirectly)
    {
        if (errorMessage)
            *errorMessage = info.errorMessage;
        return false;
    }
    *image = createJpegStream(bytes, info);
    return true;
}

bool PDFJpegImage::createDocument(const QByteArray& bytes, PDFDocument* document, QString* errorMessage)
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
    const PDFJpegImageInfo info = parseHeader(bytes);
    if (!info.canWriteDirectly)
    {
        if (errorMessage)
            *errorMessage = info.errorMessage;
        return false;
    }

    const bool landscape = info.exifOrientation == 6 || info.exifOrientation == 8;
    const QRectF mediaBox(0, 0, landscape ? 842 : 595, landscape ? 595 : 842);
    PDFDocumentBuilder builder;
    const PDFObjectReference page = builder.appendPage(mediaBox);
    const PageRotation rotation = info.exifOrientation == 3 ? PageRotation::Rotate180
                                : info.exifOrientation == 6 ? PageRotation::Rotate90
                                : info.exifOrientation == 8 ? PageRotation::Rotate270 : PageRotation::None;
    builder.setPageRotation(page, rotation);
    const PDFObjectReference image = builder.addObject(createJpegStream(bytes, info));

    const double scale = std::min(mediaBox.width() / info.width, mediaBox.height() / info.height);
    const double width = scale * info.width;
    const double height = scale * info.height;
    const QByteArray content = "q " + QByteArray::number(width, 'f', 8) + " 0 0 " + QByteArray::number(height, 'f', 8)
                             + " " + QByteArray::number((mediaBox.width() - width) / 2, 'f', 8)
                             + " " + QByteArray::number((mediaBox.height() - height) / 2, 'f', 8) + " cm /Im1 Do Q\n";
    const PDFObjectReference contents = builder.addObject(streamObject(PDFDictionary(), content));
    PDFDictionary xObjects;
    setEntry(xObjects, "Im1", PDFObject::createReference(image));
    PDFDictionary resources;
    setEntry(resources, "XObject", dictionaryObject(std::move(xObjects)));
    PDFDictionary pageUpdate;
    setEntry(pageUpdate, "Resources", dictionaryObject(std::move(resources)));
    setEntry(pageUpdate, "Contents", PDFObject::createReference(contents));
    builder.mergeTo(page, dictionaryObject(std::move(pageUpdate)));
    *document = builder.build();
    return true;
}

} // namespace pdf
