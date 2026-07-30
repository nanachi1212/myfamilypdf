// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef FORMFIELDDIALOG_H
#define FORMFIELDDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace pdfplugin
{

class FormFieldDialog : public QDialog
{
public:
    enum class FieldType
    {
        Text,
        CheckBox
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
};

} // namespace pdfplugin

#endif // FORMFIELDDIALOG_H
