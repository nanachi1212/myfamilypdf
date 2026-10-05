// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFMERGEPDFSDIALOG_H
#define PDFMERGEPDFSDIALOG_H

#include "pdf4qtlibgui_export.h"
#include "pdfdocumentmerger.h"

#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <functional>
#include <map>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace pdfviewer
{

/// "Merge PDFs": choose several PDFs, order them, pick the pages of each, and write ONE NEW PDF.
/// Nothing that is open or listed is changed. The merge itself is pdf::PDFDocumentMerger.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFMergePdfsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Request
    {
        QString directory;                                  ///< Folder the file dialogs start in
        pdf::PDFDocumentMerger::Source currentDocument;     ///< The open document (no document: nothing to add)
        std::function<QString(bool*)> passwordCallback;     ///< Replaces the password prompt (used by tests); empty: ask the user
    };

    explicit PDFMergePdfsDialog(const Request& request, QWidget* parent);
    virtual ~PDFMergePdfsDialog() override;

    /// Adds PDF files to the end of the list. Files that cannot be used are reported in one message.
    void addFiles(const QStringList& fileNames);

    /// Adds the open document (as it is in memory, including unsaved changes) to the end of the list.
    void addCurrentDocument();

    /// File written by the last successful merge, empty if there was none.
    const QString& getOutputFile() const { return m_outputFile; }

    /// True if the user asked to open the merged file after the merge.
    bool isOpenOutputRequested() const { return m_openOutputRequested; }

    virtual void reject() override;

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    enum
    {
        ColumnFile,
        ColumnPages,
        ColumnRange,
        ColumnCount
    };

    void onAddFilesClicked();
    void onRemoveClicked();
    void onMoveClicked(int direction);
    void onBrowseClicked();
    void onMergeClicked();
    void onCancelClicked();
    void onMergeFinished();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void updateState();
    void setRunning(bool running);
    void appendSource(const pdf::PDFDocumentMerger::Source& source, bool isCurrentDocument);
    bool createEntry(QTreeWidgetItem* item, pdf::PDFDocumentMerger::Entry* entry, QString* errorMessage) const;
    std::vector<pdf::PDFDocumentMerger::Entry> createEntries(QString* errorMessage, QTreeWidgetItem** errorItem) const;
    QString getSuggestedOutputFile() const;

    Request m_request;
    std::map<int, pdf::PDFDocumentMerger::Source> m_sources;    ///< By the id stored in the list rows
    int m_nextSourceId = 1;
    QString m_outputFile;
    QString m_runningOutputFile;
    bool m_openOutputRequested = false;
    bool m_running = false;
    bool m_closeWhenFinished = false;
    std::atomic_bool m_cancelRequested { false };
    std::atomic_bool m_cancelled { false };
    int m_runningPageCount = 0;
    QFutureWatcher<pdf::PDFOperationResult> m_watcher;

    QTreeWidget* m_list = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_addCurrentButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_mergeButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_closeButton = nullptr;
};

}   // namespace pdfviewer

#endif // PDFMERGEPDFSDIALOG_H
