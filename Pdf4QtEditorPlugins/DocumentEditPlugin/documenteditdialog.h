// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef DOCUMENTEDITDIALOG_H
#define DOCUMENTEDITDIALOG_H

#include "pdfdocumentdecoration.h"
#include "pdfpagegeometry.h"

#include <QColor>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QPushButton;

namespace pdfplugin
{

class PageSelectionControls
{
public:
    explicit PageSelectionControls(QWidget* parent);

    void addRows(QFormLayout* layout);
    QString pageRange() const;
    pdf::PDFPageGeometrySettings::PageSubset pageSubset() const;

private:
    QLineEdit* m_pageRange;
    QComboBox* m_pageSubset;
};

class DecorationDialog : public QDialog
{
public:
    enum class Mode
    {
        Watermark,
        Background
    };

    explicit DecorationDialog(Mode mode, QWidget* parent = nullptr);

    pdf::PDFDocumentDecorationSettings settings() const;

protected:
    void accept() override;

private:
    void chooseColor();
    void chooseImage();
    void updateColorButton();
    void updateBackgroundControls();

    Mode m_mode;
    PageSelectionControls m_pageSelection;
    QLineEdit* m_text;
    QDoubleSpinBox* m_fontSize;
    QDoubleSpinBox* m_opacity;
    QDoubleSpinBox* m_angle;
    QCheckBox* m_foreground;
    QComboBox* m_backgroundType;
    QLineEdit* m_imagePath;
    QPushButton* m_browseImage;
    QComboBox* m_imageScaleMode;
    QPushButton* m_colorButton;
    QColor m_color;
};

class PageGeometryDialog : public QDialog
{
public:
    explicit PageGeometryDialog(QWidget* parent = nullptr);

    pdf::PDFPageGeometrySettings settings() const;

protected:
    void accept() override;

private:
    PageSelectionControls m_pageSelection;
    QCheckBox* m_changePageSize;
    QDoubleSpinBox* m_pageWidth;
    QDoubleSpinBox* m_pageHeight;
    QDoubleSpinBox* m_leftMargin;
    QDoubleSpinBox* m_topMargin;
    QDoubleSpinBox* m_rightMargin;
    QDoubleSpinBox* m_bottomMargin;
    QCheckBox* m_scaleContent;
    QCheckBox* m_preserveAspectRatio;
    QCheckBox* m_scaleAnnotations;
    QCheckBox* m_applyMediaBox;
    QCheckBox* m_applyCropBox;
};

class PageSelectionDialog : public QDialog
{
public:
    explicit PageSelectionDialog(QString title, QWidget* parent = nullptr);

    QString pageRange() const;
    pdf::PDFPageGeometrySettings::PageSubset pageSubset() const;

private:
    PageSelectionControls m_pageSelection;
};

} // namespace pdfplugin

#endif // DOCUMENTEDITDIALOG_H
