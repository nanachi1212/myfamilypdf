// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#include "officeexportplugin.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>

namespace pdfplugin
{

OfficeExportPlugin::OfficeExportPlugin() :
    pdf::PDFPlugin(nullptr),
    m_exportWord(nullptr),
    m_exportExcel(nullptr)
{
}

void OfficeExportPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);
    BaseClass::setWidget(widget);

    m_exportWord = new QAction(tr("Export to &Word..."), this);
    m_exportExcel = new QAction(tr("Export to &Excel..."), this);
    m_exportWord->setObjectName(
        QStringLiteral("officeexport_exportWord"));
    m_exportExcel->setObjectName(
        QStringLiteral("officeexport_exportExcel"));

    connect(m_exportWord, &QAction::triggered,
            this, [this]() { exportDocument(QStringLiteral("docx")); });
    connect(m_exportExcel, &QAction::triggered,
            this, [this]() { exportDocument(QStringLiteral("xlsx")); });
    updateActions();
}

void OfficeExportPlugin::setDocument(
        const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);
    updateActions();
}

std::vector<QAction*> OfficeExportPlugin::getActions() const
{
    return { m_exportWord, m_exportExcel };
}

QString OfficeExportPlugin::getPluginMenuName() const
{
    return tr("&Office Export");
}

QString OfficeExportPlugin::findHelper() const
{
    const QString overridePath =
        qEnvironmentVariable("FAMILYPDF_OFFICE_EXPORT_HELPER");
    const QStringList candidates = {
        overridePath,
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral(
                "office-export/FamilyPDFOfficeExport.exe")),
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("FamilyPDFOfficeExport.exe"))
    };
    for (const QString& candidate : candidates)
    {
        if (!candidate.isEmpty() &&
            QFileInfo(candidate).isFile())
        {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return QString();
}

void OfficeExportPlugin::exportDocument(const QString& format)
{
    QMainWindow* parent = m_dataExchangeInterface->getMainWindow();
    const QString source =
        m_dataExchangeInterface->getOriginalFileName();
    if (!QFileInfo(source).isFile())
    {
        QMessageBox::warning(
            parent,
            tr("Office Export"),
            tr("Save the PDF before exporting it."));
        return;
    }

    const QString helper = findHelper();
    if (helper.isEmpty())
    {
        QMessageBox::critical(
            parent,
            tr("Office Export"),
            tr("The optional Office export component is missing. "
               "Reinstall FamilyPDF with Office Export enabled."));
        return;
    }

    bool accepted = false;
    const QString pages = QInputDialog::getText(
        parent,
        tr("Select Pages"),
        tr("Uses the last saved PDF.\n"
           "Pages (leave blank for all; example: 1-3,5):"),
        QLineEdit::Normal,
        QString(),
        &accepted);
    if (!accepted)
    {
        return;
    }

    const QFileInfo sourceInfo(source);
    const QString suffix =
        format == QStringLiteral("docx") ?
            tr("Word document (*.docx)") :
            tr("Excel workbook (*.xlsx)");
    const QString suggested = QDir(sourceInfo.absolutePath()).filePath(
        sourceInfo.completeBaseName() + QLatin1Char('.') + format);
    const QString output = QFileDialog::getSaveFileName(
        parent, tr("Export PDF"), suggested, suffix);
    if (output.isEmpty())
    {
        return;
    }

    QStringList arguments = {
        QStringLiteral("--input"), source,
        QStringLiteral("--output"), output,
        QStringLiteral("--format"), format
    };
    if (!pages.trimmed().isEmpty())
    {
        arguments << QStringLiteral("--pages") << pages.trimmed();
    }

    QProcess process;
    QProgressDialog progress(
        tr("Converting searchable PDF content..."),
        tr("Cancel"),
        0,
        0,
        parent);
    progress.setWindowTitle(tr("Office Export"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);

    bool canceled = false;
    connect(&progress, &QProgressDialog::canceled,
            &process, [&process, &canceled]()
            {
                canceled = true;
                process.kill();
            });
    connect(&process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            &progress,
            &QProgressDialog::accept);

    process.start(helper, arguments);
    if (!process.waitForStarted())
    {
        QMessageBox::critical(
            parent,
            tr("Office Export"),
            tr("Could not start the Office export component."));
        return;
    }

    progress.exec();
    if (canceled)
    {
        process.waitForFinished();
        return;
    }
    if (process.state() != QProcess::NotRunning)
    {
        process.kill();
        process.waitForFinished();
        return;
    }

    const QByteArray outputJson = process.readAllStandardOutput();
    const QJsonObject report =
        QJsonDocument::fromJson(outputJson).object();
    const QString status = report.value(
        QStringLiteral("status")).toString();
    if (process.exitCode() == 0 &&
        status == QStringLiteral("ok"))
    {
        QMessageBox::information(
            parent,
            tr("Office Export"),
            tr("Export completed:\n%1").arg(
                QDir::toNativeSeparators(output)));
        return;
    }

    QString message = report.value(
        QStringLiteral("message")).toString();
    if (message.isEmpty())
    {
        message = QString::fromUtf8(
            process.readAllStandardError()).trimmed();
    }
    if (message.isEmpty())
    {
        message = tr("Office export failed with exit code %1.")
                      .arg(process.exitCode());
    }
    QMessageBox::critical(parent, tr("Office Export"), message);
}

void OfficeExportPlugin::updateActions()
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
