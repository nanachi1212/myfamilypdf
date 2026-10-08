// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFCHMCONVERTER_H
#define PDFCHMCONVERTER_H

#include "pdf4qtlibgui_export.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QStringList>

#include <functional>
#include <map>
#include <vector>

namespace pdfviewer
{

/// Reads the files stored in a Microsoft compiled HTML help (.chm) file.
/// Names are the archive paths, for example "/index.htm", and are matched case insensitively.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFChmArchive
{
public:
    bool open(const QString& fileName, QString* errorMessage);

    QStringList fileNames() const;
    bool contains(const QString& name) const;

    /// Returns the file content, or an empty array when the file is missing or damaged.
    QByteArray read(const QString& name);

    /// Windows language identifier of the help file, used when a page names no character set.
    quint32 getLanguageId() const { return m_languageId; }

private:
    struct Entry
    {
        QString name;
        quint64 section = 0;
        quint64 offset = 0;
        quint64 length = 0;
    };

    QByteArray readRaw(quint64 offset, quint64 length);
    QByteArray readCompressed(quint64 offset, quint64 length);
    bool initializeCompressedSection();
    const QByteArray* decodedInterval(quint64 interval);

    QFile m_file;
    quint32 m_languageId = 0;
    quint64 m_dataOffset = 0;
    std::map<QString, Entry> m_entries;     ///< Key is the lower case name

    // Compressed section (LZX)
    bool m_compressedInitialized = false;
    bool m_compressedValid = false;
    quint64 m_contentOffset = 0;
    int m_windowBits = 0;
    quint64 m_blockLength = 0;
    quint64 m_blocksPerReset = 0;
    quint64 m_uncompressedLength = 0;
    quint64 m_compressedLength = 0;
    std::vector<quint64> m_resetTable;
    std::map<quint64, QByteArray> m_intervalCache;
};

/// Converts a .chm file into a searchable PDF: one or more PDF pages per help topic,
/// in table of contents order, with the table of contents as bookmarks.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFChmConverter
{
public:
    struct Topic
    {
        QString title;
        QString path;   ///< Archive path without anchor, empty for a folder without a page
        int depth = 0;
    };

    /// Parses the table of contents (.hhc), whose links are relative to \p baseDirectory.
    static std::vector<Topic> parseTableOfContents(const QString& hhc, const QString& baseDirectory);

    /// Decodes an HTML page using its charset declaration, otherwise the code page of \p languageId.
    static QString decodeText(const QByteArray& data, quint32 languageId);

    /// Normalizes a link of a page in \p directory to an archive path.
    static QString resolvePath(const QString& directory, const QString& link);

    /// Where the converted PDF of \p chmFileName is kept; it changes when the .chm changes.
    static QString cachedPdfPath(const QString& chmFileName);

    /// Writes the PDF. \p progress gets (done, total) and returns false to cancel.
    /// Returns an empty string on success, otherwise the error message.
    static QString convertToPdf(const QString& chmFileName, const QString& pdfFileName, const std::function<bool(int, int)>& progress);
};

}   // namespace pdfviewer

#endif // PDFCHMCONVERTER_H
