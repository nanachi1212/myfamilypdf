// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#include "formplugin.h"
#include "formfielddialog.h"

#include "pdfdocumentbuilder.h"
#include "pdfdrawwidget.h"
#include "pdfwidgetformmanager.h"
#include "pdfwidgettool.h"

#include <QAction>
#include <QDialog>
#include <QMainWindow>
#include <QMessageBox>

namespace pdfplugin
{

FormPlugin::FormPlugin() :
    pdf::PDFPlugin(nullptr),
    m_createTextField(nullptr),
    m_createCheckBox(nullptr),
    m_createRadioButton(nullptr),
    m_createComboBox(nullptr),
    m_createListBox(nullptr),
    m_highlightFields(nullptr),
    m_resetForm(nullptr)
{
}

void FormPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);
    BaseClass::setWidget(widget);

    m_createTextField =
        new QAction(tr("Create &Text Field..."), this);
    m_createCheckBox =
        new QAction(tr("Create &Check Box..."), this);
    m_createRadioButton =
        new QAction(tr("Create &Radio Button Group..."), this);
    m_createComboBox =
        new QAction(tr("Create &Drop-down List..."), this);
    m_createListBox =
        new QAction(tr("Create &List Box..."), this);
    m_highlightFields =
        new QAction(tr("&Highlight Form Fields"), this);
    m_resetForm =
        new QAction(tr("&Reset Form to Default Values"), this);

    m_createTextField->setObjectName("formplugin_createTextField");
    m_createCheckBox->setObjectName("formplugin_createCheckBox");
    m_createRadioButton->setObjectName("formplugin_createRadioButton");
    m_createComboBox->setObjectName("formplugin_createComboBox");
    m_createListBox->setObjectName("formplugin_createListBox");
    m_highlightFields->setObjectName("formplugin_highlightFields");
    m_resetForm->setObjectName("formplugin_resetForm");
    m_highlightFields->setCheckable(true);

    connect(m_createTextField, &QAction::triggered,
            this, [this]() { createField(FieldType::Text); });
    connect(m_createCheckBox, &QAction::triggered,
            this, [this]() { createField(FieldType::CheckBox); });
    connect(m_createRadioButton, &QAction::triggered,
            this, [this]() { createField(FieldType::RadioButton); });
    connect(m_createComboBox, &QAction::triggered,
            this, [this]() { createField(FieldType::ComboBox); });
    connect(m_createListBox, &QAction::triggered,
            this, [this]() { createField(FieldType::ListBox); });
    connect(m_highlightFields, &QAction::toggled,
            this, &FormPlugin::setHighlightFields);
    connect(m_resetForm, &QAction::triggered,
            this, &FormPlugin::resetForm);

    updateActions();
}

void FormPlugin::setDocument(const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);
    updateActions();
}

std::vector<QAction*> FormPlugin::getActions() const
{
    return {
        m_createTextField,
        m_createCheckBox,
        m_createRadioButton,
        m_createComboBox,
        m_createListBox,
        m_highlightFields,
        m_resetForm
    };
}

QString FormPlugin::getPluginMenuName() const
{
    return tr("F&orms");
}

void FormPlugin::createField(FieldType fieldType)
{
    if (!m_document || !m_widget || !m_widget->getToolManager())
    {
        return;
    }

    m_widget->getToolManager()->pickRectangle(
        [this, fieldType](pdf::PDFInteger pageIndex, QRectF pageRectangle)
        {
            createFieldAt(fieldType, pageIndex, pageRectangle);
        });
}

void FormPlugin::createFieldAt(FieldType fieldType,
                               pdf::PDFInteger pageIndex,
                               QRectF pageRectangle)
{
    if (!m_document || pageRectangle.isEmpty() ||
        pageIndex < 0 ||
        static_cast<size_t>(pageIndex) >=
            m_document->getCatalog()->getPageCount())
    {
        return;
    }

    FormFieldDialog::FieldType dialogType =
        FormFieldDialog::FieldType::Text;
    switch (fieldType)
    {
        case FieldType::Text:
            dialogType = FormFieldDialog::FieldType::Text;
            break;
        case FieldType::CheckBox:
            dialogType = FormFieldDialog::FieldType::CheckBox;
            break;
        case FieldType::RadioButton:
            dialogType = FormFieldDialog::FieldType::RadioButton;
            break;
        case FieldType::ComboBox:
            dialogType = FormFieldDialog::FieldType::ComboBox;
            break;
        case FieldType::ListBox:
            dialogType = FormFieldDialog::FieldType::ListBox;
            break;
    }
    FormFieldDialog dialog(dialogType,
                           m_dataExchangeInterface->getMainWindow());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    pdf::PDFFormField::FieldFlags flags;
    flags.setFlag(pdf::PDFFormField::Required, dialog.isRequired());
    flags.setFlag(pdf::PDFFormField::ReadOnly, dialog.isReadOnly());
    if (fieldType == FieldType::Text)
    {
        flags.setFlag(pdf::PDFFormField::Multiline, dialog.isMultiline());
    }

    pdf::PDFDocumentModifier modifier(m_document);
    pdf::PDFDocumentBuilder* builder = modifier.getBuilder();
    pdf::PDFObjectReference formField;
    if (fieldType == FieldType::Text)
    {
        const int maximumLength = dialog.maximumLength();
        formField = builder->createFormFieldText(
            dialog.fieldName(),
            dialog.defaultText(),
            flags,
            maximumLength > 0 ?
                std::optional<pdf::PDFInteger>(maximumLength) :
                std::nullopt);
    }
    else if (fieldType == FieldType::CheckBox)
    {
        formField = builder->createFormFieldCheckBox(
            dialog.fieldName(),
            dialog.isCheckedByDefault(),
            flags);
    }
    else if (fieldType == FieldType::RadioButton)
    {
        const pdf::PDFFormFieldChoice::Options options = dialog.options();
        const int selectedIndex = dialog.defaultOptionIndex();
        flags.setFlag(pdf::PDFFormField::NoToggleToOff);
        formField = builder->createFormFieldRadioGroup(
            dialog.fieldName(),
            options.at(static_cast<size_t>(selectedIndex)).exportString,
            flags);

        const pdf::PDFObjectReference page =
            m_document->getCatalog()->getPage(pageIndex)->getPageReference();
        const pdf::PDFReal optionHeight =
            pageRectangle.height() /
            static_cast<pdf::PDFReal>(options.size());
        for (size_t index = 0; index < options.size(); ++index)
        {
            QRectF optionRectangle(
                pageRectangle.left(),
                pageRectangle.top() + optionHeight * index,
                qMin(pageRectangle.width(), optionHeight),
                optionHeight);
            builder->createFormFieldRadioWidget(
                formField,
                page,
                optionRectangle.normalized(),
                options.at(index).exportString,
                static_cast<int>(index) == selectedIndex);
        }
    }
    else
    {
        const bool isComboBox = fieldType == FieldType::ComboBox;
        flags.setFlag(pdf::PDFFormField::Combo, isComboBox);
        flags.setFlag(pdf::PDFFormField::MultiSelect,
                      !isComboBox && dialog.isMultiSelect());
        formField = builder->createFormFieldChoice(
            dialog.fieldName(),
            dialog.options(),
            { dialog.defaultOptionIndex() },
            flags);
    }

    builder->setFormFieldTooltip(formField, dialog.tooltip());
    if (fieldType != FieldType::RadioButton)
    {
        const pdf::PDFObjectReference page =
            m_document->getCatalog()->getPage(pageIndex)->getPageReference();
        builder->createFormFieldWidget(
            formField,
            page,
            pageRectangle.normalized(),
            fieldType == FieldType::Text ||
                    fieldType == FieldType::ComboBox ||
                    fieldType == FieldType::ListBox ?
                QByteArrayLiteral("/Helv 10 Tf 0 g") :
                QByteArray());
    }
    builder->appendAcroFormField(formField);
    modifier.markAnnotationsChanged();
    modifier.markFormFieldChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_widget->getToolManager()->documentModified(
            pdf::PDFModifiedDocument(modifier.getDocument(),
                                     nullptr,
                                     modifier.getFlags()));
    }
}

void FormPlugin::setHighlightFields(bool enabled)
{
    if (!m_widget || !m_widget->getFormManager())
    {
        return;
    }

    pdf::PDFFormManager::FormAppearanceFlags flags =
        m_widget->getFormManager()->getAppearanceFlags();
    flags.setFlag(pdf::PDFFormManager::HighlightFields, enabled);
    m_widget->getFormManager()->setAppearanceFlags(flags);
    if (m_widget->getDrawWidget() &&
        m_widget->getDrawWidget()->getWidget())
    {
        m_widget->getDrawWidget()->getWidget()->update();
    }
}

void FormPlugin::resetForm()
{
    pdf::PDFWidgetFormManager* formManager =
        m_widget ? m_widget->getFormManager() : nullptr;
    if (!m_document || !formManager || !formManager->hasForm())
    {
        return;
    }

    if (QMessageBox::question(
            m_dataExchangeInterface->getMainWindow(),
            tr("Reset Form"),
            tr("Reset all form fields to their default values?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    pdf::PDFDocumentModifier modifier(m_document);
    pdf::PDFFormField::ResetValueParameters parameters;
    parameters.modifier = &modifier;
    parameters.formManager = formManager;
    formManager->modify(
        [&parameters](pdf::PDFFormField* field)
        {
            field->resetValue(parameters);
        });

    if (modifier.finalize())
    {
        Q_EMIT m_widget->getToolManager()->documentModified(
            pdf::PDFModifiedDocument(modifier.getDocument(),
                                     nullptr,
                                     modifier.getFlags()));
    }
}

void FormPlugin::updateActions()
{
    const bool hasDocument = m_document != nullptr;
    if (m_createTextField)
    {
        m_createTextField->setEnabled(hasDocument);
        m_createCheckBox->setEnabled(hasDocument);
        m_createRadioButton->setEnabled(hasDocument);
        m_createComboBox->setEnabled(hasDocument);
        m_createListBox->setEnabled(hasDocument);
    }

    const pdf::PDFWidgetFormManager* formManager =
        m_widget ? m_widget->getFormManager() : nullptr;
    const bool hasForm =
        hasDocument && formManager && formManager->hasForm();
    if (m_resetForm)
    {
        m_resetForm->setEnabled(hasForm);
        m_highlightFields->setEnabled(hasDocument);
        if (formManager)
        {
            m_highlightFields->setChecked(
                formManager->getAppearanceFlags().testFlag(
                    pdf::PDFFormManager::HighlightFields));
        }
    }
}

} // namespace pdfplugin
