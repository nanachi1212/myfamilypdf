// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFPAGEINSERTSPLITDIALOGS_H
#define PDFPAGEINSERTSPLITDIALOGS_H

#include "pdf4qtlibgui_export.h"
#include "pdfdocumentmerger.h"

#include <QDialog>

#include <vector>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace pdfviewer
{

/// "Insert Pages from File": which pages of an already loaded PDF go where in the open document.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFInsertPagesDialog : public QDialog
{
    Q_OBJECT

public:
    /// \param source Loaded PDF to take pages from
    /// \param targetPageCount Pages of the open document
    /// \param currentPageIndex Zero based page shown now; the default position is "after" it
    PDFInsertPagesDialog(const pdf::PDFDocumentMerger::Source& source, pdf::PDFInteger targetPageCount, pdf::PDFInteger currentPageIndex, QWidget* parent);

    /// Zero based pages of the source, in insertion order. Valid after accept().
    const std::vector<pdf::PDFInteger>& getPages() const { return m_pages; }

    /// Zero based index the first inserted page gets in the open document.
    pdf::PDFInteger getInsertIndex() const;

    virtual void accept() override;

private:
    pdf::PDFInteger m_sourcePageCount;
    QLineEdit* m_pagesEdit;
    QComboBox* m_positionCombo;
    QSpinBox* m_pageSpin;
    std::vector<pdf::PDFInteger> m_pages;
};

/// "Split Document": the open document is written as several new PDFs. Nothing open is changed.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFSplitDocumentDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        EveryPage,
        EveryNPages,
        AtPages     ///< New file starts at each listed page number
    };

    struct Plan
    {
        QString directory;
        QString baseName;
        std::vector<std::vector<pdf::PDFInteger>> parts;    ///< Zero based pages of each output file, document order
    };

    PDFSplitDocumentDialog(pdf::PDFInteger pageCount, const QString& directory, const QString& baseName, QWidget* parent);

    /// Valid after accept().
    const Plan& getPlan() const { return m_plan; }

    static std::vector<std::vector<pdf::PDFInteger>> splitEveryN(pdf::PDFInteger pageCount, pdf::PDFInteger n);
    /// \p starts are zero based pages where a new part begins (any order, duplicates and page 0 allowed).
    static std::vector<std::vector<pdf::PDFInteger>> splitAtPages(pdf::PDFInteger pageCount, std::vector<pdf::PDFInteger> starts);

    virtual void accept() override;

private:
    void updateModeWidgets();

    pdf::PDFInteger m_pageCount;
    QComboBox* m_modeCombo;
    QSpinBox* m_everyNSpin;
    QLineEdit* m_pagesEdit;
    QLineEdit* m_directoryEdit;
    QLineEdit* m_baseNameEdit;
    Plan m_plan;
};

}   // namespace pdfviewer

#endif // PDFPAGEINSERTSPLITDIALOGS_H
