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

#include "pdfprintdialog.h"

#include "pdfutils.h"
#include "pdfwidgetutils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <numeric>

#include "pdfdbgheap.h"

namespace pdfviewer
{

namespace
{

constexpr int PREVIEW_WIDTH = 300;
constexpr int PREVIEW_HEIGHT = 380;

}   // namespace

PDFPrintDialog::PDFPrintDialog(const PDFPageOutputContext& outputContext,
                               const PageSelectionInfo& pageInfo,
                               const QString& documentName,
                               QPrinter::PrinterMode printerMode,
                               QWidget* parent) :
    QDialog(parent),
    m_outputContext(outputContext),
    m_pageInfo(pageInfo),
    m_documentName(documentName),
    m_printerMode(printerMode),
    m_previewExporter(new PDFPageImageExporter(outputContext, pdf::OCUsage::Print, 1))
{
    setWindowTitle(tr("Print"));

    // Printer
    QGroupBox* printerGroup = new QGroupBox(tr("Printer"), this);
    QFormLayout* printerLayout = new QFormLayout(printerGroup);
    m_printerCombo = new QComboBox(printerGroup);
    m_paperCombo = new QComboBox(printerGroup);
    m_orientationCombo = new QComboBox(printerGroup);
    m_orientationCombo->addItem(tr("Automatic (follow each page)"), int(PDFPrintOrientation::Auto));
    m_orientationCombo->addItem(tr("Portrait"), int(PDFPrintOrientation::Portrait));
    m_orientationCombo->addItem(tr("Landscape"), int(PDFPrintOrientation::Landscape));
    m_colorCombo = new QComboBox(printerGroup);
    m_colorCombo->addItem(tr("Color"), int(QPrinter::Color));
    m_colorCombo->addItem(tr("Grayscale"), int(QPrinter::GrayScale));
    m_duplexCombo = new QComboBox(printerGroup);
    m_duplexCombo->addItem(tr("One-sided"), int(QPrinter::DuplexNone));
    m_duplexCombo->addItem(tr("Two-sided, flip on long edge"), int(QPrinter::DuplexLongSide));
    m_duplexCombo->addItem(tr("Two-sided, flip on short edge"), int(QPrinter::DuplexShortSide));
    m_copiesSpin = new QSpinBox(printerGroup);
    m_copiesSpin->setRange(1, 999);
    m_collateCheck = new QCheckBox(tr("Collate"), printerGroup);
    m_collateCheck->setChecked(true);
    QWidget* copiesWidget = new QWidget(printerGroup);
    QHBoxLayout* copiesLayout = new QHBoxLayout(copiesWidget);
    copiesLayout->setContentsMargins(0, 0, 0, 0);
    copiesLayout->addWidget(m_copiesSpin);
    copiesLayout->addWidget(m_collateCheck);
    copiesLayout->addStretch(1);
    printerLayout->addRow(tr("&Name:"), m_printerCombo);
    printerLayout->addRow(tr("Pa&per:"), m_paperCombo);
    printerLayout->addRow(tr("&Orientation:"), m_orientationCombo);
    printerLayout->addRow(tr("Co&lor:"), m_colorCombo);
    printerLayout->addRow(tr("&Duplex:"), m_duplexCombo);
    printerLayout->addRow(tr("&Copies:"), copiesWidget);

    // Pages
    QGroupBox* pagesGroup = new QGroupBox(tr("Pages"), this);
    QVBoxLayout* pagesLayout = new QVBoxLayout(pagesGroup);
    m_allPagesRadio = new QRadioButton(tr("All pages (%1)").arg(m_pageInfo.pageCount), pagesGroup);
    m_currentPageRadio = new QRadioButton(pagesGroup);
    m_selectedPagesRadio = new QRadioButton(pagesGroup);
    m_rangeRadio = new QRadioButton(tr("Pages:"), pagesGroup);
    m_rangeEdit = new QLineEdit(pagesGroup);
    m_rangeEdit->setPlaceholderText(tr("e.g. 1-3,8,10-12"));
    QWidget* rangeWidget = new QWidget(pagesGroup);
    QHBoxLayout* rangeLayout = new QHBoxLayout(rangeWidget);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    rangeLayout->addWidget(m_rangeRadio);
    rangeLayout->addWidget(m_rangeEdit, 1);
    // The range radio sits in its own container, so the options are grouped explicitly.
    QButtonGroup* pagesButtonGroup = new QButtonGroup(this);
    for (QRadioButton* radio : { m_allPagesRadio, m_currentPageRadio, m_selectedPagesRadio, m_rangeRadio })
    {
        pagesButtonGroup->addButton(radio);
    }
    pagesLayout->addWidget(m_allPagesRadio);
    pagesLayout->addWidget(m_currentPageRadio);
    pagesLayout->addWidget(m_selectedPagesRadio);
    pagesLayout->addWidget(rangeWidget);

    m_currentPageRadio->setText(m_pageInfo.currentPages.size() > 1
                                    ? tr("Current pages (%1)").arg(describePages(m_pageInfo.currentPages))
                                    : tr("Current page (%1)").arg(describePages(m_pageInfo.currentPages)));
    m_currentPageRadio->setEnabled(!m_pageInfo.currentPages.empty());
    m_selectedPagesRadio->setText(tr("Pages selected in thumbnails (%1)").arg(describePages(m_pageInfo.selectedPages)));
    m_selectedPagesRadio->setEnabled(!m_pageInfo.selectedPages.empty());
    if (m_pageInfo.preferSelectedPages && !m_pageInfo.selectedPages.empty())
    {
        m_selectedPagesRadio->setChecked(true);
    }
    else
    {
        m_allPagesRadio->setChecked(true);
    }

    // Scale
    QGroupBox* scaleGroup = new QGroupBox(tr("Page scaling"), this);
    QVBoxLayout* scaleLayout = new QVBoxLayout(scaleGroup);
    m_fitRadio = new QRadioButton(tr("Fit to printable area"), scaleGroup);
    m_actualSizeRadio = new QRadioButton(tr("Actual size (100%)"), scaleGroup);
    m_fitRadio->setChecked(true);
    scaleLayout->addWidget(m_fitRadio);
    scaleLayout->addWidget(m_actualSizeRadio);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    QVBoxLayout* optionsLayout = new QVBoxLayout();
    optionsLayout->addWidget(printerGroup);
    optionsLayout->addWidget(pagesGroup);
    optionsLayout->addWidget(scaleGroup);
    optionsLayout->addWidget(m_summaryLabel);
    optionsLayout->addStretch(1);

    // Preview
    QGroupBox* previewGroup = new QGroupBox(tr("Preview"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(previewGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setBackgroundRole(QPalette::Dark);
    m_previewLabel->setAutoFillBackground(true);
    m_previousButton = new QToolButton(previewGroup);
    m_previousButton->setText(QStringLiteral("◀"));
    m_previousButton->setToolTip(tr("Previous page"));
    m_nextButton = new QToolButton(previewGroup);
    m_nextButton->setText(QStringLiteral("▶"));
    m_nextButton->setToolTip(tr("Next page"));
    m_previewPageLabel = new QLabel(previewGroup);
    m_previewPageLabel->setAlignment(Qt::AlignCenter);
    QHBoxLayout* navigationLayout = new QHBoxLayout();
    navigationLayout->addWidget(m_previousButton);
    navigationLayout->addWidget(m_previewPageLabel, 1);
    navigationLayout->addWidget(m_nextButton);
    previewLayout->addWidget(m_previewLabel, 1);
    previewLayout->addLayout(navigationLayout);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->addLayout(optionsLayout);
    contentLayout->addWidget(previewGroup, 1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_printButton = buttonBox->addButton(tr("&Print"), QDialogButtonBox::AcceptRole);
    m_printButton->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(contentLayout, 1);
    mainLayout->addWidget(buttonBox);

    // Printers. Enumerating them is the only work done before the dialog is shown.
    const QStringList printerNames = QPrinterInfo::availablePrinterNames();
    for (const QString& name : printerNames)
    {
        m_printerCombo->addItem(name);
    }
    const int defaultPrinterIndex = m_printerCombo->findText(QPrinterInfo::defaultPrinterName());
    if (defaultPrinterIndex >= 0)
    {
        m_printerCombo->setCurrentIndex(defaultPrinterIndex);
    }

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(150);
    connect(&m_previewTimer, &QTimer::timeout, this, &PDFPrintDialog::startPreview);
    connect(&m_previewWatcher, &QFutureWatcher<PreviewResult>::finished, this, &PDFPrintDialog::onPreviewFinished);

    connect(m_printerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PDFPrintDialog::onPrinterChanged);
    connect(m_paperCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PDFPrintDialog::onOptionsChanged);
    connect(m_orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PDFPrintDialog::onOptionsChanged);
    for (QRadioButton* radio : { m_allPagesRadio, m_currentPageRadio, m_selectedPagesRadio, m_rangeRadio, m_fitRadio, m_actualSizeRadio })
    {
        connect(radio, &QRadioButton::toggled, this, &PDFPrintDialog::onOptionsChanged);
    }
    connect(m_rangeEdit, &QLineEdit::textChanged, this, [this](const QString&)
    {
        // Typing a range selects the range option.
        if (!m_rangeRadio->isChecked())
        {
            m_rangeRadio->setChecked(true);
        }
        onOptionsChanged();
    });
    connect(m_previousButton, &QToolButton::clicked, this, [this]() { onPreviewPageChanged(-1); });
    connect(m_nextButton, &QToolButton::clicked, this, [this]() { onPreviewPageChanged(1); });

    m_printerCombo->setObjectName(QStringLiteral("printPrinterCombo"));
    m_paperCombo->setObjectName(QStringLiteral("printPaperCombo"));
    m_orientationCombo->setObjectName(QStringLiteral("printOrientationCombo"));
    m_duplexCombo->setObjectName(QStringLiteral("printDuplexCombo"));
    m_copiesSpin->setObjectName(QStringLiteral("printCopiesSpin"));
    m_allPagesRadio->setObjectName(QStringLiteral("printAllPagesRadio"));
    m_currentPageRadio->setObjectName(QStringLiteral("printCurrentPageRadio"));
    m_selectedPagesRadio->setObjectName(QStringLiteral("printSelectedPagesRadio"));
    m_rangeRadio->setObjectName(QStringLiteral("printRangeRadio"));
    m_rangeEdit->setObjectName(QStringLiteral("printRangeEdit"));
    m_fitRadio->setObjectName(QStringLiteral("printFitRadio"));
    m_actualSizeRadio->setObjectName(QStringLiteral("printActualSizeRadio"));
    m_previewLabel->setObjectName(QStringLiteral("printPreviewLabel"));
    m_printButton->setObjectName(QStringLiteral("printButton"));

    pdf::PDFWidgetUtils::scaleWidget(this, QSize(780, 560));
    pdf::PDFWidgetUtils::style(this);

    onPrinterChanged();
}

PDFPrintDialog::~PDFPrintDialog()
{
    m_previewTimer.stop();
    m_previewWatcher.disconnect();
    m_previewWatcher.waitForFinished();
}

QString PDFPrintDialog::describePages(const std::vector<pdf::PDFInteger>& pages) const
{
    if (pages.empty())
    {
        return tr("none");
    }

    // Collapse consecutive pages to ranges (1-3, 8).
    QStringList parts;
    size_t begin = 0;
    while (begin < pages.size())
    {
        size_t end = begin;
        while (end + 1 < pages.size() && pages[end + 1] == pages[end] + 1)
        {
            ++end;
        }
        parts << (end > begin ? QStringLiteral("%1-%2").arg(pages[begin] + 1).arg(pages[end] + 1) : QString::number(pages[begin] + 1));
        begin = end + 1;
    }
    return parts.join(QStringLiteral(","));
}

PDFPrintOrientation PDFPrintDialog::getOrientation() const
{
    return PDFPrintOrientation(m_orientationCombo->currentData().toInt());
}

PDFPrintScaling PDFPrintDialog::getScaling() const
{
    return m_actualSizeRadio->isChecked() ? PDFPrintScaling::ActualSize : PDFPrintScaling::FitToPrintableArea;
}

std::vector<pdf::PDFInteger> PDFPrintDialog::getSelectedPages(QString* errorMessage) const
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return std::vector<pdf::PDFInteger>();
    };

    if (m_allPagesRadio->isChecked())
    {
        std::vector<pdf::PDFInteger> pages(size_t(m_pageInfo.pageCount), 0);
        std::iota(pages.begin(), pages.end(), pdf::PDFInteger(0));
        return pages;
    }

    if (m_currentPageRadio->isChecked())
    {
        return m_pageInfo.currentPages;
    }

    if (m_selectedPagesRadio->isChecked())
    {
        return m_pageInfo.selectedPages;
    }

    // Custom range. Pages are printed in document order, each page once.
    QString parseError;
    const pdf::PDFClosedIntervalSet pageNumbers = pdf::PDFClosedIntervalSet::parsePageSelection(m_pageInfo.pageCount, m_rangeEdit->text(), &parseError);
    if (!parseError.isEmpty())
    {
        return fail(parseError);
    }

    if (pageNumbers.isEmpty())
    {
        return fail(tr("Enter the pages to print, for example 1-3,8,10-12."));
    }

    std::vector<pdf::PDFInteger> pages;
    for (const pdf::PDFInteger pageNumber : pageNumbers.unfold())
    {
        pages.push_back(pageNumber - 1);
    }
    return pages;
}

PDFPrintOptions PDFPrintDialog::getOptions() const
{
    PDFPrintOptions options;
    options.pageIndices = getSelectedPages();
    options.orientation = getOrientation();
    options.scaling = getScaling();
    return options;
}

std::unique_ptr<QPrinter> PDFPrintDialog::takePrinter()
{
    applyPrinterSettings(m_printer.get());
    return std::move(m_printer);
}

void PDFPrintDialog::applyPrinterSettings(QPrinter* printer) const
{
    if (!printer)
    {
        return;
    }

    if (m_paperCombo->currentIndex() >= 0)
    {
        printer->setPageSize(m_paperCombo->currentData().value<QPageSize>());
    }
    printer->setColorMode(QPrinter::ColorMode(m_colorCombo->currentData().toInt()));
    printer->setCopyCount(m_copiesSpin->value());
    printer->setCollateCopies(m_collateCheck->isChecked());
    // The duplex mode is passed to the printer as chosen; the dialog only offers it
    // when the printer reports support for it. It is never emulated.
    printer->setDuplex(QPrinter::DuplexMode(m_duplexCombo->currentData().toInt()));
    printer->setDocName(m_documentName);
}

void PDFPrintDialog::onPrinterChanged()
{
    m_isLoading = true;

    const QString printerName = m_printerCombo->currentText();
    const QPrinterInfo info = QPrinterInfo::printerInfo(printerName);
    m_printer.reset();
    m_paperCombo->clear();

    const bool hasPrinter = !printerName.isEmpty() && !info.isNull();
    if (hasPrinter)
    {
        m_printer = std::make_unique<QPrinter>(info, m_printerMode);

        const QList<QPageSize> pageSizes = info.supportedPageSizes();
        for (const QPageSize& pageSize : pageSizes)
        {
            m_paperCombo->addItem(pageSize.name(), QVariant::fromValue(pageSize));
        }

        const QPageSize defaultPageSize = m_printer->pageLayout().pageSize();
        for (int i = 0; i < m_paperCombo->count(); ++i)
        {
            if (m_paperCombo->itemData(i).value<QPageSize>() == defaultPageSize)
            {
                m_paperCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // Duplex is offered only if the printer reports it.
    const QList<QPrinter::DuplexMode> duplexModes = hasPrinter ? info.supportedDuplexModes() : QList<QPrinter::DuplexMode>();
    const bool supportsDuplex = duplexModes.contains(QPrinter::DuplexLongSide) || duplexModes.contains(QPrinter::DuplexShortSide);
    m_duplexCombo->setCurrentIndex(0);
    m_duplexCombo->setEnabled(supportsDuplex);
    m_duplexCombo->setToolTip(supportsDuplex ? QString() : tr("The selected printer does not support two-sided printing."));

    for (QWidget* widget : std::initializer_list<QWidget*>{ m_paperCombo, m_orientationCombo, m_colorCombo, m_copiesSpin, m_collateCheck })
    {
        widget->setEnabled(hasPrinter);
    }
    m_printButton->setEnabled(hasPrinter);
    m_summaryLabel->setText(hasPrinter ? QString() : tr("No printer is installed. Install a printer (for example Microsoft Print to PDF) to print."));

    m_isLoading = false;
    onOptionsChanged();
}

void PDFPrintDialog::onOptionsChanged()
{
    if (m_isLoading)
    {
        return;
    }

    if (m_printer && m_paperCombo->currentIndex() >= 0)
    {
        m_printer->setPageSize(m_paperCombo->currentData().value<QPageSize>());
    }

    QString error;
    const std::vector<pdf::PDFInteger> pages = getSelectedPages(&error);
    m_rangeEdit->setStyleSheet(error.isEmpty() ? QString() : QStringLiteral("QLineEdit { border: 1px solid #c42b1c; }"));
    if (m_printer)
    {
        m_summaryLabel->setText(!error.isEmpty() ? error : tr("%1 page(s) will be printed.").arg(pages.size()));
    }
    m_printButton->setEnabled(m_printer && error.isEmpty() && !pages.empty());
    m_previewPageCount = int(pages.size());
    m_previewPageIndex = qBound(0, m_previewPageIndex, qMax(0, m_previewPageCount - 1));
    updatePreviewNavigation();
    schedulePreview();
}

void PDFPrintDialog::onPreviewPageChanged(int delta)
{
    m_previewPageIndex = qBound(0, m_previewPageIndex + delta, qMax(0, m_previewPageCount - 1));
    updatePreviewNavigation();
    schedulePreview();
}

void PDFPrintDialog::updatePreviewNavigation()
{
    m_previousButton->setEnabled(m_previewPageIndex > 0);
    m_nextButton->setEnabled(m_previewPageIndex + 1 < m_previewPageCount);
    m_previewPageLabel->setText(m_previewPageCount > 0 ? tr("Sheet %1 of %2").arg(m_previewPageIndex + 1).arg(m_previewPageCount) : QString());
}

void PDFPrintDialog::schedulePreview()
{
    m_previewTimer.start();
}

void PDFPrintDialog::startPreview()
{
    if (m_previewWatcher.isRunning())
    {
        // Render again when the current page is done; only one page is rendered at a time.
        m_previewRestartRequested = true;
        return;
    }

    const std::vector<pdf::PDFInteger> pages = getSelectedPages();
    if (!m_printer || pages.empty())
    {
        m_previewLabel->clear();
        return;
    }

    const pdf::PDFInteger pageIndex = pages[size_t(qBound(0, m_previewPageIndex, int(pages.size()) - 1))];
    const pdf::PDFPage* page = m_outputContext.document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        m_previewLabel->clear();
        return;
    }

    // Layout of the sheet the page will be printed on, in points.
    QPageLayout layout = m_printer->pageLayout();
    switch (getOrientation())
    {
        case PDFPrintOrientation::Portrait:
            layout.setOrientation(QPageLayout::Portrait);
            break;

        case PDFPrintOrientation::Landscape:
            layout.setOrientation(QPageLayout::Landscape);
            break;

        default:
            layout.setOrientation(PDFPageOutput::isLandscapePage(page) ? QPageLayout::Landscape : QPageLayout::Portrait);
            break;
    }

    const QSizeF sheetPoints = layout.fullRect(QPageLayout::Point).size();
    const QRectF printablePoints = layout.paintRect(QPageLayout::Point);
    const QRectF placementPoints = PDFPageOutput::calculatePagePlacement(page->getRotatedMediaBox().size(), printablePoints, 72.0, getScaling());

    const qreal devicePixelRatio = devicePixelRatioF();
    const qreal scale = qMin(PREVIEW_WIDTH / sheetPoints.width(), PREVIEW_HEIGHT / sheetPoints.height()) * devicePixelRatio;
    auto toPreview = [scale](const QRectF& rect) { return QRectF(rect.topLeft() * scale, rect.size() * scale); };

    PreviewResult preview;
    preview.sheet = sheetPoints * scale;
    preview.placement = toPreview(placementPoints);
    preview.printable = toPreview(printablePoints);

    const QSize imageSize(qMax(1, qRound(preview.placement.width())), qMax(1, qRound(preview.placement.height())));
    PDFPageImageExporter* exporter = m_previewExporter.get();
    m_previewWatcher.setFuture(QtConcurrent::run([exporter, pageIndex, imageSize, preview]() mutable
    {
        preview.image = exporter->renderPage(pageIndex, imageSize, &preview.error);
        return preview;
    }));
}

void PDFPrintDialog::onPreviewFinished()
{
    if (m_previewRestartRequested)
    {
        m_previewRestartRequested = false;
        startPreview();
        return;
    }

    const PreviewResult preview = m_previewWatcher.result();

    QPixmap sheet(preview.sheet.toSize());
    sheet.fill(Qt::white);
    {
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        // The page is clipped by the printable area, as it is on paper.
        painter.save();
        painter.setClipRect(preview.printable);
        if (!preview.image.isNull())
        {
            painter.drawImage(preview.placement, preview.image);
        }
        painter.restore();

        painter.setPen(QPen(QColor(150, 150, 150), 1.0, Qt::DashLine));
        painter.drawRect(preview.printable);
        painter.setPen(QColor(120, 120, 120));
        painter.drawRect(QRectF(QPointF(0.5, 0.5), preview.sheet - QSizeF(1.0, 1.0)));
    }
    sheet.setDevicePixelRatio(devicePixelRatioF());
    m_previewLabel->setPixmap(sheet);
    m_previewLabel->setToolTip(preview.error);
}

void PDFPrintDialog::accept()
{
    QString error;
    const std::vector<pdf::PDFInteger> pages = getSelectedPages(&error);
    if (!error.isEmpty() || pages.empty())
    {
        QMessageBox::warning(this, tr("Print"), error.isEmpty() ? tr("There are no pages to print.") : error);
        return;
    }

    if (!m_printer)
    {
        QMessageBox::warning(this, tr("Print"), tr("No printer is available."));
        return;
    }

    // Stop the preview before the dialog goes away; nothing keeps running after Cancel or OK.
    m_previewTimer.stop();
    m_previewWatcher.disconnect();
    m_previewWatcher.waitForFinished();

    QDialog::accept();
}

}   // namespace pdfviewer
