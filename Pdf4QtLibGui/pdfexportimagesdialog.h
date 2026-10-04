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

#ifndef PDFEXPORTIMAGESDIALOG_H
#define PDFEXPORTIMAGESDIALOG_H

#include "pdf4qtlibgui_export.h"
#include "pdfpageoutput.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <map>
#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QGroupBox;

namespace pdfviewer
{

/// Exports pages (or selected regions of pages) as PNG or JPEG files, one file per page.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFExportImagesDialog : public QDialog
{
    Q_OBJECT

public:
    struct Request
    {
        PDFPageOutputContext outputContext;
        QString documentFileName;                       ///< Original document file, can be empty
        pdf::PDFInteger pageCount = 0;
        std::vector<pdf::PDFInteger> currentPages;      ///< Pages shown in the viewer
        std::vector<pdf::PDFInteger> selectedPages;     ///< Pages selected in the thumbnails
        bool preferSelectedPages = false;
        std::map<pdf::PDFInteger, QRectF> selectionRegions; ///< Regions (page space) of the selection; if not empty, only those are exported
    };

    explicit PDFExportImagesDialog(const Request& request, QWidget* parent);
    virtual ~PDFExportImagesDialog() override;

    /// Returns the result of the last finished export.
    const PDFImageExportResult& getResult() const { return m_result; }

    /// Returns the pages chosen in the dialog (document order, each page once). If the
    /// choice is invalid, the result is empty and \p errorMessage describes the problem.
    std::vector<pdf::PDFInteger> getSelectedPages(QString* errorMessage = nullptr) const;

    virtual void reject() override;

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    void onOptionsChanged();
    void onBrowseClicked();
    void onExportClicked();
    void onProgressTimer();
    void onExportFinished();
    void setRunning(bool running);
    bool isRunning() const { return m_running; }
    std::vector<PDFImageExportTarget> createTargets(QString* errorMessage) const;
    PDFImageFormat getFormat() const;
    QString getBaseName() const;

    Request m_request;
    PDFImageExportResult m_result;
    std::unique_ptr<PDFPageImageExporter> m_exporter;
    QFutureWatcher<PDFImageExportResult> m_watcher;
    QTimer m_progressTimer;
    std::atomic_bool m_cancelRequested { false };
    std::atomic_int m_processed { 0 };
    int m_totalTargets = 0;
    bool m_running = false;

    QGroupBox* m_pagesGroup = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QRadioButton* m_currentPageRadio = nullptr;
    QRadioButton* m_allPagesRadio = nullptr;
    QRadioButton* m_selectedPagesRadio = nullptr;
    QRadioButton* m_rangeRadio = nullptr;
    QLineEdit* m_rangeEdit = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QSpinBox* m_dpiSpin = nullptr;
    QSpinBox* m_qualitySpin = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_filesLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_closeButton = nullptr;
};

}   // namespace pdfviewer

#endif // PDFEXPORTIMAGESDIALOG_H
