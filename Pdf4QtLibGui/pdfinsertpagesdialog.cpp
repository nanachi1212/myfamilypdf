// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfinsertpagesdialog.h"

#include "pdfcatalog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "pdfdbgheap.h"

namespace pdfviewer
{

PDFInsertPagesDialog::PDFInsertPagesDialog(const Request& request, QWidget* parent) :
    QDialog(parent),
    m_request(request)
{
    const bool blank = m_request.mode == Mode::BlankPage;
    setWindowTitle(blank ? tr("Insert Blank Page") : tr("Insert Pages from PDF"));
    setObjectName(QStringLiteral("insertPagesDialog"));

    QFormLayout* form = new QFormLayout();
    if (blank)
    {
        m_sizeComboBox = new QComboBox(this);
        m_sizeComboBox->setObjectName(QStringLiteral("insertSizeComboBox"));
        m_sizeComboBox->addItem(QString(), 0);
        m_sizeComboBox->addItem(tr("A4 (210 × 297 mm)"), -1);
        form->addRow(tr("Size:"), m_sizeComboBox);
    }
    else
    {
        m_fileEdit = new QLineEdit(this);
        m_fileEdit->setObjectName(QStringLiteral("insertFileEdit"));
        m_fileEdit->setReadOnly(true);
        QPushButton* browseButton = new QPushButton(tr("Browse..."), this);
        browseButton->setObjectName(QStringLiteral("insertBrowseButton"));
        connect(browseButton, &QPushButton::clicked, this, &PDFInsertPagesDialog::onBrowseClicked);
        QHBoxLayout* fileLayout = new QHBoxLayout();
        fileLayout->addWidget(m_fileEdit, 1);
        fileLayout->addWidget(browseButton);
        form->addRow(tr("File:"), fileLayout);

        m_sourceLabel = new QLabel(tr("Choose one PDF file."), this);
        m_sourceLabel->setObjectName(QStringLiteral("insertSourceLabel"));
        m_sourceLabel->setWordWrap(true);
        form->addRow(QString(), m_sourceLabel);

        m_pagesEdit = new QLineEdit(this);
        m_pagesEdit->setObjectName(QStringLiteral("insertPagesEdit"));
        m_pagesEdit->setPlaceholderText(tr("all, or for example 3,1 or 2-5"));
        m_pagesEdit->setToolTip(tr("Pages are inserted in the order typed. Each page can be used once."));
        connect(m_pagesEdit, &QLineEdit::textChanged, this, &PDFInsertPagesDialog::updateTexts);
        form->addRow(tr("Pages:"), m_pagesEdit);
    }

    m_positionComboBox = new QComboBox(this);
    m_positionComboBox->setObjectName(QStringLiteral("insertPositionComboBox"));
    const pdf::PDFInteger firstAnchor = m_request.anchorPages.empty() ? 0 : m_request.anchorPages.front();
    const pdf::PDFInteger lastAnchor = m_request.anchorPages.empty() ? 0 : m_request.anchorPages.back();
    m_positionComboBox->addItem(tr("Before page %1").arg(firstAnchor + 1), int(pdf::PDFPageInserter::Position::Before));
    m_positionComboBox->addItem(tr("After page %1").arg(lastAnchor + 1), int(pdf::PDFPageInserter::Position::After));
    m_positionComboBox->addItem(tr("At the beginning of the document"), int(pdf::PDFPageInserter::Position::Beginning));
    m_positionComboBox->addItem(tr("At the end of the document"), int(pdf::PDFPageInserter::Position::End));
    m_positionComboBox->setCurrentIndex(1);
    form->addRow(tr("Position:"), m_positionComboBox);

    m_positionLabel = new QLabel(this);
    m_positionLabel->setObjectName(QStringLiteral("insertPositionLabel"));
    m_positionLabel->setWordWrap(true);
    form->addRow(QString(), m_positionLabel);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("insertButtonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PDFInsertPagesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PDFInsertPagesDialog::reject);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttonBox);

    connect(m_positionComboBox, &QComboBox::currentIndexChanged, this, &PDFInsertPagesDialog::updateTexts);
    updateTexts();
    setMinimumWidth(420);
}

PDFInsertPagesDialog::~PDFInsertPagesDialog() = default;

pdf::PDFPageInserter::Position PDFInsertPagesDialog::getPosition() const
{
    return pdf::PDFPageInserter::Position(m_positionComboBox->currentData().toInt());
}

pdf::PDFInteger PDFInsertPagesDialog::getInsertIndex() const
{
    const pdf::PDFInteger pageCount = m_request.document ? pdf::PDFInteger(m_request.document->getCatalog()->getPageCount()) : 0;
    return pdf::PDFPageInserter::getInsertIndex(getPosition(), m_request.anchorPages, pageCount);
}

pdf::PDFInteger PDFInsertPagesDialog::getSizePage() const
{
    if (!m_sizeComboBox || m_sizeComboBox->currentData().toInt() < 0 || m_request.anchorPages.empty())
    {
        return -1;
    }
    return getPosition() == pdf::PDFPageInserter::Position::Before ? m_request.anchorPages.front() : m_request.anchorPages.back();
}

bool PDFInsertPagesDialog::loadSourceFile(const QString& fileName)
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
    m_request.directory = QFileInfo(fileName).absolutePath();
    if (result.status != pdf::PDFDocumentMerger::LoadStatus::OK)
    {
        if (result.status != pdf::PDFDocumentMerger::LoadStatus::Cancelled)
        {
            QMessageBox::warning(this, windowTitle(), tr("%1: %2").arg(QFileInfo(fileName).fileName(), result.errorMessage));
        }
        return false;
    }

    m_source = result.source;
    m_fileEdit->setText(QDir::toNativeSeparators(fileName));
    const QStringList blockers = pdf::PDFPageInserter::checkSource(m_source);
    if (blockers.isEmpty())
    {
        m_sourceLabel->setText(tr("%1 has %2 pages.").arg(m_source.displayName).arg(m_source.pageCount));
    }
    else
    {
        m_sourceLabel->setText(blockers.join(QLatin1Char('\n')));
    }
    updateTexts();
    return blockers.isEmpty();
}

void PDFInsertPagesDialog::accept()
{
    if (m_request.mode == Mode::PagesFromPdf)
    {
        if (!m_source.document)
        {
            QMessageBox::warning(this, windowTitle(), tr("Choose a PDF file first."));
            return;
        }
        const QStringList blockers = pdf::PDFPageInserter::checkSource(m_source);
        if (!blockers.isEmpty())
        {
            QMessageBox::warning(this, windowTitle(), blockers.join(QLatin1Char('\n')));
            return;
        }
        QString errorMessage;
        m_sourcePages = pdf::PDFPageInserter::parsePageSelection(m_source.pageCount, m_pagesEdit->text(), &errorMessage);
        if (!errorMessage.isEmpty())
        {
            QMessageBox::warning(this, windowTitle(), errorMessage);
            return;
        }
    }
    QDialog::accept();
}

void PDFInsertPagesDialog::updateTexts()
{
    const pdf::PDFInteger insertIndex = getInsertIndex();
    const pdf::PDFInteger pageCount = m_request.document ? pdf::PDFInteger(m_request.document->getCatalog()->getPageCount()) : 0;

    if (m_sizeComboBox)
    {
        const pdf::PDFInteger sizePage = getPosition() == pdf::PDFPageInserter::Position::Before
                                         ? (m_request.anchorPages.empty() ? 0 : m_request.anchorPages.front())
                                         : (m_request.anchorPages.empty() ? 0 : m_request.anchorPages.back());
        QString text = tr("Same as page %1").arg(sizePage + 1);
        if (m_request.document && sizePage < pageCount)
        {
            const QRectF box = m_request.document->getCatalog()->getPage(size_t(sizePage))->getRotatedMediaBoxMM();
            text = tr("Same as page %1 (%2 × %3 mm)").arg(sizePage + 1).arg(qRound(box.width())).arg(qRound(box.height()));
        }
        m_sizeComboBox->setItemText(0, text);
        m_positionLabel->setText(tr("The new page becomes page %1 of %2.").arg(insertIndex + 1).arg(pageCount + 1));
        return;
    }

    QString errorMessage;
    const std::vector<pdf::PDFInteger> pages = m_source.document
            ? pdf::PDFPageInserter::parsePageSelection(m_source.pageCount, m_pagesEdit->text(), &errorMessage)
            : std::vector<pdf::PDFInteger>();
    if (pages.empty())
    {
        m_positionLabel->setText(errorMessage.isEmpty() ? tr("The inserted pages start at page %1.").arg(insertIndex + 1) : errorMessage);
    }
    else
    {
        const pdf::PDFInteger count = pdf::PDFInteger(pages.size());
        m_positionLabel->setText(tr("The inserted pages become pages %1–%2 of %3.").arg(insertIndex + 1).arg(insertIndex + count).arg(pageCount + count));
    }
}

void PDFInsertPagesDialog::onBrowseClicked()
{
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Insert Pages from PDF"), m_request.directory, tr("PDF document (*.pdf)"));
    if (!fileName.isEmpty())
    {
        loadSourceFile(fileName);
    }
}

}   // namespace pdfviewer
