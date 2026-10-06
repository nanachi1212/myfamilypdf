// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFPAGEINSERTER_H
#define PDFPAGEINSERTER_H

#include "pdfdocument.h"
#include "pdfdocumentmerger.h"
#include "pdfpage.h"
#include "pdfutils.h"

#include <QCoreApplication>
#include <QStringList>

#include <vector>

namespace pdf
{

/// Inserts pages into an open document (Editor "Insert Blank Page" and "Insert Pages from PDF", v11).
/// Every function works on copies: the target and the source document are never changed. A successful
/// call returns the new target document, which the caller publishes as one modification (one Undo step).
///
/// Pages from another PDF go through a restricted import: the selected page objects are prepared in a
/// copy of the source (inherited attributes written into the page, link targets checked, the page tree
/// parent removed), everything they can reach is checked, and only then the pages are copied in one batch.
/// Forms, signatures, tagged structure, optional content and active content are refused, not converted.
class PDF4QTLIBCORESHARED_EXPORT PDFPageInserter
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFPageInserter)

public:
    enum class Position
    {
        Before,     ///< Before the first anchor page
        After,      ///< After the last anchor page
        Beginning,  ///< Before the first page of the document
        End         ///< After the last page of the document
    };

    /// Zero based index in the target page list where the new pages go. \p anchorPages are the selected
    /// pages (or the current page), sorted: Before uses the first one, After the last one.
    static PDFInteger getInsertIndex(Position position, const std::vector<PDFInteger>& anchorPages, PDFInteger pageCount);

    /// Parses the source page list ("3,1", "2-5"); the order typed by the user is kept.
    /// A page that is given twice is an error (nothing is removed or sorted silently).
    static std::vector<PDFInteger> parsePageSelection(PDFInteger pageCount, const QString& text, QString* errorMessage);

    /// Reasons why pages cannot be inserted into \p target. Empty when insertion is possible.
    /// \param externalPages Pages come from another PDF (stricter than a blank page)
    static QStringList checkTarget(const PDFDocument* target, bool externalPages);

    /// Reasons why pages of \p source cannot be inserted (permissions, forms, signatures, tagged PDF).
    static QStringList checkSource(const PDFDocumentMerger::Source& source);

    /// Inserts one empty page at \p insertIndex. MediaBox, CropBox (the media box when \p cropBox is not valid)
    /// and Rotate are written into the page. The page gets no resources and no content.
    static PDFOperationResult insertBlankPage(const PDFDocument* target,
                                              PDFInteger insertIndex,
                                              const QRectF& mediaBox,
                                              const QRectF& cropBox,
                                              PageRotation rotation,
                                              PDFDocumentPointer* result);

    /// Inserts \p pages (zero based, in this order, no repetitions) of \p source at \p insertIndex.
    /// \param warnings Things the user should know before the result is used (links that lose their
    ///        target, encryption that is not kept)
    static PDFOperationResult insertPages(const PDFDocument* target,
                                          PDFInteger insertIndex,
                                          const PDFDocumentMerger::Source& source,
                                          const std::vector<PDFInteger>& pages,
                                          PDFDocumentPointer* result,
                                          QStringList* warnings);
};

}   // namespace pdf

#endif // PDFPAGEINSERTER_H
