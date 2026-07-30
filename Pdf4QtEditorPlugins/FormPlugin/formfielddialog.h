// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef FORMFIELDDIALOG_H
#define FORMFIELDDIALOG_H

#include "pdfform.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;
class QTextEdit;

namespace pdfplugin
{

class FormFieldDialog : public QDialog
{
public:
    enum class FieldType
    {
        Text,
        CheckBox,
        RadioButton,
        ComboBox,
        ListBox
    };

    explicit FormFieldDialog(FieldType fieldType, QWidget* parent);

    QString fieldName() const;
    QString tooltip() const;
    QString defaultText() const;
    bool isRequired() const;
    bool isReadOnly() const;
    bool isMultiline() const;
    int maximumLength() const;
    bool isCheckedByDefault() const;
    pdf::PDFFormFieldChoice::Options options() const;
    int defaultOptionIndex() const;
    bool isMultiSelect() const;

private:
    void acceptIfValid();

    FieldType m_fieldType;
    QLineEdit* m_fieldName;
    QLineEdit* m_tooltip;
    QLineEdit* m_defaultText;
    QCheckBox* m_required;
    QCheckBox* m_readOnly;
    QCheckBox* m_multiline;
    QSpinBox* m_maximumLength;
    QCheckBox* m_checkedByDefault;
    QTextEdit* m_options;
    QSpinBox* m_defaultOptionIndex;
    QCheckBox* m_multiSelect;
};

} // namespace pdfplugin

#endif // FORMFIELDDIALOG_H
