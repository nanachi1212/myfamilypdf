// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFPNGIMAGE_H
#define PDFPNGIMAGE_H

#include "pdfdocument.h"

namespace pdf
{

/// Decodes PNG data through Qt and creates one centered image page.
/// Alpha is retained as a PDF soft mask. Output is reset on failure.
class PDF4QTLIBCORESHARED_EXPORT PDFPngImage
{
public:
    static bool createDocument(const QByteArray& bytes, PDFDocument* document, QString* errorMessage = nullptr);
};

} // namespace pdf

#endif // PDFPNGIMAGE_H
