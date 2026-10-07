// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFJPEGIMAGE_H
#define PDFJPEGIMAGE_H

#include "pdfdocument.h"

namespace pdf
{

enum class PDFJpegImageReason
{
    None,
    EmptyInput,
    InvalidSOI,
    InvalidSegment,
    MissingSOF,
    InvalidDimensions,
    UnsupportedSOF,
    UnsupportedPrecision,
    UnsupportedComponents,
    MirroredOrientation,
    PixelLimitExceeded,
    FileSizeLimitExceeded
};

struct PDFJpegImageInfo
{
    int width = 0;
    int height = 0;
    int bitsPerComponent = 0;
    int componentCount = 0;
    bool isProgressive = false;
    bool hasAdobeMarker = false;
    int adobeTransform = -1;
    int exifOrientation = 1;
    bool valid = false;
    bool canWriteDirectly = false;
    PDFJpegImageReason reason = PDFJpegImageReason::None;
    QString errorMessage;
};

/// Header-only JPEG inspection and lossless insertion of supported JPEG bytes.
/// Entropy-coded pixels are not validated or decoded. Malformed optional Exif
/// metadata is ignored; an absent or invalid orientation defaults to 1.
class PDF4QTLIBCORESHARED_EXPORT PDFJpegImage
{
public:
    static PDFJpegImageInfo parseHeader(const QByteArray& bytes);

    /// Output is reset on failure. The stream retains the original JPEG bytes.
    static bool createImageXObject(const QByteArray& bytes, PDFObject* image, QString* errorMessage = nullptr);

    /// Creates one centered, aspect-fitted image page on A4 (595 x 842 points).
    /// Output is reset on failure. Exif rotations use the page's /Rotate entry.
    static bool createDocument(const QByteArray& bytes, PDFDocument* document, QString* errorMessage = nullptr);
};

} // namespace pdf

#endif // PDFJPEGIMAGE_H
