// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFFAMILYPDFPATHS_H
#define PDFFAMILYPDFPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace pdfviewer
{

class PDFFamilyPDFPaths
{
public:
    static QString resolveDataRoot(const QString& applicationDirectory,
                                   const QString& genericConfigDirectory)
    {
        const QDir appDirectory(applicationDirectory);
        if (QFileInfo::exists(appDirectory.filePath(QStringLiteral("portable.mode"))))
        {
            return appDirectory.filePath(QStringLiteral("data"));
        }
        return QDir(genericConfigDirectory).filePath(QStringLiteral("FamilyPDF"));
    }

    static QString dataRoot()
    {
        return resolveDataRoot(
            QCoreApplication::applicationDirPath(),
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    }
};

} // namespace pdfviewer

#endif // PDFFAMILYPDFPATHS_H
