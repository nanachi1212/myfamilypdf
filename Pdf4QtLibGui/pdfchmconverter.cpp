// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfchmconverter.h"

#include "pdfaction.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfoutline.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSet>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTextDocument>
#include <QUrl>

#include <array>
#include <cstring>
#include <limits>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pdfviewer
{

namespace
{

quint32 readLE32(const QByteArray& data, qsizetype offset)
{
    if (offset < 0 || offset + 4 > data.size())
    {
        return 0;
    }
    const uchar* p = reinterpret_cast<const uchar*>(data.constData()) + offset;
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

quint64 readLE64(const QByteArray& data, qsizetype offset)
{
    return quint64(readLE32(data, offset)) | (quint64(readLE32(data, offset + 4)) << 32);
}

/// Variable length big-endian integer of the CHM directory: 7 bits per byte, high bit means "more".
bool readEncodedInteger(const QByteArray& data, qsizetype& position, qsizetype end, quint64& value)
{
    value = 0;
    for (int i = 0; i < 10 && position < end; ++i)
    {
        const uchar byte = uchar(data[position++]);
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80))
        {
            return true;
        }
    }
    return false;
}

/// Canonical Huffman code of the LZX format, decoded by code length counts.
struct HuffmanTree
{
    std::array<quint16, 17> count = { };
    std::vector<quint16> symbols;

    bool build(const quint8* lengths, int symbolCount)
    {
        count.fill(0);
        for (int i = 0; i < symbolCount; ++i)
        {
            ++count[lengths[i]];
        }
        count[0] = 0;

        int left = 1;
        for (int length = 1; length <= 16; ++length)
        {
            left = (left << 1) - count[length];
            if (left < 0)
            {
                return false;   // Over-subscribed
            }
        }

        std::array<int, 18> offsets = { };
        for (int length = 1; length <= 16; ++length)
        {
            offsets[length + 1] = offsets[length] + count[length];
        }
        symbols.assign(size_t(offsets[17]), 0);
        for (int i = 0; i < symbolCount; ++i)
        {
            if (lengths[i])
            {
                symbols[size_t(offsets[lengths[i]]++)] = quint16(i);
            }
        }
        return true;
    }
};

/// LZX decompressor (as used by CHM and CAB files) for one reset interval.
class LzxDecoder
{
public:
    explicit LzxDecoder(int windowBits) : m_windowBits(windowBits) { }

    bool decode(const QByteArray& input, qsizetype outputLength, QByteArray* output);

private:
    static constexpr int MainTreeMaxSymbols = 256 + 50 * 8;
    static constexpr int LengthTreeSymbols = 249;
    static constexpr qsizetype FrameSize = 32768;

    void fill()
    {
        while (m_count <= 48)
        {
            quint32 word = 0;
            if (m_position + 1 < m_inputSize)
            {
                word = quint32(m_input[m_position]) | (quint32(m_input[m_position + 1]) << 8);
            }
            m_position += 2;
            m_buffer |= quint64(word) << (48 - m_count);
            m_count += 16;
        }
    }

    quint32 peek(int n) { fill(); return n ? quint32(m_buffer >> (64 - n)) : 0; }
    void drop(int n) { m_buffer <<= n; m_count -= n; }
    quint32 bits(int n) { const quint32 value = peek(n); drop(n); return value; }
    void resetBits() { m_buffer = 0; m_count = 0; }

    int decodeSymbol(const HuffmanTree& tree)
    {
        const quint32 value = peek(16);
        int first = 0;
        int index = 0;
        for (int length = 1; length <= 16; ++length)
        {
            const int code = int(value >> (16 - length));
            const int count = tree.count[length];
            if (code - first < count)
            {
                drop(length);
                return tree.symbols[size_t(index + code - first)];
            }
            index += count;
            first = (first + count) << 1;
        }
        return -1;
    }

    bool readLengths(quint8* lengths, int first, int last);

    int m_windowBits;
    const uchar* m_input = nullptr;
    qsizetype m_inputSize = 0;
    qsizetype m_position = 0;
    quint64 m_buffer = 0;
    int m_count = 0;
};

bool LzxDecoder::readLengths(quint8* lengths, int first, int last)
{
    quint8 preLengths[20];
    for (quint8& length : preLengths)
    {
        length = quint8(bits(4));
    }
    HuffmanTree preTree;
    if (!preTree.build(preLengths, 20))
    {
        return false;
    }

    for (int x = first; x < last;)
    {
        const int z = decodeSymbol(preTree);
        if (z < 0)
        {
            return false;
        }
        if (z == 17 || z == 18)
        {
            int run = (z == 17) ? int(bits(4)) + 4 : int(bits(5)) + 20;
            while (run-- > 0 && x < last)
            {
                lengths[x++] = 0;
            }
        }
        else if (z == 19)
        {
            int run = int(bits(1)) + 4;
            const int delta = decodeSymbol(preTree);
            if (delta < 0 || delta > 16)
            {
                return false;
            }
            const quint8 value = quint8((lengths[x] + 17 - delta) % 17);
            while (run-- > 0 && x < last)
            {
                lengths[x++] = value;
            }
        }
        else
        {
            lengths[x] = quint8((lengths[x] + 17 - z) % 17);
            ++x;
        }
    }
    return true;
}

bool LzxDecoder::decode(const QByteArray& input, qsizetype outputLength, QByteArray* output)
{
    static const int positionSlots[] = { 30, 32, 34, 36, 38, 42, 50 };
    if (m_windowBits < 15 || m_windowBits > 21)
    {
        return false;
    }
    const int mainSymbols = 256 + positionSlots[m_windowBits - 15] * 8;

    std::array<int, 51> extraBits = { };
    std::array<quint32, 52> positionBase = { };
    for (int i = 0; i < 51; ++i)
    {
        extraBits[i] = (i < 4) ? 0 : std::min((i - 2) >> 1, 17);
        positionBase[i + 1] = positionBase[i] + (quint32(1) << extraBits[i]);
    }

    m_input = reinterpret_cast<const uchar*>(input.constData());
    m_inputSize = input.size();
    m_position = 0;
    resetBits();

    output->resize(outputLength);
    uchar* out = reinterpret_cast<uchar*>(output->data());

    quint8 mainLengths[MainTreeMaxSymbols] = { };
    quint8 lengthLengths[LengthTreeSymbols] = { };
    quint8 alignedLengths[8] = { };
    HuffmanTree mainTree;
    HuffmanTree lengthTree;
    HuffmanTree alignedTree;
    quint32 r0 = 1, r1 = 1, r2 = 1;
    int blockType = 0;
    qsizetype blockLength = 0;
    qsizetype blockRemaining = 0;

    quint32 intelFileSize = 0;
    if (bits(1))
    {
        const quint32 high = bits(16);
        intelFileSize = (high << 16) | bits(16);
    }

    qsizetype position = 0;
    while (position < outputLength)
    {
        const qsizetype frameEnd = std::min(position + FrameSize, outputLength);
        while (position < frameEnd)
        {
            if (blockRemaining == 0)
            {
                blockType = int(bits(3));
                const quint32 high = bits(16);
                blockLength = blockRemaining = qsizetype((high << 8) | bits(8));
                if (blockLength == 0)
                {
                    return false;
                }

                switch (blockType)
                {
                    case 2:
                        for (quint8& length : alignedLengths)
                        {
                            length = quint8(bits(3));
                        }
                        if (!alignedTree.build(alignedLengths, 8))
                        {
                            return false;
                        }
                        [[fallthrough]];
                    case 1:
                        if (!readLengths(mainLengths, 0, 256) || !readLengths(mainLengths, 256, mainSymbols) ||
                            !mainTree.build(mainLengths, mainSymbols) ||
                            !readLengths(lengthLengths, 0, LengthTreeSymbols) || !lengthTree.build(lengthLengths, LengthTreeSymbols))
                        {
                            return false;
                        }
                        break;

                    case 3:
                    {
                        // Skip to the next 16 bit boundary (a whole word when already aligned),
                        // give back the words already buffered, then read raw bytes.
                        fill();
                        int partial = m_count % 16;
                        if (partial == 0)
                        {
                            partial = 16;
                        }
                        m_position -= (m_count - partial) / 8;
                        resetBits();
                        if (m_position < 0 || m_position + 12 > m_inputSize)
                        {
                            return false;
                        }
                        const QByteArray repeated = input.mid(m_position, 12);
                        r0 = readLE32(repeated, 0);
                        r1 = readLE32(repeated, 4);
                        r2 = readLE32(repeated, 8);
                        m_position += 12;
                        break;
                    }

                    default:
                        return false;
                }
            }

            const qsizetype run = std::min(blockRemaining, frameEnd - position);
            if (blockType == 3)
            {
                if (m_position + run > m_inputSize)
                {
                    return false;
                }
                memcpy(out + position, m_input + m_position, size_t(run));
                m_position += run;
                position += run;
                blockRemaining -= run;
                if (blockRemaining == 0)
                {
                    if (blockLength & 1)
                    {
                        ++m_position;   // Padding byte
                    }
                    resetBits();
                }
                continue;
            }

            const qsizetype start = position;
            const qsizetype target = position + run;
            while (position < target)
            {
                int mainSymbol = decodeSymbol(mainTree);
                if (mainSymbol < 0)
                {
                    return false;
                }
                if (mainSymbol < 256)
                {
                    out[position++] = uchar(mainSymbol);
                    continue;
                }

                mainSymbol -= 256;
                qsizetype matchLength = mainSymbol & 7;
                if (matchLength == 7)
                {
                    const int extraLength = decodeSymbol(lengthTree);
                    if (extraLength < 0)
                    {
                        return false;
                    }
                    matchLength += extraLength;
                }
                matchLength += 2;

                const int slot = mainSymbol >> 3;
                quint32 offset = 0;
                if (slot == 0)
                {
                    offset = r0;
                }
                else if (slot == 1)
                {
                    offset = r1;
                    r1 = r0;
                    r0 = offset;
                }
                else if (slot == 2)
                {
                    offset = r2;
                    r2 = r0;
                    r0 = offset;
                }
                else
                {
                    const int extra = extraBits[slot];
                    offset = positionBase[slot] - 2;
                    if (blockType == 2 && extra >= 3)
                    {
                        if (extra > 3)
                        {
                            offset += bits(extra - 3) << 3;
                        }
                        const int aligned = decodeSymbol(alignedTree);
                        if (aligned < 0)
                        {
                            return false;
                        }
                        offset += quint32(aligned);
                    }
                    else
                    {
                        offset += bits(extra);
                    }
                    r2 = r1;
                    r1 = r0;
                    r0 = offset;
                }

                if (offset == 0 || qsizetype(offset) > position || position + matchLength > outputLength)
                {
                    return false;
                }
                const uchar* source = out + position - offset;
                for (qsizetype i = 0; i < matchLength; ++i)
                {
                    out[position + i] = source[i];
                }
                position += matchLength;
            }

            blockRemaining -= position - start;
            if (blockRemaining < 0 || position > frameEnd)
            {
                return false;
            }
        }

        // Each 32 KB frame ends on a 16 bit boundary.
        if (m_count > 0)
        {
            drop(m_count % 16);
        }
    }

    // Undo the x86 call translation on the output only; the window used above holds the untranslated data.
    if (intelFileSize)
    {
        for (qsizetype frameStart = 0; frameStart < outputLength; frameStart += FrameSize)
        {
            const qsizetype frameEnd = std::min(frameStart + FrameSize, outputLength);
            for (qsizetype i = frameStart; i < frameEnd - 10; ++i)
            {
                if (out[i] != 0xE8)
                {
                    continue;
                }
                const qint32 absolute = qint32(readLE32(*output, i + 1));
                const qint32 current = qint32(i);
                if (absolute >= -current && absolute < qint32(intelFileSize))
                {
                    const qint32 relative = (absolute >= 0) ? absolute - current : absolute + qint32(intelFileSize);
                    out[i + 1] = uchar(relative);
                    out[i + 2] = uchar(relative >> 8);
                    out[i + 3] = uchar(relative >> 16);
                    out[i + 4] = uchar(relative >> 24);
                }
                i += 4;
            }
        }
    }
    return true;
}

/// Text document whose images and style sheets come from the help archive.
class ChmTextDocument : public QTextDocument
{
public:
    ChmTextDocument(PDFChmArchive* archive, QString directory, quint32 languageId) :
        m_archive(archive), m_directory(std::move(directory)), m_languageId(languageId)
    {
    }

protected:
    virtual QVariant loadResource(int type, const QUrl& name) override
    {
        if (name.scheme().isEmpty() || name.scheme().compare(QLatin1String("ms-its"), Qt::CaseInsensitive) == 0)
        {
            const QByteArray data = m_archive->read(PDFChmConverter::resolvePath(m_directory, name.toString()));
            if (!data.isEmpty())
            {
                if (type == QTextDocument::StyleSheetResource)
                {
                    return PDFChmConverter::decodeText(data, m_languageId);
                }
                return data;
            }
        }
        return QVariant();  // Missing or remote resources are left out
    }

private:
    PDFChmArchive* m_archive;
    QString m_directory;
    quint32 m_languageId;
};

QString htmlUnescape(QString text)
{
    text.replace(QLatin1String("&quot;"), QLatin1String("\""));
    text.replace(QLatin1String("&lt;"), QLatin1String("<"));
    text.replace(QLatin1String("&gt;"), QLatin1String(">"));
    text.replace(QLatin1String("&#39;"), QLatin1String("'"));
    text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return text;
}

bool isHtml(const QString& path)
{
    return path.endsWith(QLatin1String(".htm"), Qt::CaseInsensitive) || path.endsWith(QLatin1String(".html"), Qt::CaseInsensitive);
}

}   // namespace

bool PDFChmArchive::open(const QString& fileName, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    m_file.setFileName(fileName);
    if (!m_file.open(QIODevice::ReadOnly))
    {
        return fail(m_file.errorString());
    }

    const QByteArray header = m_file.read(0x60);
    if (header.size() < 0x58 || !header.startsWith("ITSF"))
    {
        return fail(QObject::tr("The file is not a compiled HTML help (.chm) file."));
    }
    const quint32 version = readLE32(header, 4);
    m_languageId = readLE32(header, 0x14);
    const quint64 directoryOffset = readLE64(header, 0x48);
    const quint64 directoryLength = readLE64(header, 0x50);
    m_dataOffset = (version >= 3 && header.size() >= 0x60) ? readLE64(header, 0x58) : directoryOffset + directoryLength;

    const QByteArray directory = readRaw(directoryOffset, directoryLength);
    if (!directory.startsWith("ITSP"))
    {
        return fail(QObject::tr("The help file directory is damaged."));
    }
    const qsizetype directoryHeaderLength = readLE32(directory, 8);
    const qsizetype chunkSize = readLE32(directory, 0x10);
    if (chunkSize < 0x20)
    {
        return fail(QObject::tr("The help file directory is damaged."));
    }

    for (qsizetype chunk = directoryHeaderLength; chunk + chunkSize <= directory.size(); chunk += chunkSize)
    {
        if (directory.mid(chunk, 4) != "PMGL")
        {
            continue;
        }
        const qsizetype end = chunk + chunkSize - readLE32(directory, chunk + 4);
        qsizetype position = chunk + 0x14;
        while (position < end)
        {
            quint64 nameLength = 0;
            Entry entry;
            if (!readEncodedInteger(directory, position, end, nameLength) || position + qsizetype(nameLength) > end)
            {
                break;
            }
            entry.name = QString::fromUtf8(directory.mid(position, qsizetype(nameLength)));
            position += qsizetype(nameLength);
            if (!readEncodedInteger(directory, position, end, entry.section) ||
                !readEncodedInteger(directory, position, end, entry.offset) ||
                !readEncodedInteger(directory, position, end, entry.length))
            {
                break;
            }
            m_entries[entry.name.toLower()] = entry;
        }
    }

    if (m_entries.empty())
    {
        return fail(QObject::tr("The help file contains no files."));
    }
    return true;
}

QStringList PDFChmArchive::fileNames() const
{
    QStringList names;
    for (const auto& item : m_entries)
    {
        names << item.second.name;
    }
    return names;
}

bool PDFChmArchive::contains(const QString& name) const
{
    return m_entries.count(name.toLower());
}

QByteArray PDFChmArchive::read(const QString& name)
{
    auto it = m_entries.find(name.toLower());
    if (it == m_entries.end() || it->second.length == 0)
    {
        return QByteArray();
    }
    const Entry& entry = it->second;
    if (entry.section == 0)
    {
        return readRaw(m_dataOffset + entry.offset, entry.length);
    }
    if (entry.section == 1 && initializeCompressedSection())
    {
        return readCompressed(entry.offset, entry.length);
    }
    return QByteArray();
}

QByteArray PDFChmArchive::readRaw(quint64 offset, quint64 length)
{
    if (length > quint64(std::numeric_limits<int>::max()) || !m_file.seek(qint64(offset)))
    {
        return QByteArray();
    }
    QByteArray data = m_file.read(qint64(length));
    return data.size() == qsizetype(length) ? data : QByteArray();
}

bool PDFChmArchive::initializeCompressedSection()
{
    if (m_compressedInitialized)
    {
        return m_compressedValid;
    }
    m_compressedInitialized = true;

    auto content = m_entries.find(QStringLiteral("::dataspace/storage/mscompressed/content"));
    const QByteArray control = read(QStringLiteral("::DataSpace/Storage/MSCompressed/ControlData"));
    const QByteArray resetTable = read(QStringLiteral("::DataSpace/Storage/MSCompressed/Transform/{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable"));
    if (content == m_entries.end() || content->second.section != 0 || control.size() < 24 || control.mid(4, 4) != "LZXC" || resetTable.size() < 40)
    {
        return false;
    }

    quint64 resetInterval = readLE32(control, 12);
    quint64 windowSize = readLE32(control, 16);
    if (readLE32(control, 8) == 2)
    {
        resetInterval *= 0x8000;
        windowSize *= 0x8000;
    }
    m_windowBits = 0;
    while ((quint64(1) << m_windowBits) < windowSize && m_windowBits < 32)
    {
        ++m_windowBits;
    }

    const quint32 blockCount = readLE32(resetTable, 4);
    const quint32 tableOffset = readLE32(resetTable, 12);
    m_uncompressedLength = readLE64(resetTable, 16);
    m_compressedLength = readLE64(resetTable, 24);
    m_blockLength = readLE64(resetTable, 32);
    if (m_blockLength == 0 || resetInterval < m_blockLength || qsizetype(tableOffset) + qsizetype(blockCount) * 8 > resetTable.size())
    {
        return false;
    }
    m_blocksPerReset = resetInterval / m_blockLength;
    m_resetTable.resize(blockCount);
    for (quint32 i = 0; i < blockCount; ++i)
    {
        m_resetTable[i] = readLE64(resetTable, tableOffset + i * 8);
    }
    m_contentOffset = m_dataOffset + content->second.offset;
    m_compressedValid = true;
    return true;
}

const QByteArray* PDFChmArchive::decodedInterval(quint64 interval)
{
    auto it = m_intervalCache.find(interval);
    if (it != m_intervalCache.end())
    {
        return &it->second;
    }

    const quint64 firstBlock = interval * m_blocksPerReset;
    if (firstBlock >= m_resetTable.size())
    {
        return nullptr;
    }
    const quint64 nextBlock = firstBlock + m_blocksPerReset;
    const quint64 compressedStart = m_resetTable[firstBlock];
    const quint64 compressedEnd = nextBlock < m_resetTable.size() ? m_resetTable[nextBlock] : m_compressedLength;
    const quint64 intervalLength = m_blocksPerReset * m_blockLength;
    const quint64 outputLength = std::min(intervalLength, m_uncompressedLength - firstBlock * m_blockLength);
    if (compressedEnd < compressedStart)
    {
        return nullptr;
    }

    QByteArray output;
    LzxDecoder decoder(m_windowBits);
    if (!decoder.decode(readRaw(m_contentOffset + compressedStart, compressedEnd - compressedStart), qsizetype(outputLength), &output))
    {
        return nullptr;
    }

    // ponytail: small FIFO-ish cache; pages are mostly read in archive order
    if (m_intervalCache.size() >= 16)
    {
        m_intervalCache.erase(m_intervalCache.begin());
    }
    return &(m_intervalCache[interval] = std::move(output));
}

QByteArray PDFChmArchive::readCompressed(quint64 offset, quint64 length)
{
    const quint64 intervalLength = m_blocksPerReset * m_blockLength;
    QByteArray result;
    quint64 position = offset;
    const quint64 end = offset + length;
    while (position < end)
    {
        const quint64 interval = position / intervalLength;
        const QByteArray* data = decodedInterval(interval);
        const quint64 local = position - interval * intervalLength;
        if (!data || local >= quint64(data->size()))
        {
            return QByteArray();
        }
        const quint64 take = std::min(end - position, quint64(data->size()) - local);
        result.append(data->constData() + local, qsizetype(take));
        position += take;
    }
    return result;
}

std::vector<PDFChmConverter::Topic> PDFChmConverter::parseTableOfContents(const QString& hhc, const QString& baseDirectory)
{
    std::vector<Topic> topics;
    static const QRegularExpression tokenExpression(QStringLiteral("<(/?)ul\\b|<object\\b[^>]*>(.*?)</object>"),
                                                    QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression parameterExpression(QStringLiteral("<param\\s+name\\s*=\\s*\"([^\"]*)\"\\s+value\\s*=\\s*\"([^\"]*)\""),
                                                        QRegularExpression::CaseInsensitiveOption);

    int level = 0;
    QRegularExpressionMatchIterator it = tokenExpression.globalMatch(hhc);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedLength(2) == 0 && match.captured(0).startsWith(QLatin1Char('<')) && !match.captured(0).startsWith(QLatin1String("<o"), Qt::CaseInsensitive))
        {
            level += match.captured(1).isEmpty() ? 1 : -1;
            continue;
        }

        Topic topic;
        QRegularExpressionMatchIterator parameters = parameterExpression.globalMatch(match.captured(2));
        while (parameters.hasNext())
        {
            const QRegularExpressionMatch parameter = parameters.next();
            const QString name = parameter.captured(1);
            if (name.compare(QLatin1String("Name"), Qt::CaseInsensitive) == 0 && topic.title.isEmpty())
            {
                topic.title = htmlUnescape(parameter.captured(2)).trimmed();
            }
            else if (name.compare(QLatin1String("Local"), Qt::CaseInsensitive) == 0 && topic.path.isEmpty())
            {
                topic.path = resolvePath(baseDirectory, htmlUnescape(parameter.captured(2)));
            }
        }
        if (topic.title.isEmpty() && topic.path.isEmpty())
        {
            continue;   // The <object> holding the window properties
        }
        topic.depth = std::max(level - 1, 0);
        topics.push_back(topic);
    }
    return topics;
}

QString PDFChmConverter::resolvePath(const QString& directory, const QString& link)
{
    QString path = link.trimmed();
    const qsizetype archiveSeparator = path.indexOf(QLatin1String("::"));
    if (archiveSeparator >= 0)
    {
        path = path.mid(archiveSeparator + 2);
    }
    const qsizetype anchor = path.indexOf(QLatin1Char('#'));
    if (anchor >= 0)
    {
        path.truncate(anchor);
    }
    path = QUrl::fromPercentEncoding(path.toUtf8());
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.isEmpty())
    {
        return QString();
    }
    if (!path.startsWith(QLatin1Char('/')))
    {
        path = directory + QLatin1Char('/') + path;
    }
    while (path.startsWith(QLatin1String("//")))
    {
        path.remove(0, 1);  // cleanPath() would keep a leading "//" as a network path
    }
    path = QDir::cleanPath(path);
    return path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
}

QString PDFChmConverter::decodeText(const QByteArray& data, quint32 languageId)
{
    if (data.startsWith("\xEF\xBB\xBF"))
    {
        return QString::fromUtf8(data.mid(3));
    }

    static const QRegularExpression charsetExpression(QStringLiteral("charset\\s*=\\s*[\"']?([A-Za-z0-9_\\-]+)"), QRegularExpression::CaseInsensitiveOption);
    QString charset = charsetExpression.match(QString::fromLatin1(data.left(4096))).captured(1).toLower();
    if (charset.isEmpty())
    {
        const quint32 language = languageId & 0x3FF;
        if (language == 0x04)
        {
            const bool traditional = languageId == 0x0404 || languageId == 0x0C04 || languageId == 0x1404;
            charset = traditional ? QStringLiteral("big5") : QStringLiteral("gbk");
        }
        else if (language == 0x11)
        {
            charset = QStringLiteral("shift_jis");
        }
        else if (language == 0x12)
        {
            charset = QStringLiteral("euc-kr");
        }
        else if (language == 0x19)
        {
            charset = QStringLiteral("windows-1251");
        }
        else
        {
            charset = QStringLiteral("windows-1252");
        }
    }

    if (charset == QLatin1String("utf-8") || charset == QLatin1String("utf8"))
    {
        return QString::fromUtf8(data);
    }

#ifdef Q_OS_WIN
    static const std::map<QString, UINT> codePages = {
        { QStringLiteral("gb2312"), 936 }, { QStringLiteral("gbk"), 936 }, { QStringLiteral("x-gbk"), 936 }, { QStringLiteral("cp936"), 936 },
        { QStringLiteral("gb18030"), 54936 }, { QStringLiteral("big5"), 950 }, { QStringLiteral("big5-hkscs"), 950 }, { QStringLiteral("cp950"), 950 },
        { QStringLiteral("shift_jis"), 932 }, { QStringLiteral("sjis"), 932 }, { QStringLiteral("x-sjis"), 932 },
        { QStringLiteral("euc-kr"), 949 }, { QStringLiteral("ks_c_5601-1987"), 949 },
        { QStringLiteral("windows-1251"), 1251 }, { QStringLiteral("windows-1252"), 1252 }, { QStringLiteral("iso-8859-1"), 1252 }
    };
    auto codePage = codePages.find(charset);
    if (codePage != codePages.end() && !data.isEmpty() && data.size() < std::numeric_limits<int>::max())
    {
        const int length = MultiByteToWideChar(codePage->second, 0, data.constData(), int(data.size()), nullptr, 0);
        if (length > 0)
        {
            QString text(length, Qt::Uninitialized);
            MultiByteToWideChar(codePage->second, 0, data.constData(), int(data.size()), reinterpret_cast<wchar_t*>(text.data()), length);
            return text;
        }
    }
#endif

    QStringDecoder decoder(charset.toLatin1().constData());
    return decoder.isValid() ? QString(decoder(data)) : QString::fromLatin1(data);
}

QString PDFChmConverter::cachedPdfPath(const QString& chmFileName)
{
    const QFileInfo info(chmFileName);
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(info.absoluteFilePath().toLower().toUtf8());
    hash.addData(QByteArray::number(info.size()));
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/chm/") + QString::fromLatin1(hash.result().toHex().left(16));
    return directory + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".pdf");
}

QString PDFChmConverter::convertToPdf(const QString& chmFileName, const QString& pdfFileName, const std::function<bool(int, int)>& progress)
{
    PDFChmArchive archive;
    QString errorMessage;
    if (!archive.open(chmFileName, &errorMessage))
    {
        return errorMessage;
    }
    const quint32 languageId = archive.getLanguageId();

    // Table of contents: prefer the one in the archive root.
    QString hhcName;
    const QStringList names = archive.fileNames();
    for (const QString& name : names)
    {
        if (name.endsWith(QLatin1String(".hhc"), Qt::CaseInsensitive) && (hhcName.isEmpty() || name.count(QLatin1Char('/')) < hhcName.count(QLatin1Char('/'))))
        {
            hhcName = name;
        }
    }
    std::vector<Topic> topics;
    if (!hhcName.isEmpty())
    {
        topics = parseTableOfContents(decodeText(archive.read(hhcName), languageId), QFileInfo(hhcName).path());
    }

    // Pages in table of contents order, then the pages it does not list, so all text is searchable.
    QStringList pages;
    QSet<QString> seen;
    for (const Topic& topic : topics)
    {
        if (!topic.path.isEmpty() && isHtml(topic.path) && archive.contains(topic.path) && !seen.contains(topic.path.toLower()))
        {
            seen.insert(topic.path.toLower());
            pages << topic.path;
        }
    }
    QStringList others;
    for (const QString& name : names)
    {
        if (isHtml(name) && !name.startsWith(QLatin1String("/#")) && !name.startsWith(QLatin1String("::")) && !seen.contains(name.toLower()))
        {
            others << name;
        }
    }
    others.sort(Qt::CaseInsensitive);
    pages << others;
    if (pages.isEmpty())
    {
        return QObject::tr("The help file contains no pages.");
    }

    QDir().mkpath(QFileInfo(pdfFileName).absolutePath());
    const QString temporaryFileName = pdfFileName + QStringLiteral(".part");
    std::map<QString, int> firstPage;      // Lower case path -> zero based page
    int pageCount = 0;
    {
        QPdfWriter writer(temporaryFileName);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
        writer.setResolution(96);
        writer.setTitle(QFileInfo(chmFileName).completeBaseName());
        writer.setCreator(QStringLiteral("FamilyPDF"));

        QPainter painter;
        if (!painter.begin(&writer))
        {
            return QObject::tr("Cannot write file '%1'.").arg(QDir::toNativeSeparators(pdfFileName));
        }
        const QSizeF pageSize(writer.width(), writer.height());

        static const QRegularExpression scriptExpression(QStringLiteral("<script\\b.*?</script>"),
                                                         QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        for (int i = 0; i < pages.size(); ++i)
        {
            if (progress && !progress(i, int(pages.size())))
            {
                painter.end();
                QFile::remove(temporaryFileName);
                return QObject::tr("Conversion was cancelled.");
            }

            QString html = decodeText(archive.read(pages[i]), languageId);
            html.remove(scriptExpression);

            ChmTextDocument document(&archive, QFileInfo(pages[i]).path(), languageId);
            document.setPageSize(pageSize);
            document.setHtml(html);

            firstPage.emplace(pages[i].toLower(), pageCount);
            const int documentPages = std::max(document.pageCount(), 1);
            for (int page = 0; page < documentPages; ++page)
            {
                if (pageCount > 0)
                {
                    writer.newPage();
                }
                painter.save();
                painter.translate(0, -page * pageSize.height());
                document.drawContents(&painter, QRectF(QPointF(0, page * pageSize.height()), pageSize));
                painter.restore();
                ++pageCount;
            }
        }
        painter.end();
    }

    // Add the table of contents as bookmarks. Without it the PDF is still usable.
    pdf::PDFDocumentReader reader(nullptr, [](bool* ok) { *ok = false; return QString(); }, true, false);
    pdf::PDFDocument document = reader.readFromFile(temporaryFileName);
    if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
    {
        QFile::remove(temporaryFileName);
        return reader.getErrorMessage();
    }

    QSharedPointer<pdf::PDFOutlineItem> root(new pdf::PDFOutlineItem());
    std::vector<pdf::PDFOutlineItem*> parents = { root.data() };
    const pdf::PDFCatalog* catalog = document.getCatalog();
    for (const Topic& topic : topics)
    {
        auto page = firstPage.find(topic.path.toLower());
        if (topic.path.isEmpty() ? topic.title.isEmpty() : page == firstPage.end())
        {
            continue;
        }
        QSharedPointer<pdf::PDFOutlineItem> item(new pdf::PDFOutlineItem());
        item->setTitle(topic.title.isEmpty() ? QFileInfo(topic.path).completeBaseName() : topic.title);
        if (page != firstPage.end() && size_t(page->second) < catalog->getPageCount())
        {
            const pdf::PDFDestination destination = pdf::PDFDestination::createFit(catalog->getPage(size_t(page->second))->getPageReference());
            item->setAction(pdf::PDFActionPtr(new pdf::PDFActionGoTo(destination, pdf::PDFDestination())));
        }
        parents.resize(std::min(parents.size(), size_t(topic.depth) + 1));
        parents.back()->addChild(item);
        parents.push_back(item.data());
    }

    pdf::PDFDocumentBuilder builder(&document);
    builder.setOutline(root.data());
    const pdf::PDFDocument result = builder.build();

    QSaveFile output(pdfFileName);
    if (!output.open(QIODevice::WriteOnly))
    {
        QFile::remove(temporaryFileName);
        return output.errorString();
    }
    pdf::PDFDocumentWriter pdfWriter(nullptr);
    const pdf::PDFOperationResult writeResult = pdfWriter.write(&output, &result);
    QFile::remove(temporaryFileName);
    if (!writeResult)
    {
        output.cancelWriting();
        return writeResult.getErrorMessage();
    }
    if (!output.commit())
    {
        return output.errorString();
    }
    return QString();
}

}   // namespace pdfviewer
