// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFSAFESAVESERVICE_H
#define PDFSAFESAVESERVICE_H

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QUuid>

#include <cstdio>
#include <functional>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace pdfviewer
{

class PDFSafeSaveService
{
public:
    struct Baseline
    {
        bool isValid = false;
        qint64 fileSize = -1;
        QDateTime lastModifiedUtc;
        QByteArray sha256;
        quint32 volumeSerialNumber = 0;
        QByteArray fileId;
    };

    enum class Status
    {
        Success,
        InvalidBaseline,
        SourceChanged,
        UnsupportedVolume,
        CandidateInvalid,
        BackupFailed,
        CommitFailed,
        PostCommitValidationFailed
    };

    struct Result
    {
        Status status = Status::InvalidBaseline;
        QString errorMessage;
        QString backupPath;
        QString candidatePath;
    };

    using Validator = std::function<bool(const QString&, QString*)>;

    static Baseline captureBaseline(const QString& fileName)
    {
        Baseline baseline;
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
        {
            return baseline;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd())
        {
            const QByteArray chunk = file.read(1024 * 1024);
            if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
            {
                return Baseline();
            }
            hash.addData(chunk);
        }
        file.close();

        const QFileInfo info(fileName);
        baseline.fileSize = info.size();
        baseline.lastModifiedUtc = info.lastModified().toUTC();
        baseline.sha256 = hash.result();

#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                    FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return Baseline();
        }

        BY_HANDLE_FILE_INFORMATION handleInfo = {};
        FILE_ID_INFO fileIdInfo = {};
        const bool hasHandleInfo = GetFileInformationByHandle(handle, &handleInfo);
        const bool hasFileId = GetFileInformationByHandleEx(handle,
                                                            FileIdInfo,
                                                            &fileIdInfo,
                                                            sizeof(fileIdInfo));
        CloseHandle(handle);
        if (!hasHandleInfo || !hasFileId)
        {
            return Baseline();
        }

        baseline.volumeSerialNumber = handleInfo.dwVolumeSerialNumber;
        baseline.fileId = QByteArray(reinterpret_cast<const char*>(fileIdInfo.FileId.Identifier),
                                     sizeof(fileIdInfo.FileId.Identifier));
#endif
        baseline.isValid = true;
        return baseline;
    }

    static bool sourceMatchesBaseline(const QString& fileName,
                                      const Baseline& baseline,
                                      QString* errorMessage)
    {
        if (!baseline.isValid)
        {
            setError(errorMessage, QStringLiteral("The safe-save baseline is not ready."));
            return false;
        }

        const Baseline current = captureBaseline(fileName);
        if (!current.isValid)
        {
            setError(errorMessage, QStringLiteral("The source file can no longer be read."));
            return false;
        }
        if (current.fileSize != baseline.fileSize)
        {
            setError(errorMessage, QStringLiteral("The source file size changed."));
            return false;
        }
        if (current.lastModifiedUtc != baseline.lastModifiedUtc)
        {
            setError(errorMessage, QStringLiteral("The source file modification time changed."));
            return false;
        }
        if (current.volumeSerialNumber != baseline.volumeSerialNumber ||
            current.fileId != baseline.fileId)
        {
            setError(errorMessage, QStringLiteral("The source file identity changed."));
            return false;
        }
        if (current.sha256 != baseline.sha256)
        {
            setError(errorMessage, QStringLiteral("The source file SHA-256 changed."));
            return false;
        }
        return true;
    }

    static bool isReliableLocalVolume(const QString& fileName, QString* errorMessage)
    {
        const QFileInfo info(fileName);
        const QString absolutePath = info.absoluteFilePath();
        if (absolutePath.startsWith(QStringLiteral("\\\\")))
        {
            setError(errorMessage, QStringLiteral("Network paths require Save As."));
            return false;
        }

#ifdef Q_OS_WIN
        QString rootPath = QDir::toNativeSeparators(info.absoluteDir().rootPath());
        if (!rootPath.endsWith(QChar('\\')))
        {
            rootPath.append(QChar('\\'));
        }
        const UINT driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(rootPath.utf16()));
        if (driveType == DRIVE_REMOTE || driveType == DRIVE_REMOVABLE)
        {
            setError(errorMessage, QStringLiteral("Remote and removable drives require Save As."));
            return false;
        }

        wchar_t fileSystemName[MAX_PATH] = {};
        if (!GetVolumeInformationW(reinterpret_cast<LPCWSTR>(rootPath.utf16()),
                                   nullptr,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   fileSystemName,
                                   MAX_PATH))
        {
            setError(errorMessage, QStringLiteral("The source file system could not be identified."));
            return false;
        }
        const QString fileSystem = QString::fromWCharArray(fileSystemName).toUpper();
        if (fileSystem == QStringLiteral("FAT") ||
            fileSystem == QStringLiteral("FAT32") ||
            fileSystem == QStringLiteral("EXFAT"))
        {
            setError(errorMessage, QStringLiteral("FAT and exFAT drives require Save As."));
            return false;
        }
#else
        Q_UNUSED(errorMessage);
#endif
        return true;
    }

    static Result commitCandidate(const QString& sourcePath,
                                  const QString& candidatePath,
                                  const Baseline& baseline,
                                  const Validator& validator)
    {
        Result result;
        result.candidatePath = candidatePath;

        if (!baseline.isValid)
        {
            result.status = Status::InvalidBaseline;
            result.errorMessage = QStringLiteral("The safe-save baseline is not ready.");
            return result;
        }

        if (QFileInfo(sourcePath).absolutePath().compare(
                QFileInfo(candidatePath).absolutePath(), pathCaseSensitivity()) != 0)
        {
            result.status = Status::CandidateInvalid;
            result.errorMessage = QStringLiteral("The temporary file must be in the source folder.");
            return result;
        }

        if (!isReliableLocalVolume(sourcePath, &result.errorMessage))
        {
            result.status = Status::UnsupportedVolume;
            return result;
        }
        if (!sourceMatchesBaseline(sourcePath, baseline, &result.errorMessage))
        {
            result.status = Status::SourceChanged;
            return result;
        }
        if (!QFileInfo::exists(candidatePath) ||
            !validator(candidatePath, &result.errorMessage))
        {
            result.status = Status::CandidateInvalid;
            return result;
        }
        if (!flushFile(candidatePath, &result.errorMessage))
        {
            result.status = Status::CandidateInvalid;
            return result;
        }

        const QFileInfo sourceInfo(sourcePath);
        QDir sourceDirectory(sourceInfo.absolutePath());
        const QString backupDirectoryPath =
            sourceDirectory.filePath(QStringLiteral(".FamilyPDFBackup"));
        if (!sourceDirectory.mkpath(QStringLiteral(".FamilyPDFBackup")))
        {
            result.status = Status::BackupFailed;
            result.errorMessage = QStringLiteral("The backup folder could not be created.");
            return result;
        }
#ifdef Q_OS_WIN
        const QString nativeBackupDirectory = QDir::toNativeSeparators(backupDirectoryPath);
        const DWORD attributes =
            GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativeBackupDirectory.utf16()));
        if (attributes != INVALID_FILE_ATTRIBUTES)
        {
            SetFileAttributesW(reinterpret_cast<LPCWSTR>(nativeBackupDirectory.utf16()),
                               attributes | FILE_ATTRIBUTE_HIDDEN);
        }
#endif

        const QString timestamp =
            QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"));
        QString backupFileName =
            QStringLiteral("%1.%2.bak").arg(sourceInfo.fileName(), timestamp);
        QDir backupDirectory(backupDirectoryPath);
        int suffix = 1;
        while (backupDirectory.exists(backupFileName))
        {
            backupFileName =
                QStringLiteral("%1.%2-%3.bak").arg(sourceInfo.fileName(), timestamp).arg(suffix++);
        }
        result.backupPath = backupDirectory.filePath(backupFileName);

        if (!QFile::copy(sourcePath, result.backupPath) ||
            !flushFile(result.backupPath, &result.errorMessage))
        {
            result.status = Status::BackupFailed;
            if (result.errorMessage.isEmpty())
            {
                result.errorMessage = QStringLiteral("The source backup could not be created.");
            }
            return result;
        }

#ifdef Q_OS_WIN
        const QString nativeSource = QDir::toNativeSeparators(QFileInfo(sourcePath).absoluteFilePath());
        const QString nativeCandidate = QDir::toNativeSeparators(QFileInfo(candidatePath).absoluteFilePath());
        if (!ReplaceFileW(reinterpret_cast<LPCWSTR>(nativeSource.utf16()),
                          reinterpret_cast<LPCWSTR>(nativeCandidate.utf16()),
                          nullptr,
                          REPLACEFILE_WRITE_THROUGH,
                          nullptr,
                          nullptr))
        {
            result.status = Status::CommitFailed;
            result.errorMessage =
                QStringLiteral("Atomic file replacement failed (Windows error %1).")
                    .arg(GetLastError());
            return result;
        }
#else
        const QByteArray nativeSource = QFile::encodeName(sourcePath);
        const QByteArray nativeCandidate = QFile::encodeName(candidatePath);
        if (::rename(nativeCandidate.constData(), nativeSource.constData()) != 0)
        {
            result.status = Status::CommitFailed;
            result.errorMessage = QStringLiteral("Atomic file replacement failed.");
            return result;
        }
#endif

        if (!validator(sourcePath, &result.errorMessage))
        {
            result.status = Status::PostCommitValidationFailed;
            return result;
        }

        const QFileInfoList backups = backupDirectory.entryInfoList(
            QStringList{ sourceInfo.fileName() + QStringLiteral(".*.bak") },
            QDir::Files,
            QDir::Time);
        for (int i = 3; i < backups.size(); ++i)
        {
            QFile::remove(backups.at(i).absoluteFilePath());
        }

        result.status = Status::Success;
        return result;
    }

    static Result commitNewCandidate(const QString& destinationPath,
                                     const QString& candidatePath,
                                     const Validator& validator)
    {
        Result result;
        result.candidatePath = candidatePath;
        if (QFileInfo::exists(destinationPath))
        {
            result.status = Status::CommitFailed;
            result.errorMessage =
                QStringLiteral("The new destination already exists.");
            return result;
        }
        if (QFileInfo(destinationPath).absolutePath().compare(
                QFileInfo(candidatePath).absolutePath(), pathCaseSensitivity()) != 0)
        {
            result.status = Status::CandidateInvalid;
            result.errorMessage =
                QStringLiteral("The temporary file must be in the destination folder.");
            return result;
        }
        if (!QFileInfo::exists(candidatePath) ||
            !validator(candidatePath, &result.errorMessage) ||
            !flushFile(candidatePath, &result.errorMessage))
        {
            result.status = Status::CandidateInvalid;
            return result;
        }

#ifdef Q_OS_WIN
        const QString nativeDestination =
            QDir::toNativeSeparators(QFileInfo(destinationPath).absoluteFilePath());
        const QString nativeCandidate =
            QDir::toNativeSeparators(QFileInfo(candidatePath).absoluteFilePath());
        if (!MoveFileExW(reinterpret_cast<LPCWSTR>(nativeCandidate.utf16()),
                         reinterpret_cast<LPCWSTR>(nativeDestination.utf16()),
                         MOVEFILE_WRITE_THROUGH))
        {
            result.status = Status::CommitFailed;
            result.errorMessage =
                QStringLiteral("The validated temporary file could not be moved into place "
                               "(Windows error %1).")
                    .arg(GetLastError());
            return result;
        }
#else
        if (!QFile::rename(candidatePath, destinationPath))
        {
            result.status = Status::CommitFailed;
            result.errorMessage =
                QStringLiteral("The validated temporary file could not be moved into place.");
            return result;
        }
#endif
        if (!validator(destinationPath, &result.errorMessage))
        {
            result.status = Status::PostCommitValidationFailed;
            return result;
        }
        result.status = Status::Success;
        result.candidatePath.clear();
        return result;
    }

private:
    static Qt::CaseSensitivity pathCaseSensitivity()
    {
#ifdef Q_OS_WIN
        return Qt::CaseInsensitive;
#else
        return Qt::CaseSensitive;
#endif
    }

    static void setError(QString* errorMessage, const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
    }

    static bool flushFile(const QString& fileName, QString* errorMessage)
    {
#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(QFileInfo(fileName).absoluteFilePath());
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            setError(errorMessage,
                     QStringLiteral("The file could not be opened for flushing (Windows error %1).")
                         .arg(GetLastError()));
            return false;
        }
        const bool flushed = FlushFileBuffers(handle);
        const DWORD error = flushed ? ERROR_SUCCESS : GetLastError();
        CloseHandle(handle);
        if (!flushed)
        {
            setError(errorMessage,
                     QStringLiteral("FlushFileBuffers failed (Windows error %1).").arg(error));
        }
        return flushed;
#else
        QFile file(fileName);
        if (!file.open(QIODevice::ReadWrite) || !file.flush())
        {
            setError(errorMessage, QStringLiteral("The file could not be flushed."));
            return false;
        }
        return true;
#endif
    }
};

} // namespace pdfviewer

#endif // PDFSAFESAVESERVICE_H
