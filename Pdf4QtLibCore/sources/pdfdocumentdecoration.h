// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFDOCUMENTDECORATION_H
#define PDFDOCUMENTDECORATION_H

#include "pdfdocument.h"
#include "pdfpagegeometry.h"

#include <QColor>
#include <QImage>
#include <QString>

namespace pdf
{

/**
 * \brief Settings for content written before or after existing page contents.
 */
struct PDF4QTLIBCORESHARED_EXPORT PDFDocumentDecorationSettings
{
    enum class Kind
    {
        TextWatermark,
        ColorBackground,
        ImageBackground
    };

    enum class ImageScaleMode
    {
        Stretch,
        Fit,
        Fill
    };

    Kind kind = Kind::TextWatermark;
    QString pageRange = QStringLiteral("-");
    PDFPageGeometrySettings::PageSubset pageSubset =
        PDFPageGeometrySettings::PageSubset::AllPages;

    QString text;
    QString fontFamily = QStringLiteral("Arial");
    PDFReal fontPointSize = 48.0;
    QColor color = QColor(128, 128, 128);
    PDFReal opacity = 0.25;
    PDFReal angleDegrees = -45.0;
    bool foreground = true;

    QImage image;
    ImageScaleMode imageScaleMode = ImageScaleMode::Fit;
};

/**
 * \brief Writes standard PDF page content streams for watermarks/backgrounds.
 */
class PDF4QTLIBCORESHARED_EXPORT PDFDocumentDecoration
{
public:
    static PDFOperationResult apply(
        PDFDocument* document,
        const PDFDocumentDecorationSettings& settings,
        PDFModifiedDocument::ModificationFlags* modificationFlags = nullptr);
};

} // namespace pdf

#endif // PDFDOCUMENTDECORATION_H
