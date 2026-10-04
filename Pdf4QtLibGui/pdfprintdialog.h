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

#ifndef PDFPRINTDIALOG_H
#define PDFPRINTDIALOG_H

#include "pdf4qtlibgui_export.h"
#include "pdfpageoutput.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QImage>
#include <QPageLayout>
#include <QPrinter>
#include <QRectF>
#include <QTimer>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;

namespace pdfviewer
{

/// Print dialog with page selection, layout options and a live preview of the sheet.
/// The preview renders one page at a time, so opening the dialog never processes
/// the document as a whole.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFPrintDialog : public QDialog
{
    Q_OBJECT

public:
    struct PageSelectionInfo
    {
        pdf::PDFInteger pageCount = 0;
        std::vector<pdf::PDFInteger> currentPages;      ///< Pages shown in the viewer
        std::vector<pdf::PDFInteger> selectedPages;     ///< Pages selected in the thumbnails
        bool preferSelectedPages = false;               ///< Start with the selected pages option
    };

    explicit PDFPrintDialog(const PDFPageOutputContext& outputContext,
                            const PageSelectionInfo& pageInfo,
                            const QString& documentName,
                            QPrinter::PrinterMode printerMode,
                            QWidget* parent);
    virtual ~PDFPrintDialog() override;

    /// Returns the print options. Valid after the dialog has been accepted.
    PDFPrintOptions getOptions() const;

    /// Returns the configured printer. Valid after the dialog has been accepted.
    std::unique_ptr<QPrinter> takePrinter();

    /// Returns the pages selected in the dialog. If the selection is invalid,
    /// the result is empty and \p errorMessage describes the problem.
    std::vector<pdf::PDFInteger> getSelectedPages(QString* errorMessage = nullptr) const;

    virtual void accept() override;

private:
    struct PreviewResult
    {
        QImage image;
        QRectF placement;   ///< Placement of the page on the preview, in preview pixels
        QRectF printable;   ///< Printable area on the preview, in preview pixels
        QSizeF sheet;       ///< Sheet size in preview pixels
        QString error;
    };

    void onPrinterChanged();
    void onOptionsChanged();
    void onPreviewPageChanged(int delta);
    void schedulePreview();
    void startPreview();
    void onPreviewFinished();
    void updatePreviewNavigation();
    void applyPrinterSettings(QPrinter* printer) const;
    PDFPrintOrientation getOrientation() const;
    PDFPrintScaling getScaling() const;
    QString describePages(const std::vector<pdf::PDFInteger>& pages) const;

    PDFPageOutputContext m_outputContext;
    PageSelectionInfo m_pageInfo;
    QString m_documentName;
    QPrinter::PrinterMode m_printerMode;
    std::unique_ptr<PDFPageImageExporter> m_previewExporter;
    std::unique_ptr<QPrinter> m_printer;

    QComboBox* m_printerCombo = nullptr;
    QComboBox* m_paperCombo = nullptr;
    QComboBox* m_orientationCombo = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_duplexCombo = nullptr;
    QSpinBox* m_copiesSpin = nullptr;
    QCheckBox* m_collateCheck = nullptr;
    QRadioButton* m_allPagesRadio = nullptr;
    QRadioButton* m_currentPageRadio = nullptr;
    QRadioButton* m_selectedPagesRadio = nullptr;
    QRadioButton* m_rangeRadio = nullptr;
    QLineEdit* m_rangeEdit = nullptr;
    QRadioButton* m_fitRadio = nullptr;
    QRadioButton* m_actualSizeRadio = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_previewPageLabel = nullptr;
    QToolButton* m_previousButton = nullptr;
    QToolButton* m_nextButton = nullptr;
    QPushButton* m_printButton = nullptr;

    QTimer m_previewTimer;
    QFutureWatcher<PreviewResult> m_previewWatcher;
    bool m_previewRestartRequested = false;
    int m_previewPageIndex = 0;     ///< Index into the list of selected pages
    int m_previewPageCount = 0;
    bool m_isLoading = false;
};

}   // namespace pdfviewer

#endif // PDFPRINTDIALOG_H
