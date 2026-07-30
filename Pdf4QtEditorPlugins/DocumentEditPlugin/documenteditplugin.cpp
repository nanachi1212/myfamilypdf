// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#include "documenteditplugin.h"
#include "documenteditdialog.h"

#include "pdfdocumentdecoration.h"
#include "pdfdrawwidget.h"
#include "pdfpagegeometry.h"
#include "pdfwidgettool.h"

#include <QAction>
#include <QDialog>
#include <QMainWindow>
#include <QMessageBox>

namespace pdfplugin
{

DocumentEditPlugin::DocumentEditPlugin() :
    pdf::PDFPlugin(nullptr),
    m_addTextWatermark(nullptr),
    m_setPageBackground(nullptr),
    m_changePageGeometry(nullptr),
    m_rotatePagesLeft(nullptr),
    m_rotatePagesRight(nullptr)
{
}

void DocumentEditPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);
    BaseClass::setWidget(widget);

    m_addTextWatermark =
        new QAction(tr("Add Text &Watermark..."), this);
    m_setPageBackground =
        new QAction(tr("Set Page &Background..."), this);
    m_changePageGeometry =
        new QAction(tr("Page Size and &Crop..."), this);
    m_rotatePagesLeft =
        new QAction(tr("Rotate Pages &Left..."), this);
    m_rotatePagesRight =
        new QAction(tr("Rotate Pages &Right..."), this);

    m_addTextWatermark->setObjectName(
        QStringLiteral("documentedit_addTextWatermark"));
    m_setPageBackground->setObjectName(
        QStringLiteral("documentedit_setPageBackground"));
    m_changePageGeometry->setObjectName(
        QStringLiteral("documentedit_changePageGeometry"));
    m_rotatePagesLeft->setObjectName(
        QStringLiteral("documentedit_rotatePagesLeft"));
    m_rotatePagesRight->setObjectName(
        QStringLiteral("documentedit_rotatePagesRight"));

    connect(m_addTextWatermark, &QAction::triggered,
            this, &DocumentEditPlugin::addTextWatermark);
    connect(m_setPageBackground, &QAction::triggered,
            this, &DocumentEditPlugin::setPageBackground);
    connect(m_changePageGeometry, &QAction::triggered,
            this, &DocumentEditPlugin::changePageGeometry);
    connect(m_rotatePagesLeft, &QAction::triggered,
            this, [this]() { rotatePages(-1); });
    connect(m_rotatePagesRight, &QAction::triggered,
            this, [this]() { rotatePages(1); });

    updateActions();
}

void DocumentEditPlugin::setDocument(
        const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);
    updateActions();
}

std::vector<QAction*> DocumentEditPlugin::getActions() const
{
    return {
        m_addTextWatermark,
        m_setPageBackground,
        m_changePageGeometry,
        m_rotatePagesLeft,
        m_rotatePagesRight
    };
}

QString DocumentEditPlugin::getPluginMenuName() const
{
    return tr("&Document Edit");
}

void DocumentEditPlugin::addTextWatermark()
{
    DecorationDialog dialog(
        DecorationDialog::Mode::Watermark,
        m_dataExchangeInterface->getMainWindow());
    if (dialog.exec() == QDialog::Accepted)
    {
        applyDecoration(dialog.settings());
    }
}

void DocumentEditPlugin::setPageBackground()
{
    DecorationDialog dialog(
        DecorationDialog::Mode::Background,
        m_dataExchangeInterface->getMainWindow());
    if (dialog.exec() == QDialog::Accepted)
    {
        applyDecoration(dialog.settings());
    }
}

void DocumentEditPlugin::changePageGeometry()
{
    PageGeometryDialog dialog(
        m_dataExchangeInterface->getMainWindow());
    if (dialog.exec() == QDialog::Accepted)
    {
        applyGeometry(dialog.settings());
    }
}

void DocumentEditPlugin::rotatePages(int quarterTurns)
{
    PageSelectionDialog dialog(
        quarterTurns < 0 ?
            tr("Rotate Pages Left") :
            tr("Rotate Pages Right"),
        m_dataExchangeInterface->getMainWindow());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    pdf::PDFPageGeometrySettings settings;
    settings.pageRange = dialog.pageRange();
    settings.pageSubset = dialog.pageSubset();
    settings.applyMediaBox = false;
    settings.applyCropBox = false;
    settings.rotationQuarterTurns = quarterTurns;
    applyGeometry(settings);
}

void DocumentEditPlugin::applyDecoration(
        const pdf::PDFDocumentDecorationSettings& settings)
{
    if (!m_document || !m_widget || !m_widget->getToolManager())
    {
        return;
    }

    pdf::PDFDocumentPointer modifiedDocument(
        new pdf::PDFDocument(*m_document));
    pdf::PDFModifiedDocument::ModificationFlags flags;
    const pdf::PDFOperationResult result =
        pdf::PDFDocumentDecoration::apply(
            modifiedDocument.data(), settings, &flags);
    if (!result)
    {
        QMessageBox::critical(
            m_dataExchangeInterface->getMainWindow(),
            tr("Document Edit"),
            result.getErrorMessage());
        return;
    }
    if (flags == pdf::PDFModifiedDocument::ModificationFlags())
    {
        return;
    }

    Q_EMIT m_widget->getToolManager()->documentModified(
        pdf::PDFModifiedDocument(modifiedDocument, nullptr, flags));
}

void DocumentEditPlugin::applyGeometry(
        const pdf::PDFPageGeometrySettings& settings)
{
    if (!m_document || !m_widget || !m_widget->getToolManager())
    {
        return;
    }

    pdf::PDFDocumentPointer modifiedDocument(
        new pdf::PDFDocument(*m_document));
    pdf::PDFModifiedDocument::ModificationFlags flags;
    const pdf::PDFOperationResult result =
        pdf::PDFPageGeometry::apply(
            modifiedDocument.data(), settings, &flags);
    if (!result)
    {
        QMessageBox::critical(
            m_dataExchangeInterface->getMainWindow(),
            tr("Document Edit"),
            result.getErrorMessage());
        return;
    }
    if (flags == pdf::PDFModifiedDocument::ModificationFlags())
    {
        return;
    }

    Q_EMIT m_widget->getToolManager()->documentModified(
        pdf::PDFModifiedDocument(modifiedDocument, nullptr, flags));
}

void DocumentEditPlugin::updateActions()
{
    const bool enabled =
        m_document &&
        m_document->getCatalog() &&
        m_document->getCatalog()->getPageCount() > 0;
    for (QAction* action : getActions())
    {
        if (action)
        {
            action->setEnabled(enabled);
        }
    }
}

} // namespace pdfplugin
