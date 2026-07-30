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
    m_highlightFields =
        new QAction(tr("&Highlight Form Fields"), this);
    m_resetForm =
        new QAction(tr("&Reset Form to Default Values"), this);

    m_createTextField->setObjectName("formplugin_createTextField");
    m_createCheckBox->setObjectName("formplugin_createCheckBox");
    m_highlightFields->setObjectName("formplugin_highlightFields");
    m_resetForm->setObjectName("formplugin_resetForm");
    m_highlightFields->setCheckable(true);

    connect(m_createTextField, &QAction::triggered,
            this, [this]() { createField(FieldType::Text); });
    connect(m_createCheckBox, &QAction::triggered,
            this, [this]() { createField(FieldType::CheckBox); });
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

    const FormFieldDialog::FieldType dialogType =
        fieldType == FieldType::Text ?
            FormFieldDialog::FieldType::Text :
            FormFieldDialog::FieldType::CheckBox;
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
    else
    {
        formField = builder->createFormFieldCheckBox(
            dialog.fieldName(),
            dialog.isCheckedByDefault(),
            flags);
    }

    builder->setFormFieldTooltip(formField, dialog.tooltip());
    const pdf::PDFObjectReference page =
        m_document->getCatalog()->getPage(pageIndex)->getPageReference();
    builder->createFormFieldWidget(
        formField,
        page,
        pageRectangle.normalized(),
        fieldType == FieldType::Text ?
            QByteArrayLiteral("/Helv 10 Tf 0 g") :
            QByteArray());
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
