// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFINSERTPAGESDIALOG_H
#define PDFINSERTPAGESDIALOG_H

#include "pdf4qtlibgui_export.h"
#include "pdfdocumentmerger.h"
#include "pdfpageinserter.h"

#include <QDialog>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;

namespace pdfviewer
{

/// Editor "Insert Blank Page..." / "Insert Pages from PDF...": asks for the page size or the source PDF and
/// its pages, and for the position. It shows the actual anchor page and where the new pages end up.
/// The dialog changes nothing; the controller inserts after the dialog was accepted.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFInsertPagesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        BlankPage,
        PagesFromPdf
    };

    struct Request
    {
        Mode mode = Mode::BlankPage;
        const pdf::PDFDocument* document = nullptr;         ///< Target document (page sizes and page count)
        std::vector<pdf::PDFInteger> anchorPages;           ///< Selected pages or the current page, sorted
        QString directory;                                  ///< Folder the file dialog starts in
        std::function<QString(bool*)> passwordCallback;     ///< Replaces the password prompt (used by tests); empty: ask the user
    };

    explicit PDFInsertPagesDialog(const Request& request, QWidget* parent);
    virtual ~PDFInsertPagesDialog() override;

    pdf::PDFPageInserter::Position getPosition() const;
    pdf::PDFInteger getInsertIndex() const;

    /// Blank page: the page whose size is used ("same as page"), or -1 for A4.
    pdf::PDFInteger getSizePage() const;

    /// Pages from PDF: loads the source (asks for a password if needed). Returns false if it cannot be used.
    bool loadSourceFile(const QString& fileName);
    const pdf::PDFDocumentMerger::Source& getSource() const { return m_source; }

    /// Pages from PDF: the source pages (zero based, in insert order); valid after the dialog was accepted.
    const std::vector<pdf::PDFInteger>& getSourcePages() const { return m_sourcePages; }

    virtual void accept() override;

private:
    void updateTexts();
    void onBrowseClicked();

    Request m_request;
    QComboBox* m_positionComboBox = nullptr;
    QComboBox* m_sizeComboBox = nullptr;
    QLineEdit* m_fileEdit = nullptr;
    QLineEdit* m_pagesEdit = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    pdf::PDFDocumentMerger::Source m_source;
    std::vector<pdf::PDFInteger> m_sourcePages;
};

}   // namespace pdfviewer

#endif // PDFINSERTPAGESDIALOG_H
