// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#include "formfielddialog.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace pdfplugin
{

FormFieldDialog::FormFieldDialog(FieldType fieldType, QWidget* parent) :
    QDialog(parent),
    m_fieldType(fieldType),
    m_fieldName(new QLineEdit(this)),
    m_tooltip(new QLineEdit(this)),
    m_defaultText(new QLineEdit(this)),
    m_required(new QCheckBox(tr("Required"), this)),
    m_readOnly(new QCheckBox(tr("Read only"), this)),
    m_multiline(new QCheckBox(tr("Multiline"), this)),
    m_maximumLength(new QSpinBox(this)),
    m_checkedByDefault(new QCheckBox(tr("Checked by default"), this)),
    m_options(new QTextEdit(this)),
    m_defaultOptionIndex(new QSpinBox(this)),
    m_multiSelect(new QCheckBox(tr("Allow multiple selections"), this))
{
    QString title;
    QString prefix;
    switch (fieldType)
    {
        case FieldType::Text:
            title = tr("Create Text Form Field");
            prefix = QStringLiteral("Text");
            break;
        case FieldType::CheckBox:
            title = tr("Create Check Box Form Field");
            prefix = QStringLiteral("CheckBox");
            break;
        case FieldType::RadioButton:
            title = tr("Create Radio Button Group");
            prefix = QStringLiteral("Radio");
            break;
        case FieldType::ComboBox:
            title = tr("Create Drop-down Form Field");
            prefix = QStringLiteral("Combo");
            break;
        case FieldType::ListBox:
            title = tr("Create List Form Field");
            prefix = QStringLiteral("List");
            break;
    }
    setWindowTitle(title);
    m_fieldName->setText(QStringLiteral("FamilyPDF_%1_%2")
                            .arg(prefix)
                            .arg(QDateTime::currentMSecsSinceEpoch()));
    m_maximumLength->setRange(0, 1000000);
    m_maximumLength->setSpecialValueText(tr("No limit"));
    m_defaultOptionIndex->setRange(1, 1000000);
    m_options->setPlaceholderText(
        tr("One option per line. Use export value=display text."));
    m_options->setPlainText(QStringLiteral("option1=Option 1\n"
                                           "option2=Option 2"));

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(tr("Field name:"), m_fieldName);
    formLayout->addRow(tr("Tooltip:"), m_tooltip);
    if (fieldType == FieldType::Text)
    {
        formLayout->addRow(tr("Default text:"), m_defaultText);
        formLayout->addRow(tr("Maximum length:"), m_maximumLength);
    }
    else if (fieldType == FieldType::RadioButton)
    {
        formLayout->addRow(tr("Options:"), m_options);
        formLayout->addRow(tr("Default option (1-based):"),
                           m_defaultOptionIndex);
    }
    else if (fieldType == FieldType::ComboBox ||
             fieldType == FieldType::ListBox)
    {
        formLayout->addRow(tr("Options:"), m_options);
        formLayout->addRow(tr("Default option (1-based):"),
                           m_defaultOptionIndex);
    }

    QVBoxLayout* flagsLayout = new QVBoxLayout();
    flagsLayout->addWidget(m_required);
    flagsLayout->addWidget(m_readOnly);
    if (fieldType == FieldType::Text)
    {
        flagsLayout->addWidget(m_multiline);
    }
    else if (fieldType == FieldType::CheckBox)
    {
        flagsLayout->addWidget(m_checkedByDefault);
    }
    if (fieldType == FieldType::ListBox)
    {
        flagsLayout->addWidget(m_multiSelect);
    }

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok |
                                 QDialogButtonBox::Cancel,
                             this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &FormFieldDialog::acceptIfValid);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addLayout(flagsLayout);
    layout->addWidget(buttons);
}

QString FormFieldDialog::fieldName() const
{
    return m_fieldName->text().trimmed();
}

QString FormFieldDialog::tooltip() const
{
    return m_tooltip->text().trimmed();
}

QString FormFieldDialog::defaultText() const
{
    return m_defaultText->text();
}

bool FormFieldDialog::isRequired() const
{
    return m_required->isChecked();
}

bool FormFieldDialog::isReadOnly() const
{
    return m_readOnly->isChecked();
}

bool FormFieldDialog::isMultiline() const
{
    return m_fieldType == FieldType::Text && m_multiline->isChecked();
}

int FormFieldDialog::maximumLength() const
{
    return m_fieldType == FieldType::Text ? m_maximumLength->value() : 0;
}

bool FormFieldDialog::isCheckedByDefault() const
{
    return m_fieldType == FieldType::CheckBox &&
           m_checkedByDefault->isChecked();
}

pdf::PDFFormFieldChoice::Options FormFieldDialog::options() const
{
    pdf::PDFFormFieldChoice::Options result;
    const QStringList lines = m_options->toPlainText().split(
        QChar::LineFeed, Qt::SkipEmptyParts);
    for (const QString& sourceLine : lines)
    {
        const QString line = sourceLine.trimmed();
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        pdf::PDFFormFieldChoice::Option option;
        if (separator > 0)
        {
            option.exportString = line.left(separator).trimmed();
            option.userString = line.mid(separator + 1).trimmed();
        }
        else
        {
            option.exportString = line;
            option.userString = line;
        }
        if (!option.exportString.isEmpty() && !option.userString.isEmpty())
        {
            result.emplace_back(qMove(option));
        }
    }
    return result;
}

int FormFieldDialog::defaultOptionIndex() const
{
    return m_defaultOptionIndex->value() - 1;
}

bool FormFieldDialog::isMultiSelect() const
{
    return m_fieldType == FieldType::ListBox &&
           m_multiSelect->isChecked();
}

void FormFieldDialog::acceptIfValid()
{
    if (fieldName().isEmpty())
    {
        QMessageBox::warning(this,
                             tr("Invalid Field"),
                             tr("Field name cannot be empty."));
        m_fieldName->setFocus();
        return;
    }
    if ((m_fieldType == FieldType::RadioButton ||
         m_fieldType == FieldType::ComboBox ||
         m_fieldType == FieldType::ListBox) &&
        options().empty())
    {
        QMessageBox::warning(this,
                             tr("Invalid Field"),
                             tr("Enter at least one valid option."));
        m_options->setFocus();
        return;
    }
    if (defaultOptionIndex() >= static_cast<int>(options().size()))
    {
        QMessageBox::warning(this,
                             tr("Invalid Field"),
                             tr("The default option is outside the option list."));
        m_defaultOptionIndex->setFocus();
        return;
    }

    accept();
}

} // namespace pdfplugin
