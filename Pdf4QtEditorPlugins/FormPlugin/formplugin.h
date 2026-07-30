// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef FORMPLUGIN_H
#define FORMPLUGIN_H

#include "pdfplugin.h"

#include <QObject>

namespace pdfplugin
{

class FormPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.FormPlugin" FILE "FormPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    FormPlugin();

    void setWidget(pdf::PDFWidget* widget) override;
    void setDocument(const pdf::PDFModifiedDocument& document) override;
    std::vector<QAction*> getActions() const override;
    QString getPluginMenuName() const override;

private:
    enum class FieldType
    {
        Text,
        CheckBox,
        RadioButton,
        ComboBox,
        ListBox
    };

    void createField(FieldType fieldType);
    void createFieldAt(FieldType fieldType,
                       pdf::PDFInteger pageIndex,
                       QRectF pageRectangle);
    void setHighlightFields(bool enabled);
    void resetForm();
    void updateActions();

    QAction* m_createTextField;
    QAction* m_createCheckBox;
    QAction* m_createRadioButton;
    QAction* m_createComboBox;
    QAction* m_createListBox;
    QAction* m_highlightFields;
    QAction* m_resetForm;
};

} // namespace pdfplugin

#endif // FORMPLUGIN_H
