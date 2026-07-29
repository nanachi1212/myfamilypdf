// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFSTARTUPRECOVERY_H
#define PDFSTARTUPRECOVERY_H

#include "pdfprogramcontroller.h"
#include "pdfrecoverymanager.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace pdfviewer
{

inline void scheduleStartupRecoveryPrompt(PDFProgramController* controller, QWidget* parent)
{
    QTimer::singleShot(0, parent, [controller, parent]()
    {
        const QList<PDFRecoveryManager::Record> records = PDFRecoveryManager::findRecords();
        if (records.isEmpty())
        {
            return;
        }

        const PDFRecoveryManager::Record record = records.front();
        QMessageBox dialog(
            QMessageBox::Warning,
            QApplication::translate("StartupRecovery", "Unsaved PDF recovery found"),
            QApplication::translate(
                "StartupRecovery",
                "FamilyPDF found an unsaved recovery copy for:\n%1")
                .arg(QDir::toNativeSeparators(record.sourcePath)),
            QMessageBox::NoButton,
            parent);
        dialog.setInformativeText(
            QApplication::translate(
                "StartupRecovery",
                "Recovery time: %1\nPages: %2\n\n"
                "Opening the recovery copy never overwrites the original PDF. "
                "Use Save As after checking it.")
                .arg(QLocale().toString(record.createdUtc.toLocalTime(), QLocale::ShortFormat))
                .arg(record.pageCount));

        QPushButton* openButton = dialog.addButton(
            QApplication::translate("StartupRecovery", "Open recovery copy"),
            QMessageBox::AcceptRole);
        QPushButton* keepButton = dialog.addButton(
            QApplication::translate("StartupRecovery", "Keep for later"),
            QMessageBox::RejectRole);
        QPushButton* discardButton = dialog.addButton(
            QApplication::translate("StartupRecovery", "Discard recovery"),
            QMessageBox::DestructiveRole);
        dialog.setDefaultButton(openButton);
        dialog.exec();

        if (dialog.clickedButton() == openButton)
        {
            controller->openRecoveryDocument(record.snapshotPath, record.sourcePath);
        }
        else if (dialog.clickedButton() == discardButton)
        {
            PDFRecoveryManager::removeRecord(record.sourcePath);
        }
        else
        {
            Q_UNUSED(keepButton);
        }
    });
}

} // namespace pdfviewer

#endif // PDFSTARTUPRECOVERY_H
