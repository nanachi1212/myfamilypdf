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

#include "pdfexportimagesdialog.h"

#include "pdfutils.h"
#include "pdfwidgetutils.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <numeric>

#include "pdfdbgheap.h"

namespace pdfviewer
{

namespace
{

constexpr const char* SETTINGS_GROUP = "ExportImages";
constexpr int MAX_LISTED_FAILURES = 5;

}   // namespace

PDFExportImagesDialog::PDFExportImagesDialog(const Request& request, QWidget* parent) :
    QDialog(parent),
    m_request(request)
{
    const bool isRegionMode = !m_request.selectionRegions.empty();
    setWindowTitle(isRegionMode ? tr("Export Selection as Image") : tr("Export Pages as Images"));

    // Pages
    m_pagesGroup = new QGroupBox(tr("Pages"), this);
    QVBoxLayout* pagesLayout = new QVBoxLayout(m_pagesGroup);
    m_currentPageRadio = new QRadioButton(tr("Current page"), m_pagesGroup);
    m_allPagesRadio = new QRadioButton(tr("All pages (%1)").arg(m_request.pageCount), m_pagesGroup);
    m_selectedPagesRadio = new QRadioButton(tr("Pages selected in thumbnails (%1)").arg(m_request.selectedPages.size()), m_pagesGroup);
    m_rangeRadio = new QRadioButton(tr("Pages:"), m_pagesGroup);
    m_rangeEdit = new QLineEdit(m_pagesGroup);
    m_rangeEdit->setPlaceholderText(tr("e.g. 1-3,8,10-12"));
    QWidget* rangeWidget = new QWidget(m_pagesGroup);
    QHBoxLayout* rangeLayout = new QHBoxLayout(rangeWidget);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    rangeLayout->addWidget(m_rangeRadio);
    rangeLayout->addWidget(m_rangeEdit, 1);
    // The range radio sits in its own container, so the options are grouped explicitly.
    QButtonGroup* pagesButtonGroup = new QButtonGroup(this);
    for (QRadioButton* radio : { m_currentPageRadio, m_allPagesRadio, m_selectedPagesRadio, m_rangeRadio })
    {
        pagesButtonGroup->addButton(radio);
    }
    pagesLayout->addWidget(m_currentPageRadio);
    pagesLayout->addWidget(m_allPagesRadio);
    pagesLayout->addWidget(m_selectedPagesRadio);
    pagesLayout->addWidget(rangeWidget);

    m_currentPageRadio->setEnabled(!m_request.currentPages.empty());
    m_selectedPagesRadio->setEnabled(!m_request.selectedPages.empty());
    if (m_request.preferSelectedPages && !m_request.selectedPages.empty())
    {
        m_selectedPagesRadio->setChecked(true);
    }
    else if (!m_request.currentPages.empty())
    {
        m_currentPageRadio->setChecked(true);
    }
    else
    {
        m_allPagesRadio->setChecked(true);
    }

    m_selectionLabel = new QLabel(tr("The selected area of %1 page(s) is exported, one image per page.").arg(m_request.selectionRegions.size()), this);
    m_selectionLabel->setWordWrap(true);
    m_pagesGroup->setVisible(!isRegionMode);
    m_selectionLabel->setVisible(isRegionMode);

    // Image settings
    QGroupBox* imageGroup = new QGroupBox(tr("Image"), this);
    QFormLayout* imageLayout = new QFormLayout(imageGroup);
    m_formatCombo = new QComboBox(imageGroup);
    m_formatCombo->addItem(tr("PNG (lossless, best for text and drawings)"), int(PDFImageFormat::Png));
    m_formatCombo->addItem(tr("JPEG (smaller files, best for photos and scans)"), int(PDFImageFormat::Jpeg));
    m_dpiSpin = new QSpinBox(imageGroup);
    m_dpiSpin->setRange(PDFPageImageExporter::getMinDpi(), PDFPageImageExporter::getMaxDpi());
    m_dpiSpin->setSuffix(tr(" DPI"));
    m_dpiSpin->setValue(PDFPageImageExporter::getDefaultDpi());
    m_sizeLabel = new QLabel(imageGroup);
    m_qualitySpin = new QSpinBox(imageGroup);
    m_qualitySpin->setRange(1, 100);
    m_qualitySpin->setValue(PDFPageImageExporter::getDefaultJpegQuality());
    imageLayout->addRow(tr("&Format:"), m_formatCombo);
    imageLayout->addRow(tr("&Resolution:"), m_dpiSpin);
    imageLayout->addRow(QString(), m_sizeLabel);
    imageLayout->addRow(tr("JPEG &quality:"), m_qualitySpin);

    // Output
    QGroupBox* outputGroup = new QGroupBox(tr("Output"), this);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
    m_directoryEdit = new QLineEdit(outputGroup);
    m_browseButton = new QPushButton(tr("&Browse..."), outputGroup);
    QHBoxLayout* directoryLayout = new QHBoxLayout();
    QLabel* directoryLabel = new QLabel(tr("Fol&der:"), outputGroup);
    directoryLabel->setBuddy(m_directoryEdit);
    directoryLayout->addWidget(directoryLabel);
    directoryLayout->addWidget(m_directoryEdit, 1);
    directoryLayout->addWidget(m_browseButton);
    m_filesLabel = new QLabel(outputGroup);
    m_filesLabel->setWordWrap(true);
    outputLayout->addLayout(directoryLayout);
    outputLayout->addWidget(m_filesLabel);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    m_exportButton = buttonBox->addButton(tr("&Export"), QDialogButtonBox::AcceptRole);
    m_exportButton->setDefault(true);
    m_closeButton = buttonBox->addButton(QDialogButtonBox::Close);
    connect(m_exportButton, &QPushButton::clicked, this, &PDFExportImagesDialog::onExportClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &PDFExportImagesDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_pagesGroup);
    mainLayout->addWidget(m_selectionLabel);
    mainLayout->addWidget(imageGroup);
    mainLayout->addWidget(outputGroup);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch(1);
    mainLayout->addWidget(buttonBox);

    // Remembered settings and the default folder
    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    const int formatIndex = m_formatCombo->findData(settings.value(QStringLiteral("format"), int(PDFImageFormat::Png)).toInt());
    m_formatCombo->setCurrentIndex(qMax(0, formatIndex));
    m_dpiSpin->setValue(qBound(PDFPageImageExporter::getMinDpi(), settings.value(QStringLiteral("dpi"), PDFPageImageExporter::getDefaultDpi()).toInt(), PDFPageImageExporter::getMaxDpi()));
    m_qualitySpin->setValue(qBound(1, settings.value(QStringLiteral("quality"), PDFPageImageExporter::getDefaultJpegQuality()).toInt(), 100));

    const QFileInfo documentInfo(m_request.documentFileName);
    QString directory = documentInfo.exists() ? documentInfo.absolutePath() : QString();
    if (directory.isEmpty() || !QFileInfo(directory).isWritable())
    {
        const QString lastDirectory = settings.value(QStringLiteral("directory")).toString();
        directory = QFileInfo(lastDirectory).isDir() ? lastDirectory : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    settings.endGroup();
    m_directoryEdit->setText(QDir::toNativeSeparators(directory));

    connect(&m_watcher, &QFutureWatcher<PDFImageExportResult>::finished, this, &PDFExportImagesDialog::onExportFinished);
    m_progressTimer.setInterval(100);
    connect(&m_progressTimer, &QTimer::timeout, this, &PDFExportImagesDialog::onProgressTimer);

    for (QRadioButton* radio : { m_currentPageRadio, m_allPagesRadio, m_selectedPagesRadio, m_rangeRadio })
    {
        connect(radio, &QRadioButton::toggled, this, &PDFExportImagesDialog::onOptionsChanged);
    }
    connect(m_rangeEdit, &QLineEdit::textChanged, this, [this](const QString&)
    {
        if (!m_rangeRadio->isChecked())
        {
            m_rangeRadio->setChecked(true);
        }
        onOptionsChanged();
    });
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PDFExportImagesDialog::onOptionsChanged);
    connect(m_dpiSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PDFExportImagesDialog::onOptionsChanged);
    connect(m_directoryEdit, &QLineEdit::textChanged, this, &PDFExportImagesDialog::onOptionsChanged);
    connect(m_browseButton, &QPushButton::clicked, this, &PDFExportImagesDialog::onBrowseClicked);

    m_currentPageRadio->setObjectName(QStringLiteral("exportCurrentPageRadio"));
    m_allPagesRadio->setObjectName(QStringLiteral("exportAllPagesRadio"));
    m_selectedPagesRadio->setObjectName(QStringLiteral("exportSelectedPagesRadio"));
    m_rangeRadio->setObjectName(QStringLiteral("exportRangeRadio"));
    m_rangeEdit->setObjectName(QStringLiteral("exportRangeEdit"));
    m_formatCombo->setObjectName(QStringLiteral("exportFormatCombo"));
    m_dpiSpin->setObjectName(QStringLiteral("exportDpiSpin"));
    m_qualitySpin->setObjectName(QStringLiteral("exportQualitySpin"));
    m_directoryEdit->setObjectName(QStringLiteral("exportDirectoryEdit"));
    m_filesLabel->setObjectName(QStringLiteral("exportFilesLabel"));
    m_progressBar->setObjectName(QStringLiteral("exportProgressBar"));
    m_statusLabel->setObjectName(QStringLiteral("exportStatusLabel"));
    m_exportButton->setObjectName(QStringLiteral("exportButton"));
    m_closeButton->setObjectName(QStringLiteral("exportCloseButton"));

    pdf::PDFWidgetUtils::scaleWidget(this, QSize(560, 520));
    pdf::PDFWidgetUtils::style(this);

    onOptionsChanged();
}

PDFExportImagesDialog::~PDFExportImagesDialog()
{
    if (m_running)
    {
        m_cancelRequested = true;
        m_watcher.disconnect();
        m_watcher.waitForFinished();
    }
}

PDFImageFormat PDFExportImagesDialog::getFormat() const
{
    return PDFImageFormat(m_formatCombo->currentData().toInt());
}

QString PDFExportImagesDialog::getBaseName() const
{
    const QFileInfo info(m_request.documentFileName);
    return PDFPageImageExporter::sanitizeBaseName(info.completeBaseName());
}

std::vector<pdf::PDFInteger> PDFExportImagesDialog::getSelectedPages(QString* errorMessage) const
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return std::vector<pdf::PDFInteger>();
    };

    if (!m_request.selectionRegions.empty())
    {
        std::vector<pdf::PDFInteger> pages;
        for (const auto& item : m_request.selectionRegions)
        {
            pages.push_back(item.first);
        }
        return pages;
    }

    if (m_allPagesRadio->isChecked())
    {
        std::vector<pdf::PDFInteger> pages(size_t(m_request.pageCount), 0);
        std::iota(pages.begin(), pages.end(), pdf::PDFInteger(0));
        return pages;
    }

    if (m_currentPageRadio->isChecked())
    {
        return m_request.currentPages;
    }

    if (m_selectedPagesRadio->isChecked())
    {
        return m_request.selectedPages;
    }

    QString parseError;
    const pdf::PDFClosedIntervalSet pageNumbers = pdf::PDFClosedIntervalSet::parsePageSelection(m_request.pageCount, m_rangeEdit->text(), &parseError);
    if (!parseError.isEmpty())
    {
        return fail(parseError);
    }

    if (pageNumbers.isEmpty())
    {
        return fail(tr("Enter the pages to export, for example 1-3,8,10-12."));
    }

    std::vector<pdf::PDFInteger> pages;
    for (const pdf::PDFInteger pageNumber : pageNumbers.unfold())
    {
        pages.push_back(pageNumber - 1);
    }
    return pages;
}

std::vector<PDFImageExportTarget> PDFExportImagesDialog::createTargets(QString* errorMessage) const
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return std::vector<PDFImageExportTarget>();
    };

    QString pagesError;
    const std::vector<pdf::PDFInteger> pages = getSelectedPages(&pagesError);
    if (!pagesError.isEmpty())
    {
        return fail(pagesError);
    }

    if (pages.empty())
    {
        return fail(tr("There are no pages to export."));
    }

    const QString directory = m_directoryEdit->text().trimmed();
    if (directory.isEmpty())
    {
        return fail(tr("Choose the folder for the images."));
    }

    const QDir dir(directory);
    if (!dir.exists())
    {
        return fail(tr("The folder '%1' does not exist.").arg(QDir::toNativeSeparators(directory)));
    }

    const QString baseName = getBaseName();
    const PDFImageFormat format = getFormat();
    const bool isRegionMode = !m_request.selectionRegions.empty();

    std::vector<PDFImageExportTarget> targets;
    targets.reserve(pages.size());
    for (const pdf::PDFInteger pageIndex : pages)
    {
        PDFImageExportTarget target;
        target.pageIndex = pageIndex;
        if (isRegionMode)
        {
            target.region = m_request.selectionRegions.at(pageIndex);
        }
        target.fileName = dir.absoluteFilePath(PDFPageImageExporter::getFileName(baseName, pageIndex, m_request.pageCount, format, isRegionMode));
        targets.push_back(target);
    }
    return targets;
}

void PDFExportImagesDialog::onOptionsChanged()
{
    m_qualitySpin->setEnabled(getFormat() == PDFImageFormat::Jpeg && !m_running);

    QString error;
    const std::vector<PDFImageExportTarget> targets = createTargets(&error);
    m_rangeEdit->setStyleSheet(QString());

    if (targets.empty())
    {
        m_filesLabel->setText(error);
        m_sizeLabel->clear();
        m_exportButton->setEnabled(false);
        return;
    }

    const QString first = QFileInfo(targets.front().fileName).fileName();
    const QString last = QFileInfo(targets.back().fileName).fileName();
    m_filesLabel->setText(targets.size() == 1 ? tr("File: %1").arg(first)
                                              : tr("%1 files, from %2 to %3").arg(targets.size()).arg(first, last));

    // Image size of the largest page in the export, to warn before rendering.
    QSize largest;
    const pdf::PDFCatalog* catalog = m_request.outputContext.document->getCatalog();
    for (const PDFImageExportTarget& target : targets)
    {
        if (const pdf::PDFPage* page = catalog->getPage(target.pageIndex))
        {
            const QSize size = PDFPageImageExporter::getImageSize(page, m_dpiSpin->value());
            if (qint64(size.width()) * size.height() > qint64(largest.width()) * largest.height())
            {
                largest = size;
            }
        }
    }

    const bool tooLarge = qint64(largest.width()) * largest.height() > PDFPageImageExporter::getMaxImagePixels();
    m_sizeLabel->setText(tooLarge ? tr("Too large: %1 x %2 pixels. Use a lower resolution.").arg(largest.width()).arg(largest.height())
                                  : tr("Page images up to %1 x %2 pixels").arg(largest.width()).arg(largest.height()));
    m_exportButton->setEnabled(!tooLarge && !m_running);
}

void PDFExportImagesDialog::onBrowseClicked()
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Select output folder"), m_directoryEdit->text());
    if (!directory.isEmpty())
    {
        m_directoryEdit->setText(QDir::toNativeSeparators(directory));
    }
}

void PDFExportImagesDialog::setRunning(bool running)
{
    m_running = running;
    for (QWidget* widget : std::initializer_list<QWidget*>{ m_pagesGroup, m_formatCombo, m_dpiSpin, m_directoryEdit, m_browseButton, m_exportButton })
    {
        widget->setEnabled(!running);
    }
    m_qualitySpin->setEnabled(!running && getFormat() == PDFImageFormat::Jpeg);
    m_closeButton->setText(running ? tr("&Cancel") : tr("&Close"));
    m_progressBar->setVisible(running);
    if (!running)
    {
        onOptionsChanged();
    }
}

void PDFExportImagesDialog::onExportClicked()
{
    QString error;
    const std::vector<PDFImageExportTarget> targets = createTargets(&error);
    if (targets.empty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    const int dpi = m_dpiSpin->value();
    const PDFImageFormat format = getFormat();
    const int quality = m_qualitySpin->value();

    const pdf::PDFCatalog* catalog = m_request.outputContext.document->getCatalog();
    QSize largest;
    for (const PDFImageExportTarget& target : targets)
    {
        if (const pdf::PDFPage* page = catalog->getPage(target.pageIndex))
        {
            const QSize size = PDFPageImageExporter::getImageSize(page, dpi);
            if (qint64(size.width()) * size.height() > qint64(largest.width()) * largest.height())
            {
                largest = size;
            }
        }
    }

    // Never silently replace existing files.
    int existingCount = 0;
    for (const PDFImageExportTarget& target : targets)
    {
        if (QFileInfo::exists(target.fileName))
        {
            ++existingCount;
        }
    }

    if (existingCount > 0)
    {
        const QMessageBox::StandardButton answer = QMessageBox::question(this,
                                                                         windowTitle(),
                                                                         tr("%1 of the %2 files already exist in the folder. Replace them?").arg(existingCount).arg(targets.size()),
                                                                         QMessageBox::Yes | QMessageBox::No,
                                                                         QMessageBox::No);
        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    QSettings settings;
    settings.beginGroup(QLatin1String(SETTINGS_GROUP));
    settings.setValue(QStringLiteral("format"), int(format));
    settings.setValue(QStringLiteral("dpi"), dpi);
    settings.setValue(QStringLiteral("quality"), quality);
    settings.setValue(QStringLiteral("directory"), m_directoryEdit->text().trimmed());
    settings.endGroup();

    m_exporter = std::make_unique<PDFPageImageExporter>(m_request.outputContext, pdf::OCUsage::Export, PDFPageImageExporter::getRecommendedRasterizerCount(largest));
    m_cancelRequested = false;
    m_processed = 0;
    m_totalTargets = int(targets.size());
    m_progressBar->setRange(0, m_totalTargets);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Exporting..."));
    setRunning(true);
    m_progressTimer.start();

    const PDFPageImageExporter* exporter = m_exporter.get();
    m_watcher.setFuture(QtConcurrent::run([this, exporter, targets, format, dpi, quality]()
    {
        return exporter->exportTargets(targets, format, dpi, quality, &m_cancelRequested, &m_processed);
    }));
}

void PDFExportImagesDialog::onProgressTimer()
{
    const int processed = m_processed.load();
    m_progressBar->setValue(processed);
    m_statusLabel->setText(m_cancelRequested ? tr("Cancelling...") : tr("Exporting... %1 of %2").arg(processed).arg(m_totalTargets));
}

void PDFExportImagesDialog::onExportFinished()
{
    m_progressTimer.stop();
    m_result = m_watcher.result();
    m_exporter.reset();
    setRunning(false);

    const int succeeded = m_result.getSucceededCount();
    const int failed = m_result.getFailedCount();
    const int total = int(m_result.items.size());
    const QString directory = QDir::toNativeSeparators(m_directoryEdit->text().trimmed());

    if (m_result.isComplete())
    {
        m_statusLabel->setText(tr("%1 image(s) were saved to %2.").arg(succeeded).arg(directory));
        QMessageBox::information(this, windowTitle(), tr("%1 image(s) were saved to:\n%2").arg(succeeded).arg(directory));
        accept();
        return;
    }

    QStringList lines;
    if (m_result.cancelled)
    {
        lines << tr("The export was cancelled. %1 of %2 image(s) were saved; the others were not created.").arg(succeeded).arg(total);
    }
    else
    {
        lines << tr("Only %1 of %2 image(s) could be saved.").arg(succeeded).arg(total);
    }

    int listed = 0;
    for (const PDFImageExportItemResult& item : m_result.items)
    {
        if (item.status == PDFImageExportItemResult::Status::Failed)
        {
            if (listed++ < MAX_LISTED_FAILURES)
            {
                lines << tr("Page %1: %2").arg(item.target.pageIndex + 1).arg(item.errorMessage);
            }
        }
    }
    if (failed > MAX_LISTED_FAILURES)
    {
        lines << tr("... and %1 more.").arg(failed - MAX_LISTED_FAILURES);
    }

    m_statusLabel->setText(lines.join(QLatin1Char('\n')));
    QMessageBox::warning(this, windowTitle(), lines.join(QLatin1Char('\n')));
}

void PDFExportImagesDialog::reject()
{
    if (m_running)
    {
        // Stop at the next page; pages already written are kept and reported.
        m_cancelRequested = true;
        return;
    }

    QDialog::reject();
}

void PDFExportImagesDialog::closeEvent(QCloseEvent* event)
{
    if (m_running)
    {
        m_cancelRequested = true;
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

}   // namespace pdfviewer
