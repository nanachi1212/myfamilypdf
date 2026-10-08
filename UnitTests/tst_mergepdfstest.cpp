// MIT License
// Copyright (c) 2026 FamilyPDF contributors

// Merge PDFs (v9): engine tests. They run on real files so every check reads the written PDF back.

#include "pdfdocumentbuilder.h"
#include "pdfdocumentmanipulator.h"
#include "pdfdocumentmerger.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfform.h"
#include "pdfsecurityhandler.h"
#include "pdfsignaturehandler.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <atomic>

using pdf::PDFDocumentMerger;

namespace
{

struct RangeSpec
{
    QString file;
    QString range;
};

QByteArray rawPdf(const QList<QByteArray>& objects)
{
    QByteArray pdfData = "%PDF-1.7\n";
    QList<qsizetype> offsets;
    for (qsizetype index = 0; index < objects.size(); ++index)
    {
        offsets << pdfData.size();
        pdfData += QByteArray::number(index + 1) + " 0 obj\n" + objects[index] + "\nendobj\n";
    }
    const qsizetype xrefOffset = pdfData.size();
    pdfData += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const qsizetype offset : offsets)
    {
        pdfData += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    }
    pdfData += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n"
               + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    return pdfData;
}

/// A PDF with, on every page: unique text ("<prefix> page N", uncompressed content), width baseWidth + N - 1,
/// a Square annotation; page 2 is rotated 90 degrees, page 3 has a CropBox. Page 1 has a text form field.
/// The catalog has named destinations (one per page) and an outline (one item per page).
QByteArray featurePdf(const QString& prefix, int pageCount, int baseWidth, const QString& fieldName)
{
    const int pageBase = 10;
    auto pageId = [&](int i) { return pageBase + i * 5; };
    const int outlineBase = pageBase + pageCount * 5;
    QList<QByteArray> objects(outlineBase + pageCount);

    objects[0] = "<< /Type /Catalog /Pages 2 0 R /Names 4 0 R /Outlines 7 0 R /AcroForm 6 0 R >>";
    QByteArray kids;
    for (int i = 0; i < pageCount; ++i)
    {
        kids += QByteArray::number(pageId(i)) + " 0 R ";
    }
    objects[1] = "<< /Type /Pages /Kids [" + kids + "] /Count " + QByteArray::number(pageCount) + " >>";
    objects[2] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";
    objects[3] = "<< /Dests 5 0 R >>";
    QByteArray names;
    for (int i = 0; i < pageCount; ++i)
    {
        names += "(" + prefix.toLatin1() + "-p" + QByteArray::number(i + 1) + ") [" + QByteArray::number(pageId(i)) + " 0 R /Fit] ";
    }
    objects[4] = "<< /Names [" + names + "] >>";
    objects[5] = "<< /Fields [" + QByteArray::number(pageId(0) + 3) + " 0 R] /DA (/Helv 0 Tf 0 g) >>";
    objects[6] = "<< /Type /Outlines /First " + QByteArray::number(outlineBase + 1) + " 0 R /Last " + QByteArray::number(outlineBase + pageCount) + " 0 R /Count " + QByteArray::number(pageCount) + " >>";
    objects[7] = "<< >>";
    objects[8] = "<< >>";
    objects[9] = "<< >>";

    for (int i = 0; i < pageCount; ++i)
    {
        const int id = pageId(i);
        const int width = baseWidth + i;
        QByteArray page = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + QByteArray::number(width) + " 400] /Contents "
                          + QByteArray::number(id + 1) + " 0 R /Resources << /Font << /F1 3 0 R >> >> /Annots ["
                          + QByteArray::number(id + 2) + " 0 R" + (i == 0 ? " " + QByteArray::number(id + 3) + " 0 R" : QByteArray()) + " "
                          + QByteArray::number(id + 4) + " 0 R]";
        if (i == 1)
        {
            page += " /Rotate 90";
        }
        if (i == 2)
        {
            page += " /CropBox [10 10 " + QByteArray::number(width - 10) + " 390]";
        }
        page += " >>";
        objects[id - 1] = page;
        const QByteArray content = "BT /F1 18 Tf 20 360 Td (" + prefix.toLatin1() + " page " + QByteArray::number(i + 1) + ") Tj ET";
        objects[id] = "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "\nendstream";
        objects[id + 1] = "<< /Type /Annot /Subtype /Square /Rect [20 20 80 80] /C [1 0 0] /Contents (" + prefix.toLatin1() + " note "
                          + QByteArray::number(i + 1) + ") /P " + QByteArray::number(id) + " 0 R >>";
        objects[id + 2] = i == 0 ? "<< /Type /Annot /Subtype /Widget /FT /Tx /T (" + fieldName.toLatin1() + ") /V (" + prefix.toLatin1()
                                       + " value) /Rect [100 100 200 120] /P " + QByteArray::number(id) + " 0 R /DA (/Helv 12 Tf 0 g) >>"
                                 : QByteArray("<< >>");
        // Link to the next page (the last page links to the first one) with an explicit destination.
        objects[id + 3] = "<< /Type /Annot /Subtype /Link /Rect [100 300 200 330] /Border [0 0 0] /Dest ["
                          + QByteArray::number(pageId((i + 1) % pageCount)) + " 0 R /Fit] /P " + QByteArray::number(id) + " 0 R >>";
    }
    for (int i = 0; i < pageCount; ++i)
    {
        QByteArray item = "<< /Title (" + prefix.toLatin1() + " bookmark " + QByteArray::number(i + 1) + ") /Parent 7 0 R /Dest ["
                          + QByteArray::number(pageId(i)) + " 0 R /Fit]";
        if (i > 0)
        {
            item += " /Prev " + QByteArray::number(outlineBase + i) + " 0 R";
        }
        if (i + 1 < pageCount)
        {
            item += " /Next " + QByteArray::number(outlineBase + i + 2) + " 0 R";
        }
        objects[outlineBase + i] = item + " >>";
    }
    return rawPdf(objects);
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

pdf::PDFDocument readPdf(const QString& path, bool* ok = nullptr)
{
    pdf::PDFDocumentReader reader(nullptr, {}, true, false);
    pdf::PDFDocument document = reader.readFromFile(path);
    if (ok)
    {
        *ok = reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK;
    }
    return document;
}

/// The text between parentheses in the page content: the marker written by featurePdf.
QString pageMarker(const pdf::PDFDocument& document, size_t pageIndex)
{
    const pdf::PDFObjectStorage& storage = document.getStorage();
    const pdf::PDFObject contents = storage.getObject(document.getCatalog()->getPage(pageIndex)->getContents());
    QByteArray data;
    auto append = [&](const pdf::PDFObject& object)
    {
        if (const pdf::PDFStream* stream = object.isStream() ? object.getStream() : nullptr)
        {
            data += storage.getDecodedStream(stream);
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

QString annotationText(const pdf::PDFDocument& document, pdf::PDFObjectReference annotation, const char* key)
{
    const pdf::PDFObjectStorage& storage = document.getStorage();
    const pdf::PDFDictionary* dictionary = storage.getDictionaryFromObject(storage.getObjectByReference(annotation));
    return dictionary ? pdf::PDFDocumentDataLoaderDecorator(&storage).readTextString(dictionary->get(key), QString()) : QString();
}

int countPageObjects(const pdf::PDFDocument& document)
{
    int count = 0;
    for (const auto& entry : document.getStorage().getObjects())
    {
        if (const pdf::PDFDictionary* dictionary = document.getStorage().getDictionaryFromObject(entry.object))
        {
            const pdf::PDFObject type = dictionary->get("Type");
            if (type.isName() && type.getString() == "Page")
            {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

class MergePdfsTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void parsePageList_data();
    void parsePageList();
    void twoPdfsAllPages();
    void threePdfsOrdering();
    void mergeToDocumentSharesSource();
    void pageRangesAndCustomOrder();
    void mixedSizesRotationsAndCropBox();
    void annotationsAndAppearance();
    void acroFormDifferentNames();
    void acroFormDuplicateNames();
    void excludedPagesLeaveNothingBehind();
    void bookmarksAndNamedDestinations();
    void pageLinksFollowTheirTargetPage();
    void optionalContentIsKept();
    void formAndLayersWithIndirectEntries();
    void jpeg2000ImagesAreNotReencoded();
    void encryptedSourceWithPassword();
    void invalidPasswordAndCancel();
    void permissionDenied();
    void signedSourceWarningAndInvalidSignature();
    void outputOverwriteAndSourceProtection();
    void cancelAndFailureLeaveDestinationAlone();
    void sourcesAreUnchanged();
    void measureMerges();
    void writeSmokeArtifacts();

private:
    QString path(const QString& name) const { return m_directory->filePath(name); }
    QString makeFeatureFile(const QString& name, const QString& prefix, int pages, int baseWidth, const QString& field = QStringLiteral("name"));
    PDFDocumentMerger::LoadResult load(const QString& file);
    std::vector<PDFDocumentMerger::Entry> entries(const QList<RangeSpec>& specs, QString* error = nullptr);
    pdf::PDFDocument mergeToOutput(const QList<RangeSpec>& specs, const QString& output);

    std::unique_ptr<QTemporaryDir> m_directory;
};

void MergePdfsTest::init()
{
    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());
}

QString MergePdfsTest::makeFeatureFile(const QString& name, const QString& prefix, int pages, int baseWidth, const QString& field)
{
    const QString file = path(name);
    if (!writeFile(file, featurePdf(prefix, pages, baseWidth, field)))
    {
        return QString();
    }
    return file;
}

PDFDocumentMerger::LoadResult MergePdfsTest::load(const QString& file)
{
    return PDFDocumentMerger::loadSource(file, {});
}

std::vector<PDFDocumentMerger::Entry> MergePdfsTest::entries(const QList<RangeSpec>& specs, QString* error)
{
    std::vector<PDFDocumentMerger::Entry> result;
    for (const RangeSpec& spec : specs)
    {
        PDFDocumentMerger::LoadResult loaded = load(spec.file);
        if (loaded.status != PDFDocumentMerger::LoadStatus::OK)
        {
            if (error)
            {
                *error = loaded.errorMessage;
            }
            return { };
        }
        PDFDocumentMerger::Entry entry;
        QString parseError;
        entry.pages = PDFDocumentMerger::parsePageList(loaded.source.pageCount, spec.range, &parseError);
        if (!parseError.isEmpty() && error)
        {
            *error = parseError;
        }
        entry.source = loaded.source;
        result.push_back(entry);
    }
    return result;
}

pdf::PDFDocument MergePdfsTest::mergeToOutput(const QList<RangeSpec>& specs, const QString& output)
{
    QString error;
    const auto list = entries(specs, &error);
    if (!error.isEmpty() || list.empty())
    {
        qWarning() << "entries failed:" << error;
        return pdf::PDFDocument();
    }
    bool cancelled = false;
    const pdf::PDFOperationResult result = PDFDocumentMerger::mergeToFile(list, output, nullptr, &cancelled);
    if (!result)
    {
        qWarning() << "merge failed:" << result.getErrorMessage();
        return pdf::PDFDocument();
    }
    bool ok = false;
    pdf::PDFDocument document = readPdf(output, &ok);
    return ok ? document : pdf::PDFDocument();
}

void MergePdfsTest::parsePageList_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");     // comma separated 1-based output; empty = error
    QTest::newRow("empty is all") << QString() << QStringLiteral("1,2,3,4,5,6");
    QTest::newRow("all") << QStringLiteral("all") << QStringLiteral("1,2,3,4,5,6");
    QTest::newRow("ALL spaces") << QStringLiteral("  All ") << QStringLiteral("1,2,3,4,5,6");
    QTest::newRow("single") << QStringLiteral("4") << QStringLiteral("4");
    QTest::newRow("range") << QStringLiteral("2-4") << QStringLiteral("2,3,4");
    QTest::newRow("mixed") << QStringLiteral("1-3,5,6") << QStringLiteral("1,2,3,5,6");
    QTest::newRow("order is kept") << QStringLiteral("3,1") << QStringLiteral("3,1");
    QTest::newRow("reverse order of ranges") << QStringLiteral("5-6,1-2") << QStringLiteral("5,6,1,2");
    QTest::newRow("repetition is kept") << QStringLiteral("2,2,1") << QStringLiteral("2,2,1");
    QTest::newRow("spaces") << QStringLiteral(" 1 - 2 , 6 ") << QStringLiteral("1,2,6");
    for (const char* input : {"0", "7", "1-7", "-5", "5-", "5-2", "1,abc", "1,,3", "1,", "1.5", "99999999999999999999999999", "all,1"})
    {
        QTest::newRow(input) << QString::fromLatin1(input) << QString();
    }
}

void MergePdfsTest::parsePageList()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QString error;
    const std::vector<pdf::PDFInteger> pages = PDFDocumentMerger::parsePageList(6, input, &error);
    if (expected.isEmpty())
    {
        QVERIFY(pages.empty());
        QVERIFY(!error.isEmpty());
    }
    else
    {
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QStringList numbers;
        for (const pdf::PDFInteger page : pages)
        {
            numbers << QString::number(page + 1);
        }
        QCOMPARE(numbers.join(','), expected);
    }
}

void MergePdfsTest::twoPdfsAllPages()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400);
    const pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, QString() } }, path("out.pdf"));
    QVERIFY(merged.getCatalog());
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(5));
    QCOMPARE(pageMarkers(merged), QStringList({ "A page 1", "A page 2", "A page 3", "B page 1", "B page 2" }));
    // Page content is copied as is, not re-encoded: the original text operators are in the file.
    QVERIFY(readFile(path("out.pdf")).contains("(B page 2) Tj"));
}

void MergePdfsTest::threePdfsOrdering()
{
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400);
    const QString c = makeFeatureFile("c.pdf", "C", 2, 500);
    // The list order is the output order, not alphabetical and not "as added".
    pdf::PDFDocument merged = mergeToOutput({ { c, "all" }, { a, "all" }, { b, "all" } }, path("out.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "C page 1", "C page 2", "A page 1", "A page 2", "B page 1", "B page 2" }));

    // The same file twice is two independent rows.
    merged = mergeToOutput({ { a, "1" }, { b, "2" }, { a, "2" } }, path("out2.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "A page 1", "B page 2", "A page 2" }));
}

void MergePdfsTest::mergeToDocumentSharesSource()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300, "nameA");
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400, "nameB");
    const PDFDocumentMerger::LoadResult loadedA = load(a);
    const PDFDocumentMerger::LoadResult loadedB = load(b);
    QCOMPARE(loadedA.status, PDFDocumentMerger::LoadStatus::OK);
    QCOMPARE(loadedB.status, PDFDocumentMerger::LoadStatus::OK);

    // "Insert B (pages 2,1) after page 1 of A": A is listed twice with the same document object.
    const PDFDocumentMerger::Entry before{ loadedA.source, { 0 } };
    const PDFDocumentMerger::Entry inserted{ loadedB.source, { 1, 0 } };
    const PDFDocumentMerger::Entry after{ loadedA.source, { 1, 2 } };
    pdf::PDFDocument merged;
    const pdf::PDFOperationResult result = PDFDocumentMerger::mergeToDocument({ before, inserted, after }, &merged);
    QVERIFY2(result, qPrintable(result.getErrorMessage()));
    const QStringList expected = { "A page 1", "B page 2", "B page 1", "A page 2", "A page 3" };
    QCOMPARE(pageMarkers(merged), expected);
    QCOMPARE(countPageObjects(merged), 5);

    // One source for A: its form field is in the merged form once, next to B's field.
    const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
    QVERIFY(form.isAcroForm());
    QStringList names;
    for (const auto& field : form.getFormFields())
    {
        names << field->getName(pdf::PDFFormField::FullyQualified);
    }
    names.sort();
    QCOMPARE(names, QStringList({ "nameA", "nameB" }));

    // The in-memory result is a complete PDF once written.
    const QString output = path("inserted.pdf");
    {
        QFile file(output);
        QVERIFY(file.open(QIODevice::WriteOnly));
        pdf::PDFDocumentWriter writer(nullptr);
        const pdf::PDFOperationResult written = writer.write(&file, &merged);
        QVERIFY2(written, qPrintable(written.getErrorMessage()));
    }
    bool ok = false;
    const pdf::PDFDocument reopened = readPdf(output, &ok);
    QVERIFY(ok);
    QCOMPARE(pageMarkers(reopened), expected);
}

void MergePdfsTest::pageRangesAndCustomOrder()
{
    const QString a = makeFeatureFile("a.pdf", "A", 6, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 6, 400);
    const QString c = makeFeatureFile("c.pdf", "C", 3, 500);
    // A:1-2  B:3,1  C:all   ->   A1 A2 B3 B1 C1 C2 C3
    pdf::PDFDocument merged = mergeToOutput({ { a, "1-2" }, { b, "3,1" }, { c, "all" } }, path("out.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "A page 1", "A page 2", "B page 3", "B page 1", "C page 1", "C page 2", "C page 3" }));

    // Discontinuous ranges and ranges in reverse order of position.
    merged = mergeToOutput({ { a, "1-2,5,6" }, { b, "6,4-5,1" } }, path("out2.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "A page 1", "A page 2", "A page 5", "A page 6", "B page 6", "B page 4", "B page 5", "B page 1" }));

    // A page used twice is copied twice, as typed. Output page widths prove each copy is the right page.
    merged = mergeToOutput({ { b, "2,2,1" } }, path("out3.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "B page 2", "B page 2", "B page 1" }));

    // 3,1,3: three separate page objects (a page tree must not list one page object twice).
    merged = mergeToOutput({ { b, "3,1,3" } }, path("out4.pdf"));
    QCOMPARE(pageMarkers(merged), QStringList({ "B page 3", "B page 1", "B page 3" }));
    QCOMPARE(countPageObjects(merged), 3);
    QVERIFY(merged.getCatalog()->getPage(0)->getPageReference() != merged.getCatalog()->getPage(2)->getPageReference());
}

void MergePdfsTest::mixedSizesRotationsAndCropBox()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 3, 600);
    const pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, "3,2,1" } }, path("out.pdf"));
    const pdf::PDFCatalog* catalog = merged.getCatalog();
    QCOMPARE(catalog->getPageCount(), size_t(6));
    const int widths[] = { 300, 301, 302, 602, 601, 600 };
    for (size_t i = 0; i < 6; ++i)
    {
        QCOMPARE(int(catalog->getPage(i)->getMediaBox().width()), widths[i]);
        QCOMPARE(int(catalog->getPage(i)->getMediaBox().height()), 400);
    }
    // Page 2 of each source is rotated 90 degrees, no other page is.
    const pdf::PageRotation rotations[] = { pdf::PageRotation::None, pdf::PageRotation::Rotate90, pdf::PageRotation::None,
                                            pdf::PageRotation::None, pdf::PageRotation::Rotate90, pdf::PageRotation::None };
    for (size_t i = 0; i < 6; ++i)
    {
        QCOMPARE(catalog->getPage(i)->getPageRotation(), rotations[i]);
    }
    // Page 3 of each source has a CropBox smaller than its MediaBox.
    for (const size_t index : { size_t(2), size_t(3) })
    {
        const pdf::PDFPage* page = catalog->getPage(index);
        QVERIFY(page->getCropBox().width() < page->getMediaBox().width());
        QCOMPARE(page->getCropBox().left(), 10.0);
        QCOMPARE(page->getCropBox().top(), 10.0);
    }
    QCOMPARE(catalog->getPage(0)->getCropBox(), catalog->getPage(0)->getMediaBox());
}

void MergePdfsTest::annotationsAndAppearance()
{
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400, "other");
    const pdf::PDFDocument merged = mergeToOutput({ { b, "2,1" }, { a, "2" } }, path("out.pdf"));
    const pdf::PDFCatalog* catalog = merged.getCatalog();
    QCOMPARE(catalog->getPageCount(), size_t(3));
    const QStringList expectedNotes = { "B note 2", "B note 1", "A note 2" };
    for (size_t i = 0; i < 3; ++i)
    {
        const pdf::PDFPage* page = catalog->getPage(i);
        QVERIFY(!page->getAnnotations().empty());
        const pdf::PDFObjectReference square = page->getAnnotations().front();
        QCOMPARE(annotationText(merged, square, "Contents"), expectedNotes[int(i)]);
        // The annotation points back to ITS page in the new document.
        const pdf::PDFDictionary* dictionary = merged.getStorage().getDictionaryFromObject(merged.getStorage().getObjectByReference(square));
        QVERIFY(dictionary);
        QVERIFY(dictionary->get("P").isReference());
        QVERIFY(dictionary->get("P").getReference() == page->getPageReference());
        QCOMPARE(dictionary->get("Rect").getArray()->getCount(), size_t(4));
    }
}

void MergePdfsTest::acroFormDifferentNames()
{
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300, "nameA");
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400, "nameB");
    QString error;
    const auto list = entries({ { a, "all" }, { b, "all" } }, &error);
    const auto report = PDFDocumentMerger::analyze(list);
    QVERIFY2(report.blockers.isEmpty(), qPrintable(report.blockers.join('\n')));
    for (const QString& warning : report.warnings)
    {
        QVERIFY2(!warning.contains("same name"), qPrintable(warning));    // different names: no form conflict
    }

    const pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, "all" } }, path("out.pdf"));
    const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
    QVERIFY(form.isAcroForm());
    QCOMPARE(form.getFormFields().size(), size_t(2));
    QStringList names;
    QStringList values;
    for (const auto& field : form.getFormFields())
    {
        names << field->getName(pdf::PDFFormField::FullyQualified);
        values << pdf::PDFDocumentDataLoaderDecorator(&merged).readTextString(merged.getStorage().getObject(field->getValue()), QString());
        QCOMPARE(field->getWidgets().size(), size_t(1));
    }
    QCOMPARE(names, QStringList({ "nameA", "nameB" }));
    QCOMPARE(values, QStringList({ "A value", "B value" }));
    // Each widget sits on the page that holds its own text.
    for (const auto& field : form.getFormFields())
    {
        const pdf::PDFObjectReference page = field->getWidgets().front().getPage();
        bool found = false;
        for (size_t i = 0; i < merged.getCatalog()->getPageCount(); ++i)
        {
            if (merged.getCatalog()->getPage(i)->getPageReference() == page)
            {
                found = true;
                QVERIFY(pageMarker(merged, i).endsWith("page 1"));
            }
        }
        QVERIFY(found);
    }
}

void MergePdfsTest::acroFormDuplicateNames()
{
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300, "same");
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400, "same");
    QString error;
    const auto list = entries({ { a, "all" }, { b, "all" } }, &error);
    const auto report = PDFDocumentMerger::analyze(list);
    QVERIFY(report.blockers.isEmpty());
    QStringList formWarnings;
    for (const QString& warning : report.warnings)
    {
        if (warning.contains("same name"))
        {
            formWarnings << warning;
        }
    }
    QCOMPARE(formWarnings.size(), 1);
    QVERIFY2(formWarnings.front().contains("(same)"), qPrintable(formWarnings.front()));

    // Behaviour that the warning is about: both fields stay, with their own values and own widgets.
    // They are not renamed (the existing manipulator has no safe rename), so equal names remain equal.
    const pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, "all" } }, path("out.pdf"));
    const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
    QCOMPARE(form.getFormFields().size(), size_t(2));
    QCOMPARE(form.getFormFields()[0]->getName(pdf::PDFFormField::FullyQualified), QString("same"));
    QCOMPARE(form.getFormFields()[1]->getName(pdf::PDFFormField::FullyQualified), QString("same"));
    QVERIFY(form.getFormFields()[0]->getWidgets().front().getWidget() != form.getFormFields()[1]->getWidgets().front().getWidget());

    // The same name inside one source is not a merge conflict.
    const auto single = entries({ { a, "all" } }, &error);
    for (const QString& warning : PDFDocumentMerger::analyze(single).warnings)
    {
        QVERIFY2(!warning.contains("same name"), qPrintable(warning));
    }
}

void MergePdfsTest::excludedPagesLeaveNothingBehind()
{
    // A: only page 4 (its link points to the left-out page 5; its form field is on the left-out page 1).
    // B: pages 1-2 (page 1 has the field and a link to page 2, page 2 links to the left-out page 3).
    const QString a = makeFeatureFile("a.pdf", "A", 6, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 6, 400, "other");
    const pdf::PDFDocument merged = mergeToOutput({ { a, "4" }, { b, "1-2" } }, path("out.pdf"));
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(3));
    QCOMPARE(countPageObjects(merged), 3);    // no left-out page is kept as an unused object
    const QByteArray bytes = readFile(path("out.pdf"));
    QVERIFY(bytes.contains("(A page 4) Tj"));
    QVERIFY(bytes.contains("(B page 2) Tj"));
    for (const char* excluded : { "(A page 1)", "(A page 2)", "(A page 3)", "(A page 5)", "(A page 6)", "(B page 3)", "(B page 6)",
                                  "A note 1", "A note 5", "B note 3", "A value" })
    {
        QVERIFY2(!bytes.contains(excluded), excluded);
    }

    // The field that lives on a kept page stays, the field of the left-out page is gone.
    const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
    QCOMPARE(form.getFormFields().size(), size_t(1));
    QCOMPARE(form.getFormFields().front()->getName(pdf::PDFFormField::FullyQualified), QString("other"));

    // Links: B page 1 -> B page 2 still works, links to left-out pages lead nowhere (no page, no crash).
    for (size_t i = 0; i < 3; ++i)
    {
        const pdf::PDFPage* page = merged.getCatalog()->getPage(i);
        for (const pdf::PDFObjectReference annotation : page->getAnnotations())
        {
            const pdf::PDFDictionary* dictionary = merged.getStorage().getDictionaryFromObject(merged.getStorage().getObjectByReference(annotation));
            if (!dictionary || !dictionary->get("Subtype").isName() || dictionary->get("Subtype").getString() != "Link")
            {
                continue;
            }
            const pdf::PDFDestination destination = pdf::PDFDestination::parse(&merged.getStorage(), dictionary->get("Dest"));
            const QString marker = pageMarker(merged, i);
            if (marker == "B page 1")
            {
                QVERIFY(destination.getPageReference() == merged.getCatalog()->getPage(2)->getPageReference());
            }
            else
            {
                QVERIFY2(!destination.getPageReference().isValid(), qPrintable(marker));
            }
        }
    }
}

void MergePdfsTest::bookmarksAndNamedDestinations()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300, "fa");
    const QString b = makeFeatureFile("b.pdf", "B", 3, 400, "fb");
    pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, "all" } }, path("out.pdf"));
    QVERIFY(merged.getCatalog()->getOutlineRootPtr());
    const auto root = merged.getCatalog()->getOutlineRootPtr();
    // Join mode: one part per source (named after the file), source bookmarks below it.
    QCOMPARE(root->getChildCount(), size_t(2));
    QCOMPARE(root->getChild(0)->getTitle(), QString("a.pdf"));
    QCOMPARE(root->getChild(1)->getTitle(), QString("b.pdf"));
    QCOMPARE(root->getChild(0)->getChildCount(), size_t(3));
    QCOMPARE(root->getChild(1)->getChildCount(), size_t(3));
    // Every bookmark goes to the page that carries its source marker.
    for (size_t part = 0; part < 2; ++part)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            const auto* goTo = dynamic_cast<const pdf::PDFActionGoTo*>(root->getChild(part)->getChild(i)->getAction());
            QVERIFY(goTo);
            const pdf::PDFObjectReference target = goTo->getDestination().getPageReference();
            bool found = false;
            for (size_t p = 0; p < merged.getCatalog()->getPageCount(); ++p)
            {
                if (merged.getCatalog()->getPage(p)->getPageReference() == target)
                {
                    found = true;
                    QCOMPARE(pageMarker(merged, p), QString("%1 page %2").arg(part == 0 ? "A" : "B").arg(i + 1));
                }
            }
            QVERIFY(found);
        }
    }

    // Named destinations are NOT carried over (upstream mergeNames writes an invalid name tree), so the
    // user is warned instead. The check is on the sources, which do have named destinations.
    QVERIFY(merged.getCatalog()->getNamedDestinations().empty());
    QString error;
    const auto list = entries({ { a, "all" }, { b, "all" } }, &error);
    QVERIFY(list[0].source.hasNamedDestinations);
    const auto report = PDFDocumentMerger::analyze(list);
    QVERIFY(report.blockers.isEmpty());
    bool namedWarning = false;
    for (const QString& warning : report.warnings)
    {
        namedWarning = namedWarning || warning.contains("a.pdf");
    }
    QVERIFY2(namedWarning, qPrintable(report.warnings.join('\n')));
}

void MergePdfsTest::pageLinksFollowTheirTargetPage()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300, "fa");
    const QString b = makeFeatureFile("b.pdf", "B", 3, 400, "fb");
    // A links 1->2->3->1, B the same. B is listed first and reversed; the links still go to the right pages.
    const pdf::PDFDocument merged = mergeToOutput({ { b, "3,1,2" }, { a, "all" } }, path("out.pdf"));
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(6));
    for (size_t i = 0; i < 6; ++i)
    {
        const pdf::PDFPage* page = merged.getCatalog()->getPage(i);
        pdf::PDFObjectReference link;
        for (const pdf::PDFObjectReference annotation : page->getAnnotations())
        {
            const pdf::PDFDictionary* annotationDictionary = merged.getStorage().getDictionaryFromObject(merged.getStorage().getObjectByReference(annotation));
            if (annotationDictionary && annotationDictionary->get("Subtype").isName() && annotationDictionary->get("Subtype").getString() == "Link")
            {
                link = annotation;
            }
        }
        QVERIFY(link.isValid());
        const pdf::PDFDictionary* dictionary = merged.getStorage().getDictionaryFromObject(merged.getStorage().getObjectByReference(link));
        const pdf::PDFDestination destination = pdf::PDFDestination::parse(&merged.getStorage(), dictionary->get("Dest"));
        const QString marker = pageMarker(merged, i);          // "<prefix> page N"
        const int number = marker.mid(marker.lastIndexOf(' ') + 1).toInt();
        const QString expected = QString("%1 page %2").arg(marker.left(1)).arg(number % 3 + 1);
        QString target;
        for (size_t p = 0; p < 6; ++p)
        {
            if (merged.getCatalog()->getPage(p)->getPageReference() == destination.getPageReference())
            {
                target = pageMarker(merged, p);
            }
        }
        QCOMPARE(target, expected);
    }
}

void MergePdfsTest::optionalContentIsKept()
{
    auto layerPdf = [](const QByteArray& layerName)
    {
        const QByteArray content = "/OC /MC0 BDC 10 10 50 50 re f EMC";
        return rawPdf({
            "<< /Type /Catalog /Pages 2 0 R /OCProperties 5 0 R >>",
            "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R /Resources << /Properties << /MC0 6 0 R >> >> >>",
            "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "\nendstream",
            "<< /OCGs [6 0 R] /D << /Order [6 0 R] /ON [6 0 R] >> >>",
            "<< /Type /OCG /Name (" + layerName + ") >>" });
    };
    QVERIFY(writeFile(path("layer-a.pdf"), layerPdf("Layer A")));
    QVERIFY(writeFile(path("layer-b.pdf"), layerPdf("Layer B")));
    const pdf::PDFDocument merged = mergeToOutput({ { path("layer-a.pdf"), "all" }, { path("layer-b.pdf"), "all" } }, path("out.pdf"));
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(2));

    const pdf::PDFObjectStorage& storage = merged.getStorage();
    const pdf::PDFDictionary* trailer = storage.getDictionaryFromObject(storage.getTrailerDictionary());
    QVERIFY(trailer);
    const pdf::PDFDictionary* catalog = storage.getDictionaryFromObject(storage.getObject(trailer->get("Root")));
    QVERIFY(catalog);
    const pdf::PDFDictionary* properties = storage.getDictionaryFromObject(storage.getObject(catalog->get("OCProperties")));
    QVERIFY(properties);
    const pdf::PDFObject groups = storage.getObject(properties->get("OCGs"));
    QVERIFY(groups.isArray());
    QCOMPARE(groups.getArray()->getCount(), size_t(2));
    // Every page keeps pointing at its own layer, and that layer is listed in the merged OCProperties.
    const QStringList expected = { "Layer A", "Layer B" };
    for (size_t i = 0; i < 2; ++i)
    {
        const pdf::PDFDictionary* resources = storage.getDictionaryFromObject(storage.getObject(merged.getCatalog()->getPage(i)->getResources()));
        const pdf::PDFDictionary* layers = resources ? storage.getDictionaryFromObject(storage.getObject(resources->get("Properties"))) : nullptr;
        QVERIFY(layers);
        const pdf::PDFObject layerReference = layers->get("MC0");
        QVERIFY(layerReference.isReference());
        QCOMPARE(annotationText(merged, layerReference.getReference(), "Name"), expected[int(i)]);
        bool listed = false;
        for (const pdf::PDFObject& item : *groups.getArray())
        {
            listed = listed || (item.isReference() && item.getReference() == layerReference.getReference());
        }
        QVERIFY(listed);
    }
}

void MergePdfsTest::formAndLayersWithIndirectEntries()
{
    // /Fields, /DR, /OCGs and /D arrays may be indirect objects. Every order of direct and indirect sources must
    // keep the fields and layers of all sources. A also needs appearances; B says it does not (that must not win).
    auto sourcePdf = [](const QByteArray& name, bool indirect, const QByteArray& extraFormEntries)
    {
        const QByteArray content = "/OC /MC0 BDC 10 10 50 50 re f EMC";
        return rawPdf({
            "<< /Type /Catalog /Pages 2 0 R /AcroForm 5 0 R /OCProperties 8 0 R >>",
            "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R /Annots [6 0 R] /Resources << /Properties << /MC0 10 0 R >> >> >>",
            "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "\nendstream",
            (indirect ? QByteArray("<< /Fields 7 0 R /DR 11 0 R") : QByteArray("<< /Fields [6 0 R] /DR << /Font << /Helv 12 0 R >> >>"))
                + " /DA (/Helv 0 Tf 0 g) " + extraFormEntries + " >>",
            "<< /Type /Annot /Subtype /Widget /FT /Tx /T (" + name + ") /V (" + name + " value) /Rect [10 10 100 30] /P 3 0 R >>",
            "[6 0 R]",
            indirect ? "<< /OCGs 9 0 R /D << /Order 9 0 R /ON 9 0 R >> >>" : "<< /OCGs [10 0 R] /D << /Order [10 0 R] /ON [10 0 R] >> >>",
            "[10 0 R]",
            "<< /Type /OCG /Name (" + name + " layer) >>",
            "<< /Font << /Helv 12 0 R >> >>",
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
            "<< /Length 7 >>\nstream\n<xdp/>\n\nendstream" });
    };

    for (const bool indirectA : { false, true })
    {
        for (const bool indirectB : { false, true })
        {
            const QString tag = QStringLiteral("A %1, B %2").arg(indirectA ? "indirect" : "direct", indirectB ? "indirect" : "direct");
            QVERIFY(writeFile(path("a.pdf"), sourcePdf("a", indirectA, "/NeedAppearances true /XFA 13 0 R")));
            QVERIFY(writeFile(path("b.pdf"), sourcePdf("b", indirectB, "/NeedAppearances false")));
            const pdf::PDFDocument merged = mergeToOutput({ { path("a.pdf"), "all" }, { path("b.pdf"), "all" } }, path("out.pdf"));
            QVERIFY2(merged.getCatalog(), qPrintable(tag));

            const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
            QStringList names;
            for (const auto& field : form.getFormFields())
            {
                names << field->getName(pdf::PDFFormField::FullyQualified);
            }
            QVERIFY2(names == QStringList({ "a", "b" }), qPrintable(tag + ": " + names.join(',')));

            const pdf::PDFObjectStorage& storage = merged.getStorage();
            const pdf::PDFDictionary* acroForm = storage.getDictionaryFromObject(merged.getCatalog()->getFormObject());
            QVERIFY2(acroForm, qPrintable(tag));
            const pdf::PDFDictionary* resources = storage.getDictionaryFromObject(acroForm->get("DR"));
            const pdf::PDFDictionary* fonts = resources ? storage.getDictionaryFromObject(resources->get("Font")) : nullptr;
            QVERIFY2(fonts && fonts->hasKey("Helv"), qPrintable(tag));
            QVERIFY2(storage.getObject(acroForm->get("NeedAppearances")) == pdf::PDFObject::createBool(true), qPrintable(tag));
            // The XFA form of one source cannot describe the merged document: it is not carried over (see the warning).
            QVERIFY2(!acroForm->hasKey("XFA"), qPrintable(tag));

            const pdf::PDFDictionary* trailer = storage.getDictionaryFromObject(storage.getTrailerDictionary());
            const pdf::PDFDictionary* catalog = storage.getDictionaryFromObject(storage.getObject(trailer->get("Root")));
            const pdf::PDFDictionary* properties = storage.getDictionaryFromObject(storage.getObject(catalog->get("OCProperties")));
            QVERIFY2(properties, qPrintable(tag));
            const pdf::PDFObject groups = storage.getObject(properties->get("OCGs"));
            QVERIFY2(groups.isArray() && groups.getArray()->getCount() == 2, qPrintable(tag));
            const pdf::PDFDictionary* defaults = storage.getDictionaryFromObject(properties->get("D"));
            QVERIFY2(defaults, qPrintable(tag));
            const pdf::PDFObject on = storage.getObject(defaults->get("ON"));
            QVERIFY2(on.isArray() && on.getArray()->getCount() == 2, qPrintable(tag));
        }
    }
}

void MergePdfsTest::jpeg2000ImagesAreNotReencoded()
{
    const QString scan = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    QVERIFY(!scan.isEmpty());
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300);
    bool sourceOk = false;
    const pdf::PDFDocument source = readPdf(scan, &sourceOk);
    QVERIFY(sourceOk);

    auto firstImage = [](const pdf::PDFDocument& document, size_t pageIndex, QByteArray* content, QByteArray* filter)
    {
        const pdf::PDFObjectStorage& storage = document.getStorage();
        const pdf::PDFDictionary* resources = storage.getDictionaryFromObject(storage.getObject(document.getCatalog()->getPage(pageIndex)->getResources()));
        const pdf::PDFDictionary* xobjects = resources ? storage.getDictionaryFromObject(storage.getObject(resources->get("XObject"))) : nullptr;
        if (!xobjects)
        {
            return false;
        }
        for (size_t i = 0; i < xobjects->getCount(); ++i)
        {
            const pdf::PDFObject object = storage.getObject(xobjects->getValue(i));
            if (object.isStream())
            {
                const pdf::PDFStream* stream = object.getStream();
                if (stream->getDictionary()->get("Subtype").isName() && stream->getDictionary()->get("Subtype").getString() == "Image")
                {
                    *content = *stream->getContent();
                    const pdf::PDFObject filterObject = storage.getObject(stream->getDictionary()->get("Filter"));
                    *filter = filterObject.isName() ? filterObject.getString() : QByteArray();
                    return true;
                }
            }
        }
        return false;
    };

    QByteArray sourceContent;
    QByteArray sourceFilter;
    QVERIFY(firstImage(source, 0, &sourceContent, &sourceFilter));
    QCOMPARE(sourceFilter, QByteArray("JPXDecode"));

    const pdf::PDFDocument merged = mergeToOutput({ { a, "1" }, { scan, "1" } }, path("out.pdf"));
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(2));
    QByteArray mergedContent;
    QByteArray mergedFilter;
    QVERIFY(firstImage(merged, 1, &mergedContent, &mergedFilter));
    QCOMPARE(mergedFilter, QByteArray("JPXDecode"));
    QVERIFY(mergedContent == sourceContent);    // byte identical JPEG 2000 data
}

namespace
{

QString writeEncryptedFile(const QString& file, const QString& userPassword, const QString& ownerPassword, uint32_t permissions)
{
    pdf::PDFDocumentBuilder builder;
    for (int i = 1; i <= 2; ++i)
    {
        const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 200 + i, 300));
        pdf::PDFPageContentStreamBuilder content(&builder);
        QPainter* painter = content.begin(page);
        painter->fillRect(QRectF(10, 10, 50, 50), Qt::blue);
        content.end(painter);
    }
    pdf::PDFSecurityHandlerFactory::SecuritySettings settings;
    settings.algorithm = pdf::PDFSecurityHandlerFactory::AES_256;
    settings.encryptContents = pdf::PDFSecurityHandlerFactory::All;
    settings.userPassword = userPassword;
    settings.ownerPassword = ownerPassword;
    settings.permissions = permissions;
    settings.id = QByteArrayLiteral("merge-test-id-0123456789");
    builder.setSecurityHandler(pdf::PDFSecurityHandlerFactory::createSecurityHandler(settings));
    const pdf::PDFDocument document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult result = writer.write(file, &document, true);
    return result ? QString() : result.getErrorMessage();
}

constexpr uint32_t AllPermissions = 0xFFFFFFFFu;
constexpr uint32_t PrintOnly = uint32_t(pdf::PDFSecurityHandler::Permission::PrintLowResolution) | uint32_t(pdf::PDFSecurityHandler::Permission::PrintHighResolution);

} // namespace

void MergePdfsTest::encryptedSourceWithPassword()
{
    const QString encrypted = path("enc.pdf");
    const QString error = writeEncryptedFile(encrypted, "secret", "owner", AllPermissions);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QString plain = makeFeatureFile("a.pdf", "A", 2, 300);

    int asked = 0;
    auto goodPassword = [&asked](bool* ok) { ++asked; *ok = true; return QStringLiteral("secret"); };
    const auto loaded = PDFDocumentMerger::loadSource(encrypted, goodPassword);
    QCOMPARE(int(loaded.status), int(PDFDocumentMerger::LoadStatus::OK));
    QCOMPARE(asked, 1);
    QVERIFY(loaded.source.encrypted);
    QVERIFY(loaded.source.copyAllowed && loaded.source.assembleAllowed);

    PDFDocumentMerger::Entry encryptedEntry;
    encryptedEntry.source = loaded.source;
    encryptedEntry.pages = { 1, 0 };
    PDFDocumentMerger::Entry plainEntry;
    plainEntry.source = load(plain).source;
    plainEntry.pages = { 0 };
    const std::vector<PDFDocumentMerger::Entry> list = { plainEntry, encryptedEntry };

    const auto report = PDFDocumentMerger::analyze(list);
    QVERIFY(report.blockers.isEmpty());
    QStringList encryptionWarnings;
    for (const QString& warning : report.warnings)
    {
        if (warning.contains("not encrypted"))
        {
            encryptionWarnings << warning;
        }
    }
    QCOMPARE(encryptionWarnings.size(), 1);      // the output is not encrypted
    QVERIFY2(encryptionWarnings.front().contains("enc.pdf"), qPrintable(encryptionWarnings.front()));

    bool cancelled = false;
    const auto result = PDFDocumentMerger::mergeToFile(list, path("out.pdf"), nullptr, &cancelled);
    QVERIFY2(bool(result), qPrintable(result.getErrorMessage()));

    // The output opens without any password, is not encrypted and shows the decrypted page sizes in the chosen order.
    bool ok = false;
    const pdf::PDFDocument merged = readPdf(path("out.pdf"), &ok);   // reader without a password callback
    QVERIFY(ok);
    QCOMPARE(int(merged.getStorage().getSecurityHandler()->getMode()), int(pdf::EncryptionMode::None));
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(3));
    QCOMPARE(int(merged.getCatalog()->getPage(0)->getMediaBox().width()), 300);
    QCOMPARE(int(merged.getCatalog()->getPage(1)->getMediaBox().width()), 202);
    QCOMPARE(int(merged.getCatalog()->getPage(2)->getMediaBox().width()), 201);
    QVERIFY(!readFile(path("out.pdf")).contains("/Encrypt"));
}

void MergePdfsTest::invalidPasswordAndCancel()
{
    const QString encrypted = path("enc.pdf");
    QVERIFY(writeEncryptedFile(encrypted, "secret", "owner", AllPermissions).isEmpty());

    int asked = 0;
    auto wrongPassword = [&asked](bool* ok) { ++asked; *ok = true; return QStringLiteral("nope"); };
    auto result = PDFDocumentMerger::loadSource(encrypted, wrongPassword);
    QCOMPARE(int(result.status), int(PDFDocumentMerger::LoadStatus::WrongPassword));
    QVERIFY(!result.errorMessage.isEmpty());
    QCOMPARE(asked, PDFDocumentMerger::MAX_PASSWORD_ATTEMPTS);
    QVERIFY(!result.source.document);

    auto userCancels = [](bool* ok) { *ok = false; return QString(); };
    result = PDFDocumentMerger::loadSource(encrypted, userCancels);
    QCOMPARE(int(result.status), int(PDFDocumentMerger::LoadStatus::Cancelled));

    // No callback at all (no way to ask): the source cannot be merged.
    result = PDFDocumentMerger::loadSource(encrypted, {});
    QVERIFY(result.status != PDFDocumentMerger::LoadStatus::OK);

    // A file that is not a PDF fails with a message.
    QVERIFY(writeFile(path("broken.pdf"), "this is not a pdf"));
    result = PDFDocumentMerger::loadSource(path("broken.pdf"), {});
    QCOMPARE(int(result.status), int(PDFDocumentMerger::LoadStatus::Failed));
    QVERIFY(!result.errorMessage.isEmpty());
}

void MergePdfsTest::permissionDenied()
{
    // Opens with the empty user password but forbids copying and assembling.
    const QString restricted = path("restricted.pdf");
    QString error = writeEncryptedFile(restricted, QString(), "owner", PrintOnly);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const auto loaded = PDFDocumentMerger::loadSource(restricted, {});
    QCOMPARE(int(loaded.status), int(PDFDocumentMerger::LoadStatus::OK));
    QVERIFY(loaded.source.encrypted);
    QVERIFY(!loaded.source.copyAllowed);
    QVERIFY(!loaded.source.assembleAllowed);

    PDFDocumentMerger::Entry entry;
    entry.source = loaded.source;
    entry.pages = { 0 };
    const auto report = PDFDocumentMerger::analyze({ entry });
    QCOMPARE(report.blockers.size(), 1);
    QVERIFY2(report.blockers.front().contains("restricted.pdf"), qPrintable(report.blockers.front()));

    // With the owner password the same file may be merged.
    auto ownerPassword = [](bool* ok) { *ok = true; return QStringLiteral("owner"); };
    // (the empty password already opens it as user, so the callback is not even asked)
    Q_UNUSED(ownerPassword);
    const QString openOwner = path("owner-open.pdf");
    error = writeEncryptedFile(openOwner, "u", "owner", PrintOnly);
    QVERIFY(error.isEmpty());
    const auto ownerLoaded = PDFDocumentMerger::loadSource(openOwner, [](bool* ok) { *ok = true; return QStringLiteral("owner"); });
    QCOMPARE(int(ownerLoaded.status), int(PDFDocumentMerger::LoadStatus::OK));
    QVERIFY(ownerLoaded.source.copyAllowed);
    const auto userLoaded = PDFDocumentMerger::loadSource(openOwner, [](bool* ok) { *ok = true; return QStringLiteral("u"); });
    QCOMPARE(int(userLoaded.status), int(PDFDocumentMerger::LoadStatus::OK));
    QVERIFY(!userLoaded.source.copyAllowed);

    // Nothing is written for a blocked list by the caller; the engine itself still refuses nothing,
    // so the workflow must stop on blockers (see the dialog test).
}

void MergePdfsTest::signedSourceWarningAndInvalidSignature()
{
    const QString signedFile = QFINDTESTDATA("fixtures/pyhanko-signed.pdf");
    QVERIFY(!signedFile.isEmpty());
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300);

    QString error;
    const auto list = entries({ { a, "1" }, { signedFile, "all" } }, &error);
    QVERIFY(!list.empty());
    QVERIFY(list[1].source.hasSignature);
    QVERIFY(!list[0].source.hasSignature);
    const auto report = PDFDocumentMerger::analyze(list);
    QVERIFY(report.blockers.isEmpty());
    bool signatureWarning = false;
    for (const QString& warning : report.warnings)
    {
        signatureWarning = signatureWarning || warning.contains("pyhanko-signed.pdf");
    }
    QVERIFY2(signatureWarning, qPrintable(report.warnings.join('\n')));

    // What the warning promises: a merged copy never reports a valid signature.
    const pdf::PDFDocument merged = mergeToOutput({ { a, "1" }, { signedFile, "all" } }, path("out.pdf"));
    QVERIFY(merged.getCatalog());
    const QByteArray mergedBytes = readFile(path("out.pdf"));
    pdf::PDFCertificateStore store;
    pdf::PDFSignatureHandler::Parameters parameters;
    parameters.store = &store;
    parameters.useSystemCertificateStore = false;
    parameters.ignoreExpirationDate = true;
    const pdf::PDFForm form = pdf::PDFForm::parse(&merged, merged.getCatalog()->getFormObject());
    const auto results = pdf::PDFSignatureHandler::verifySignatures(form, mergedBytes, parameters);
    for (const auto& result : results)
    {
        QVERIFY(!result.isSignatureValid());
    }
    qInfo() << "signature entries in merged output:" << results.size();
}

void MergePdfsTest::outputOverwriteAndSourceProtection()
{
    const QString a = makeFeatureFile("a.pdf", "A", 2, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 2, 400, "other");
    const QByteArray aBefore = readFile(a);
    const QByteArray bBefore = readFile(b);

    // Overwriting an existing, different file replaces it with the complete new PDF (and leaves no temp files).
    const QString out = path("out.pdf");
    QVERIFY(writeFile(out, "OLD CONTENT"));
    pdf::PDFDocument merged = mergeToOutput({ { a, "all" }, { b, "all" } }, out);
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(4));
    QStringList files = QDir(m_directory->path()).entryList(QDir::Files);
    files.sort();
    QCOMPARE(files, QStringList({ "a.pdf", "b.pdf", "out.pdf" }));

    // Output == one of the sources is refused, the source is not touched.
    QString error;
    const auto list = entries({ { a, "all" }, { b, "all" } }, &error);
    for (const QString& destination : { a, b, QDir::cleanPath(a), QDir(m_directory->path()).filePath("./a.pdf") })
    {
        bool cancelled = false;
        const auto result = PDFDocumentMerger::mergeToFile(list, destination, nullptr, &cancelled);
        QVERIFY(!result);
        QVERIFY(!result.getErrorMessage().isEmpty());
        QVERIFY(!cancelled);
    }
    QCOMPARE(readFile(a), aBefore);
    QCOMPARE(readFile(b), bBefore);

    // Destination in a folder that does not exist: error, nothing created.
    bool cancelled = false;
    const auto result = PDFDocumentMerger::mergeToFile(list, path("missing-folder/out.pdf"), nullptr, &cancelled);
    QVERIFY(!result);
    QVERIFY(!QDir(path("missing-folder")).exists());
}

void MergePdfsTest::cancelAndFailureLeaveDestinationAlone()
{
    const QString a = makeFeatureFile("a.pdf", "A", 3, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 3, 400, "other");
    const QString out = path("out.pdf");
    const QByteArray existing = "EXISTING DESTINATION";
    QVERIFY(writeFile(out, existing));
    QString error;
    auto list = entries({ { a, "all" }, { b, "all" } }, &error);

    // Cancel before the work starts.
    std::atomic_bool cancel { true };
    bool cancelled = false;
    auto result = PDFDocumentMerger::mergeToFile(list, out, &cancel, &cancelled);
    QVERIFY(!result);
    QVERIFY(cancelled);
    QCOMPARE(readFile(out), existing);

    // Cancel is also seen after the (long) assemble phase: simulate by cancelling from the outside while merging.
    // The engine checks after assemble and after the write, before it commits.
    cancel = false;
    QThread* thread = QThread::create([&cancel]()
    {
        QThread::usleep(1);
        cancel = true;
    });
    thread->start();
    result = PDFDocumentMerger::mergeToFile(list, out, &cancel, &cancelled);
    thread->wait();
    delete thread;
    if (!result)
    {
        QVERIFY(cancelled);
        QCOMPARE(readFile(out), existing);      // never a partial file
    }
    else
    {
        bool ok = false;
        const pdf::PDFDocument merged = readPdf(out, &ok);      // finished before the cancel: a complete PDF
        QVERIFY(ok);
        QCOMPARE(merged.getCatalog()->getPageCount(), size_t(6));
        QVERIFY(writeFile(out, existing));
    }

    // Failure: a page that does not exist in the source.
    list[0].pages.push_back(99);
    cancelled = false;
    result = PDFDocumentMerger::mergeToFile(list, out, nullptr, &cancelled);
    QVERIFY(!result);
    QVERIFY(!cancelled);
    QCOMPARE(readFile(out), existing);

    // Failure: a document that is gone.
    list[0].pages.pop_back();
    list[1].source.document.reset();
    result = PDFDocumentMerger::mergeToFile(list, out, nullptr, &cancelled);
    QVERIFY(!result);
    QCOMPARE(readFile(out), existing);

    // Failure: empty page list for the only entry.
    result = PDFDocumentMerger::mergeToFile({ }, out, nullptr, &cancelled);
    QVERIFY(!result);
    QCOMPARE(readFile(out), existing);

    QStringList files = QDir(m_directory->path()).entryList(QDir::Files);
    files.sort();
    QCOMPARE(files, QStringList({ "a.pdf", "b.pdf", "out.pdf" }));    // no stray temp files
}

void MergePdfsTest::sourcesAreUnchanged()
{
    const QString a = makeFeatureFile("a.pdf", "A", 4, 300);
    const QString b = makeFeatureFile("b.pdf", "B", 4, 400, "other");
    const QString scan = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    const QByteArray aBefore = readFile(a);
    const QByteArray bBefore = readFile(b);
    const QByteArray scanBefore = readFile(scan);

    QString error;
    const auto list = entries({ { a, "4,1" }, { b, "2-3" }, { scan, "all" } }, &error);
    // The in-memory documents are not modified either: same object count and same page markers afterwards.
    std::vector<size_t> objectCounts;
    for (const auto& entry : list)
    {
        objectCounts.push_back(entry.source.document->getStorage().getObjects().size());
    }
    bool cancelled = false;
    QVERIFY(bool(PDFDocumentMerger::mergeToFile(list, path("out.pdf"), nullptr, &cancelled)));
    for (size_t i = 0; i < list.size(); ++i)
    {
        QCOMPARE(list[i].source.document->getStorage().getObjects().size(), objectCounts[i]);
    }
    QCOMPARE(pageMarker(*list[0].source.document, 0), QString("A page 1"));
    QCOMPARE(readFile(a), aBefore);
    QCOMPARE(readFile(b), bBefore);
    QCOMPARE(readFile(scan), scanBefore);
}

void MergePdfsTest::measureMerges()
{
    // Numbers only (printed with qInfo). Pages carry text, a Square annotation and a bookmark.
    auto timed = [this](const QString& label, const QList<RangeSpec>& specs, int expectedPages)
    {
        QElapsedTimer total;
        total.start();
        QString error;
        const auto list = entries(specs, &error);
        const qint64 loadMs = total.elapsed();
        QElapsedTimer mergeTimer;
        mergeTimer.start();
        bool cancelled = false;
        const QString out = path("perf-out.pdf");
        const auto result = PDFDocumentMerger::mergeToFile(list, out, nullptr, &cancelled);
        const qint64 mergeMs = mergeTimer.elapsed();
        QVERIFY2(bool(result), qPrintable(result.getErrorMessage()));
        bool ok = false;
        const pdf::PDFDocument merged = readPdf(out, &ok);
        QVERIFY(ok);
        QCOMPARE(int(merged.getCatalog()->getPageCount()), expectedPages);
        qInfo().noquote() << QString("PERF %1: %2 pages, load %3 ms, merge+write %4 ms, output %5 KB")
                                 .arg(label).arg(expectedPages).arg(loadMs).arg(mergeMs).arg(QFileInfo(out).size() / 1024);
    };

    QList<RangeSpec> two;
    two << RangeSpec{ makeFeatureFile("p1.pdf", "P1", 3, 300), "all" } << RangeSpec{ makeFeatureFile("p2.pdf", "P2", 3, 300), "all" };
    timed("2 small documents", two, 6);

    QList<RangeSpec> ten;
    for (int i = 0; i < 10; ++i)
    {
        ten << RangeSpec{ makeFeatureFile(QString("t%1.pdf").arg(i), QString("T%1").arg(i), 20, 300), "all" };
    }
    timed("10 documents x 20 pages", ten, 200);

    QList<RangeSpec> big;
    for (int i = 0; i < 5; ++i)
    {
        big << RangeSpec{ makeFeatureFile(QString("big%1.pdf").arg(i), QString("G%1").arg(i), 200, 300), "all" };
    }
    timed("5 documents x 200 pages", big, 1000);

    QList<RangeSpec> bigOne;
    bigOne << RangeSpec{ big[0].file, "all" } << RangeSpec{ big[0].file, "all" } << RangeSpec{ big[0].file, "all" } << RangeSpec{ big[0].file, "all" } << RangeSpec{ big[0].file, "all" };
    timed("same 200 page document 5 times", bigOne, 1000);

    // Scanned (JPEG 2000) pages: the data is copied, never decoded or encoded again.
    const QString scan = QFINDTESTDATA("fixtures/print-export-jpx.pdf");
    const int scanPages = int(readPdf(scan).getCatalog()->getPageCount());
    QList<RangeSpec> scans;
    for (int i = 0; i < 10; ++i)
    {
        scans << RangeSpec{ scan, "all" };
    }
    timed("JPEG 2000 scan x 10", scans, scanPages * 10);
    QList<RangeSpec> manyScans;
    for (int i = 0; i < 500; ++i)
    {
        manyScans << RangeSpec{ scan, "all" };
    }
    timed("JPEG 2000 scan x 500", manyScans, scanPages * 500);
    QList<RangeSpec> scansAndText = scans;
    scansAndText << RangeSpec{ big[0].file, "all" } << RangeSpec{ big[1].file, "all" };
    timed("JPEG 2000 scan x 10 + 2 text documents", scansAndText, scanPages * 10 + 400);
}

namespace
{

/// Page with a heading drawn by QPainter (real embedded font, real text layer).
pdf::PDFObjectReference smokePage(pdf::PDFDocumentBuilder* builder, QRectF mediaBox, const QString& heading)
{
    const pdf::PDFObjectReference page = builder->appendPage(mediaBox);
    pdf::PDFPageContentStreamBuilder content(builder, pdf::PDFContentStreamBuilder::CoordinateSystem::PDF, pdf::PDFPageContentStreamBuilder::Mode::Replace);
    QPainter* painter = content.begin(page);
    painter->setPen(QPen(QColor(40, 80, 160), 3.0));
    painter->drawRect(mediaBox.adjusted(15, 15, -15, -15));
    painter->save();
    painter->translate(0.0, mediaBox.height());
    painter->scale(1.0, -1.0);
    QFont font(QStringLiteral("Arial"));
    font.setPointSizeF(20.0);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::black);
    painter->drawText(QPointF(40.0, 60.0), heading);
    painter->restore();
    content.end(painter);
    return page;
}

void addTextField(pdf::PDFDocumentBuilder* builder, pdf::PDFObjectReference page, const QString& name, const QString& value, QRectF rect)
{
    const pdf::PDFObjectReference field = builder->createFormFieldText(name, value, pdf::PDFFormField::FieldFlags(), 0);
    builder->createFormFieldWidget(field, page, rect, QByteArrayLiteral("/Helv 12 Tf 0 g"));
    builder->appendAcroFormField(field);
}

} // namespace

void MergePdfsTest::writeSmokeArtifacts()
{
    // Real PDFs (text, form fields, check box, annotations with appearances, rotation) for a look in other viewers.
    // Only runs when FAMILYPDF_MERGE_ARTIFACT_DIR is set.
    const QString directory = qEnvironmentVariable("FAMILYPDF_MERGE_ARTIFACT_DIR");
    if (directory.isEmpty())
    {
        QSKIP("FAMILYPDF_MERGE_ARTIFACT_DIR is not set");
    }
    QVERIFY(QDir().mkpath(directory));
    auto save = [&](const QString& name, pdf::PDFDocumentBuilder& builder)
    {
        const pdf::PDFDocument document = builder.build();
        pdf::PDFDocumentWriter writer(nullptr);
        const pdf::PDFOperationResult result = writer.write(QDir(directory).filePath(name), &document, true);
        QVERIFY2(bool(result), qPrintable(result.getErrorMessage()));
    };

    {
        pdf::PDFDocumentBuilder builder;
        const pdf::PDFObjectReference page1 = smokePage(&builder, QRectF(0, 0, 595, 842), "ALPHA page 1");
        addTextField(&builder, page1, "nameA", "Alice", QRectF(50, 700, 240, 30));
        builder.createAnnotationSquare(page1, QRectF(50, 500, 120, 80), 2.0, QColor(255, 230, 120), QColor(200, 60, 60), "Alpha", "note", "ALPHA note");
        const pdf::PDFObjectReference page2 = smokePage(&builder, QRectF(0, 0, 595, 842), "ALPHA page 2 (rotated)");
        builder.setPageRotation(page2, pdf::PageRotation::Rotate90);
        smokePage(&builder, QRectF(0, 0, 595, 842), "ALPHA page 3");
        save("smoke-a.pdf", builder);
    }
    {
        pdf::PDFDocumentBuilder builder;
        const pdf::PDFObjectReference page1 = smokePage(&builder, QRectF(0, 0, 420, 595), "BETA page 1 (A5)");
        addTextField(&builder, page1, "nameB", "Bob", QRectF(40, 450, 200, 30));
        const pdf::PDFObjectReference check = builder.createFormFieldCheckBox("agree", true, pdf::PDFFormField::FieldFlags());
        builder.createFormFieldWidget(check, page1, QRectF(40, 400, 24, 24), QByteArray());
        builder.appendAcroFormField(check);
        builder.createAnnotationSquare(page1, QRectF(40, 250, 100, 60), 2.0, QColor(150, 220, 150), QColor(30, 120, 30), "Beta", "note", "BETA note");
        smokePage(&builder, QRectF(0, 0, 420, 595), "BETA page 2");
        save("smoke-b.pdf", builder);
    }
    {
        pdf::PDFDocumentBuilder builder;
        const pdf::PDFObjectReference page1 = smokePage(&builder, QRectF(0, 0, 595, 842), "DUP page 1");
        addTextField(&builder, page1, "nameA", "Zed", QRectF(50, 700, 240, 30));
        save("smoke-dup.pdf", builder);
    }

    auto mergeSpecs = [&](const QList<RangeSpec>& specs, const QString& name)
    {
        const auto list = entries(specs);
        bool cancelled = false;
        const auto result = PDFDocumentMerger::mergeToFile(list, QDir(directory).filePath(name), nullptr, &cancelled);
        QVERIFY2(bool(result), qPrintable(result.getErrorMessage()));
    };
    const QString a = QDir(directory).filePath("smoke-a.pdf");
    const QString b = QDir(directory).filePath("smoke-b.pdf");
    const QString dup = QDir(directory).filePath("smoke-dup.pdf");
    mergeSpecs({ { a, "all" }, { b, "2,1" } }, "smoke-merged.pdf");            // different field names, A1 A2 A3 B2 B1
    mergeSpecs({ { a, "1" }, { dup, "all" } }, "smoke-merged-duplicate-names.pdf");
    mergeSpecs({ { b, "1" }, { a, "3,1" } }, "smoke-merged-order.pdf");        // B1 A3 A1
}

QTEST_MAIN(MergePdfsTest)

#include "tst_mergepdfstest.moc"
