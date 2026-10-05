// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfmergepdfsdialog.h"

#include "pdfwidgetutils.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <climits>

#include "pdfdbgheap.h"

namespace pdfviewer
{

namespace
{

constexpr int SourceIdRole = Qt::UserRole;

QString allPagesText()
{
    return PDFMergePdfsDialog::tr("All pages");
}

/// Only the page range column can be edited.
class RangeOnlyDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return index.column() == 2 ? QStyledItemDelegate::createEditor(parent, option, index) : nullptr;
    }
};

}   // namespace

PDFMergePdfsDialog::PDFMergePdfsDialog(const Request& request, QWidget* parent) :
    QDialog(parent),
    m_request(request)
{
    setWindowTitle(tr("Merge PDFs"));

    QLabel* introLabel = new QLabel(tr("Add the PDFs to merge, put them in the order you want and choose the pages of each one. "
                                       "The result is saved as a new PDF; the PDFs in the list are not changed."), this);
    introLabel->setWordWrap(true);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(ColumnCount);
    m_list->setHeaderLabels({ tr("File"), tr("Pages"), tr("Page range") });
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragEnabled(true);
    m_list->setAcceptDrops(true);
    m_list->setDropIndicatorShown(true);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_list->setItemDelegate(new RangeOnlyDelegate(m_list));
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(ColumnPages, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(ColumnRange, QHeaderView::Interactive);
    m_list->header()->resizeSection(ColumnRange, 180);
    m_list->headerItem()->setToolTip(ColumnRange, tr("all, or pages and ranges such as 1-3,8,10-12. The order you type is kept: 3,1 gives page 3 and then page 1."));

    m_addButton = new QPushButton(tr("&Add Files..."), this);
    m_addCurrentButton = new QPushButton(tr("Add &Open Document"), this);
    m_addCurrentButton->setToolTip(tr("Adds the open document as it is now, including changes that are not saved yet."));
    m_addCurrentButton->setVisible(bool(m_request.currentDocument.document));
    m_removeButton = new QPushButton(tr("&Remove"), this);
    m_upButton = new QPushButton(tr("Move &Up"), this);
    m_downButton = new QPushButton(tr("Move &Down"), this);

    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_addCurrentButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_upButton);
    buttonLayout->addWidget(m_downButton);
    buttonLayout->addStretch(1);

    QHBoxLayout* listLayout = new QHBoxLayout();
    listLayout->addWidget(m_list, 1);
    listLayout->addLayout(buttonLayout);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setTextFormat(Qt::PlainText);

    m_outputEdit = new QLineEdit(this);
    m_browseButton = new QPushButton(tr("&Browse..."), this);
    QLabel* outputLabel = new QLabel(tr("&Output file:"), this);
    outputLabel->setBuddy(m_outputEdit);
    QHBoxLayout* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_browseButton);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    m_mergeButton = new QPushButton(tr("&Merge"), this);
    m_mergeButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("&Cancel"), this);
    m_cancelButton->setVisible(false);
    m_closeButton = new QPushButton(tr("C&lose"), this);
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(m_mergeButton);
    bottomLayout->addWidget(m_cancelButton);
    bottomLayout->addWidget(m_closeButton);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(introLabel);
    mainLayout->addLayout(listLayout, 1);
    mainLayout->addWidget(m_infoLabel);
    mainLayout->addLayout(outputLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(bottomLayout);

    m_list->setObjectName(QStringLiteral("mergeList"));
    m_addButton->setObjectName(QStringLiteral("mergeAddButton"));
    m_addCurrentButton->setObjectName(QStringLiteral("mergeAddCurrentButton"));
    m_removeButton->setObjectName(QStringLiteral("mergeRemoveButton"));
    m_upButton->setObjectName(QStringLiteral("mergeUpButton"));
    m_downButton->setObjectName(QStringLiteral("mergeDownButton"));
    m_infoLabel->setObjectName(QStringLiteral("mergeInfoLabel"));
    m_outputEdit->setObjectName(QStringLiteral("mergeOutputEdit"));
    m_browseButton->setObjectName(QStringLiteral("mergeBrowseButton"));
    m_progressBar->setObjectName(QStringLiteral("mergeProgressBar"));
    m_statusLabel->setObjectName(QStringLiteral("mergeStatusLabel"));
    m_mergeButton->setObjectName(QStringLiteral("mergeButton"));
    m_cancelButton->setObjectName(QStringLiteral("mergeCancelButton"));
    m_closeButton->setObjectName(QStringLiteral("mergeCloseButton"));

    connect(m_addButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::onAddFilesClicked);
    connect(m_addCurrentButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::addCurrentDocument);
    connect(m_removeButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::onRemoveClicked);
    connect(m_upButton, &QPushButton::clicked, this, [this]() { onMoveClicked(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { onMoveClicked(+1); });
    connect(m_browseButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::onBrowseClicked);
    connect(m_mergeButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::onMergeClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::onCancelClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &PDFMergePdfsDialog::reject);
    connect(m_list, &QTreeWidget::itemChanged, this, &PDFMergePdfsDialog::onItemChanged);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &PDFMergePdfsDialog::updateState);
    connect(m_list->model(), &QAbstractItemModel::rowsInserted, this, &PDFMergePdfsDialog::updateState);
    connect(m_list->model(), &QAbstractItemModel::rowsRemoved, this, &PDFMergePdfsDialog::updateState);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &PDFMergePdfsDialog::updateState);
    connect(&m_watcher, &QFutureWatcher<pdf::PDFOperationResult>::finished, this, &PDFMergePdfsDialog::onMergeFinished);

    pdf::PDFWidgetUtils::scaleWidget(this, QSize(720, 520));
    pdf::PDFWidgetUtils::style(this);

    updateState();
}

PDFMergePdfsDialog::~PDFMergePdfsDialog()
{
    if (m_running)
    {
        m_cancelRequested = true;
        m_watcher.disconnect();
        m_watcher.waitForFinished();
    }
}

void PDFMergePdfsDialog::appendSource(const pdf::PDFDocumentMerger::Source& source, bool isCurrentDocument)
{
    const int sourceId = m_nextSourceId++;
    m_sources[sourceId] = source;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(ColumnFile, isCurrentDocument ? tr("%1 (open document)").arg(source.displayName) : source.displayName);
    item->setToolTip(ColumnFile, source.fileName.isEmpty() ? source.displayName : QDir::toNativeSeparators(source.fileName));
    item->setText(ColumnPages, QString::number(source.pageCount));
    item->setTextAlignment(ColumnPages, Qt::AlignRight | Qt::AlignVCenter);
    item->setText(ColumnRange, allPagesText());
    item->setData(ColumnFile, SourceIdRole, sourceId);
    item->setFlags((item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsEditable) & ~Qt::ItemIsDropEnabled);
    m_list->addTopLevelItem(item);
}

void PDFMergePdfsDialog::addFiles(const QStringList& fileNames)
{
    QStringList problems;
    for (const QString& fileName : fileNames)
    {
        auto queryPassword = [this, &fileName](bool* ok)
        {
            if (m_request.passwordCallback)
            {
                return m_request.passwordCallback(ok);
            }
            return QInputDialog::getText(this, tr("Encrypted document"),
                                         tr("Enter password to access document content") + QLatin1Char('\n') + QFileInfo(fileName).fileName(),
                                         QLineEdit::Password, QString(), ok);
        };

        const pdf::PDFDocumentMerger::LoadResult result = pdf::PDFDocumentMerger::loadSource(fileName, queryPassword);
        switch (result.status)
        {
            case pdf::PDFDocumentMerger::LoadStatus::OK:
                appendSource(result.source, false);
                break;

            case pdf::PDFDocumentMerger::LoadStatus::Cancelled:
                break;      // the user did not want to enter the password

            case pdf::PDFDocumentMerger::LoadStatus::WrongPassword:
            case pdf::PDFDocumentMerger::LoadStatus::Failed:
                problems << tr("%1: %2").arg(QFileInfo(fileName).fileName(), result.errorMessage);
                break;
        }
        if (!fileName.isEmpty())
        {
            m_request.directory = QFileInfo(fileName).absolutePath();
        }
    }

    if (!problems.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), tr("These files cannot be merged and were not added:") + QLatin1Char('\n') + problems.join(QLatin1Char('\n')));
    }
}

void PDFMergePdfsDialog::addCurrentDocument()
{
    if (m_request.currentDocument.document)
    {
        appendSource(m_request.currentDocument, true);
    }
}

void PDFMergePdfsDialog::onAddFilesClicked()
{
    const QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Add PDF Files"), m_request.directory, tr("PDF document (*.pdf)"));
    if (!fileNames.isEmpty())
    {
        addFiles(fileNames);
    }
}

void PDFMergePdfsDialog::onRemoveClicked()
{
    const QList<QTreeWidgetItem*> items = m_list->selectedItems();
    for (QTreeWidgetItem* item : items)
    {
        m_sources.erase(item->data(ColumnFile, SourceIdRole).toInt());
        delete item;
    }
    updateState();
}

void PDFMergePdfsDialog::onMoveClicked(int direction)
{
    // Move the selected rows one step, as a block, keeping their relative order.
    QList<int> rows;
    for (QTreeWidgetItem* item : m_list->selectedItems())
    {
        rows << m_list->indexOfTopLevelItem(item);
    }
    std::sort(rows.begin(), rows.end());
    if (direction > 0)
    {
        std::reverse(rows.begin(), rows.end());
    }

    for (const int row : rows)
    {
        const int target = row + direction;
        if (target < 0 || target >= m_list->topLevelItemCount() || m_list->topLevelItem(target)->isSelected())
        {
            continue;       // at the border (or blocked by another selected row that could not move)
        }
        QTreeWidgetItem* item = m_list->takeTopLevelItem(row);
        m_list->insertTopLevelItem(target, item);
        item->setSelected(true);
    }
    updateState();
}

void PDFMergePdfsDialog::onBrowseClicked()
{
    QString current = m_outputEdit->text().trimmed();
    if (current.isEmpty())
    {
        current = getSuggestedOutputFile();
    }
    const QString fileName = QFileDialog::getSaveFileName(this, tr("Save Merged PDF"), current, tr("PDF document (*.pdf)"));
    if (!fileName.isEmpty())
    {
        m_outputEdit->setText(QDir::toNativeSeparators(fileName));
    }
}

QString PDFMergePdfsDialog::getSuggestedOutputFile() const
{
    QString directory = m_request.directory;
    if (directory.isEmpty() && m_list->topLevelItemCount() > 0)
    {
        const auto it = m_sources.find(m_list->topLevelItem(0)->data(ColumnFile, SourceIdRole).toInt());
        if (it != m_sources.end() && !it->second.fileName.isEmpty())
        {
            directory = QFileInfo(it->second.fileName).absolutePath();
        }
    }
    return QDir(directory).filePath(tr("merged.pdf"));
}

bool PDFMergePdfsDialog::createEntry(QTreeWidgetItem* item, pdf::PDFDocumentMerger::Entry* entry, QString* errorMessage) const
{
    const auto it = m_sources.find(item->data(ColumnFile, SourceIdRole).toInt());
    if (it == m_sources.end())
    {
        *errorMessage = tr("Missing document.");
        return false;
    }

    QString text = item->text(ColumnRange).trimmed();
    if (text.compare(allPagesText(), Qt::CaseInsensitive) == 0)
    {
        text = QStringLiteral("all");
    }
    entry->source = it->second;
    entry->pages = pdf::PDFDocumentMerger::parsePageList(it->second.pageCount, text, errorMessage);
    return errorMessage->isEmpty();
}

std::vector<pdf::PDFDocumentMerger::Entry> PDFMergePdfsDialog::createEntries(QString* errorMessage, QTreeWidgetItem** errorItem) const
{
    std::vector<pdf::PDFDocumentMerger::Entry> entries;
    for (int row = 0; row < m_list->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* item = m_list->topLevelItem(row);
        QString error;
        pdf::PDFDocumentMerger::Entry entry;
        if (!createEntry(item, &entry, &error))
        {
            *errorMessage = tr("%1: %2").arg(item->text(ColumnFile), error);
            *errorItem = item;
            return { };
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void PDFMergePdfsDialog::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != ColumnRange)
    {
        return;
    }
    QString error;
    pdf::PDFDocumentMerger::Entry entry;
    createEntry(item, &entry, &error);
    item->setToolTip(ColumnRange, error);
    item->setForeground(ColumnRange, error.isEmpty() ? QBrush() : QBrush(Qt::red));
    updateState();
}

void PDFMergePdfsDialog::updateState()
{
    const bool hasRows = m_list->topLevelItemCount() > 0;
    const QList<QTreeWidgetItem*> selected = m_list->selectedItems();
    int firstRow = INT_MAX;
    int lastRow = -1;
    for (QTreeWidgetItem* item : selected)
    {
        const int row = m_list->indexOfTopLevelItem(item);
        firstRow = qMin(firstRow, row);
        lastRow = qMax(lastRow, row);
    }

    const bool enabled = !m_running;
    m_list->setEnabled(enabled);
    m_addButton->setEnabled(enabled);
    m_addCurrentButton->setEnabled(enabled);
    m_removeButton->setEnabled(enabled && !selected.isEmpty());
    m_upButton->setEnabled(enabled && !selected.isEmpty() && firstRow > 0);
    m_downButton->setEnabled(enabled && !selected.isEmpty() && lastRow < m_list->topLevelItemCount() - 1);
    m_outputEdit->setEnabled(enabled);
    m_browseButton->setEnabled(enabled);
    m_mergeButton->setEnabled(enabled && hasRows);

    // What would be merged, and what the user must know first.
    QStringList lines;
    std::vector<pdf::PDFDocumentMerger::Entry> entries;
    qint64 totalPages = 0;
    for (int row = 0; row < m_list->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* item = m_list->topLevelItem(row);
        QString error;
        pdf::PDFDocumentMerger::Entry entry;
        if (!createEntry(item, &entry, &error))
        {
            lines << tr("%1: %2").arg(item->text(ColumnFile), error);
            continue;
        }
        totalPages += qint64(entry.pages.size());
        entries.push_back(std::move(entry));
    }

    if (hasRows)
    {
        const pdf::PDFDocumentMerger::Report report = pdf::PDFDocumentMerger::analyze(entries);
        for (const QString& blocker : report.blockers)
        {
            lines << tr("Cannot merge: %1").arg(blocker);
        }
        for (const QString& warning : report.warnings)
        {
            lines << tr("Warning: %1").arg(warning);
        }
        if (lines.isEmpty())
        {
            lines << tr("%1 page(s) will be written.").arg(totalPages);
        }
    }
    m_infoLabel->setText(lines.join(QLatin1Char('\n')));
}

void PDFMergePdfsDialog::setRunning(bool running)
{
    m_running = running;
    m_progressBar->setVisible(running);
    m_cancelButton->setVisible(running);
    m_cancelButton->setEnabled(running);
    m_closeButton->setEnabled(!running);
    updateState();
}

void PDFMergePdfsDialog::onMergeClicked()
{
    if (m_running)
    {
        return;
    }

    QString error;
    QTreeWidgetItem* errorItem = nullptr;
    const std::vector<pdf::PDFDocumentMerger::Entry> entries = createEntries(&error, &errorItem);
    if (!error.isEmpty())
    {
        m_list->setCurrentItem(errorItem);
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    const pdf::PDFDocumentMerger::Report report = pdf::PDFDocumentMerger::analyze(entries);
    if (!report.blockers.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), tr("The PDFs cannot be merged:") + QLatin1Char('\n') + report.blockers.join(QLatin1Char('\n')));
        return;
    }

    QString output = m_outputEdit->text().trimmed();
    if (output.isEmpty())
    {
        onBrowseClicked();
        output = m_outputEdit->text().trimmed();
        if (output.isEmpty())
        {
            return;
        }
    }
    if (QFileInfo(output).suffix().isEmpty())
    {
        output += QStringLiteral(".pdf");
        m_outputEdit->setText(QDir::toNativeSeparators(output));
    }
    for (const pdf::PDFDocumentMerger::Entry& entry : entries)
    {
        if (!entry.source.fileName.isEmpty() && QFileInfo(entry.source.fileName) == QFileInfo(output))
        {
            QMessageBox::warning(this, windowTitle(), tr("The output file must be different from the PDFs in the list (%1). Choose another file name.").arg(entry.source.displayName));
            return;
        }
    }
    if (QFileInfo::exists(output) &&
        QMessageBox::question(this, windowTitle(), tr("%1 already exists. Do you want to replace it?").arg(QDir::toNativeSeparators(output)),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    if (!report.warnings.isEmpty() &&
        QMessageBox::warning(this, windowTitle(),
                             report.warnings.join(QStringLiteral("\n\n")) + QStringLiteral("\n\n") + tr("Do you want to merge anyway?"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    m_runningOutputFile = output;
    m_runningPageCount = 0;
    for (const pdf::PDFDocumentMerger::Entry& entry : entries)
    {
        m_runningPageCount += int(entry.pages.size());
    }
    m_cancelRequested = false;
    m_cancelled = false;
    m_statusLabel->setText(tr("Merging %1 page(s)...").arg(m_runningPageCount));
    setRunning(true);
    m_watcher.setFuture(QtConcurrent::run([this, entries, output]()
    {
        bool cancelled = false;
        const pdf::PDFOperationResult result = pdf::PDFDocumentMerger::mergeToFile(entries, output, &m_cancelRequested, &cancelled);
        m_cancelled = cancelled;
        return result;
    }));
}

void PDFMergePdfsDialog::onCancelClicked()
{
    if (m_running)
    {
        m_cancelRequested = true;
        m_cancelButton->setEnabled(false);
        m_statusLabel->setText(tr("Cancelling..."));
    }
}

void PDFMergePdfsDialog::onMergeFinished()
{
    const pdf::PDFOperationResult result = m_watcher.result();
    setRunning(false);

    if (result)
    {
        m_outputFile = m_runningOutputFile;
        m_statusLabel->setText(tr("Saved %1 page(s) to %2.").arg(m_runningPageCount).arg(QDir::toNativeSeparators(m_outputFile)));
        if (!m_closeWhenFinished)
        {
            QMessageBox box(QMessageBox::Information, windowTitle(), m_statusLabel->text(), QMessageBox::NoButton, this);
            QPushButton* openButton = box.addButton(tr("&Open Merged PDF"), QMessageBox::AcceptRole);
            box.addButton(QMessageBox::Close);
            box.exec();
            if (box.clickedButton() == openButton)
            {
                m_openOutputRequested = true;
                accept();
                return;
            }
        }
    }
    else if (m_cancelled)
    {
        m_statusLabel->setText(tr("The merge was cancelled. No file was written."));
    }
    else
    {
        m_statusLabel->setText(tr("The merge failed. No file was written."));
        if (!m_closeWhenFinished)
        {
            QMessageBox::critical(this, windowTitle(), result.getErrorMessage() + QStringLiteral("\n\n") + tr("No file was written."));
        }
    }

    if (m_closeWhenFinished)
    {
        m_closeWhenFinished = false;
        QDialog::reject();
    }
}

void PDFMergePdfsDialog::reject()
{
    if (m_running)
    {
        // Stop the merge first; the dialog closes when the worker is done.
        m_closeWhenFinished = true;
        onCancelClicked();
        return;
    }
    QDialog::reject();
}

void PDFMergePdfsDialog::closeEvent(QCloseEvent* event)
{
    if (m_running)
    {
        reject();
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

}   // namespace pdfviewer
