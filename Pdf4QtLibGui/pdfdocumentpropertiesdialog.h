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

#ifndef PDFDOCUMENTPROPERTIESDIALOG_H
#define PDFDOCUMENTPROPERTIESDIALOG_H

#include "pdfglobal.h"
#include "pdf4qtlibgui_export.h"

#include <QDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>

class QTreeWidgetItem;
class QLineEdit;
class QLabel;

namespace Ui
{
class PDFDocumentPropertiesDialog;
}

namespace pdf
{
class PDFDocument;
struct PDFDocumentInfo;
}

namespace pdfviewer
{

struct PDFFileInfo
{
    QString originalFileName;
    QString absoluteFilePath;
    QString fileName;
    QString path;
    pdf::PDFInteger fileSize = 0;
    bool writable = false;
    QDateTime creationTime;
    QDateTime lastModifiedTime;
    QDateTime lastReadTime;
};

class PDF4QTLIBGUILIBSHARED_EXPORT PDFDocumentPropertiesDialog : public QDialog
{
    Q_OBJECT

private:
    using BaseClass = QDialog;

public:
    /// \param canEditInfo Title, Author, Subject, Keywords and Creator can be edited
    explicit PDFDocumentPropertiesDialog(const pdf::PDFDocument* document,
                                         const PDFFileInfo* fileInfo,
                                         QWidget* parent,
                                         bool canEditInfo = false);
    virtual ~PDFDocumentPropertiesDialog() override;

    QByteArray getXMPMetadata() const;
    bool isXMPMetadataModified() const;

    /// Document information entries changed by the user, as (key, new value).
    /// An empty value means the entry should be removed.
    std::vector<std::pair<QByteArray, QString>> getModifiedInfoEntries() const;

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    Ui::PDFDocumentPropertiesDialog* ui;

    void initializeProperties(const pdf::PDFDocument* document, bool canEditInfo);
    void initializeFileInfoProperties(const PDFFileInfo* fileInfo);
    void initializeSecurity(const pdf::PDFDocument* document);
    void initializeFonts(const pdf::PDFDocument* document);
    void initializeDisplayAndPrintSettings(const pdf::PDFDocument* document);
    void initializeXMPMetadata(const pdf::PDFDocument* document);
    void createDefaultXMPMetadata();
    void updateXMPMismatchHint();

    void onFontsFinished();

    struct InfoEditor
    {
        QByteArray key;
        QString originalValue;
        QLineEdit* edit = nullptr;
    };

    std::vector<InfoEditor> m_infoEditors;
    QLabel* m_xmpMismatchLabel = nullptr;

    std::vector<QTreeWidgetItem*> m_fontTreeWidgetItems;
    QFuture<void> m_future;
    QFutureWatcher<void> m_futureWatcher;
    QString m_originalXMPMetadataText;
    const pdf::PDFDocument* m_document = nullptr;
    bool m_hasOriginalXMPMetadataStream = false;
};

}   // namespace pdfviewer

#endif // PDFDOCUMENTPROPERTIESDIALOG_H
