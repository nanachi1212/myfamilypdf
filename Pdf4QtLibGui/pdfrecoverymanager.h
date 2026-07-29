// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFRECOVERYMANAGER_H
#define PDFRECOVERYMANAGER_H

#include "pdffamilypdfpaths.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace pdfviewer
{

class PDFRecoveryManager
{
public:
    struct Record
    {
        QString sourcePath;
        QString snapshotPath;
        QString metadataPath;
        QDateTime createdUtc;
        int pageCount = 0;
    };

    static QString defaultRoot()
    {
        return QDir(PDFFamilyPDFPaths::dataRoot()).filePath(QStringLiteral("recovery"));
    }

    static QString documentKey(const QString& sourcePath)
    {
        QString normalized = QDir::cleanPath(QFileInfo(sourcePath).absoluteFilePath());
#ifdef Q_OS_WIN
        normalized = normalized.toLower();
#endif
        return QString::fromLatin1(
            QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex());
    }

    static QString snapshotPath(const QString& sourcePath,
                                const QString& recoveryRoot = defaultRoot())
    {
        return QDir(recoveryRoot).filePath(documentKey(sourcePath) + QStringLiteral(".pdf"));
    }

    static QString metadataPath(const QString& sourcePath,
                                const QString& recoveryRoot = defaultRoot())
    {
        return QDir(recoveryRoot).filePath(documentKey(sourcePath) + QStringLiteral(".json"));
    }

    static bool writeMetadata(const QString& sourcePath,
                              const QString& recoverySnapshotPath,
                              int pageCount,
                              const QString& recoveryRoot = defaultRoot())
    {
        QDir directory;
        if (!directory.mkpath(recoveryRoot))
        {
            return false;
        }

        QJsonObject object;
        object[QStringLiteral("version")] = 1;
        object[QStringLiteral("sourcePath")] = QFileInfo(sourcePath).absoluteFilePath();
        object[QStringLiteral("snapshotPath")] =
            QFileInfo(recoverySnapshotPath).absoluteFilePath();
        object[QStringLiteral("createdUtc")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        object[QStringLiteral("pageCount")] = pageCount;

        QSaveFile metadataFile(metadataPath(sourcePath, recoveryRoot));
        if (!metadataFile.open(QIODevice::WriteOnly))
        {
            return false;
        }
        if (metadataFile.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0)
        {
            metadataFile.cancelWriting();
            return false;
        }
        return metadataFile.commit();
    }

    static QList<Record> findRecords(const QString& recoveryRoot = defaultRoot())
    {
        QList<Record> records;
        QDir directory(recoveryRoot);
        const QFileInfoList metadataFiles =
            directory.entryInfoList(QStringList{ QStringLiteral("*.json") },
                                    QDir::Files,
                                    QDir::Name);
        for (const QFileInfo& metadataInfo : metadataFiles)
        {
            QFile metadataFile(metadataInfo.absoluteFilePath());
            if (!metadataFile.open(QIODevice::ReadOnly))
            {
                continue;
            }
            const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll());
            const QJsonObject object = document.object();

            Record record;
            record.sourcePath = object.value(QStringLiteral("sourcePath")).toString();
            record.snapshotPath = object.value(QStringLiteral("snapshotPath")).toString();
            record.metadataPath = metadataInfo.absoluteFilePath();
            record.createdUtc =
                QDateTime::fromString(object.value(QStringLiteral("createdUtc")).toString(),
                                      Qt::ISODateWithMs);
            record.pageCount = object.value(QStringLiteral("pageCount")).toInt();

            if (record.sourcePath.isEmpty() ||
                record.snapshotPath.isEmpty() ||
                !record.createdUtc.isValid() ||
                !QFileInfo::exists(record.snapshotPath))
            {
                continue;
            }
            records.push_back(record);
        }

        std::sort(records.begin(), records.end(), [](const Record& left, const Record& right)
        {
            return left.createdUtc > right.createdUtc;
        });
        return records;
    }

    static void removeRecord(const QString& sourcePath,
                             const QString& recoveryRoot = defaultRoot())
    {
        QFile::remove(snapshotPath(sourcePath, recoveryRoot));
        QFile::remove(metadataPath(sourcePath, recoveryRoot));
    }
};

} // namespace pdfviewer

#endif // PDFRECOVERYMANAGER_H
