// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfsplitdocumentdialog.h"
#include "pdfdocumentmerger.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>

namespace pdfviewer
{

PDFSplitDocumentDialog::PDFSplitDocumentDialog(pdf::PDFInteger pageCount, const QString& directory, const QString& baseName, QWidget* parent) :
    QDialog(parent),
    m_pageCount(pageCount),
    m_modeCombo(new QComboBox(this)),
    m_everyNSpin(new QSpinBox(this)),
    m_pagesEdit(new QLineEdit(this)),
    m_directoryEdit(new QLineEdit(directory, this)),
    m_baseNameEdit(new QLineEdit(baseName, this))
{
    setWindowTitle(tr("Split Document"));
    QFormLayout* layout = new QFormLayout(this);
    layout->addRow(new QLabel(tr("Each part is saved as a new PDF named <base name>_p<pages>.pdf. The open document is not changed."), this));

    m_modeCombo->setObjectName(QStringLiteral("splitModeCombo"));
    m_modeCombo->addItem(tr("Every page"), int(Mode::EveryPage));
    m_modeCombo->addItem(tr("Every N pages"), int(Mode::EveryNPages));
    m_modeCombo->addItem(tr("New file starts at pages"), int(Mode::AtPages));
    layout->addRow(tr("Split"), m_modeCombo);

    m_everyNSpin->setObjectName(QStringLiteral("splitEveryNSpin"));
    m_everyNSpin->setRange(1, int(std::max<pdf::PDFInteger>(pageCount, 1)));
    m_everyNSpin->setValue(std::min(2, m_everyNSpin->maximum()));
    layout->addRow(tr("Pages per file"), m_everyNSpin);

    m_pagesEdit->setObjectName(QStringLiteral("splitPagesEdit"));
    m_pagesEdit->setPlaceholderText(tr("for example 5,12 (parts 1-4, 5-11, 12-%1)").arg(pageCount));
    layout->addRow(tr("Start pages"), m_pagesEdit);

    m_directoryEdit->setObjectName(QStringLiteral("splitDirectoryEdit"));
    QPushButton* browseButton = new QPushButton(tr("Browse..."), this);
    connect(browseButton, &QPushButton::clicked, this, [this]()
    {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), m_directoryEdit->text());
        if (!chosen.isEmpty())
        {
            m_directoryEdit->setText(QDir::toNativeSeparators(chosen));
        }
    });
    QWidget* directoryWidget = new QWidget(this);
    QHBoxLayout* directoryLayout = new QHBoxLayout(directoryWidget);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->addWidget(m_directoryEdit, 1);
    directoryLayout->addWidget(browseButton);
    layout->addRow(tr("Output folder"), directoryWidget);

    m_baseNameEdit->setObjectName(QStringLiteral("splitBaseNameEdit"));
    layout->addRow(tr("Base name"), m_baseNameEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Split"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &PDFSplitDocumentDialog::updateModeWidgets);
    updateModeWidgets();
}

void PDFSplitDocumentDialog::updateModeWidgets()
{
    const Mode mode = Mode(m_modeCombo->currentData().toInt());
    m_everyNSpin->setEnabled(mode == Mode::EveryNPages);
    m_pagesEdit->setEnabled(mode == Mode::AtPages);
}

std::vector<std::vector<pdf::PDFInteger>> PDFSplitDocumentDialog::splitEveryN(pdf::PDFInteger pageCount, pdf::PDFInteger n)
{
    std::vector<std::vector<pdf::PDFInteger>> parts;
    if (n < 1)
    {
        return parts;
    }
    for (pdf::PDFInteger start = 0; start < pageCount; start += n)
    {
        std::vector<pdf::PDFInteger> part;
        for (pdf::PDFInteger page = start; page < std::min(start + n, pageCount); ++page)
        {
            part.push_back(page);
        }
        parts.push_back(std::move(part));
    }
    return parts;
}

std::vector<std::vector<pdf::PDFInteger>> PDFSplitDocumentDialog::splitAtPages(pdf::PDFInteger pageCount, std::vector<pdf::PDFInteger> starts)
{
    starts.push_back(0);
    starts.push_back(pageCount);
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    std::vector<std::vector<pdf::PDFInteger>> parts;
    for (size_t i = 0; i + 1 < starts.size(); ++i)
    {
        if (starts[i] < 0 || starts[i] >= pageCount)
        {
            continue;
        }
        std::vector<pdf::PDFInteger> part;
        for (pdf::PDFInteger page = starts[i]; page < std::min(starts[i + 1], pageCount); ++page)
        {
            part.push_back(page);
        }
        parts.push_back(std::move(part));
    }
    return parts;
}

void PDFSplitDocumentDialog::accept()
{
    Plan plan;
    plan.directory = m_directoryEdit->text().trimmed();
    plan.baseName = m_baseNameEdit->text().trimmed();
    if (plan.directory.isEmpty() || !QDir(plan.directory).exists())
    {
        QMessageBox::warning(this, windowTitle(), tr("Select an existing output folder."));
        return;
    }
    if (plan.baseName.isEmpty() || plan.baseName.contains(QLatin1Char('/')) || plan.baseName.contains(QLatin1Char('\\')))
    {
        QMessageBox::warning(this, windowTitle(), tr("Enter a base name without folder separators."));
        return;
    }

    switch (Mode(m_modeCombo->currentData().toInt()))
    {
        case Mode::EveryPage:
            plan.parts = splitEveryN(m_pageCount, 1);
            break;

        case Mode::EveryNPages:
            plan.parts = splitEveryN(m_pageCount, m_everyNSpin->value());
            break;

        case Mode::AtPages:
        {
            if (m_pagesEdit->text().trimmed().isEmpty())
            {
                QMessageBox::warning(this, windowTitle(), tr("Enter the page numbers where a new file starts."));
                return;
            }
            QString errorMessage;
            const std::vector<pdf::PDFInteger> starts = pdf::PDFDocumentMerger::parsePageList(m_pageCount, m_pagesEdit->text(), &errorMessage);
            if (!errorMessage.isEmpty())
            {
                QMessageBox::warning(this, windowTitle(), errorMessage);
                return;
            }
            plan.parts = splitAtPages(m_pageCount, starts);
            break;
        }
    }

    if (plan.parts.size() < 2)
    {
        QMessageBox::warning(this, windowTitle(), tr("This split would produce a single file. Choose different settings."));
        return;
    }

    m_plan = std::move(plan);
    QDialog::accept();
}

}   // namespace pdfviewer
