// MIT License
// Copyright (c) 2026 FamilyPDF contributors

// Insert pages (v11): engine tests. Results are written and read back, so the checks see what a saved file contains.

#include "pdfcatalog.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentmerger.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfform.h"
#include "pdfnumbertreeloader.h"
#include "pdfoutline.h"
#include "pdfpageinserter.h"
#include "pdfsecurityhandler.h"

#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>
#include <numeric>
#include <set>

#ifdef Q_OS_WIN
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#endif

using pdf::PDFDocumentMerger;
using pdf::PDFPageInserter;
using Position = pdf::PDFPageInserter::Position;

namespace
{

QByteArray num(qint64 value)
{
    return QByteArray::number(value);
}

QByteArray ref(int id)
{
    return num(id) + " 0 R";
}

/// Minimal PDF writer for fixtures: objects are numbered in the order they are reserved or added.
class RawPdf
{
public:
    int reserve() { m_objects << QByteArray("null"); return int(m_objects.size()); }
    int add(const QByteArray& object) { m_objects << object; return int(m_objects.size()); }
    void set(int id, const QByteArray& object) { m_objects[id - 1] = object; }
    int addStream(const QByteArray& dictionary, const QByteArray& data)
    {
        return add("<< " + dictionary + " /Length " + num(data.size()) + " >>\nstream\n" + data + "\nendstream");
    }

    QByteArray bytes(int root) const
    {
        QByteArray pdfData = "%PDF-1.7\n";
        QList<qsizetype> offsets;
        for (qsizetype index = 0; index < m_objects.size(); ++index)
        {
            offsets << pdfData.size();
            pdfData += num(index + 1) + " 0 obj\n" + m_objects[index] + "\nendobj\n";
        }
        const qsizetype xrefOffset = pdfData.size();
        pdfData += "xref\n0 " + num(m_objects.size() + 1) + "\n0000000000 65535 f \n";
        for (const qsizetype offset : offsets)
        {
            pdfData += num(offset).rightJustified(10, '0') + " 00000 n \n";
        }
        pdfData += "trailer\n<< /Size " + num(m_objects.size() + 1) + " /Root " + ref(root) + " >>\nstartxref\n" + num(xrefOffset) + "\n%%EOF\n";
        return pdfData;
    }

private:
    QList<QByteArray> m_objects;
};

/// A simple document: every page shows "<prefix> page N" (uncompressed, so markers can be searched),
/// page N is (baseWidth + N - 1) x 400. Extra page entries, annotations and catalog entries can be added
/// before build().
struct Fixture
{
    Fixture(const QByteArray& prefix, int count, int baseWidth = 300) :
        prefix(prefix),
        baseWidth(baseWidth)
    {
        catalog = pdf.reserve();
        pagesNode = pdf.reserve();
        font = pdf.add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
        for (int i = 0; i < count; ++i)
        {
            pages.push_back(pdf.reserve());
        }
        pageExtra.resize(size_t(count));
        annots.resize(size_t(count));
    }

    QByteArray build()
    {
        QByteArray kids;
        for (size_t i = 0; i < pages.size(); ++i)
        {
            const QByteArray content = "BT /F1 18 Tf 20 360 Td (" + prefix + " page " + num(qint64(i) + 1) + ") Tj ET";
            const int contentId = pdf.addStream(QByteArray(), content);
            QByteArray page = "<< /Type /Page /Parent " + ref(pagesNode) + " /MediaBox [0 0 " + num(baseWidth + qint64(i)) + " 400]"
                              " /Resources << /Font << /F1 " + ref(font) + " >> >> /Contents " + ref(contentId);
            if (!annots[i].isEmpty())
            {
                page += " /Annots [" + annots[i] + "]";
            }
            pdf.set(pages[i], page + pageExtra[i] + " >>");
            kids += ref(pages[i]) + " ";
        }
        pdf.set(pagesNode, "<< /Type /Pages /Kids [" + kids + "] /Count " + num(qint64(pages.size())) + " >>");
        pdf.set(catalog, "<< /Type /Catalog /Pages " + ref(pagesNode) + catalogExtra + " >>");
        return pdf.bytes(catalog);
    }

    RawPdf pdf;
    QByteArray prefix;
    int baseWidth = 300;
    int catalog = 0;
    int pagesNode = 0;
    int font = 0;
    std::vector<int> pages;
    std::vector<QByteArray> pageExtra;
    std::vector<QByteArray> annots;
    QByteArray catalogExtra;
};

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

QByteArray decodedContents(const pdf::PDFDocument& document, size_t pageIndex)
{
    const pdf::PDFObjectStorage& storage = document.getStorage();
    const pdf::PDFObject contents = storage.getObject(document.getCatalog()->getPage(pageIndex)->getContents());
    QByteArray data;
    auto append = [&](const pdf::PDFObject& object)
    {
        if (object.isStream())
        {
            data += storage.getDecodedStream(object.getStream());
        }
    };
    if (contents.isArray())
    {
        for (const pdf::PDFObject& item : *contents.getArray())
        {
            append(storage.getObject(item));
        }
    }
    else
    {
        append(contents);
    }
    return data;
}

/// The "<prefix> page N" marker of a page, "blank" for a page without content.
QString pageMarker(const pdf::PDFDocument& document, size_t pageIndex)
{
    const QByteArray data = decodedContents(document, pageIndex);
    if (data.isEmpty())
    {
        return QStringLiteral("blank");
    }
    const int open = data.indexOf('(');
    const int close = data.indexOf(')', open);
    return open >= 0 && close > open ? QString::fromLatin1(data.mid(open + 1, close - open - 1)) : QString();
}

QStringList pageMarkers(const pdf::PDFDocument& document)
{
    QStringList markers;
    for (size_t i = 0; i < document.getCatalog()->getPageCount(); ++i)
    {
        markers << pageMarker(document, i);
    }
    return markers;
}

std::vector<pdf::PDFObjectReference> pageReferences(const pdf::PDFDocument& document)
{
    std::vector<pdf::PDFObjectReference> references;
    for (size_t i = 0; i < document.getCatalog()->getPageCount(); ++i)
    {
        references.push_back(document.getCatalog()->getPage(i)->getPageReference());
    }
    return references;
}

int countObjectsOfType(const pdf::PDFDocument& document, const QByteArray& type)
{
    int count = 0;
    for (const auto& entry : document.getStorage().getObjects())
    {
        if (const pdf::PDFDictionary* dictionary = document.getStorage().getDictionaryFromObject(entry.object))
        {
            const pdf::PDFObject value = dictionary->get("Type");
            count += (value.isName() && value.getString() == type) ? 1 : 0;
        }
    }
    return count;
}

int countNonNullObjects(const pdf::PDFDocument& document)
{
    int count = 0;
    for (const auto& entry : document.getStorage().getObjects())
    {
        count += entry.object.isNull() ? 0 : 1;
    }
    return count;
}

/// True if any stream of the file (decoded) contains \p marker.
bool anyStreamContains(const pdf::PDFDocument& document, const QByteArray& marker)
{
    const pdf::PDFObjectStorage& storage = document.getStorage();
    for (const auto& entry : storage.getObjects())
    {
        if (entry.object.isStream() && storage.getDecodedStream(entry.object.getStream()).contains(marker))
        {
            return true;
        }
    }
    return false;
}

QStringList pageLabelTexts(const pdf::PDFDocument& document)
{
    const pdf::PDFObjectStorage& storage = document.getStorage();
    const pdf::PDFDictionary* catalog = storage.getDictionaryFromObject(document.getTrailerDictionary()->get("Root"));
    const auto ranges = pdf::PDFNumberTreeLoader<pdf::PDFPageLabel>::parse(&storage, catalog->get("PageLabels"));
    QStringList texts;
    for (pdf::PDFInteger page = 0; page < pdf::PDFInteger(document.getCatalog()->getPageCount()); ++page)
    {
        const pdf::PDFPageLabel* range = nullptr;
        for (const pdf::PDFPageLabel& candidate : ranges)
        {
            if (candidate.getPageIndex() <= page)
            {
                range = &candidate;
            }
        }
        if (!range)
        {
            texts << QString();
            continue;
        }
        const pdf::PDFInteger number = range->getPageStartNumber() + page - range->getPageIndex();
        texts << range->getPrefix() + (range->getNumberingStyle() == pdf::PDFPageLabel::NumberingStyle::None
                                       ? QString() : pdf::PDFPageLabel::formatPageNumber(range->getNumberingStyle(), number));
    }
    return texts;
}

const pdf::PDFDictionary* dictionaryOf(const pdf::PDFDocument& document, pdf::PDFObjectReference reference)
{
    return document.getStorage().getDictionaryFromObject(document.getStorage().getObjectByReference(reference));
}

QString writeEncryptedFile(const QString& file, const QString& userPassword, const QString& ownerPassword, uint32_t permissions, int pageCount = 2)
{
    pdf::PDFDocumentBuilder builder;
    for (int i = 1; i <= pageCount; ++i)
    {
        const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 200 + i, 300));
        pdf::PDFPageContentStreamBuilder content(&builder);
        QPainter* painter = content.begin(page);
        painter->fillRect(QRectF(10, 10, 50 + i, 50), Qt::blue);
        content.end(painter);
    }
    pdf::PDFSecurityHandlerFactory::SecuritySettings settings;
    settings.algorithm = pdf::PDFSecurityHandlerFactory::AES_256;
    settings.encryptContents = pdf::PDFSecurityHandlerFactory::All;
    settings.userPassword = userPassword;
    settings.ownerPassword = ownerPassword;
    settings.permissions = permissions;
    settings.id = QByteArrayLiteral("insert-test-id-0123456789");
    builder.setSecurityHandler(pdf::PDFSecurityHandlerFactory::createSecurityHandler(settings));
    const pdf::PDFDocument document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult result = writer.write(file, &document, true);
    return result ? QString() : result.getErrorMessage();
}

constexpr uint32_t AllPermissions = 0xFFFFFFFFu;

uint32_t permissionsWithout(pdf::PDFSecurityHandler::Permission permission)
{
    return AllPermissions & ~uint32_t(permission);
}

qint64 privateBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters = { };
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
    {
        return qint64(counters.PrivateUsage);
    }
#endif
    return -1;
}

const QString FormBlocker = QStringLiteral("The selected pages of this PDF contain form fields and cannot be inserted safely yet.");

} // namespace

class InsertPagesTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void insertIndexAndSelection();
    void blankPagePositions();
    void blankPageGeometry();
    void blankPageNestedPageTree();
    void blankPageSaveReopen();
    void externalSinglePage();
    void externalMultiplePagesAndOrder();
    void duplicateSelectionIsRejected();
    void mixedBoxesAndInheritedRotation();
    void graphIsolationSinglePage();
    void jpeg2000ImageStream();
    void embeddedFontProgram();
    void annotationsAndAppearance();
    void linksAndDestinations();
    void formsAreBlocked();
    void signedSourceIsBlocked();
    void taggedSourceIsBlocked();
    void optionalContentDependency();
    void activeContentIsBlocked();
    void encryptedSource();
    void wrongPasswordAndCancel();
    void permissionsDenied();
    void encryptedTargetRoundtrip();
    void targetFormPreserved();
    void targetOutlinePreserved();
    void targetXfaAndTaggedTarget();
    void pageLabelsPreserved();
    void failureLeavesTargetUnchanged();
    void sourceLifetime();
    void measureInsertions();

private:
    QString path(const QString& name) const { return m_directory->filePath(name); }
    QString writeFixture(const QString& name, const QByteArray& data);
    pdf::PDFDocumentPointer openDocument(const QString& file, const QString& password = QString());
    PDFDocumentMerger::Source loadSource(const QString& file);
    pdf::PDFDocument saveAndReopen(const pdf::PDFDocument& document, const QString& name, const QString& password = QString());
    pdf::PDFDocumentPointer insertOk(const pdf::PDFDocument* target, pdf::PDFInteger index, const PDFDocumentMerger::Source& source,
                                    const std::vector<pdf::PDFInteger>& pages, QStringList* warnings = nullptr);
    QString insertError(const pdf::PDFDocument* target, const PDFDocumentMerger::Source& source, const std::vector<pdf::PDFInteger>& pages);

    std::unique_ptr<QTemporaryDir> m_directory;
};

void InsertPagesTest::init()
{
    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());
}

QString InsertPagesTest::writeFixture(const QString& name, const QByteArray& data)
{
    const QString file = path(name);
    return writeFile(file, data) ? file : QString();
}

pdf::PDFDocumentPointer InsertPagesTest::openDocument(const QString& file, const QString& password)
{
    pdf::PDFDocumentReader reader(nullptr, [password](bool* ok) { *ok = !password.isEmpty(); return password; }, true, false);
    pdf::PDFDocument document = reader.readFromFile(file);
    if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
    {
        qWarning() << "cannot open" << file << reader.getErrorMessage();
        return pdf::PDFDocumentPointer();
    }
    return pdf::PDFDocumentPointer(new pdf::PDFDocument(std::move(document)));
}

PDFDocumentMerger::Source InsertPagesTest::loadSource(const QString& file)
{
    const PDFDocumentMerger::LoadResult result = PDFDocumentMerger::loadSource(file, {});
    if (result.status != PDFDocumentMerger::LoadStatus::OK)
    {
        qWarning() << "cannot load source" << file << result.errorMessage;
    }
    return result.source;
}

pdf::PDFDocument InsertPagesTest::saveAndReopen(const pdf::PDFDocument& document, const QString& name, const QString& password)
{
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult result = writer.write(path(name), &document, true);
    if (!result)
    {
        qWarning() << "write failed" << result.getErrorMessage();
        return pdf::PDFDocument();
    }
    pdf::PDFDocumentPointer reopened = openDocument(path(name), password);
    return reopened ? *reopened : pdf::PDFDocument();
}

pdf::PDFDocumentPointer InsertPagesTest::insertOk(const pdf::PDFDocument* target, pdf::PDFInteger index, const PDFDocumentMerger::Source& source,
                                                 const std::vector<pdf::PDFInteger>& pages, QStringList* warnings)
{
    pdf::PDFDocumentPointer result;
    QStringList localWarnings;
    const pdf::PDFOperationResult status = PDFPageInserter::insertPages(target, index, source, pages, &result, warnings ? warnings : &localWarnings);
    if (!status)
    {
        qWarning() << "insert failed:" << status.getErrorMessage();
    }
    return result;
}

QString InsertPagesTest::insertError(const pdf::PDFDocument* target, const PDFDocumentMerger::Source& source, const std::vector<pdf::PDFInteger>& pages)
{
    pdf::PDFDocumentPointer result;
    QStringList warnings;
    const pdf::PDFOperationResult status = PDFPageInserter::insertPages(target, 0, source, pages, &result, &warnings);
    if (status || result)
    {
        return QString();
    }
    return status.getErrorMessage();
}

void InsertPagesTest::insertIndexAndSelection()
{
    // Before uses the first selected page, After the last one.
    QCOMPARE(PDFPageInserter::getInsertIndex(Position::Before, { 1, 3 }, 5), pdf::PDFInteger(1));
    QCOMPARE(PDFPageInserter::getInsertIndex(Position::After, { 1, 3 }, 5), pdf::PDFInteger(4));
    QCOMPARE(PDFPageInserter::getInsertIndex(Position::Beginning, { 1, 3 }, 5), pdf::PDFInteger(0));
    QCOMPARE(PDFPageInserter::getInsertIndex(Position::End, { 1, 3 }, 5), pdf::PDFInteger(5));
    QCOMPARE(PDFPageInserter::getInsertIndex(Position::After, { 4 }, 5), pdf::PDFInteger(5));

    QString error;
    QCOMPARE(PDFPageInserter::parsePageSelection(5, "3,1", &error), std::vector<pdf::PDFInteger>({ 2, 0 }));
    QVERIFY(error.isEmpty());
    QCOMPARE(PDFPageInserter::parsePageSelection(5, "4-5,1", &error), std::vector<pdf::PDFInteger>({ 3, 4, 0 }));
    QCOMPARE(PDFPageInserter::parsePageSelection(3, "all", &error), std::vector<pdf::PDFInteger>({ 0, 1, 2 }));
    QVERIFY(PDFPageInserter::parsePageSelection(5, "3,1,3", &error).empty());
    QVERIFY2(error.contains("3") && error.contains("more than once"), qPrintable(error));
    QVERIFY(PDFPageInserter::parsePageSelection(5, "2-4,3", &error).empty());
    QVERIFY(!error.isEmpty());
    QVERIFY(PDFPageInserter::parsePageSelection(5, "6", &error).empty());
    QVERIFY(!error.isEmpty());
}

void InsertPagesTest::blankPagePositions()
{
    Fixture fixture("A", 4);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", fixture.build()));
    QVERIFY(target);
    const std::vector<pdf::PDFObjectReference> original = pageReferences(*target);

    struct Case { Position position; std::vector<pdf::PDFInteger> anchors; pdf::PDFInteger expectedIndex; };
    const QList<Case> cases = {
        { Position::Before, { 1 }, 1 },             // 1. before page 2
        { Position::After, { 1 }, 2 },              // 2. after page 2
        { Position::Beginning, { 2 }, 0 },          // 3.
        { Position::End, { 0 }, 4 },                // 4.
        { Position::Before, { 1, 3 }, 1 },          // multi selection: first anchor
        { Position::After, { 1, 3 }, 4 },           // multi selection: last anchor
    };
    for (const Case& testCase : cases)
    {
        const pdf::PDFInteger index = PDFPageInserter::getInsertIndex(testCase.position, testCase.anchors, 4);
        QCOMPARE(index, testCase.expectedIndex);
        pdf::PDFDocumentPointer result;
        const auto status = PDFPageInserter::insertBlankPage(target.data(), index, QRectF(0, 0, 300, 400), QRectF(), pdf::PageRotation::None, &result);
        QVERIFY2(bool(status), qPrintable(status.getErrorMessage()));
        QVERIFY(result);

        // Old pages keep their objects and order; exactly one new page appears at the index.
        std::vector<pdf::PDFObjectReference> references = pageReferences(*result);
        QCOMPARE(references.size(), size_t(5));
        const pdf::PDFObjectReference blank = references[size_t(index)];
        QVERIFY(std::find(original.cbegin(), original.cend(), blank) == original.cend());
        references.erase(references.begin() + index);
        QVERIFY(references == original);
        QCOMPARE(pageMarker(*result, size_t(index)), QStringLiteral("blank"));

        // The blank page has no content and no resources of its own.
        const pdf::PDFDictionary* blankPage = dictionaryOf(*result, blank);
        QVERIFY(!blankPage->hasKey("Contents"));
        QVERIFY(!blankPage->hasKey("Resources"));
    }
    QCOMPARE(pageMarkers(*target), QStringList({ "A page 1", "A page 2", "A page 3", "A page 4" }));     // target unchanged
}

void InsertPagesTest::blankPageGeometry()
{
    Fixture fixture("A", 3);
    fixture.pageExtra[1] = " /CropBox [10 20 290 380] /Rotate 90";
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", fixture.build()));
    QVERIFY(target);

    // 5. Same as page 2: MediaBox, CropBox and Rotate are copied.
    const pdf::PDFPage* anchor = target->getCatalog()->getPage(1);
    pdf::PDFDocumentPointer result;
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 2, anchor->getMediaBox(), anchor->getCropBox(), anchor->getPageRotation(), &result));
    const pdf::PDFPage* blank = result->getCatalog()->getPage(2);
    QCOMPARE(blank->getMediaBox(), QRectF(0, 0, 301, 400));
    QCOMPARE(blank->getCropBox(), QRectF(10, 20, 280, 360));
    QCOMPARE(blank->getPageRotation(), pdf::PageRotation::Rotate90);

    // 6. A4.
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 0, QRectF(0, 0, 595.276, 841.89), QRectF(), pdf::PageRotation::None, &result));
    const pdf::PDFPage* a4 = result->getCatalog()->getPage(0);
    QVERIFY(qAbs(a4->getMediaBox().width() - 595.276) < 0.01);
    QVERIFY(qAbs(a4->getMediaBox().height() - 841.89) < 0.01);
    QCOMPARE(a4->getPageRotation(), pdf::PageRotation::None);
    QCOMPARE(a4->getCropBox(), a4->getMediaBox());

    // A flat page tree whose root carries Rotate and CropBox: the blank page does not inherit them.
    RawPdf pdf;
    const int catalog = pdf.reserve();
    const int root = pdf.reserve();
    const int page = pdf.add("<< /Type /Page /Parent " + ref(root) + " /MediaBox [0 0 300 400] >>");
    pdf.set(root, "<< /Type /Pages /Kids [" + ref(page) + "] /Count 1 /Rotate 90 /CropBox [10 10 200 200] >>");
    pdf.set(catalog, "<< /Type /Catalog /Pages " + ref(root) + " >>");
    const pdf::PDFDocumentPointer rootAttributes = openDocument(writeFixture("root-attributes.pdf", pdf.bytes(catalog)));
    QVERIFY(rootAttributes);
    QVERIFY(PDFPageInserter::insertBlankPage(rootAttributes.data(), 1, QRectF(0, 0, 595.276, 841.89), QRectF(), pdf::PageRotation::None, &result));
    const pdf::PDFDocument saved = saveAndReopen(*result, "root-attributes-out.pdf");
    QCOMPARE(saved.getCatalog()->getPage(1)->getPageRotation(), pdf::PageRotation::None);
    QCOMPARE(saved.getCatalog()->getPage(1)->getCropBox(), saved.getCatalog()->getPage(1)->getMediaBox());
    QCOMPARE(saved.getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);     // the old page is unchanged
}

void InsertPagesTest::blankPageNestedPageTree()
{
    // 7. Nested page tree with inherited MediaBox, Rotate and Resources.
    RawPdf pdf;
    const int catalog = pdf.reserve();
    const int root = pdf.reserve();
    const int node = pdf.reserve();
    const int font = pdf.add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    QList<int> pages;
    QByteArray nodeKids;
    for (int i = 0; i < 3; ++i)
    {
        const QByteArray content = "BT /F1 18 Tf 20 360 Td (N page " + num(i + 1) + ") Tj ET";
        const int contentId = pdf.addStream(QByteArray(), content);
        const int parent = i < 2 ? node : root;
        const int page = pdf.add("<< /Type /Page /Parent " + ref(parent) + " /Contents " + ref(contentId) + (i == 2 ? " /MediaBox [0 0 500 500]" : "") + " >>");
        pages << page;
        if (i < 2)
        {
            nodeKids += ref(page) + " ";
        }
    }
    pdf.set(node, "<< /Type /Pages /Parent " + ref(root) + " /Kids [" + nodeKids + "] /Count 2 /MediaBox [0 0 250 350] /Rotate 90 >>");
    pdf.set(root, "<< /Type /Pages /Kids [" + ref(node) + " " + ref(pages[2]) + "] /Count 3 /MediaBox [0 0 612 792] /Resources << /Font << /F1 " + ref(font) + " >> >> >>");
    pdf.set(catalog, "<< /Type /Catalog /Pages " + ref(root) + " >>");
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("nested.pdf", pdf.bytes(catalog)));
    QVERIFY(target);
    QCOMPARE(target->getCatalog()->getPageCount(), size_t(3));

    pdf::PDFDocumentPointer result;
    const auto status = PDFPageInserter::insertBlankPage(target.data(), 1, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result);
    QVERIFY2(bool(status), qPrintable(status.getErrorMessage()));
    const pdf::PDFDocument saved = saveAndReopen(*result, "nested-out.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "N page 1", "blank", "N page 2", "N page 3" }));
    QCOMPARE(saved.getCatalog()->getPage(0)->getMediaBox(), QRectF(0, 0, 250, 350));
    QCOMPARE(saved.getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);
    QCOMPARE(saved.getCatalog()->getPage(2)->getPageRotation(), pdf::PageRotation::Rotate90);
    QCOMPARE(saved.getCatalog()->getPage(3)->getMediaBox(), QRectF(0, 0, 500, 500));
    QCOMPARE(saved.getCatalog()->getPage(1)->getMediaBox(), QRectF(0, 0, 100, 100));
    QCOMPARE(saved.getCatalog()->getPage(1)->getPageRotation(), pdf::PageRotation::None);
    // The inherited font still reaches the old pages.
    QVERIFY(saved.getStorage().getDictionaryFromObject(saved.getCatalog()->getPage(0)->getResources()));
}

void InsertPagesTest::blankPageSaveReopen()
{
    // 9. Save / reopen.
    Fixture fixture("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", fixture.build()));
    pdf::PDFDocumentPointer result;
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 1, QRectF(0, 0, 222, 333), QRectF(0, 0, 200, 300), pdf::PageRotation::Rotate180, &result));
    const pdf::PDFDocument saved = saveAndReopen(*result, "blank-out.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "blank", "A page 2" }));
    QCOMPARE(saved.getCatalog()->getPage(1)->getMediaBox(), QRectF(0, 0, 222, 333));
    QCOMPARE(saved.getCatalog()->getPage(1)->getCropBox(), QRectF(0, 0, 200, 300));
    QCOMPARE(saved.getCatalog()->getPage(1)->getPageRotation(), pdf::PageRotation::Rotate180);
}

void InsertPagesTest::externalSinglePage()
{
    // 10. One page.
    Fixture a("A", 4);
    Fixture b("B", 5, 500);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    QVERIFY(target && source.document);

    QStringList warnings;
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 2, source, { 2 }, &warnings);
    QVERIFY(result);
    QVERIFY(warnings.isEmpty());
    const pdf::PDFDocument saved = saveAndReopen(*result, "out.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "A page 2", "B page 3", "A page 3", "A page 4" }));
    QCOMPARE(saved.getCatalog()->getPage(2)->getMediaBox(), QRectF(0, 0, 502, 400));
    QCOMPARE(pageMarkers(*source.document), QStringList({ "B page 1", "B page 2", "B page 3", "B page 4", "B page 5" }));    // source unchanged
}

void InsertPagesTest::externalMultiplePagesAndOrder()
{
    Fixture a("A", 4);
    Fixture b("B", 5, 500);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));

    // 11. Several pages.
    pdf::PDFDocumentPointer result = insertOk(target.data(), 0, source, { 1, 2, 3 });
    QVERIFY(result);
    QCOMPARE(pageMarkers(*result), QStringList({ "B page 2", "B page 3", "B page 4", "A page 1", "A page 2", "A page 3", "A page 4" }));

    // 12. "3,1" after A2 gives A1 A2 B3 B1 A3 A4 (the typed order is kept).
    QString error;
    const std::vector<pdf::PDFInteger> pages = PDFPageInserter::parsePageSelection(source.pageCount, "3,1", &error);
    result = insertOk(target.data(), PDFPageInserter::getInsertIndex(Position::After, { 1 }, 4), source, pages);
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "order.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "A page 2", "B page 3", "B page 1", "A page 3", "A page 4" }));
    QCOMPARE(saved.getCatalog()->getPage(2)->getMediaBox().width(), 502.0);
    QCOMPARE(saved.getCatalog()->getPage(3)->getMediaBox().width(), 500.0);
}

void InsertPagesTest::duplicateSelectionIsRejected()
{
    // 13. "3,1,3" is an error, nothing is inserted; the engine refuses a repeated page as well.
    Fixture a("A", 2);
    Fixture b("B", 4, 500);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    QString error;
    QVERIFY(PDFPageInserter::parsePageSelection(source.pageCount, "3,1,3", &error).empty());
    QVERIFY(!error.isEmpty());
    QVERIFY(!insertError(target.data(), source, { 2, 0, 2 }).isEmpty());
    QVERIFY(!insertError(target.data(), source, { }).isEmpty());
    QVERIFY(!insertError(target.data(), source, { 7 }).isEmpty());
}

void InsertPagesTest::mixedBoxesAndInheritedRotation()
{
    // 14. Mixed MediaBox / CropBox. 15. Inherited Rotate: parent 90, page without Rotate -> 90; page with Rotate 0 -> 0.
    RawPdf pdf;
    const int catalog = pdf.reserve();
    const int root = pdf.reserve();
    const int node = pdf.reserve();
    const int font = pdf.add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    auto addPage = [&](int parent, const QByteArray& marker, const QByteArray& extra)
    {
        const QByteArray content = "BT /F1 18 Tf 20 360 Td (" + marker + ") Tj ET";
        return pdf.add("<< /Type /Page /Parent " + ref(parent) + " /Contents " + ref(pdf.addStream(QByteArray(), content)) + extra + " >>");
    };
    const int p1 = addPage(node, "S page 1", QByteArray());
    const int p2 = addPage(node, "S page 2", " /Rotate 0");
    const int p3 = addPage(root, "S page 3", " /MediaBox [0 0 400 600] /CropBox [50 50 350 550]");
    pdf.set(node, "<< /Type /Pages /Parent " + ref(root) + " /Kids [" + ref(p1) + " " + ref(p2) + "] /Count 2 /Rotate 90 /MediaBox [0 0 300 500] /CropBox [5 5 295 495] >>");
    pdf.set(root, "<< /Type /Pages /Kids [" + ref(node) + " " + ref(p3) + "] /Count 3 /MediaBox [0 0 612 792] /Resources << /Font << /F1 " + ref(font) + " >> >> >>");
    pdf.set(catalog, "<< /Type /Catalog /Pages " + ref(root) + " >>");

    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("inherit.pdf", pdf.bytes(catalog)));
    QVERIFY(source.document);
    QCOMPARE(source.document->getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);
    QCOMPARE(source.document->getCatalog()->getPage(1)->getPageRotation(), pdf::PageRotation::None);

    const pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 0, 1, 2 });
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "inherit-out.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "S page 1", "S page 2", "S page 3", "A page 2" }));
    const pdf::PDFCatalog* catalogOut = saved.getCatalog();
    QCOMPARE(catalogOut->getPage(1)->getPageRotation(), pdf::PageRotation::Rotate90);
    QCOMPARE(catalogOut->getPage(2)->getPageRotation(), pdf::PageRotation::None);
    QCOMPARE(catalogOut->getPage(1)->getMediaBox(), QRectF(0, 0, 300, 500));
    QCOMPARE(catalogOut->getPage(1)->getCropBox(), QRectF(5, 5, 290, 490));
    QCOMPARE(catalogOut->getPage(3)->getMediaBox(), QRectF(0, 0, 400, 600));
    QCOMPARE(catalogOut->getPage(3)->getCropBox(), QRectF(50, 50, 300, 500));
    // The inherited font resource was written into each imported page.
    for (size_t i = 1; i <= 3; ++i)
    {
        const pdf::PDFDictionary* resources = saved.getStorage().getDictionaryFromObject(catalogOut->getPage(i)->getResources());
        QVERIFY(resources && saved.getStorage().getDictionaryFromObject(resources->get("Font")));
        const pdf::PDFDictionary* page = dictionaryOf(saved, catalogOut->getPage(i)->getPageReference());
        QCOMPARE(page->get("Parent"), saved.getStorage().getDictionaryFromObject(saved.getTrailerDictionary()->get("Root"))->get("Pages"));
    }
    QCOMPARE(countObjectsOfType(saved, "Pages"), 1);
}

void InsertPagesTest::graphIsolationSinglePage()
{
    // The main correctness gate: one selected page must not bring other pages along through /Parent, links,
    // annotation relations or threads.
    Fixture b("SRC", 5, 500);
    std::vector<int> notes;
    for (int i = 0; i < 5; ++i)
    {
        notes.push_back(b.pdf.reserve());
    }
    const int thread = b.pdf.reserve();
    const int bead = b.pdf.reserve();
    for (int i = 0; i < 5; ++i)
    {
        // Text note (page 3's note replies to page 2's note) and a link to the next page.
        const int link = b.pdf.add("<< /Type /Annot /Subtype /Link /Rect [0 0 50 50] /Dest [" + ref(b.pages[size_t((i + 1) % 5)]) + " /Fit] >>");
        b.pdf.set(notes[size_t(i)], "<< /Type /Annot /Subtype /Text /Rect [60 60 80 80] /Contents (note " + num(i + 1) + ") /P " + ref(b.pages[size_t(i)])
                                    + (i == 2 ? " /IRT " + ref(notes[1]) : QByteArray()) + " >>");
        b.annots[size_t(i)] = ref(notes[size_t(i)]) + " " + ref(link);
    }
    b.pdf.set(thread, "<< /Type /Thread /F " + ref(bead) + " >>");
    b.pdf.set(bead, "<< /Type /Bead /T " + ref(thread) + " /N " + ref(bead) + " /V " + ref(bead) + " /P " + ref(b.pages[1]) + " /R [0 0 10 10] >>");
    b.pageExtra[2] = " /B [" + ref(bead) + "]";
    b.catalogExtra = " /Threads [" + ref(thread) + "]";

    Fixture a("TGT", 3);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    QVERIFY(target && source.document);
    const int targetObjects = countNonNullObjects(*target);

    QStringList warnings;
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 2 }, &warnings);
    QVERIFY(result);
    QCOMPARE(warnings.size(), 1);       // the link to page 4 lost its target

    for (const pdf::PDFDocument* document : { result.data() })
    {
        QCOMPARE(document->getCatalog()->getPageCount(), size_t(4));
        QCOMPARE(countObjectsOfType(*document, "Page"), 4);
        QCOMPARE(countObjectsOfType(*document, "Bead"), 0);
        QCOMPARE(countObjectsOfType(*document, "Thread"), 0);
        QVERIFY(anyStreamContains(*document, "SRC page 3"));
        for (const QByteArray& other : { QByteArray("SRC page 1"), QByteArray("SRC page 2"), QByteArray("SRC page 4"), QByteArray("SRC page 5") })
        {
            QVERIFY2(!anyStreamContains(*document, other), other.constData());
        }
    }
    // Added objects: page, content, note, link (font shared resource also comes along).
    const int added = countNonNullObjects(*result) - targetObjects;
    qInfo() << "objects added for one page:" << added;
    QVERIFY2(added <= 6, qPrintable(QString::number(added)));

    const pdf::PDFDocument saved = saveAndReopen(*result, "isolated.pdf");
    QCOMPARE(countObjectsOfType(saved, "Page"), 4);
    QVERIFY(!anyStreamContains(saved, "SRC page 2"));
    QVERIFY(!anyStreamContains(saved, "SRC page 4"));
    const pdf::PDFPage* inserted = saved.getCatalog()->getPage(1);
    QCOMPARE(inserted->getAnnotations().size(), size_t(2));
    for (const pdf::PDFObjectReference annotation : inserted->getAnnotations())
    {
        const pdf::PDFDictionary* dictionary = dictionaryOf(saved, annotation);
        QVERIFY(!dictionary->hasKey("IRT"));
        QVERIFY(!dictionary->hasKey("Dest"));
        QVERIFY(!dictionary->hasKey("P") || dictionary->get("P").getReference() == inserted->getPageReference());
    }
    QVERIFY(!dictionaryOf(saved, inserted->getPageReference())->hasKey("B"));
}

void InsertPagesTest::jpeg2000ImageStream()
{
    // 16. JPEG 2000 image streams are copied byte for byte.
    const QString scan = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    QVERIFY(!scan.isEmpty());
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(scan);
    QVERIFY(source.document);
    QVERIFY2(PDFPageInserter::checkSource(source).isEmpty(), qPrintable(PDFPageInserter::checkSource(source).join('\n')));

    auto jpxImages = [](const pdf::PDFDocument& document, size_t pageIndex)
    {
        QList<QByteArray> images;
        const pdf::PDFObjectStorage& storage = document.getStorage();
        const pdf::PDFDictionary* resources = storage.getDictionaryFromObject(document.getCatalog()->getPage(pageIndex)->getResources());
        const pdf::PDFDictionary* xobjects = resources ? storage.getDictionaryFromObject(resources->get("XObject")) : nullptr;
        for (size_t i = 0; xobjects && i < xobjects->getCount(); ++i)
        {
            const pdf::PDFObject object = storage.getObject(xobjects->getValue(i));
            const pdf::PDFObject filter = object.isStream() ? storage.getObject(object.getStream()->getDictionary()->get("Filter")) : pdf::PDFObject();
            if (filter.isName() && filter.getString() == "JPXDecode")
            {
                images << *object.getStream()->getContent();
            }
        }
        return images;
    };
    const QList<QByteArray> sourceImages = jpxImages(*source.document, 0);
    QVERIFY(!sourceImages.isEmpty());

    const pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 0 });
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "jpx.pdf");
    QCOMPARE(jpxImages(saved, 1), sourceImages);
}

void InsertPagesTest::embeddedFontProgram()
{
    // 17. Embedded font program (written by Qt) and text come along unchanged.
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 300));
    {
        pdf::PDFPageContentStreamBuilder content(&builder);
        QPainter* painter = content.begin(page);
        QFont font(QStringLiteral("Arial"));
        font.setPixelSize(24);
        painter->setFont(font);
        painter->drawText(QPointF(20, 60), QStringLiteral("Embedded font text"));
        content.end(painter);
    }
    const pdf::PDFDocument fontDocument = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(path("font.pdf"), &fontDocument, true));

    auto fontPrograms = [](const pdf::PDFDocument& document, size_t pageIndex)
    {
        QList<QByteArray> programs;
        const pdf::PDFObjectStorage& storage = document.getStorage();
        std::set<pdf::PDFObjectReference> visited;
        std::function<void(const pdf::PDFObject&)> walk = [&](const pdf::PDFObject& object)
        {
            if (object.isReference())
            {
                if (visited.insert(object.getReference()).second)
                {
                    walk(storage.getObjectByReference(object.getReference()));
                }
                return;
            }
            const pdf::PDFDictionary* dictionary = object.isStream() ? object.getStream()->getDictionary() : (object.isDictionary() ? object.getDictionary() : nullptr);
            if (dictionary)
            {
                for (size_t i = 0; i < dictionary->getCount(); ++i)
                {
                    const QByteArray key = dictionary->getKey(i).getString();
                    const pdf::PDFObject value = storage.getObject(dictionary->getValue(i));
                    if (key.startsWith("FontFile") && value.isStream())
                    {
                        programs << *value.getStream()->getContent();
                    }
                    walk(dictionary->getValue(i));
                }
            }
            else if (object.isArray())
            {
                for (const pdf::PDFObject& item : *object.getArray())
                {
                    walk(item);
                }
            }
        };
        walk(document.getCatalog()->getPage(pageIndex)->getResources());
        return programs;
    };

    Fixture a("A", 1);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(path("font.pdf"));
    const QList<QByteArray> sourcePrograms = fontPrograms(*source.document, 0);
    if (sourcePrograms.isEmpty())
    {
        QSKIP("Qt embedded no font program (no usable system font on this machine).");
    }
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 0 });
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "font-out.pdf");
    QCOMPARE(fontPrograms(saved, 1), sourcePrograms);
    QCOMPARE(decodedContents(saved, 1), decodedContents(*source.document, 0));
}

void InsertPagesTest::annotationsAndAppearance()
{
    // 18. Text and Highlight annotations with appearance streams.
    Fixture b("B", 2, 500);
    const int appearance = b.pdf.addStream("/Type /XObject /Subtype /Form /BBox [0 0 20 20]", "1 0 0 rg 0 0 20 20 re f");
    const int highlightAppearance = b.pdf.addStream("/Type /XObject /Subtype /Form /BBox [0 0 100 20]", "1 1 0 rg 0 0 100 20 re f");
    const int text = b.pdf.add("<< /Type /Annot /Subtype /Text /Rect [20 20 40 40] /Contents (B text note) /AP << /N " + ref(appearance) + " >> /P " + ref(b.pages[0]) + " >>");
    const int highlight = b.pdf.add("<< /Type /Annot /Subtype /Highlight /Rect [20 350 120 370] /QuadPoints [20 370 120 370 20 350 120 350] /C [1 1 0]"
                                    " /Contents (B highlight) /AP << /N " + ref(highlightAppearance) + " >> /P " + ref(b.pages[0]) + " >>");
    b.annots[0] = ref(text) + " " + ref(highlight);

    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 2, source, { 0 });
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "annotations.pdf");
    const pdf::PDFPage* page = saved.getCatalog()->getPage(2);
    QCOMPARE(page->getAnnotations().size(), size_t(2));
    QStringList contents;
    for (const pdf::PDFObjectReference annotation : page->getAnnotations())
    {
        const pdf::PDFDictionary* dictionary = dictionaryOf(saved, annotation);
        QCOMPARE(dictionary->get("P").getReference(), page->getPageReference());
        contents << pdf::PDFDocumentDataLoaderDecorator(&saved).readTextString(dictionary->get("Contents"), QString());
        const pdf::PDFDictionary* ap = saved.getStorage().getDictionaryFromObject(dictionary->get("AP"));
        QVERIFY(ap);
        const pdf::PDFObject normal = saved.getStorage().getObject(ap->get("N"));
        QVERIFY(normal.isStream());
        QVERIFY(!saved.getStorage().getDecodedStream(normal.getStream()).isEmpty());
    }
    QCOMPARE(contents, QStringList({ "B text note", "B highlight" }));
}

void InsertPagesTest::linksAndDestinations()
{
    // 19. URI. 20. GoTo to a selected page (explicit, named, page number). 21. GoTo to a page that is not inserted.
    Fixture b("B", 4, 500);
    const int linkAppearance = b.pdf.addStream("/Type /XObject /Subtype /Form /BBox [0 0 50 20]", "0 0 1 RG 0 0 50 20 re S");
    const int dests = b.pdf.add("<< /Names [(dest-p2) [" + ref(b.pages[1]) + " /Fit] (dest-p3) [" + ref(b.pages[2]) + " /XYZ 0 400 0]] >>");
    b.catalogExtra = " /Names << /Dests " + ref(dests) + " >>";
    QByteArray links;
    auto addLink = [&](const QByteArray& navigation)
    {
        links += ref(b.pdf.add("<< /Type /Annot /Subtype /Link /Rect [0 0 50 20] /AP << /N " + ref(linkAppearance) + " >> " + navigation + " >>")) + " ";
    };
    addLink("/Dest [" + ref(b.pages[2]) + " /Fit]");                        // L1 explicit -> page 3
    addLink("/A << /S /GoTo /D (dest-p3) >>");                              // L2 named -> page 3
    addLink("/Dest [2 /FitH 300]");                                          // L3 page number (zero based) -> page 3
    addLink("/A << /S /URI /URI (https://example.com/familypdf) >>");       // L4 URI
    addLink("/Dest [" + ref(b.pages[1]) + " /Fit]");                        // L5 explicit -> page 2
    addLink("/A << /S /GoTo /D (dest-p2) >>");                              // L6 named -> page 2
    addLink("/Dest (missing-name)");                                         // L7 unknown name
    b.annots[0] = links;

    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));

    QStringList warnings;
    pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 0, 2 }, &warnings);
    QVERIFY(result);
    QCOMPARE(warnings.size(), 1);
    QVERIFY2(warnings.front().startsWith("3 "), qPrintable(warnings.front()));     // L5, L6, L7
    pdf::PDFDocument saved = saveAndReopen(*result, "links.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "B page 1", "B page 3", "A page 2" }));
    const pdf::PDFObjectReference insertedPage3 = saved.getCatalog()->getPage(2)->getPageReference();

    const std::vector<pdf::PDFObjectReference> annotations = saved.getCatalog()->getPage(1)->getAnnotations();
    QCOMPARE(annotations.size(), size_t(7));
    auto destinationPage = [&](size_t index) -> pdf::PDFObjectReference
    {
        const pdf::PDFDictionary* link = dictionaryOf(saved, annotations[index]);
        pdf::PDFObject destination = link->get("Dest");
        if (const pdf::PDFDictionary* action = saved.getStorage().getDictionaryFromObject(link->get("A")))
        {
            destination = action->get("D");
        }
        const pdf::PDFObject array = saved.getStorage().getObject(destination);
        return array.isArray() && array.getArray()->getItem(0).isReference() ? array.getArray()->getItem(0).getReference() : pdf::PDFObjectReference();
    };
    QCOMPARE(destinationPage(0), insertedPage3);
    QCOMPARE(destinationPage(1), insertedPage3);
    QCOMPARE(destinationPage(2), insertedPage3);
    const pdf::PDFDictionary* uri = saved.getStorage().getDictionaryFromObject(dictionaryOf(saved, annotations[3])->get("A"));
    QVERIFY(uri);
    QCOMPARE(saved.getStorage().getObject(uri->get("URI")).getString(), QByteArray("https://example.com/familypdf"));
    for (size_t index : { size_t(4), size_t(5), size_t(6) })
    {
        const pdf::PDFDictionary* link = dictionaryOf(saved, annotations[index]);
        QVERIFY(!link->hasKey("Dest"));
        QVERIFY(!link->hasKey("A"));
        QVERIFY(link->hasKey("AP"));        // the link stays visible
    }
    // The source name tree is not imported.
    const pdf::PDFDictionary* catalog = saved.getStorage().getDictionaryFromObject(saved.getTrailerDictionary()->get("Root"));
    QVERIFY(!catalog->hasKey("Names"));
    QVERIFY(saved.getCatalog()->getNamedDestinations().empty());

    // Only page 1: every internal link loses its target, the URI stays.
    result = insertOk(target.data(), 1, source, { 0 }, &warnings);
    QVERIFY(result);
    QVERIFY2(warnings.front().startsWith("6 "), qPrintable(warnings.front()));
    QCOMPARE(countObjectsOfType(*result, "Page"), 3);
}

void InsertPagesTest::formsAreBlocked()
{
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));

    // 22. Source with an AcroForm (the field is on page 2, page 1 is selected): blocked before anything is copied.
    Fixture form("F", 2, 500);
    const int widget = form.pdf.add("<< /Type /Annot /Subtype /Widget /FT /Tx /T (name) /V (x) /Rect [10 10 100 30] /P " + ref(form.pages[1]) + " >>");
    form.annots[1] = ref(widget);
    form.catalogExtra = " /AcroForm << /Fields [" + ref(widget) + "] >>";
    const PDFDocumentMerger::Source formSource = loadSource(writeFixture("form.pdf", form.build()));
    QCOMPARE(PDFPageInserter::checkSource(formSource), QStringList({ FormBlocker }));
    QCOMPARE(insertError(target.data(), formSource, { 0 }), FormBlocker);

    // 23. A Widget that no AcroForm lists ("rogue"), and a field dictionary used as a plain annotation.
    Fixture rogue("R", 2, 500);
    rogue.annots[0] = ref(rogue.pdf.add("<< /Type /Annot /Subtype /Widget /Rect [10 10 100 30] /P " + ref(rogue.pages[0]) + " >>"));
    const PDFDocumentMerger::Source rogueSource = loadSource(writeFixture("rogue.pdf", rogue.build()));
    QVERIFY(PDFPageInserter::checkSource(rogueSource).isEmpty());        // nothing in the catalog...
    QCOMPARE(insertError(target.data(), rogueSource, { 0 }), FormBlocker); // ...but the page graph is checked
    QVERIFY(insertOk(target.data(), 0, rogueSource, { 1 }));               // page 2 has no widget

    Fixture field("FT", 1, 500);
    field.annots[0] = ref(field.pdf.add("<< /Type /Annot /Subtype /Text /FT /Btn /T (x) /Rect [10 10 30 30] >>"));
    QCOMPARE(insertError(target.data(), loadSource(writeFixture("field.pdf", field.build())), { 0 }), FormBlocker);

    // XFA in the source.
    Fixture xfa("X", 1, 500);
    xfa.catalogExtra = " /AcroForm << /Fields [] /XFA (xdp) >>";
    QCOMPARE(insertError(target.data(), loadSource(writeFixture("xfa.pdf", xfa.build())), { 0 }), FormBlocker);

    // An empty AcroForm is not a form.
    Fixture empty("E", 1, 500);
    empty.catalogExtra = " /AcroForm << /Fields [] >>";
    QVERIFY(insertOk(target.data(), 0, loadSource(writeFixture("empty-form.pdf", empty.build())), { 0 }));
}

void InsertPagesTest::signedSourceIsBlocked()
{
    // 24. Signed PDF.
    const QString signedFile = QFINDTESTDATA("fixtures/pyhanko-signed.pdf");
    QVERIFY(!signedFile.isEmpty());
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const PDFDocumentMerger::Source source = loadSource(signedFile);
    QVERIFY(source.document);
    const QString error = insertError(target.data(), source, { 0 });
    QVERIFY2(error.contains("signed"), qPrintable(error));
}

void InsertPagesTest::taggedSourceIsBlocked()
{
    // 25. Tagged source: structure tree, "marked" flag, or structure parents on the page without a tree.
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));

    Fixture tree("T", 1, 500);
    const int structRoot = tree.pdf.add("<< /Type /StructTreeRoot >>");
    tree.catalogExtra = " /StructTreeRoot " + ref(structRoot);
    tree.pageExtra[0] = " /StructParents 0";
    QVERIFY(insertError(target.data(), loadSource(writeFixture("tree.pdf", tree.build())), { 0 }).contains("tagged"));

    Fixture marked("M", 1, 500);
    marked.catalogExtra = " /MarkInfo << /Marked true >>";
    QVERIFY(insertError(target.data(), loadSource(writeFixture("marked.pdf", marked.build())), { 0 }).contains("tagged"));

    Fixture parents("P", 1, 500);
    parents.pageExtra[0] = " /StructParents 3";
    QVERIFY(insertError(target.data(), loadSource(writeFixture("parents.pdf", parents.build())), { 0 }).contains("tagged"));
}

void InsertPagesTest::optionalContentDependency()
{
    // 26. Optional content used by the selected page blocks; unrelated optional content does not.
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));

    Fixture layers("L", 2, 500);
    const int ocg = layers.pdf.add("<< /Type /OCG /Name (Layer) >>");
    layers.catalogExtra = " /OCProperties << /OCGs [" + ref(ocg) + "] /D << /ON [" + ref(ocg) + "] >> >>";
    layers.pageExtra[0] = " /Group << /S /Transparency >> /PieceInfo << /Test << /Private " + ref(layers.pdf.add("<< /Properties << /MC0 " + ref(ocg) + " >> >>")) + " >> >>";
    const PDFDocumentMerger::Source source = loadSource(writeFixture("layers.pdf", layers.build()));
    QVERIFY(insertError(target.data(), source, { 0 }).contains("optional content"));
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 0, source, { 1 });     // page 2 does not use the layer
    QVERIFY(result);
    QVERIFY(result->getCatalog()->getOptionalContentProperties()->getAllOptionalContentGroups().empty());

    Fixture form("X", 1, 500);
    const int xobject = form.pdf.addStream("/Type /XObject /Subtype /Form /BBox [0 0 10 10] /OC << /Type /OCMD >>", "0 0 10 10 re f");
    form.pageExtra[0] = " /Thumb " + ref(xobject);
    QVERIFY(insertError(target.data(), loadSource(writeFixture("ocmd.pdf", form.build())), { 0 }).contains("optional content"));
}

void InsertPagesTest::activeContentIsBlocked()
{
    // 27. JavaScript, Launch, form actions, page actions and action chains are refused.
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const QList<QByteArray> annotations = {
        "<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /JavaScript /JS (app.alert(1)) >> >>",
        "<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /Launch /F (cmd.exe) >> >>",
        "<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /SubmitForm /F (https://example.com) >> >>",
        "<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /Named /N /NextPage >> >>",
        "<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /URI /URI (https://example.com) /Next << /S /JavaScript /JS (1) >> >> >>",
        "<< /Type /Annot /Subtype /Square /Rect [0 0 10 10] /AA << /E << /S /URI /URI (x) >> >> >>",
        "<< /Type /Annot /Subtype /RichMedia /Rect [0 0 10 10] >>",
        "<< /Type /Annot /Subtype /Screen /Rect [0 0 10 10] >>",
    };
    int index = 0;
    for (const QByteArray& annotation : annotations)
    {
        Fixture b("B", 1, 500);
        b.annots[0] = ref(b.pdf.add(annotation));
        const QString error = insertError(target.data(), loadSource(writeFixture(QString("active-%1.pdf").arg(index++), b.build())), { 0 });
        QVERIFY2(!error.isEmpty(), annotation.constData());
    }

    Fixture pageAction("P", 1, 500);
    pageAction.pageExtra[0] = " /AA << /O << /S /JavaScript /JS (1) >> >>";
    QVERIFY(!insertError(target.data(), loadSource(writeFixture("page-action.pdf", pageAction.build())), { 0 }).isEmpty());
}

void InsertPagesTest::encryptedSource()
{
    // 28. Encrypted source with the correct password; the inserted pages are not encrypted in an unencrypted target.
    const QString encrypted = path("enc.pdf");
    QVERIFY(writeEncryptedFile(encrypted, "secret", "owner", AllPermissions).isEmpty());
    const auto loaded = PDFDocumentMerger::loadSource(encrypted, [](bool* ok) { *ok = true; return QStringLiteral("secret"); });
    QCOMPARE(int(loaded.status), int(PDFDocumentMerger::LoadStatus::OK));
    QVERIFY(loaded.source.encrypted);

    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    QStringList warnings;
    const pdf::PDFDocumentPointer result = insertOk(target.data(), 1, loaded.source, { 1, 0 }, &warnings);
    QVERIFY(result);
    QCOMPARE(warnings.size(), 1);
    QVERIFY2(warnings.front().contains("encryption"), qPrintable(warnings.front()));

    const pdf::PDFDocument saved = saveAndReopen(*result, "enc-out.pdf");       // opens without a password
    QCOMPARE(int(saved.getStorage().getSecurityHandler()->getMode()), int(pdf::EncryptionMode::None));
    QCOMPARE(saved.getCatalog()->getPageCount(), size_t(4));
    QCOMPARE(int(saved.getCatalog()->getPage(1)->getMediaBox().width()), 202);
    QCOMPARE(int(saved.getCatalog()->getPage(2)->getMediaBox().width()), 201);
    QCOMPARE(decodedContents(saved, 1), decodedContents(*loaded.source.document, 1));   // decrypted content
}

void InsertPagesTest::wrongPasswordAndCancel()
{
    // 29. Wrong password / cancel: there is no source, so nothing can be inserted.
    const QString encrypted = path("enc.pdf");
    QVERIFY(writeEncryptedFile(encrypted, "secret", "owner", AllPermissions).isEmpty());
    auto result = PDFDocumentMerger::loadSource(encrypted, [](bool* ok) { *ok = true; return QStringLiteral("wrong"); });
    QCOMPARE(int(result.status), int(PDFDocumentMerger::LoadStatus::WrongPassword));
    QVERIFY(!result.source.document);
    result = PDFDocumentMerger::loadSource(encrypted, [](bool* ok) { *ok = false; return QString(); });
    QCOMPARE(int(result.status), int(PDFDocumentMerger::LoadStatus::Cancelled));

    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const pdf::PDFDocument before = *target;
    QVERIFY(!insertError(target.data(), result.source, { 0 }).isEmpty());
    QVERIFY(*target == before);
}

void InsertPagesTest::permissionsDenied()
{
    // 30. CopyContent denied. 31. Assemble denied. Both permissions are required.
    Fixture a("A", 2);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const QString noCopy = path("no-copy.pdf");
    const QString noAssemble = path("no-assemble.pdf");
    QVERIFY(writeEncryptedFile(noCopy, QString(), "owner", permissionsWithout(pdf::PDFSecurityHandler::Permission::CopyContent)).isEmpty());
    QVERIFY(writeEncryptedFile(noAssemble, QString(), "owner", permissionsWithout(pdf::PDFSecurityHandler::Permission::Assemble)).isEmpty());

    for (const QString& file : { noCopy, noAssemble })
    {
        const PDFDocumentMerger::Source source = loadSource(file);
        QVERIFY(source.document);
        const QString error = insertError(target.data(), source, { 0 });
        QVERIFY2(error.contains("permissions"), qPrintable(error));
    }

    // With the owner password the same restrictions do not apply (a file with a user password, so the owner password is asked).
    const QString ownerFile = path("no-copy-user.pdf");
    QVERIFY(writeEncryptedFile(ownerFile, "user", "owner", permissionsWithout(pdf::PDFSecurityHandler::Permission::CopyContent)).isEmpty());
    const auto owner = PDFDocumentMerger::loadSource(ownerFile, [](bool* ok) { *ok = true; return QStringLiteral("owner"); });
    QVERIFY(owner.source.document);
    QVERIFY(insertOk(target.data(), 0, owner.source, { 0 }));
}

void InsertPagesTest::encryptedTargetRoundtrip()
{
    // 32. Encrypted target: the result keeps the target encryption after Save and reopen.
    const QString encrypted = path("target-enc.pdf");
    QVERIFY(writeEncryptedFile(encrypted, "user", "owner", AllPermissions, 3).isEmpty());
    const pdf::PDFDocumentPointer target = openDocument(encrypted, "user");
    QVERIFY(target);
    QCOMPARE(int(target->getStorage().getSecurityHandler()->getMode()), int(pdf::EncryptionMode::Standard));

    Fixture b("B", 2, 500);
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    QStringList warnings;
    pdf::PDFDocumentPointer result = insertOk(target.data(), 1, source, { 1 }, &warnings);
    QVERIFY(result);
    QVERIFY(warnings.isEmpty());
    pdf::PDFDocumentPointer withBlank;
    QVERIFY(PDFPageInserter::insertBlankPage(result.data(), 0, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &withBlank));

    const pdf::PDFDocument saved = saveAndReopen(*withBlank, "target-enc-out.pdf", "user");
    QCOMPARE(int(saved.getStorage().getSecurityHandler()->getMode()), int(pdf::EncryptionMode::Standard));
    QCOMPARE(saved.getCatalog()->getPageCount(), size_t(5));
    QCOMPARE(pageMarker(saved, 2), QStringLiteral("B page 2"));
    QVERIFY(!openDocument(path("target-enc-out.pdf")));     // needs the password
    QFile file(path("target-enc-out.pdf"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains("B page 2"));            // the inserted content is encrypted on disk
}

void InsertPagesTest::targetFormPreserved()
{
    // 33. The target's own form keeps its fields, values and widgets.
    Fixture a("A", 3);
    const int widget = a.pdf.add("<< /Type /Annot /Subtype /Widget /FT /Tx /T (target-field) /V (kept value) /Rect [100 100 200 120] /P "
                                 + ref(a.pages[0]) + " /DA (/Helv 12 Tf 0 g) >>");
    a.annots[0] = ref(widget);
    a.catalogExtra = " /AcroForm << /Fields [" + ref(widget) + "] /DA (/Helv 0 Tf 0 g) >>";
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    Fixture b("B", 2, 500);
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));

    pdf::PDFDocumentPointer result = insertOk(target.data(), 0, source, { 0, 1 });
    QVERIFY(result);
    pdf::PDFDocumentPointer withBlank;
    QVERIFY(PDFPageInserter::insertBlankPage(result.data(), 3, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &withBlank));
    const pdf::PDFDocument saved = saveAndReopen(*withBlank, "form-out.pdf");
    const pdf::PDFForm form = pdf::PDFForm::parse(&saved, saved.getCatalog()->getFormObject());
    QCOMPARE(form.getFormFields().size(), size_t(1));
    const pdf::PDFFormField* field = form.getFormFields().front().get();
    QCOMPARE(field->getName(pdf::PDFFormField::FullyQualified), QStringLiteral("target-field"));
    QCOMPARE(pdf::PDFDocumentDataLoaderDecorator(&saved).readTextString(field->getValue(), QString()), QStringLiteral("kept value"));
    QCOMPARE(field->getWidgets().size(), size_t(1));
    QCOMPARE(field->getWidgets().front().getPage(), saved.getCatalog()->getPage(2)->getPageReference());   // still on "A page 1"
    QCOMPARE(pageMarker(saved, 2), QStringLiteral("A page 1"));
}

void InsertPagesTest::targetOutlinePreserved()
{
    // 34. A bookmark to A3 still leads to A3 (the same page object) after a page is inserted before A2.
    Fixture a("A", 4);
    const int outlines = a.pdf.reserve();
    const int item = a.pdf.add("<< /Title (to A3) /Parent " + ref(outlines) + " /Dest [" + ref(a.pages[2]) + " /Fit] >>");
    a.pdf.set(outlines, "<< /Type /Outlines /First " + ref(item) + " /Last " + ref(item) + " /Count 1 >>");
    a.catalogExtra = " /Outlines " + ref(outlines);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const pdf::PDFObjectReference a3 = target->getCatalog()->getPage(2)->getPageReference();

    pdf::PDFDocumentPointer result;
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 1, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    Fixture b("B", 2, 500);
    result = insertOk(result.data(), 1, loadSource(writeFixture("b.pdf", b.build())), { 1 });
    QVERIFY(result);
    const pdf::PDFDocument saved = saveAndReopen(*result, "outline.pdf");
    const auto root = saved.getCatalog()->getOutlineRootPtr();
    QVERIFY(root && root->getChildCount() == 1);
    const auto* goTo = dynamic_cast<const pdf::PDFActionGoTo*>(root->getChild(0)->getAction());
    QVERIFY(goTo);
    QCOMPARE(goTo->getDestination().getPageReference(), a3);
    QCOMPARE(saved.getCatalog()->getPageIndexFromPageReference(a3), size_t(4));
    QCOMPARE(pageMarker(saved, 4), QStringLiteral("A page 3"));

    // A bookmark that names a page by number would change its target: insertion is refused.
    Fixture numeric("N", 3);
    const int numericOutlines = numeric.pdf.reserve();
    const int numericItem = numeric.pdf.add("<< /Title (page 3) /Parent " + ref(numericOutlines) + " /Dest [2 /Fit] >>");
    numeric.pdf.set(numericOutlines, "<< /Type /Outlines /First " + ref(numericItem) + " /Last " + ref(numericItem) + " /Count 1 >>");
    numeric.catalogExtra = " /Outlines " + ref(numericOutlines);
    const pdf::PDFDocumentPointer numericTarget = openDocument(writeFixture("numeric.pdf", numeric.build()));
    QCOMPARE(PDFPageInserter::checkTarget(numericTarget.data(), false).size(), 1);
    QVERIFY(!PDFPageInserter::insertBlankPage(numericTarget.data(), 0, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    QVERIFY(!result);
}

void InsertPagesTest::targetXfaAndTaggedTarget()
{
    Fixture b("B", 1, 500);
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));

    // XFA target: no insertion at all.
    Fixture xfa("X", 2);
    xfa.catalogExtra = " /AcroForm << /Fields [] /XFA (xdp) >>";
    const pdf::PDFDocumentPointer xfaTarget = openDocument(writeFixture("xfa.pdf", xfa.build()));
    pdf::PDFDocumentPointer result;
    QVERIFY(!PDFPageInserter::insertBlankPage(xfaTarget.data(), 0, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    QVERIFY(!insertError(xfaTarget.data(), source, { 0 }).isEmpty());

    // Tagged target: blank pages are allowed (structure kept), pages from other PDFs are refused.
    Fixture tagged("T", 2);
    const int structRoot = tagged.pdf.add("<< /Type /StructTreeRoot /K [] >>");
    tagged.catalogExtra = " /StructTreeRoot " + ref(structRoot) + " /MarkInfo << /Marked true >>";
    tagged.pageExtra[0] = " /StructParents 0";
    const pdf::PDFDocumentPointer taggedTarget = openDocument(writeFixture("tagged.pdf", tagged.build()));
    QVERIFY(!insertError(taggedTarget.data(), source, { 0 }).isEmpty());
    QVERIFY(PDFPageInserter::insertBlankPage(taggedTarget.data(), 1, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    const pdf::PDFDocument saved = saveAndReopen(*result, "tagged-out.pdf");
    QVERIFY(!saved.getObject(saved.getCatalog()->getStructureTreeRoot()).isNull());
    QVERIFY(dictionaryOf(saved, saved.getCatalog()->getPage(0)->getPageReference())->hasKey("StructParents"));
}

void InsertPagesTest::pageLabelsPreserved()
{
    // 35. i ii 1 2 3: every old page keeps its label, inserted pages get their own range.
    Fixture a("A", 5);
    a.catalogExtra = " /PageLabels << /Nums [0 << /S /r >> 2 << /S /D >>] >>";
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("labels.pdf", a.build()));
    QCOMPARE(pageLabelTexts(*target), QStringList({ "i", "ii", "1", "2", "3" }));

    pdf::PDFDocumentPointer result;
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 2, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    pdf::PDFDocument saved = saveAndReopen(*result, "labels-blank.pdf");
    QCOMPARE(pageLabelTexts(saved), QStringList({ "i", "ii", "ii.1", "1", "2", "3" }));

    // Inside a range with a start number and a prefix: the range is split and continues with /St.
    Fixture c("C", 5);
    c.catalogExtra = " /PageLabels << /Nums [0 << /S /r >> 2 << /S /D /St 5 /P (A-) >>] >>";
    const pdf::PDFDocumentPointer prefixed = openDocument(writeFixture("labels-prefix.pdf", c.build()));
    QCOMPARE(pageLabelTexts(*prefixed), QStringList({ "i", "ii", "A-5", "A-6", "A-7" }));
    Fixture b("B", 3, 500);
    result = insertOk(prefixed.data(), 3, loadSource(writeFixture("b.pdf", b.build())), { 2, 0 });
    QVERIFY(result);
    saved = saveAndReopen(*result, "labels-external.pdf");
    QCOMPARE(pageLabelTexts(saved), QStringList({ "i", "ii", "A-5", "A-5.1", "A-5.2", "A-6", "A-7" }));
    QCOMPARE(pageMarkers(saved), QStringList({ "C page 1", "C page 2", "C page 3", "B page 3", "B page 1", "C page 4", "C page 5" }));

    // At the beginning.
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 0, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    QCOMPARE(pageLabelTexts(*result), QStringList({ "0.1", "i", "ii", "1", "2", "3" }));

    // Without labels nothing is added.
    Fixture plain("P", 2);
    const pdf::PDFDocumentPointer plainTarget = openDocument(writeFixture("plain.pdf", plain.build()));
    QVERIFY(PDFPageInserter::insertBlankPage(plainTarget.data(), 1, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    const pdf::PDFDocument plainSaved = saveAndReopen(*result, "plain-out.pdf");
    QVERIFY(!plainSaved.getStorage().getDictionaryFromObject(plainSaved.getTrailerDictionary()->get("Root"))->hasKey("PageLabels"));

    // Labels that cannot be read reliably: refused, not guessed.
    Fixture broken("X", 3);
    broken.catalogExtra = " /PageLabels << /Nums [1 << /S /D >>] >>";
    const pdf::PDFDocumentPointer brokenTarget = openDocument(writeFixture("labels-broken.pdf", broken.build()));
    QVERIFY(!PDFPageInserter::insertBlankPage(brokenTarget.data(), 1, QRectF(0, 0, 100, 100), QRectF(), pdf::PageRotation::None, &result));
    QVERIFY(!result);
}

void InsertPagesTest::failureLeavesTargetUnchanged()
{
    // 38. A refused import leaves the target exactly as it was (and returns no document).
    Fixture a("A", 3);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const pdf::PDFDocument before = *target;
    const size_t objectCount = target->getStorage().getObjects().size();

    Fixture b("B", 3, 500);
    b.annots[2] = ref(b.pdf.add("<< /Type /Annot /Subtype /Link /Rect [0 0 10 10] /A << /S /JavaScript /JS (1) >> >>"));
    const PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
    const pdf::PDFDocument sourceBefore = *source.document;
    pdf::PDFDocumentPointer result;
    QStringList warnings;
    QVERIFY(!PDFPageInserter::insertPages(target.data(), 1, source, { 0, 2 }, &result, &warnings));     // page 1 is fine, page 3 is not
    QVERIFY(!result);
    QVERIFY(*target == before);
    QCOMPARE(target->getStorage().getObjects().size(), objectCount);
    QVERIFY(*source.document == sourceBefore);
    QVERIFY(!PDFPageInserter::insertPages(target.data(), 9, source, { 0 }, &result, &warnings));       // bad position
    QVERIFY(!PDFPageInserter::insertBlankPage(target.data(), -1, QRectF(0, 0, 10, 10), QRectF(), pdf::PageRotation::None, &result));
    QVERIFY(*target == before);
}

void InsertPagesTest::sourceLifetime()
{
    // The result owns everything it needs: the source can be released before the result is saved.
    Fixture a("A", 2);
    Fixture b("B", 2, 500);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    pdf::PDFDocumentPointer result;
    {
        PDFDocumentMerger::Source source = loadSource(writeFixture("b.pdf", b.build()));
        result = insertOk(target.data(), 2, source, { 1, 0 });
        QVERIFY(result);
    }
    QVERIFY(QFile::remove(path("b.pdf")));
    const pdf::PDFDocument saved = saveAndReopen(*result, "lifetime.pdf");
    QCOMPARE(pageMarkers(saved), QStringList({ "A page 1", "A page 2", "B page 2", "B page 1" }));
}

void InsertPagesTest::measureInsertions()
{
    // Release measurements (recorded, not thresholds) and the leakage gate for a large scanned source.
    Fixture a("A", 10);
    const pdf::PDFDocumentPointer target = openDocument(writeFixture("a.pdf", a.build()));
    const int targetObjects = countNonNullObjects(*target);
    QElapsedTimer timer;

    timer.start();
    pdf::PDFDocumentPointer result;
    QVERIFY(PDFPageInserter::insertBlankPage(target.data(), 5, QRectF(0, 0, 595, 842), QRectF(), pdf::PageRotation::None, &result));
    qInfo().noquote() << QString("blank page: %1 ms, objects +%2").arg(timer.elapsed()).arg(countNonNullObjects(*result) - targetObjects);

    Fixture hundred("H", 100, 500);
    const PDFDocumentMerger::Source hundredSource = loadSource(writeFixture("hundred.pdf", hundred.build()));
    timer.restart();
    result = insertOk(target.data(), 5, hundredSource, { 50 });
    QVERIFY(result);
    qInfo().noquote() << QString("external 1 page: %1 ms, objects +%2").arg(timer.elapsed()).arg(countNonNullObjects(*result) - targetObjects);
    std::vector<pdf::PDFInteger> all(100);
    std::iota(all.begin(), all.end(), 0);
    timer.restart();
    result = insertOk(target.data(), 5, hundredSource, all);
    QVERIFY(result);
    qInfo().noquote() << QString("external 100 pages: %1 ms, objects +%2").arg(timer.elapsed()).arg(countNonNullObjects(*result) - targetObjects);

    // 500 scanned pages, each with its own 30 kB image.
    Fixture scan("SCAN", 500, 600);
    for (int i = 0; i < 500; ++i)
    {
        QByteArray pixels(100 * 100 * 3, char(i % 251));
        pixels.replace(0, 12, "SCANIMG" + num(1000 + i));
        const int image = scan.pdf.addStream("/Type /XObject /Subtype /Image /Width 100 /Height 100 /ColorSpace /DeviceRGB /BitsPerComponent 8", pixels);
        scan.pageExtra[size_t(i)] = " /Thumb " + ref(image);
    }
    timer.restart();
    const PDFDocumentMerger::Source scanSource = loadSource(writeFixture("scan.pdf", scan.build()));
    QVERIFY(scanSource.document);
    qInfo().noquote() << QString("500-page scan load: %1 ms, source objects %2").arg(timer.elapsed()).arg(countNonNullObjects(*scanSource.document));

    const qint64 memoryBefore = privateBytes();
    timer.restart();
    result = insertOk(target.data(), 0, scanSource, { 250 });
    QVERIFY(result);
    const int addedForOne = countNonNullObjects(*result) - targetObjects;
    qInfo().noquote() << QString("500-page scan, 1 page: %1 ms, objects +%2, private memory %3 -> %4 MB")
                             .arg(timer.elapsed()).arg(addedForOne).arg(memoryBefore / 1048576.0, 0, 'f', 1).arg(privateBytes() / 1048576.0, 0, 'f', 1);
    QVERIFY2(addedForOne <= 6, qPrintable(QString("page tree leakage: %1 objects").arg(addedForOne)));     // BLOCKER if it grows with the source
    QVERIFY(anyStreamContains(*result, "SCANIMG1250"));
    QVERIFY(!anyStreamContains(*result, "SCANIMG1249"));

    std::vector<pdf::PDFInteger> many(400);
    std::iota(many.begin(), many.end(), 0);
    timer.restart();
    result = insertOk(target.data(), 0, scanSource, many);
    QVERIFY(result);
    const qint64 elapsed = timer.elapsed();
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(path("scan-400.pdf"), result.data(), true));
    qInfo().noquote() << QString("500-page scan, 400 pages: %1 ms, objects +%2, output %3 MB, private memory %4 MB")
                             .arg(elapsed).arg(countNonNullObjects(*result) - targetObjects)
                             .arg(QFileInfo(path("scan-400.pdf")).size() / 1048576.0, 0, 'f', 1).arg(privateBytes() / 1048576.0, 0, 'f', 1);
}

QTEST_MAIN(InsertPagesTest)

#include "tst_insertpagestest.moc"
