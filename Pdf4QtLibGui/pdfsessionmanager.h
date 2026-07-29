// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFSESSIONMANAGER_H
#define PDFSESSIONMANAGER_H

#include "pdffamilypdfpaths.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

namespace pdfviewer
{

class PDFSessionManager
{
public:
    static QString defaultPath()
    {
        return QDir(PDFFamilyPDFPaths::dataRoot()).filePath(QStringLiteral("session.json"));
    }

    static QStringList normalizePaths(const QStringList& paths, bool requireExistingFiles)
    {
        QStringList result;
        QSet<QString> keys;
        for (const QString& path : paths)
        {
            const QFileInfo info(path);
            const QString absolutePath = QDir::cleanPath(info.absoluteFilePath());
            if (absolutePath.isEmpty() || (requireExistingFiles && !info.isFile()))
            {
                continue;
            }
#ifdef Q_OS_WIN
            const QString key = absolutePath.toLower();
#else
            const QString key = absolutePath;
#endif
            if (!keys.contains(key))
            {
                keys.insert(key);
                result.push_back(absolutePath);
            }
        }
        return result;
    }

    static bool savePaths(const QStringList& paths,
                          const QString& sessionPath = defaultPath())
    {
        const QStringList normalized = normalizePaths(paths, true);
        QJsonArray documents;
        for (const QString& path : normalized)
        {
            documents.push_back(path);
        }

        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("documents")] = documents;

        if (!QDir().mkpath(QFileInfo(sessionPath).absolutePath()))
        {
            return false;
        }
        QSaveFile file(sessionPath);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }
        if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0)
        {
            file.cancelWriting();
            return false;
        }
        return file.commit();
    }

    static QStringList loadPaths(const QString& sessionPath = defaultPath())
    {
        QFile file(sessionPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            return {};
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonArray documents =
            document.object().value(QStringLiteral("documents")).toArray();
        QStringList paths;
        paths.reserve(documents.size());
        for (const QJsonValue& value : documents)
        {
            if (value.isString())
            {
                paths.push_back(value.toString());
            }
        }
        return normalizePaths(paths, true);
    }
};

} // namespace pdfviewer

#endif // PDFSESSIONMANAGER_H
