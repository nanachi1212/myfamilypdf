// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFSPLITDOCUMENTDIALOG_H
#define PDFSPLITDOCUMENTDIALOG_H

#include "pdf4qtlibgui_export.h"
#include "pdfglobal.h"

#include <QDialog>

#include <vector>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace pdfviewer
{

/// "Split Document": the open document is written as several new PDFs. Nothing open is changed.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFSplitDocumentDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        EveryPage,
        EveryNPages,
        AtPages     ///< A new file starts at each listed page number
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
    /// \p starts are zero based pages where a new part begins (any order; duplicates and page 0 are allowed).
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

#endif // PDFSPLITDOCUMENTDIALOG_H
