// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#include "documenteditdialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace pdfplugin
{

PageSelectionControls::PageSelectionControls(QWidget* parent) :
    m_pageRange(new QLineEdit(QStringLiteral("-"), parent)),
    m_pageSubset(new QComboBox(parent))
{
    m_pageRange->setPlaceholderText(
        QObject::tr("Examples: -, 1-5, 1,3,8-12"));
    m_pageSubset->addItem(
        QObject::tr("All matching pages"),
        static_cast<int>(
            pdf::PDFPageGeometrySettings::PageSubset::AllPages));
    m_pageSubset->addItem(
        QObject::tr("Odd pages only"),
        static_cast<int>(
            pdf::PDFPageGeometrySettings::PageSubset::OddPages));
    m_pageSubset->addItem(
        QObject::tr("Even pages only"),
        static_cast<int>(
            pdf::PDFPageGeometrySettings::PageSubset::EvenPages));
}

void PageSelectionControls::addRows(QFormLayout* layout)
{
    layout->addRow(QObject::tr("Page range:"), m_pageRange);
    layout->addRow(QObject::tr("Page subset:"), m_pageSubset);
}

QString PageSelectionControls::pageRange() const
{
    return m_pageRange->text().trimmed();
}

pdf::PDFPageGeometrySettings::PageSubset
PageSelectionControls::pageSubset() const
{
    return static_cast<pdf::PDFPageGeometrySettings::PageSubset>(
        m_pageSubset->currentData().toInt());
}

DecorationDialog::DecorationDialog(Mode mode, QWidget* parent) :
    QDialog(parent),
    m_mode(mode),
    m_pageSelection(this),
    m_text(new QLineEdit(this)),
    m_fontSize(new QDoubleSpinBox(this)),
    m_opacity(new QDoubleSpinBox(this)),
    m_angle(new QDoubleSpinBox(this)),
    m_foreground(new QCheckBox(tr("Place over existing page content"), this)),
    m_backgroundType(new QComboBox(this)),
    m_imagePath(new QLineEdit(this)),
    m_browseImage(new QPushButton(tr("Browse..."), this)),
    m_imageScaleMode(new QComboBox(this)),
    m_colorButton(new QPushButton(this)),
    m_color(mode == Mode::Watermark ?
                QColor(160, 0, 0) :
                QColor(245, 240, 220))
{
    setWindowTitle(mode == Mode::Watermark ?
                       tr("Add Text Watermark") :
                       tr("Set Page Background"));
    resize(520, 360);

    auto* formLayout = new QFormLayout;
    m_pageSelection.addRows(formLayout);

    m_fontSize->setRange(6.0, 300.0);
    m_fontSize->setValue(48.0);
    m_fontSize->setSuffix(tr(" pt"));
    m_opacity->setRange(1.0, 100.0);
    m_opacity->setValue(mode == Mode::Watermark ? 25.0 : 100.0);
    m_opacity->setSuffix(tr(" %"));
    m_angle->setRange(-180.0, 180.0);
    m_angle->setValue(-45.0);
    m_angle->setSuffix(QChar(0x00B0));
    m_foreground->setChecked(mode == Mode::Watermark);

    m_backgroundType->addItem(tr("Solid color"), 0);
    m_backgroundType->addItem(tr("Image"), 1);
    m_imageScaleMode->addItem(
        tr("Fit inside page"),
        static_cast<int>(
            pdf::PDFDocumentDecorationSettings::ImageScaleMode::Fit));
    m_imageScaleMode->addItem(
        tr("Fill page (crop edges)"),
        static_cast<int>(
            pdf::PDFDocumentDecorationSettings::ImageScaleMode::Fill));
    m_imageScaleMode->addItem(
        tr("Stretch to page"),
        static_cast<int>(
            pdf::PDFDocumentDecorationSettings::ImageScaleMode::Stretch));

    auto* imageLayout = new QHBoxLayout;
    imageLayout->addWidget(m_imagePath, 1);
    imageLayout->addWidget(m_browseImage);

    if (mode == Mode::Watermark)
    {
        m_text->setText(tr("CONFIDENTIAL"));
        formLayout->addRow(tr("Text:"), m_text);
        formLayout->addRow(tr("Font size:"), m_fontSize);
        formLayout->addRow(tr("Angle:"), m_angle);
        formLayout->addRow(QString(), m_foreground);
    }
    else
    {
        formLayout->addRow(tr("Background type:"), m_backgroundType);
        formLayout->addRow(tr("Image file:"), imageLayout);
        formLayout->addRow(tr("Image scaling:"), m_imageScaleMode);
    }
    formLayout->addRow(tr("Color:"), m_colorButton);
    formLayout->addRow(tr("Opacity:"), m_opacity);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &DecorationDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &DecorationDialog::reject);
    connect(m_colorButton, &QPushButton::clicked,
            this, &DecorationDialog::chooseColor);
    connect(m_browseImage, &QPushButton::clicked,
            this, &DecorationDialog::chooseImage);
    connect(m_backgroundType,
            &QComboBox::currentIndexChanged,
            this,
            [this]() { updateBackgroundControls(); });

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addStretch();
    layout->addWidget(buttons);

    updateColorButton();
    updateBackgroundControls();
}

pdf::PDFDocumentDecorationSettings DecorationDialog::settings() const
{
    pdf::PDFDocumentDecorationSettings result;
    result.pageRange = m_pageSelection.pageRange();
    result.pageSubset = m_pageSelection.pageSubset();
    result.opacity = m_opacity->value() / 100.0;
    result.foreground = m_mode == Mode::Watermark ?
                            m_foreground->isChecked() :
                            false;
    result.color = m_color;

    if (m_mode == Mode::Watermark)
    {
        result.kind =
            pdf::PDFDocumentDecorationSettings::Kind::TextWatermark;
        result.text = m_text->text();
        result.fontPointSize = m_fontSize->value();
        result.angleDegrees = m_angle->value();
        const QLocale locale;
        result.fontFamily =
            locale.territory() == QLocale::China ||
                    locale.territory() == QLocale::Singapore ?
                QStringLiteral("Microsoft YaHei UI") :
                QStringLiteral("Microsoft JhengHei UI");
    }
    else if (m_backgroundType->currentData().toInt() == 1)
    {
        result.kind =
            pdf::PDFDocumentDecorationSettings::Kind::ImageBackground;
        result.image = QImage(m_imagePath->text());
        result.imageScaleMode = static_cast<
            pdf::PDFDocumentDecorationSettings::ImageScaleMode>(
                m_imageScaleMode->currentData().toInt());
    }
    else
    {
        result.kind =
            pdf::PDFDocumentDecorationSettings::Kind::ColorBackground;
    }
    return result;
}

void DecorationDialog::accept()
{
    if (m_pageSelection.pageRange().isEmpty())
    {
        QMessageBox::warning(
            this, windowTitle(), tr("Enter a page range, or use '-' for all pages."));
        return;
    }
    if (m_mode == Mode::Watermark && m_text->text().trimmed().isEmpty())
    {
        QMessageBox::warning(
            this, windowTitle(), tr("Watermark text cannot be empty."));
        return;
    }
    if (m_mode == Mode::Background &&
        m_backgroundType->currentData().toInt() == 1)
    {
        const QString path = m_imagePath->text().trimmed();
        QImageReader reader(path);
        if (path.isEmpty() || !QFileInfo::exists(path) || !reader.canRead())
        {
            QMessageBox::warning(
                this, windowTitle(), tr("Select a readable background image."));
            return;
        }
    }
    QDialog::accept();
}

void DecorationDialog::chooseColor()
{
    const QColor color = QColorDialog::getColor(
        m_color, this, tr("Choose Color"), QColorDialog::ShowAlphaChannel);
    if (color.isValid())
    {
        m_color = color;
        updateColorButton();
    }
}

void DecorationDialog::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Choose Background Image"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*)"));
    if (!path.isEmpty())
    {
        m_imagePath->setText(path);
    }
}

void DecorationDialog::updateColorButton()
{
    m_colorButton->setText(m_color.name(QColor::HexArgb));
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(m_color.name(),
                 m_color.lightness() < 128 ?
                     QStringLiteral("white") :
                     QStringLiteral("black")));
}

void DecorationDialog::updateBackgroundControls()
{
    const bool isImage =
        m_mode == Mode::Background &&
        m_backgroundType->currentData().toInt() == 1;
    m_imagePath->setEnabled(isImage);
    m_browseImage->setEnabled(isImage);
    m_imageScaleMode->setEnabled(isImage);
    m_colorButton->setEnabled(!isImage || m_mode == Mode::Watermark);
}

PageGeometryDialog::PageGeometryDialog(QWidget* parent) :
    QDialog(parent),
    m_pageSelection(this),
    m_changePageSize(new QCheckBox(tr("Set an explicit page size"), this)),
    m_pageWidth(new QDoubleSpinBox(this)),
    m_pageHeight(new QDoubleSpinBox(this)),
    m_leftMargin(new QDoubleSpinBox(this)),
    m_topMargin(new QDoubleSpinBox(this)),
    m_rightMargin(new QDoubleSpinBox(this)),
    m_bottomMargin(new QDoubleSpinBox(this)),
    m_scaleContent(new QCheckBox(tr("Scale page content to fit"), this)),
    m_preserveAspectRatio(new QCheckBox(tr("Preserve aspect ratio"), this)),
    m_scaleAnnotations(new QCheckBox(
        tr("Scale annotations and form fields"), this)),
    m_applyMediaBox(new QCheckBox(tr("Update MediaBox"), this)),
    m_applyCropBox(new QCheckBox(tr("Update CropBox"), this))
{
    setWindowTitle(tr("Page Size and Crop"));
    resize(540, 480);

    auto* formLayout = new QFormLayout;
    m_pageSelection.addRows(formLayout);

    for (QDoubleSpinBox* spinBox :
         {m_pageWidth, m_pageHeight,
          m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin})
    {
        spinBox->setDecimals(2);
        spinBox->setSuffix(tr(" mm"));
    }
    m_pageWidth->setRange(1.0, 5000.0);
    m_pageHeight->setRange(1.0, 5000.0);
    m_pageWidth->setValue(210.0);
    m_pageHeight->setValue(297.0);
    for (QDoubleSpinBox* spinBox :
         {m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin})
    {
        spinBox->setRange(0.0, 1000.0);
    }

    m_changePageSize->setChecked(true);
    m_preserveAspectRatio->setChecked(true);
    m_scaleAnnotations->setChecked(true);
    m_applyMediaBox->setChecked(true);
    m_applyCropBox->setChecked(true);

    formLayout->addRow(QString(), m_changePageSize);
    formLayout->addRow(tr("Page width:"), m_pageWidth);
    formLayout->addRow(tr("Page height:"), m_pageHeight);
    formLayout->addRow(tr("Left margin:"), m_leftMargin);
    formLayout->addRow(tr("Top margin:"), m_topMargin);
    formLayout->addRow(tr("Right margin:"), m_rightMargin);
    formLayout->addRow(tr("Bottom margin:"), m_bottomMargin);
    formLayout->addRow(QString(), m_scaleContent);
    formLayout->addRow(QString(), m_preserveAspectRatio);
    formLayout->addRow(QString(), m_scaleAnnotations);
    formLayout->addRow(QString(), m_applyMediaBox);
    formLayout->addRow(QString(), m_applyCropBox);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &PageGeometryDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &PageGeometryDialog::reject);
    connect(m_changePageSize, &QCheckBox::toggled,
            m_pageWidth, &QDoubleSpinBox::setEnabled);
    connect(m_changePageSize, &QCheckBox::toggled,
            m_pageHeight, &QDoubleSpinBox::setEnabled);
    connect(m_scaleContent, &QCheckBox::toggled,
            m_preserveAspectRatio, &QCheckBox::setEnabled);
    connect(m_scaleContent, &QCheckBox::toggled,
            m_scaleAnnotations, &QCheckBox::setEnabled);
    m_preserveAspectRatio->setEnabled(false);
    m_scaleAnnotations->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addStretch();
    layout->addWidget(buttons);
}

pdf::PDFPageGeometrySettings PageGeometryDialog::settings() const
{
    pdf::PDFPageGeometrySettings result;
    result.pageRange = m_pageSelection.pageRange();
    result.pageSubset = m_pageSelection.pageSubset();
    result.useTargetPageSize = m_changePageSize->isChecked();
    result.targetPageSizeMM =
        QSizeF(m_pageWidth->value(), m_pageHeight->value());
    result.marginsMM =
        QMarginsF(m_leftMargin->value(),
                  m_topMargin->value(),
                  m_rightMargin->value(),
                  m_bottomMargin->value());
    result.scaleContent = m_scaleContent->isChecked();
    result.preserveAspectRatio = m_preserveAspectRatio->isChecked();
    result.scaleAnnotationsAndFormFields =
        m_scaleAnnotations->isChecked();
    result.applyMediaBox = m_applyMediaBox->isChecked();
    result.applyCropBox = m_applyCropBox->isChecked();
    return result;
}

void PageGeometryDialog::accept()
{
    if (m_pageSelection.pageRange().isEmpty())
    {
        QMessageBox::warning(
            this, windowTitle(), tr("Enter a page range, or use '-' for all pages."));
        return;
    }
    if (!m_applyMediaBox->isChecked() && !m_applyCropBox->isChecked())
    {
        QMessageBox::warning(
            this, windowTitle(), tr("Select at least one page box to update."));
        return;
    }
    QDialog::accept();
}

PageSelectionDialog::PageSelectionDialog(QString title, QWidget* parent) :
    QDialog(parent),
    m_pageSelection(this)
{
    setWindowTitle(std::move(title));
    auto* formLayout = new QFormLayout;
    m_pageSelection.addRows(formLayout);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(buttons);
}

QString PageSelectionDialog::pageRange() const
{
    return m_pageSelection.pageRange();
}

pdf::PDFPageGeometrySettings::PageSubset
PageSelectionDialog::pageSubset() const
{
    return m_pageSelection.pageSubset();
}

} // namespace pdfplugin
