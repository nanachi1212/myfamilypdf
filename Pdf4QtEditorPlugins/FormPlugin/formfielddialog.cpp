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
    m_checkedByDefault(new QCheckBox(tr("Checked by default"), this))
{
    setWindowTitle(fieldType == FieldType::Text ?
                       tr("Create Text Form Field") :
                       tr("Create Check Box Form Field"));

    const QString prefix =
        fieldType == FieldType::Text ? QStringLiteral("Text") :
                                       QStringLiteral("CheckBox");
    m_fieldName->setText(QStringLiteral("FamilyPDF_%1_%2")
                            .arg(prefix)
                            .arg(QDateTime::currentMSecsSinceEpoch()));
    m_maximumLength->setRange(0, 1000000);
    m_maximumLength->setSpecialValueText(tr("No limit"));

    QFormLayout* formLayout = new QFormLayout();
    formLayout->addRow(tr("Field name:"), m_fieldName);
    formLayout->addRow(tr("Tooltip:"), m_tooltip);
    if (fieldType == FieldType::Text)
    {
        formLayout->addRow(tr("Default text:"), m_defaultText);
        formLayout->addRow(tr("Maximum length:"), m_maximumLength);
    }

    QVBoxLayout* flagsLayout = new QVBoxLayout();
    flagsLayout->addWidget(m_required);
    flagsLayout->addWidget(m_readOnly);
    if (fieldType == FieldType::Text)
    {
        flagsLayout->addWidget(m_multiline);
    }
    else
    {
        flagsLayout->addWidget(m_checkedByDefault);
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

    accept();
}

} // namespace pdfplugin
