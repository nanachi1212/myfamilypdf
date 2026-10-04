// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfviewermainwindow.h"
#include "pdfeditormainwindow.h"
#include "pdfdrawwidget.h"
#include "pdfdrawspacecontroller.h"
#include "pdftextlayout.h"
#include "pdfcompiler.h"
#include "pdfsettings.h"
#include "pdfapplicationtranslator.h"
#include "pdfwidgetutils.h"

#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDockWidget>
#include <QFile>
#include <QItemSelectionModel>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPluginLoader>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QScrollBar>
#include <QSettings>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QElapsedTimer>
#include <QPointer>
#include <QTabBar>
#include "pdfwidgettool.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfwidgetformmanager.h"
#include "pdfsidebarwidget.h"
#include <QTreeWidget>
#include "pdfdocumentbuilder.h"
#include "pdfannotation.h"
#include "pdfwidgetannotation.h"
#include <QToolButton>
#include <QTreeView>
#include <QTextBrowser>
#include <QMimeData>
#include <QtConcurrent/QtConcurrentRun>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <psapi.h>
#endif
#include <memory>

namespace
{

bool writePdfFixture(const QString& path, int pageCount, int lines = 1, bool withText = true, bool unicode = false, bool malformed = false)
{
    const int fontObject = 3 + pageCount * 2;
    QByteArray kids;
    for (int page = 0; page < pageCount; ++page)
    {
        kids += QByteArray::number(3 + page * 2) + " 0 R ";
    }

    QList<QByteArray> objects;
    objects << "<< /Type /Catalog /Pages 2 0 R >>"
            << QByteArray("<< /Type /Pages /Kids [") + kids + "] /Count " + QByteArray::number(pageCount) + " >>";
    for (int page = 0; page < pageCount; ++page)
    {
        const int contentObject = 4 + page * 2;
        objects << QByteArray("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 420 595] ")
                       + "/Resources << /Font << /F1 " + QByteArray::number(fontObject) + " 0 R >> >> /Contents "
                       + QByteArray::number(contentObject) + " 0 R >>";
        QByteArray stream;
        if (withText)
            for (int line = 0; line < lines; ++line)
                stream += "BT /F1 16 Tf 30 " + QByteArray::number(535-line*20) + " Td ("
                    + (unicode ? QByteArray("ABCD 2026 ABCD") : QByteArray("FamilyPDF smoke page ") + QByteArray::number(page+1)) + ") Tj ET\n";
        else
            stream = "q 0.7 g 20 20 300 500 re f Q\n";
        if (malformed && page == 0) stream += "Q\n"; // Unbalanced restore after valid text.
        objects << QByteArray("<< /Length ") + QByteArray::number(stream.size())
                       + " >>\nstream\n" + stream + "endstream";
    }
    if (unicode)
    {
        // A self-contained Type 3 font keeps Unicode extraction independent of
        // installed fonts. Glyph outlines are deliberately simple test shapes.
        QByteArray widths;
        for (int code = 32; code <= 68; ++code)
            widths += "600 ";
        const QByteArray glyphRef = QByteArray::number(fontObject + 2) + " 0 R ";
        objects << QByteArray("<< /Type /Font /Subtype /Type3 /FontBBox [0 0 600 800] ")
            + "/FontMatrix [0.001 0 0 0.001 0 0] /FirstChar 32 /LastChar 68 /Widths [" + widths
            + "] /Encoding << /Type /Encoding /Differences [32 /space 48 /zero 50 /two 54 /six 65 /A /B /C /D] >> "
            + "/CharProcs << /space " + glyphRef + "/zero " + glyphRef + "/two " + glyphRef
            + "/six " + glyphRef + "/A " + glyphRef + "/B " + glyphRef + "/C " + glyphRef + "/D " + glyphRef
            + ">> /Resources << >> /ToUnicode " + QByteArray::number(fontObject + 1) + " 0 R >>";
        const QByteArray cmap = "1 begincodespacerange <00> <FF> endcodespacerange 8 beginbfchar <41> <4E2D> <42> <6587> <43> <641C> <44> <5C0B> <20> <0020> <32> <0032> <30> <0030> <36> <0036> endbfchar";
        objects << QByteArray("<< /Length ") + QByteArray::number(cmap.size()) + " >>\nstream\n" + cmap + "\nendstream";
        const QByteArray glyph = "600 0 0 0 600 800 d1 50 50 500 700 re f";
        objects << QByteArray("<< /Length ") + QByteArray::number(glyph.size()) + " >>\nstream\n" + glyph + "\nendstream";
    }
    else
        objects << QByteArray("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

    QByteArray pdfData = "%PDF-1.4\n";
    QList<qsizetype> offsets;
    offsets << 0;
    for (qsizetype index = 0; index < objects.size(); ++index)
    {
        offsets << pdfData.size();
        pdfData += QByteArray::number(index + 1) + " 0 obj\n" + objects[index] + "\nendobj\n";
    }
    const qsizetype xrefOffset = pdfData.size();
    pdfData += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n";
    pdfData += "0000000000 65535 f \n";
    for (qsizetype index = 1; index < offsets.size(); ++index)
    {
        pdfData += QByteArray::number(offsets[index]).rightJustified(10, '0') + " 00000 n \n";
    }
    pdfData += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1)
             + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    QFile fixture(path);
    return fixture.open(QIODevice::WriteOnly | QIODevice::Truncate) && fixture.write(pdfData) == pdfData.size();
}

}

class ViewerContextMenuTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void menuActionsOperateOnTheDocument();
    void bookmarkUsesClickedPage_data();
    void bookmarkUsesClickedPage();
    void emptyDocumentCannotBeBookmarked();
    void traditionalChineseMenuAndSvgResources();
    void largePdfReadingBenchmark();
    void searchExperience_data();
    void searchExperience();
    void searchCancellationAndDocumentLifecycle_data() { searchExperience_data(); }
    void searchCancellationAndDocumentLifecycle();
    void searchTextCacheOwnership();
    void searchCacheReuse_data() { searchExperience_data(); }
    void searchCacheReuse();
    void searchEditorContentInvalidation();
    void searchRejectsPartialLegacyLayout();
    void searchPerformanceBenchmark();
    void searchWarmCacheBenchmark();
    void extractionRejectsInvalidInputAndCancellation();
    void readingPositionRestoresZoomAndClamps();
    void thumbnailSelectionAndPageManagement();
    void annotationMarkupWorkflow_data();
    void annotationMarkupWorkflow();
    void annotationNoteWorkflow();
    void formWorkflow();
    void formNavigation();
    void signaturePresentation();
    void signatureVerificationWorkflow();
    void formValidationAndMalformed();
    void formAppearanceFallback();
    void annotationListLargeDocument();

private:
    QAction* action(const char* name) const { return m_window->findChild<QAction*>(QLatin1String(name)); }
    QWidget* drawWidget() const { return m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getWidget(); }
    pdf::PDFDrawWidgetProxy* proxy() const { return m_window->getProgramController()->getPdfWidget()->getDrawWidgetProxy(); }
    QPoint pagePoint(int page) const;
    void withMenu(const QPoint& point, const std::function<void(QMenu*)>& inspect);
    void clickAction(const char* name);
    QAction* bookmarkAction(QMenu* menu) const;
    void saveImage(const QPixmap& pixmap, const QString& name);

    QTemporaryDir m_temp;
    QString m_pdfPath;
    std::unique_ptr<pdfviewer::PDFViewerMainWindow> m_window;
};

void ViewerContextMenuTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("FamilyPDFTests");
    QCoreApplication::setApplicationName("ViewerContextMenu");
    pdf::PDFSettings::setSettingsPath(m_temp.filePath("settings"));
    pdf::PDFWidgetUtils::setDarkTheme(true, false);
    m_pdfPath = m_temp.filePath("three-pages.pdf");

    // Build the fixture with a PDF base font. QPdfWriter depends on fonts from
    // the platform plugin, while the offscreen CI plugin intentionally has no
    // system font directory and would otherwise produce pages without text.
    QVERIFY(writePdfFixture(m_pdfPath, 3));
    const QString artifactDirectory = qEnvironmentVariable("FAMILYPDF_TEST_ARTIFACT_DIR");
    if (!artifactDirectory.isEmpty())
    {
        QVERIFY(QDir().mkpath(artifactDirectory));
        QVERIFY(QFile::copy(m_pdfPath, QDir(artifactDirectory).filePath("viewer-smoke.pdf")));
    }
}

void ViewerContextMenuTest::init()
{
    const QByteArray testFunction = QTest::currentTestFunction();
#ifdef Q_OS_LINUX
    if (testFunction == "readingPositionRestoresZoomAndClamps")
    {
        return;
    }
#endif
    if (testFunction == "thumbnailSelectionAndPageManagement" || testFunction.startsWith("annotation"))
    {
        return;
    }

    qInfo() << "ViewerContextMenuTest: construct window";
    m_window = std::make_unique<pdfviewer::PDFViewerMainWindow>();
    qInfo() << "ViewerContextMenuTest: show window";
    m_window->resize(1100, 900);
    m_window->show();
    if (testFunction == "readingPositionRestoresZoomAndClamps")
    {
        return;
    }
    qInfo() << "ViewerContextMenuTest: open fixture";
    m_window->getProgramController()->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    QTRY_VERIFY(!m_window->getProgramController()->getIsBusy());
    qInfo() << "ViewerContextMenuTest: fixture opened";
    QCOMPARE(m_window->getProgramController()->getDocument()->getCatalog()->getPageCount(), size_t(3));
    auto* bookmarks = m_window->getProgramController()->getBookmarkManager();
    bookmarks->setGenerateBookmarksAutomatically(false);
    while (!bookmarks->isEmpty()) bookmarks->toggleBookmark(bookmarks->getBookmark(0).pageIndex);
    proxy()->setPageLayout(pdf::PageLayout::OneColumn);
    proxy()->zoom(0.5);
    QTRY_VERIFY(pagePoint(0).x() >= 0);
    qInfo() << "ViewerContextMenuTest: initialization complete";
}

void ViewerContextMenuTest::cleanup()
{
    m_window.reset();
}

void ViewerContextMenuTest::searchExperience_data()
{
    QTest::addColumn<bool>("editor");
    QTest::newRow("viewer") << false;
    QTest::newRow("editor") << true;
}

void ViewerContextMenuTest::searchExperience()
{
    QFETCH(bool, editor);
    std::unique_ptr<pdfviewer::PDFEditorMainWindow> editorWindow;
    QMainWindow* window = m_window.get();
    auto* controller = m_window->getProgramController();
    if (editor)
    {
        editorWindow = std::make_unique<pdfviewer::PDFEditorMainWindow>();
        window = editorWindow.get();
        controller = editorWindow->getProgramController();
        window->resize(1100, 900);
        window->show();
        controller->openDocument(m_pdfPath);
        QTRY_VERIFY(controller->getDocument());
        QTRY_VERIFY(!controller->getIsBusy());
    }
    auto* drawProxy = controller->getPdfWidget()->getDrawWidgetProxy();
    drawProxy->setPageLayout(pdf::PageLayout::SinglePage);
    drawProxy->zoom(1.2);
    window->activateWindow();
    QTest::qWait(30);
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(controller->getToolManager()->getFindTextTool()->isActive());
    auto* dialog = window->findChild<QDialog*>("findDialog");
    QVERIFY(dialog);
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    QVERIFY(query && status);
    QTRY_VERIFY(query->hasFocus());
    query->setText("FamilyPDF");
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QString("1 / 3"), 10000);
    QCOMPARE(drawProxy->getZoom(), 1.2);
    QCOMPARE(drawProxy->getPageLayout(), pdf::PageLayout::SinglePage);
    QTest::keyClick(query, Qt::Key_Return);
    QCOMPARE(status->text(), QString("2 / 3"));
    QCOMPARE(controller->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(1));
    QTest::keyClick(query, Qt::Key_Return, Qt::ShiftModifier);
    QCOMPARE(status->text(), QString("1 / 3"));
    dialog->findChild<QPushButton*>("findPrevious")->click();
    QCOMPARE(status->text(), QString("3 / 3"));
    dialog->findChild<QPushButton*>("findNext")->click();
    QCOMPARE(status->text(), QString("1 / 3"));

    query->setText("e"); // smoke + page: two hits on each page
    QTRY_COMPARE(status->text(), QString("1 / 6"));
    QTest::keyClick(query, Qt::Key_Return);
    QCOMPARE(status->text(), QString("2 / 6"));
    QCOMPARE(controller->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(0));
    QTest::keyClick(query, Qt::Key_Return);
    QCOMPARE(controller->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(1));
    query->setText("familypdf");
    QTRY_COMPARE(status->text(), QString("1 / 3"));
    auto* caseSensitive = dialog->findChildren<QCheckBox*>().front();
    caseSensitive->click();
    QTRY_COMPARE(status->text(), QString("No results."));
    QVERIFY(dialog->isVisible());
    QTest::keyClick(query, Qt::Key_Return); // no match must not accept/hide the dialog
    QVERIFY(dialog->isVisible());
    caseSensitive->click();
    query->setText("zzzz_no_results");
    QTRY_COMPARE(status->text(), QString("No results."));
    query->clear();
    QCOMPARE(status->text(), QString("Enter text to search."));
    QVERIFY(!dialog->findChild<QPushButton*>("findNext")->isEnabled());
    query->setText("  \t ");
    QCOMPARE(status->text(), QString("Enter text to search."));
    QTest::keyClick(query, Qt::Key_Escape);
    QVERIFY(!controller->getToolManager()->getFindTextTool()->isActive());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const QString unicodePath = m_temp.filePath(editor ? "unicode-editor.pdf" : "unicode-viewer.pdf");
    QVERIFY(writePdfFixture(unicodePath, 2, 1, true, true));
    controller->closeDocument();
    controller->openDocument(unicodePath);
    QTRY_VERIFY(controller->getDocument());
    QTRY_COMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(2));
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    query = dialog->findChild<QLineEdit*>("findQuery");
    status = dialog->findChild<QLabel*>("findStatus");
    query->setText(QStringLiteral("中文搜尋"));
    QTRY_COMPARE(status->text(), QString("1 / 4"));
    query->setText(QStringLiteral("中文搜尋 2026"));
    QTRY_COMPARE(status->text(), QString("1 / 2"));
    dialog->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const QString emptyPath = m_temp.filePath(editor ? "empty-editor.pdf" : "empty-viewer.pdf");
    QVERIFY(writePdfFixture(emptyPath, 3, 1, false));
    controller->closeDocument();
    controller->openDocument(emptyPath);
    QTRY_VERIFY(controller->getDocument());
    QTRY_COMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(3));
    drawProxy->getTextLayoutCompiler()->makeTextLayout();
    QTRY_VERIFY(drawProxy->getTextLayoutCompiler()->isTextLayoutReady());
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    query = dialog->findChild<QLineEdit*>("findQuery");
    status = dialog->findChild<QLabel*>("findStatus");
    query->setText("alpha");
    QTRY_VERIFY(status->text().contains("Use OCR"));
    dialog->reject();
    controller->closeDocument();
}

void ViewerContextMenuTest::searchCancellationAndDocumentLifecycle()
{
    QFETCH(bool, editor);
    std::unique_ptr<pdfviewer::PDFEditorMainWindow> editorWindow;
    QMainWindow* window = m_window.get();
    auto* controller = m_window->getProgramController();
    if (editor)
    {
        editorWindow = std::make_unique<pdfviewer::PDFEditorMainWindow>();
        window = editorWindow.get();
        controller = editorWindow->getProgramController();
        window->show();
    }
    auto* drawProxy = controller->getPdfWidget()->getDrawWidgetProxy();
    const QString path = m_temp.filePath("search-large.pdf");
    QVERIFY(writePdfFixture(path, 1200, 20));
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY(controller->getDocument());
    QTRY_COMPARE_WITH_TIMEOUT(controller->getDocument()->getCatalog()->getPageCount(), size_t(1200), 15000);
    window->findChild<QAction*>("actionFind")->trigger();
    auto* dialog = window->findChild<QDialog*>("findDialog");
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    QElapsedTimer timer; timer.start();
    query->setText("FamilyPDF");
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(" / ") && !status->text().contains("0 / 0"), 10000);
    QVERIFY2(status->text().startsWith("Searching"), "First result must arrive before full scan completes.");
    qInfo() << "SEARCH_PROGRESS first_ms=" << timer.elapsed() << status->text();
    drawProxy->goToPage(600);
    query->setText("a");
    query->setText("ab");
    query->setText("abc_missing");
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QString("No results."), 30000);
    QTest::qWait(100);
    QCOMPARE(status->text(), QString("No results."));
    query->setText("FamilyPDF");
    QTest::qWait(180);
    query->clear();
    QTest::qWait(100);
    QCOMPARE(status->text(), QString("Enter text to search."));
    // Viewer tab activation; both applications also switch documents below.
    if (!editor)
    {
        auto other = std::make_unique<pdfviewer::PDFViewerMainWindow>();
        other->show();
        query->setText("FamilyPDF");
        QTest::qWait(180);
        auto* tabs = window->findChild<QTabBar*>();
        QVERIFY(tabs && tabs->count() == 2);
        tabs->setCurrentIndex(1);
        QVERIFY(!controller->getToolManager()->getFindTextTool()->isActive());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    query = dialog->findChild<QLineEdit*>("findQuery");
    query->setText("FamilyPDF");
    QTest::qWait(180);
    timer.restart();
    controller->closeDocument();
    QVERIFY(timer.elapsed() < 2000);
    QVERIFY(!controller->getToolManager()->getFindTextTool()->isActive());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY(controller->getDocument());
    QTRY_COMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(3));
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    query = dialog->findChild<QLineEdit*>("findQuery");
    status = dialog->findChild<QLabel*>("findStatus");
    query->setText("FamilyPDF");
    QTRY_COMPARE(status->text(), QString("1 / 3"));
    controller->closeDocument();
    controller->openDocument(path); // switching documents invalidates all old results
    QTRY_VERIFY(controller->getDocument());
    QTRY_COMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(1200));
    QVERIFY(!controller->getToolManager()->getFindTextTool()->isActive());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    dialog->findChild<QLineEdit*>("findQuery")->setText("FamilyPDF");
    QTest::qWait(180);
    timer.restart();
    if (editor) editorWindow.reset();
    else m_window.reset();
    QVERIFY(timer.elapsed() < 2000);
    QTest::qWait(100); // queued result delivery after destruction must be harmless
}

void ViewerContextMenuTest::searchTextCacheOwnership()
{
    auto storage = std::make_shared<pdf::PDFSearchTextCache>(100);
    auto writer = QtConcurrent::run([storage]() {
        for (int page=0; page<100; ++page) storage->setPage(page, pdf::PDFTextFlows());
    });
    QTRY_VERIFY(writer.isFinished());
    QCOMPARE(storage->getCompletedPageCount(), size_t(100));
    // Empty successfully extracted pages must also be cached.
    QVERIFY(storage->getPage(0));
    QVERIFY(storage->getPage(0)->empty());
    const auto* first = storage->getPage(0);
    storage->setPage(0, pdf::PDFTextFlows());
    QCOMPARE(storage->getPage(0), first);
    QCOMPARE(storage->getCompletedPageCount(), size_t(100));
    std::weak_ptr<pdf::PDFSearchTextCache> lifetime = storage;
    storage.reset();
    QVERIFY(lifetime.expired());
}

void ViewerContextMenuTest::searchCacheReuse()
{
    QFETCH(bool, editor);
    std::unique_ptr<pdfviewer::PDFEditorMainWindow> editorWindow;
    QMainWindow* window = m_window.get();
    auto* controller = m_window->getProgramController();
    if (editor)
    {
        editorWindow = std::make_unique<pdfviewer::PDFEditorMainWindow>();
        window = editorWindow.get();
        controller = editorWindow->getProgramController();
        window->show();
    }
    const QString path = m_temp.filePath("cache-large.pdf");
    QVERIFY(writePdfFixture(path, 1200, 20));
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY(controller->getDocument());
    QTRY_VERIFY(!controller->getIsBusy());
    auto* compiler = controller->getPdfWidget()->getDrawWidgetProxy()->getTextLayoutCompiler();
    auto storage = compiler->acquireSearchTextCache();
    window->findChild<QAction*>("actionFind")->trigger();
    auto* dialog = window->findChild<QDialog*>("findDialog");
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    query->setText("FamilyPDF");
    QTRY_VERIFY(storage->getCompletedPageCount() > 0);
    QVERIFY(storage->getCompletedPageCount() < 1200);
    query->clear();
    auto first = storage;
    const auto partial = storage->getCompletedPageCount();
    query->setText("page 1200");
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QString("1 / 20"), 30000);
    QCOMPARE(storage->getCompletedPageCount(), size_t(1200));
    QCOMPARE(compiler->acquireSearchTextCache(), first);
    QVERIFY(partial > 0);
    query->setText("FamilyPDF");
    QTRY_COMPARE_WITH_TIMEOUT(status->text(), QString("1 / 24000"), 15000);
    QCOMPARE(compiler->acquireSearchTextCache(), first);
    query->clear();
    QCOMPARE(storage->getCompletedPageCount(), size_t(1200));
    std::weak_ptr<pdf::PDFSearchTextCache> lifetime = first;
    first.reset();
    storage.reset();
    controller->closeDocument();
    QTRY_VERIFY_WITH_TIMEOUT(lifetime.expired(), 5000);
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY(controller->getDocument());
    QCOMPARE(compiler->acquireSearchTextCache()->getCompletedPageCount(), size_t(0));
    compiler->makeTextLayout();
    QTRY_VERIFY(compiler->isTextLayoutReady());
    QVERIFY(compiler->getVerifiedTextLayoutStorage());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    window->findChild<QAction*>("actionFind")->trigger();
    dialog = window->findChild<QDialog*>("findDialog");
    dialog->findChild<QLineEdit*>("findQuery")->setText("FamilyPDF");
    QTRY_COMPARE(dialog->findChild<QLabel*>("findStatus")->text(), QString("1 / 3"));
    QCOMPARE(compiler->acquireSearchTextCache()->getCompletedPageCount(), size_t(3));
    controller->closeDocument();
}

void ViewerContextMenuTest::searchEditorContentInvalidation()
{
    pdfviewer::PDFEditorMainWindow editor;
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY(controller->getDocument());
    QTRY_VERIFY(!controller->getIsBusy());
    auto* compiler = controller->getPdfWidget()->getDrawWidgetProxy()->getTextLayoutCompiler();
    editor.findChild<QAction*>("actionFind")->trigger();
    auto* dialog = editor.findChild<QDialog*>("findDialog");
    dialog->findChild<QLineEdit*>("findQuery")->setText("FamilyPDF");
    QTRY_COMPARE(dialog->findChild<QLabel*>("findStatus")->text(), QString("1 / 3"));
    auto oldStorage = compiler->acquireSearchTextCache();
    // Also retire an in-flight legacy layout compilation. Its queued finished
    // signal must not restore the old document's cache after this edit.
    compiler->makeTextLayout();
    pdf::PDFDocumentModifier modifier(controller->getDocument());
    QByteArray content("BT /F1 16 Tf 30 535 Td (Replacement) Tj ET");
    pdf::PDFDictionary dict;
    dict.addEntry(pdf::PDFInplaceOrMemoryString("Length"), pdf::PDFObject::createInteger(content.size()));
    modifier.getBuilder()->setObject(pdf::PDFObjectReference(4, 0),
        pdf::PDFObject::createStream(std::make_shared<pdf::PDFStream>(std::move(dict), std::move(content))));
    modifier.markPageContentsChanged();
    QVERIFY(modifier.finalize());
    controller->onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    QVERIFY(!controller->getToolManager()->getFindTextTool()->isActive());
    QVERIFY(compiler->acquireSearchTextCache() != oldStorage);
    QCOMPARE(compiler->acquireSearchTextCache()->getCompletedPageCount(), size_t(0));
    QTest::qWait(50);
    QVERIFY(!compiler->isTextLayoutReady());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    editor.findChild<QAction*>("actionFind")->trigger();
    dialog = editor.findChild<QDialog*>("findDialog");
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    query->setText("FamilyPDF");
    QTRY_COMPARE(status->text(), QString("1 / 2"));
    query->setText("Replacement");
    QTRY_COMPARE(status->text(), QString("1 / 1"));
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    editor.findChild<QAction*>("actionFind")->trigger();
    dialog = editor.findChild<QDialog*>("findDialog");
    dialog->findChild<QLineEdit*>("findQuery")->setText("FamilyPDF");
    QTRY_COMPARE(dialog->findChild<QLabel*>("findStatus")->text(), QString("1 / 3"));
    controller->closeDocument();
}

void ViewerContextMenuTest::searchRejectsPartialLegacyLayout()
{
    const QString path = m_temp.filePath("partial-legacy-layout.pdf");
    QVERIFY(writePdfFixture(path, 3, 1, true, false, true));
    auto* controller = m_window->getProgramController();
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY(controller->getDocument());
    QTRY_VERIFY(!controller->getIsBusy());
    auto* compiler = proxy()->getTextLayoutCompiler();
    compiler->makeTextLayout();
    QTRY_VERIFY(compiler->isTextLayoutReady());
    QVERIFY(!compiler->getTextLayoutStorage()->getTextLayout(0).getTextBlocks().empty());
    QVERIFY(!compiler->getVerifiedTextLayoutStorage());
    action("actionFind")->trigger();
    auto* dialog = m_window->findChild<QDialog*>("findDialog");
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    query->setText("FamilyPDF");
    QTRY_COMPARE(status->text(), QString("Search incomplete. 1 / 3"));
    auto cache = compiler->acquireSearchTextCache();
    QCOMPARE(cache->getCompletedPageCount(), size_t(2));
    query->setText("smoke");
    QTRY_COMPARE(status->text(), QString("Search incomplete. 1 / 3"));
    QCOMPARE(cache->getCompletedPageCount(), size_t(2));
    controller->closeDocument();
}

void ViewerContextMenuTest::searchWarmCacheBenchmark()
{
    const QString root = qEnvironmentVariable("FAMILYPDF_SEARCH_FIXTURES");
    if (root.isEmpty()) QSKIP("Set FAMILYPDF_SEARCH_FIXTURES for local benchmark.");
    auto memory = [](const char* stage) {
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS_EX counters{};
        using Query = BOOL (WINAPI*)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
        auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo"));
        if (query && query(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counters), sizeof(counters)))
            qInfo() << "SEARCH_MEMORY" << stage << "working_mb" << counters.WorkingSetSize/1048576.0 << "private_mb" << counters.PrivateUsage/1048576.0;
#else
        Q_UNUSED(stage);
#endif
    };
    auto* controller = m_window->getProgramController();
    controller->closeDocument();
    controller->openDocument(root + "/text-1200.pdf");
    QTRY_VERIFY(controller->getDocument());
    QTRY_VERIFY(!controller->getIsBusy());
    memory("open");
    action("actionFind")->trigger();
    auto* dialog = m_window->findChild<QDialog*>("findDialog");
    auto* query = dialog->findChild<QLineEdit*>("findQuery");
    auto* status = dialog->findChild<QLabel*>("findStatus");
    for (const QString& phrase : {QStringLiteral("alpha"), QStringLiteral("alpha"), QStringLiteral("中文搜尋"), QStringLiteral("EndNeedle"), QStringLiteral("ABC")})
    {
        query->clear();
        QElapsedTimer timer; timer.start();
        qint64 first=-1, last=0, maxGap=0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&]() { auto now=timer.elapsed(); maxGap=qMax(maxGap, now-last); last=now; });
        heartbeat.start(10);
        if (phrase == "ABC") { query->setText("A"); QTest::qWait(150); query->setText("AB"); QTest::qWait(150); }
        query->setText(phrase);
        while (status->text().startsWith("Searching") && timer.elapsed() < 60000)
        {
            QTest::qWait(1);
            if (first < 0 && dialog->windowTitle().contains("1/")) first=timer.elapsed();
        }
        QVERIFY(timer.elapsed() < 60000);
        qInfo() << "SEARCH_WARM" << phrase << "first_ms" << first << "complete_ms" << timer.elapsed() << "ui_gap_ms" << maxGap << status->text();
        memory(qPrintable(phrase));
        heartbeat.stop();
    }
    query->clear();
    memory("clear");
    dialog->reject();
    controller->closeDocument();
    QTest::qWait(100);
    memory("closed");
}

void ViewerContextMenuTest::searchPerformanceBenchmark()
{
    const QString root = qEnvironmentVariable("FAMILYPDF_SEARCH_FIXTURES");
    if (root.isEmpty()) QSKIP("Set FAMILYPDF_SEARCH_FIXTURES for local before/after benchmark.");
    for (const QString& name : {QStringLiteral("text-12.pdf"), QStringLiteral("text-1200.pdf"), QStringLiteral("scan-3.pdf")})
    {
        auto* controller = m_window->getProgramController();
        controller->closeDocument();
        controller->openDocument(root + "/" + name);
        QTRY_VERIFY(controller->getDocument());
        QTRY_VERIFY(!controller->getIsBusy());
        qint64 maxGap = 0;
        QElapsedTimer clock; clock.start();
        qint64 last = clock.elapsed();
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&]() { const auto now = clock.elapsed(); maxGap = qMax(maxGap, now-last); last=now; });
        heartbeat.start(10);
        action("actionFind")->trigger();
        auto* dialog = m_window->findChild<QDialog*>("findDialog");
        auto* query = dialog->findChild<QLineEdit*>("findQuery");
        auto* status = dialog->findChild<QLabel*>("findStatus");
        query->setText("alpha");
        qint64 first = -1;
        while (status->text().startsWith("Searching") && clock.elapsed() < 60000)
        {
            QTest::qWait(1);
            if (first < 0 && dialog->windowTitle().contains("1/")) first = clock.elapsed();
        }
        QVERIFY(clock.elapsed() < 60000);
        qInfo().noquote() << "SEARCH_AFTER" << name << "first_ms=" << first << "complete_ms=" << clock.elapsed()
                         << "max_ui_gap_ms=" << maxGap << "status=" << status->text();
        dialog->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        heartbeat.stop();
    }
}

void ViewerContextMenuTest::largePdfReadingBenchmark()
{
    const QString path = qEnvironmentVariable("FAMILYPDF_LARGE_PDF");
    if (path.isEmpty()) QSKIP("Set FAMILYPDF_LARGE_PDF for the real-document benchmark.");
    QElapsedTimer timer;
    timer.start();
    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getOriginalFileName() == path, 120000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getIsBusy(), 120000);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 120000);
    const auto count = m_window->getProgramController()->getDocument()->getCatalog()->getPageCount();
    qInfo() << "READING open_ms" << timer.elapsed() << "pages" << count;
    proxy()->setPageLayout(pdf::PageLayout::SinglePage);
    proxy()->goToPage(0);
    auto waitForPage = [&](int page) {
        auto ready = [&]() {
            for (const auto& item : proxy()->getSnapshot().items)
                if (item.pageIndex == page && item.compiledPage) return true;
            return false;
        };
        QElapsedTimer wait;
        wait.start();
        while (!ready() && wait.elapsed() < 120000)
            QTest::qWait(10);
        return ready();
    };
    QVERIFY(waitForPage(0));
    qInfo() << "READING first_page_ms" << timer.elapsed();
    QVERIFY(!proxy()->drawThumbnailImage(0, 100).isNull());
    qInfo() << "READING first_thumbnail_ms" << timer.elapsed();
    // Request the same visible thumbnail strip before jumping away.
    timer.restart();
    for (int page = 1; page < qMin(9, int(count)); ++page)
        proxy()->drawThumbnailImage(page, 100);
    for (int page : {19, 99, 199, qMin(363, int(count) - 1)})
    {
        if (page >= int(count)) continue;
        timer.restart();
        proxy()->goToPage(page);
        QVERIFY(waitForPage(page));
        qInfo() << "READING jump_page" << page + 1 << "ms" << timer.elapsed();
        QElapsedTimer thumbnailTimer;
        thumbnailTimer.start();
        const QImage image = proxy()->drawThumbnailImage(page, 100);
        QVERIFY(!image.isNull());
        qInfo() << "READING thumbnail_page" << page + 1 << "ms" << thumbnailTimer.elapsed() << "size" << image.size();
        // Normal page drawing, independent of loading overlays and window geometry.
        QImage normal(600, 825, QImage::Format_ARGB32_Premultiplied);
        normal.fill(Qt::white);
        QPainter painter(&normal);
        const auto* pdfPage = m_window->getProgramController()->getDocument()->getCatalog()->getPage(page);
        QTransform transform;
        transform.scale(600.0 / pdfPage->getMediaBox().width(), -825.0 / pdfPage->getMediaBox().height());
        transform.translate(-pdfPage->getMediaBox().left(), -pdfPage->getMediaBox().bottom());
        for (const auto& item : proxy()->getSnapshot().items)
            if (item.pageIndex == page && item.compiledPage)
                item.compiledPage->draw(&painter, pdfPage->getCropBox(), transform, proxy()->getFeatures(), 1.0);
        painter.end();
        qInfo() << "READING page_pixel_sha256" << page + 1
                << QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(normal.constBits()), normal.sizeInBytes()), QCryptographicHash::Sha256).toHex();
    }
    timer.restart();
    for (int page : {99, 199, qMin(363, int(count) - 1)})
        if (page < int(count)) proxy()->goToPage(page);
    QVERIFY(waitForPage(qMin(363, int(count) - 1)));
    qInfo() << "READING rapid_jump_ms" << timer.elapsed();
    timer.restart();
    proxy()->goToPage(0);
    QVERIFY(waitForPage(0));
    qInfo() << "READING return_first_ms" << timer.elapsed();

    // Cold rapid jumps: reopening discards the previous document's compiled cache.
    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 120000);
    QVERIFY(waitForPage(0));
    timer.restart();
    for (int page : {99, 199, qMin(363, int(count) - 1)})
        if (page < int(count)) proxy()->goToPage(page);
    QVERIFY(waitForPage(qMin(363, int(count) - 1)));
    qInfo() << "READING cold_rapid_jump_ms" << timer.elapsed();

    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const QString output = m_temp.filePath("real-extracted.pdf");
    int stage = 0;
    QTimer dialogTimer;
    connect(&dialogTimer, &QTimer::timeout, this, [&]() {
        auto* modal = QApplication::activeModalWidget();
        if (auto* input = qobject_cast<QInputDialog*>(modal); input && stage == 0)
        {
            input->setTextValue("1-3,8,10-12");
            ++stage;
            input->accept();
        }
        else if (auto* save = qobject_cast<QFileDialog*>(modal); save && stage == 1)
        {
            save->selectFile(output);
            ++stage;
            static_cast<QDialog*>(save)->accept();
        }
        else if (auto* message = qobject_cast<QMessageBox*>(modal))
        {
            ++stage;
            message->accept();
        }
    });
    dialogTimer.start(10);
    m_window->getProgramController()->extractPages();
    dialogTimer.stop();
    QCOMPARE(stage, 3);
    QVERIFY(QFile::exists(output));
    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(output);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 120000);
    QCOMPARE(m_window->getProgramController()->getDocument()->getCatalog()->getPageCount(), size_t(7));
    proxy()->goToPage(0);
    QVERIFY(waitForPage(0));
    qInfo() << "READING extracted_pdf_reopened_pages" << 7;
}

void ViewerContextMenuTest::extractionRejectsInvalidInputAndCancellation()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        bool messageSeen = false;
        bool saveSeen = false;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&]() {
            auto* modal = QApplication::activeModalWidget();
            if (auto* input = qobject_cast<QInputDialog*>(modal))
            {
                if (scenario == 0) input->reject();
                else { input->setTextValue(scenario == 1 ? "0" : "1-3"); input->accept(); }
            }
            else if (auto* save = qobject_cast<QFileDialog*>(modal))
            { saveSeen = true; save->reject(); }
            else if (auto* message = qobject_cast<QMessageBox*>(modal))
            { messageSeen = true; QVERIFY(!message->text().isEmpty()); message->accept(); }
        });
        timer.start(10);
        m_window->getProgramController()->extractPages();
        timer.stop();
        QCOMPARE(messageSeen, scenario == 1);
        QCOMPARE(saveSeen, scenario == 2);
        QCOMPARE(m_window->getProgramController()->getDocument()->getCatalog()->getPageCount(), size_t(3));
    }
}

void ViewerContextMenuTest::readingPositionRestoresZoomAndClamps()
{
#ifdef Q_OS_LINUX
    QSKIP("Per-document window-state restoration is covered by the Windows runtime job.");
#endif
    const QString longPdfPath = m_temp.filePath("reading-position-25-pages.pdf");
    QVERIFY(writePdfFixture(longPdfPath, 25));

    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(longPdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    proxy()->setPageLayout(pdf::PageLayout::OneColumn);
    proxy()->goToPage(19);
    proxy()->zoom(1.5);
    proxy()->scrollByPixels(QPoint(0, -120));
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().empty(), 5000);
    QCOMPARE(m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(19));

    QScrollBar* verticalScrollBar = m_window->getProgramController()->getPdfWidget()->getVerticalScrollbar();
    const qreal savedScrollPosition = verticalScrollBar->maximum() > verticalScrollBar->minimum()
        ? qreal(verticalScrollBar->value() - verticalScrollBar->minimum()) /
          qreal(verticalScrollBar->maximum() - verticalScrollBar->minimum())
        : 0.0;

    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(longPdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().empty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(19), 5000);
    QVERIFY(qAbs(proxy()->getZoom() - 1.5) < 0.001);
    QTRY_VERIFY_WITH_TIMEOUT(verticalScrollBar->maximum() > verticalScrollBar->minimum(), 5000);
    const qreal restoredScrollPosition = qreal(verticalScrollBar->value() - verticalScrollBar->minimum()) /
                                         qreal(verticalScrollBar->maximum() - verticalScrollBar->minimum());
    QVERIFY(qAbs(restoredScrollPosition - savedScrollPosition) < 0.03);

    m_window->getProgramController()->closeDocument();
    QVERIFY(writePdfFixture(longPdfPath, 5));
    m_window->getProgramController()->openDocument(longPdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().empty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(4), 5000);

    m_window->getProgramController()->closeDocument();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("DocumentViewStates");
    for (const QString& group : settings.childGroups())
    {
        settings.beginGroup(group);
        if (QFileInfo(settings.value("path").toString()).absoluteFilePath() == QFileInfo(longPdfPath).absoluteFilePath())
        {
            settings.setValue("page", QStringLiteral("damaged"));
            settings.setValue("zoom", QStringLiteral("damaged"));
        }
        settings.endGroup();
    }
    settings.endGroup();

    m_window->getProgramController()->openDocument(longPdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().empty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(0), 5000);

    const QString firstOpenPath = m_temp.filePath("first-open-default.pdf");
    QVERIFY(writePdfFixture(firstOpenPath, 4));
    m_window->getProgramController()->closeDocument();
    m_window->getProgramController()->openDocument(firstOpenPath);
    QTRY_VERIFY_WITH_TIMEOUT(m_window->getProgramController()->getDocument() != nullptr, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().empty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(0), 5000);
}

void ViewerContextMenuTest::thumbnailSelectionAndPageManagement()
{
#ifdef Q_OS_LINUX
    QSKIP("Editor thumbnail interactions are covered by the Windows runtime job.");
#endif
    const QString editorPath = m_temp.filePath("thumbnail-management.pdf");
    QVERIFY(writePdfFixture(editorPath, 6));

    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(editorPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    saveImage(editor.grab(), "editor-reading-smoke.png");

    auto* sidebarDock = editor.findChild<QDockWidget*>("SidebarDockWidget");
    auto* thumbnails = editor.findChild<QListView*>("thumbnailsListView");
    QVERIFY(sidebarDock);
    QVERIFY(thumbnails);
    sidebarDock->show();
    thumbnails->show();
    QTRY_COMPARE(thumbnails->model()->rowCount(), 6);
    QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(0, 0)).isValid());

    const auto clickThumbnail = [thumbnails](int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        const QModelIndex index = thumbnails->model()->index(row, 0);
        QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, modifiers, thumbnails->visualRect(index).center());
        QCoreApplication::processEvents();
    };

    clickThumbnail(1);
    QCOMPARE(thumbnails->selectionModel()->selectedIndexes().size(), 1);
    clickThumbnail(3, Qt::ControlModifier);
    QModelIndexList selected = thumbnails->selectionModel()->selectedIndexes();
    QCOMPARE(selected.size(), 2);
    QCOMPARE(selected.front().row(), 1);
    QCOMPARE(selected.back().row(), 3);

    clickThumbnail(1);
    clickThumbnail(4, Qt::ShiftModifier);
    selected = thumbnails->selectionModel()->selectedIndexes();
    QCOMPARE(selected.size(), 4);
    for (int index = 0; index < selected.size(); ++index)
    {
        QCOMPARE(selected[index].row(), index + 1);
    }

#ifndef Q_OS_LINUX
    bool menuInspected = false;
    QTimer menuTimer;
    connect(&menuTimer, &QTimer::timeout, &editor, [&]()
    {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
        {
            for (QWidget* widget : QApplication::allWidgets())
            {
                auto* candidate = qobject_cast<QMenu*>(widget);
                if (candidate && candidate->findChild<QAction*>("thumbnailExtractPagesAction"))
                {
                    menu = candidate;
                    break;
                }
            }
        }
        if (!menu)
        {
            return;
        }
        menuTimer.stop();
        const QStringList requiredActions = {
            "thumbnailExtractPagesAction",
            "thumbnailDeletePagesAction",
            "thumbnailRotatePagesRightAction",
            "thumbnailRotatePagesLeftAction"
        };
        for (const QString& objectName : requiredActions)
        {
            QVERIFY(menu->findChild<QAction*>(objectName));
        }
        menuInspected = true;
        menu->close();
    });
    menuTimer.start(10);
    const QPoint contextPoint = thumbnails->visualRect(thumbnails->model()->index(2, 0)).center();
    QContextMenuEvent contextEvent(QContextMenuEvent::Mouse, contextPoint, thumbnails->viewport()->mapToGlobal(contextPoint));
    QApplication::sendEvent(thumbnails->viewport(), &contextEvent);
    QVERIFY(menuInspected);
#endif

    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto extractAndVerify = [&](const std::vector<pdf::PDFInteger>& pages, const QString& fileName, size_t expectedPageCount)
    {
        const QString extractedPath = m_temp.filePath(fileName);
        QString savedPath;
        int extractionStage = 0;
        QTimer extractionTimer;
        connect(&extractionTimer, &QTimer::timeout, &editor, [&]()
        {
            QWidget* modal = QApplication::activeModalWidget();
            if (auto* save = qobject_cast<QFileDialog*>(modal); save && extractionStage == 0)
            {
                save->selectFile(extractedPath);
                savedPath = save->selectedFiles().value(0);
                ++extractionStage;
                static_cast<QDialog*>(save)->accept();
            }
            else if (auto* message = qobject_cast<QMessageBox*>(modal); message && extractionStage == 1)
            {
                ++extractionStage;
                message->accept();
            }
        });
        extractionTimer.start(10);
        controller->extractPages(pages);
        extractionTimer.stop();
        QCOMPARE(extractionStage, 2);
        QVERIFY(QFile::exists(savedPath));
        pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
        const pdf::PDFDocument extracted = reader.readFromFile(savedPath);
        QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
        QCOMPARE(extracted.getCatalog()->getPageCount(), expectedPageCount);
    };
    extractAndVerify({2}, QStringLiteral("thumbnail-single-extracted.pdf"), size_t(1));
    extractAndVerify({3, 1}, QStringLiteral("thumbnail-multi-extracted.pdf"), size_t(2));

    controller->rotatePages({0, 2, 4}, 1);
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(1)->getPageRotation(), pdf::PageRotation::None);
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(2)->getPageRotation(), pdf::PageRotation::Rotate90);
    QAction* undoAction = editor.findChild<QAction*>("actionUndo");
    QAction* redoAction = editor.findChild<QAction*>("actionRedo");
    QVERIFY(undoAction);
    QVERIFY(redoAction);
    undoAction->trigger();
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::None);
    redoAction->trigger();
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);

    controller->deletePages({1, 3});
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(4));
    QTRY_COMPARE(thumbnails->model()->rowCount(), 4);
    QVERIFY(thumbnails->currentIndex().isValid());
    QVERIFY(thumbnails->currentIndex().row() >= 0 && thumbnails->currentIndex().row() < 4);
    undoAction->trigger();
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(6));
    redoAction->trigger();
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(4));
    undoAction->trigger();
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(6));

    bool warningSeen = false;
    QTimer warningTimer;
    connect(&warningTimer, &QTimer::timeout, &editor, [&]()
    {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        {
            warningSeen = true;
            QVERIFY(!message->text().isEmpty());
            message->accept();
        }
    });
    warningTimer.start(10);
    controller->deletePages({0, 1, 2, 3, 4, 5});
    warningTimer.stop();
    QVERIFY(warningSeen);
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(6));

    controller->deletePages({5});
    const QString modifiedPath = m_temp.filePath("thumbnail-management-saved.pdf");
    QString savedModifiedPath;
    QTimer saveTimer;
    connect(&saveTimer, &QTimer::timeout, &editor, [&]()
    {
        if (auto* save = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
        {
            save->selectFile(modifiedPath);
            savedModifiedPath = save->selectedFiles().value(0);
            static_cast<QDialog*>(save)->accept();
        }
    });
    saveTimer.start(10);
    controller->performSaveAs();
    saveTimer.stop();
    QVERIFY(QFile::exists(savedModifiedPath));
    controller->closeDocument();
    controller->openDocument(savedModifiedPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(5));
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getPageRotation(), pdf::PageRotation::Rotate90);
    controller->closeDocument();
    QCoreApplication::processEvents();
}

QPoint ViewerContextMenuTest::pagePoint(int page) const
{
    const QRect rect = drawWidget()->rect().adjusted(4, 4, -4, -4);
    for (int y = rect.top(); y <= rect.bottom(); y += 4)
        for (int x = rect.left(); x <= rect.right(); x += 4)
            if (proxy()->getPageUnderPoint(QPoint(x, y), nullptr) == page) return QPoint(x + 2, y + 2);
    return QPoint(-1, -1);
}

void ViewerContextMenuTest::withMenu(const QPoint& point, const std::function<void(QMenu*)>& inspect)
{
    bool opened = false;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, m_window.get(), [&]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) return;
        opened = true;
        inspect(menu);
        menu->close();
    });
    timer.start(0);
    // Deliver the same event generated by a platform right-click to the child
    // draw widget. This exercises propagation and coordinate mapping as well
    // as the real modal menu, without relying on a desktop mouse position.
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, drawWidget()->mapToGlobal(point));
    QApplication::sendEvent(drawWidget(), &event);
    QVERIFY2(opened, "Right-click on the draw widget did not open the Viewer menu.");
}

QAction* ViewerContextMenuTest::bookmarkAction(QMenu* menu) const
{
    for (QAction* candidate : menu->actions())
        if (candidate->text() == action("actionBookmarkPage")->text()) return candidate;
    return nullptr;
}

void ViewerContextMenuTest::clickAction(const char* name)
{
    withMenu(pagePoint(0), [&](QMenu* menu) {
        QAction* target = action(name);
        QVERIFY(target);
        QVERIFY(target->isEnabled());
        QMenu* owner = menu;
        if (!menu->actions().contains(target))
        {
            owner = nullptr;
            for (QAction* entry : menu->actions())
                if (entry->menu() && entry->menu()->actions().contains(target)) owner = entry->menu();
            QVERIFY(owner);
            owner->popup(menu->mapToGlobal(menu->rect().topRight()));
        }
        QTest::mouseClick(owner, Qt::LeftButton, Qt::NoModifier, owner->actionGeometry(target).center());
    });
}

void ViewerContextMenuTest::menuActionsOperateOnTheDocument()
{
    clickAction("actionSelectText");
    QVERIFY(action("actionSelectText")->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(proxy()->getTextLayoutCompiler()->isTextLayoutReady(), 15000);
    clickAction("actionSelectTextAll");
    QApplication::clipboard()->clear();
    clickAction("actionCopyText");
    QVERIFY(QApplication::clipboard()->text().contains("FamilyPDF smoke page"));
    clickAction("actionDeselectText");
    QVERIFY(!action("actionCopyText")->isEnabled());
    QVERIFY(!action("actionDeselectText")->isEnabled());
    clickAction("actionSelectTable");
    QVERIFY(action("actionSelectTable")->isChecked());
    QVERIFY(!action("actionSelectText")->isChecked());
    clickAction("actionMagnifier");
    QVERIFY(action("actionMagnifier")->isChecked());
    QVERIFY(!action("actionSelectTable")->isChecked());
    const double oldZoom = proxy()->getZoom();
    clickAction("actionZoom_In");
    QVERIFY(proxy()->getZoom() > oldZoom);
    clickAction("actionZoom_Out");
    QVERIFY(qAbs(proxy()->getZoom() - oldZoom) < 0.0001);
    clickAction("actionFitPage");
    const double pageZoom = proxy()->getZoom();
    QVERIFY(pageZoom > 0);
    clickAction("actionFitWidth");
    QVERIFY(proxy()->getZoom() >= pageZoom);
    auto* sidebar = m_window->findChild<QDockWidget*>();
    QVERIFY(sidebar);
    const bool wasVisible = sidebar->isVisible();
    withMenu(pagePoint(0), [&](QMenu* menu) {
        QAction* toggle = sidebar->toggleViewAction();
        QVERIFY(menu->actions().contains(toggle));
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(toggle).center());
    });
    QCOMPARE(sidebar->isVisible(), !wasVisible);
    QCOMPARE(m_window->getProgramController()->getPdfWidget()->getPageRenderingErrorCount(), 0);
}

void ViewerContextMenuTest::bookmarkUsesClickedPage_data()
{
    QTest::addColumn<int>("layout");
    QTest::newRow("continuous") << int(pdf::PageLayout::OneColumn);
    QTest::newRow("two-pages") << int(pdf::PageLayout::TwoPagesLeft);
}

void ViewerContextMenuTest::bookmarkUsesClickedPage()
{
    QFETCH(int, layout);
    proxy()->setPageLayout(static_cast<pdf::PageLayout>(layout));
    proxy()->zoom(0.5);
    QTRY_VERIFY(pagePoint(1).x() >= 0);
    const auto pages = m_window->getProgramController()->getPdfWidget()->getDrawWidget()->getCurrentPages();
    QVERIFY(std::find(pages.begin(), pages.end(), 0) != pages.end());
    auto* bookmarks = m_window->getProgramController()->getBookmarkManager();
    withMenu(pagePoint(1), [&](QMenu* menu) {
        auto* bookmark = bookmarkAction(menu);
        QVERIFY(bookmark);
        QVERIFY(bookmark->isEnabled());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(bookmark).center());
    });
    QCOMPARE(bookmarks->getBookmarkCount(), 1);
    QCOMPARE(bookmarks->getBookmark(0).pageIndex, pdf::PDFInteger(1));
    QPoint gap(-1, -1);
    const QPoint start = pagePoint(0), end = pagePoint(1);
    const int steps = qMax(qAbs(end.x() - start.x()), qAbs(end.y() - start.y()));
    for (int step = 1; step < steps; ++step)
    {
        const QPoint point = start + (end - start) * (double(step) / steps);
        if (proxy()->getPageUnderPoint(point, nullptr) == -1) { gap = point; break; }
    }
    QVERIFY2(gap.x() >= 0, "Fixture must expose an actual gap between visible pages.");
    withMenu(gap, [&](QMenu* menu) {
        QVERIFY(bookmarkAction(menu));
        QVERIFY(!bookmarkAction(menu)->isEnabled());
    });
    QCOMPARE(bookmarks->getBookmarkCount(), 1);
}

void ViewerContextMenuTest::emptyDocumentCannotBeBookmarked()
{
    m_window->getProgramController()->closeDocument();
    withMenu(QPoint(40, 40), [&](QMenu* menu) {
        QVERIFY(bookmarkAction(menu));
        QVERIFY(!bookmarkAction(menu)->isEnabled());
        QVERIFY(!action("actionSelectText")->isEnabled());
    });
}

void ViewerContextMenuTest::saveImage(const QPixmap& pixmap, const QString& name)
{
    const QString directory = qEnvironmentVariable("FAMILYPDF_TEST_ARTIFACT_DIR");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(pixmap.save(QDir(directory).filePath(name)));
}

void ViewerContextMenuTest::traditionalChineseMenuAndSvgResources()
{
    m_window.reset();
    pdf::PDFApplicationTranslator translator;
    translator.setLanguage(pdf::PDFApplicationTranslator::E_LANGUAGE_CHINESE_TRADITIONAL);
    translator.installTranslator();
    init();
    QVERIFY(QCoreApplication::translate("pdf::PDFTranslationContext", "Loading page %1...").contains(QString::fromUtf8("載入")));
    QVERIFY(QCoreApplication::translate("pdf::PDFTranslationContext", "Page numbers must be between 1 and %1.").contains(QString::fromUtf8("頁碼")));
    QVERIFY(QCoreApplication::translate("pdf::PDFTranslationContext", "Unable to render this page.").contains(QString::fromUtf8("無法")));
    QVERIFY(QCoreApplication::translate("pdfviewer::PDFProgramController", "The optional FamilyPDF OCR plugin is not installed.").contains(QString::fromUtf8("尚未安裝")));
    QVERIFY(action("actionBookmarkPage")->text().contains(QString::fromUtf8("\xE6\x94\xB6\xE8\x97\x8F")));
    withMenu(pagePoint(0), [&](QMenu* menu) {
        QMenu* tools = nullptr;
        for (QAction* entry : menu->actions()) if (entry->menu()) tools = entry->menu();
        QVERIFY(tools);
        QVERIFY(tools->title().contains(QString::fromUtf8("\xE5\xB7\xA5\xE5\x85\xB7")));
        saveImage(menu->grab(), "viewer-context-menu-zh-TW.png");
    });
    saveImage(m_window->grab(), "viewer-zh-TW.png");
    QPluginLoader editor(QString::fromUtf8(EDITOR_PLUGIN_PATH));
    QPluginLoader signature(QString::fromUtf8(SIGNATURE_PLUGIN_PATH));
    QPluginLoader softproofing(QString::fromUtf8(SOFTPROOFING_PLUGIN_PATH));
    QVERIFY2(editor.load(), qPrintable(editor.errorString()));
    QVERIFY2(signature.load(), qPrintable(signature.errorString()));
    QVERIFY2(softproofing.load(), qPrintable(softproofing.errorString()));
    QStringList resources;
    for (const QString& name : {"error", "information", "ok", "warning"})
        resources << ":/resources/result-" + name + ".svg";
    for (const QString& plugin : {"editorplugin", "signatureplugin"})
        for (const QString& name : {"accept-mark", "reject-mark", "clear", "create-no-mark", "create-yes-mark"})
            resources << ":/pdfplugins/" + plugin + "/" + name + ".svg";
    resources << ":/pdfplugins/signatureplugin/sign-electronically.svg" << ":/pdfplugins/softproofing/gamut-checking.svg";
    QPixmap sheet(16 * 56, 112);
    sheet.fill(Qt::white);
    QPainter painter(&sheet);
    painter.fillRect(0, 56, sheet.width(), 56, QColor("#172033"));
    int index = 0;
    for (const QString& path : resources)
    {
        QSvgRenderer renderer(path);
        QVERIFY2(renderer.isValid(), qPrintable(path));
        const QPixmap icon = QIcon(path).pixmap(32, 32);
        QVERIFY2(!icon.isNull(), qPrintable(path));
        const QImage image = icon.toImage().convertToFormat(QImage::Format_ARGB32);
        bool hasInk = false;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) hasInk |= qAlpha(image.pixel(x, y)) > 0;
        QVERIFY2(hasInk, qPrintable(path));
        renderer.render(&painter, QRectF(index * 56 + 8, 8, 40, 40));
        renderer.render(&painter, QRectF(index * 56 + 8, 64, 40, 40));
        ++index;
    }
    painter.end();
    QCOMPARE(index, 16);
    saveImage(sheet, "semantic-icons.png");
}


namespace
{
QList<pdf::PDFAnnotationPtr> annotations(pdfviewer::PDFProgramController* controller, int pageIndex = 0)
{
    QList<pdf::PDFAnnotationPtr> result;
    auto* doc = controller->getDocument();
    if (!doc) return result;
    const auto* page = doc->getCatalog()->getPage(pageIndex);
    const auto* dictionary = doc->getDictionaryFromObject(doc->getObjectByReference(page->getPageReference()));
    pdf::PDFDocumentDataLoaderDecorator loader(&doc->getStorage());
    for (const auto ref : loader.readReferenceArrayFromDictionary(dictionary, "Annots"))
        if (auto annotation = pdf::PDFAnnotation::parse(&doc->getStorage(), ref); annotation && annotation->asMarkupAnnotation()) result.append(annotation);
    return result;
}

bool annotationSaveAs(pdfviewer::PDFProgramController* controller, QWidget* owner, const QString& path)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    bool selected = false;
    bool timedOut = false;
    QTimer timer;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, owner, [&]() {
        timedOut = true;
        timer.stop();
        for (auto* dialog : owner->findChildren<QDialog*>())
            if (dialog->isVisible()) dialog->reject();
    });
    QObject::connect(&timer, &QTimer::timeout, owner, [&]() {
        for (auto* dialog : owner->findChildren<QFileDialog*>())
        {
            if (!dialog->isVisible()) continue;
            // selectFile() can preserve the focused, prefilled filename on
            // Linux. Enter the destination in the actual nonnative widget.
            dialog->setDirectory(QFileInfo(path).absolutePath());
            dialog->selectFile(QFileInfo(path).fileName());
            if (auto* filename = dialog->findChild<QLineEdit*>("fileNameEdit"))
                filename->setText(path);
            selected = dialog->selectedFiles().value(0) == path;
            qInfo() << "Annotation Save As destination" << dialog->selectedFiles();
            timer.stop();
            if (selected) static_cast<QDialog*>(dialog)->accept();
            else dialog->reject();
            return;
        }
    });
    deadline.start(5000);
    timer.start(10);
    controller->performSaveAs();
    return selected && !timedOut;
}

void annotationMenu(pdf::PDFWidgetAnnotationManager* manager, pdf::PDFObjectReference reference,
                    pdf::PDFObjectReference page, QWidget* owner, const char* actionName,
                    bool& found)
{
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, owner, [&]() {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
        {
            timer.stop();
            auto* action = menu->findChild<QAction*>(QLatin1String(actionName));
            found = action != nullptr;
            menu->close();
            if (action) action->trigger();
        }
    });
    timer.start(10);
    manager->showAnnotationMenu(reference, page, owner->mapToGlobal(QPoint(100, 100)));
}
}

void ViewerContextMenuTest::annotationMarkupWorkflow_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("highlight") << int(pdf::AnnotationType::Highlight);
    QTest::newRow("underline") << int(pdf::AnnotationType::Underline);
    QTest::newRow("strikeout") << int(pdf::AnnotationType::StrikeOut);
}

void ViewerContextMenuTest::annotationMarkupWorkflow()
{
    QFETCH(int, type);
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    const QString path = m_temp.filePath(QString("markup-%1.pdf").arg(type));
    QVERIFY(writePdfFixture(path, 3, 2));
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* drawProxy = widget->getDrawWidgetProxy();
    auto* tree = editor.findChild<QTreeView*>("notesTreeView");
    auto* filter = editor.findChild<QLineEdit*>("notesSearchLineEdit");
    auto* notesButton = editor.findChild<QToolButton*>("notesButton");
    QVERIFY(tree && filter && notesButton);
    QCOMPARE(tree->model()->rowCount(), 0);
    QVERIFY(notesButton->isEnabled());
    editor.findChild<QAction*>("actionSelectText")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(drawProxy->getTextLayoutCompiler()->isTextLayoutReady(), 15000);
    editor.findChild<QAction*>("actionSelectTextAll")->trigger();
    QVERIFY(!controller->getToolManager()->getSelectedText().isEmpty());
    const auto originalPage = controller->getDocument()->getCatalog()->getPage(0)->getPageReference();
    const auto* dictionary = controller->getDocument()->getDictionaryFromObject(controller->getDocument()->getObjectByReference(originalPage));
    const auto originalContents = controller->getDocument()->getObject(dictionary->get("Contents"));

    // Exercise the actual Editor context menu, using the active text selection.
    bool clicked = false;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&]() {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
        {
            timer.stop();
            auto* mark = menu->findChild<QAction*>(QString("selectionMarkup%1").arg(type));
            QVERIFY(mark && mark->isEnabled());
            clicked = true;
            menu->close();
            mark->trigger();
        }
    });
    timer.start(10);
    Q_EMIT widget->customContextMenuRequested(QPoint(100, 100));
    QVERIFY(clicked);
    QCOMPARE(tree->model()->rowCount(), 3); // One annotation per page, ordered by page.
    for (int page = 0; page < 3; ++page)
    {
        const auto items = annotations(controller, page);
        QCOMPARE(items.size(), 1);
        QCOMPARE(int(items.front()->getType()), type);
        QVERIFY(items.front()->getContents().contains(QString("FamilyPDF smoke page %1").arg(page + 1)));
        const auto* markup = dynamic_cast<const pdf::PDFHighlightAnnotation*>(items.front().data());
        QVERIFY(markup);
        QCOMPARE(markup->getHiglightArea().getQuadrilaterals().size(), size_t(2));
        QVERIFY(markup->getRectangle().isValid());
        auto* doc = controller->getDocument();
        const auto* dict = doc->getDictionaryFromObject(doc->getObjectByReference(items.front()->getSelfReference()));
        const auto* appearance = doc->getDictionaryFromObject(dict->get("AP"));
        QVERIFY(appearance && doc->getObject(appearance->get("N")).isStream());
        QVERIFY(tree->model()->index(page, 0).data().toString().contains(QString::number(page + 1)));
    }
    const auto* currentDictionary = controller->getDocument()->getDictionaryFromObject(controller->getDocument()->getObjectByReference(originalPage));
    QCOMPARE(controller->getDocument()->getObject(currentDictionary->get("Contents")), originalContents);
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(tree->model()->rowCount(), 0);
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(tree->model()->rowCount(), 3);
    filter->setText("SMOKE PAGE 2");
    QCOMPARE(tree->model()->rowCount(), 1);
    filter->clear();
    QCOMPARE(tree->model()->rowCount(), 3);
    editor.findChild<QDockWidget*>("SidebarDockWidget")->show();
    notesButton->click();
    drawProxy->zoom(2.0);
    Q_EMIT tree->clicked(tree->model()->index(2, 0));
    QTRY_VERIFY([&]() { const auto pages = widget->getDrawWidget()->getCurrentPages(); return std::find(pages.begin(), pages.end(), 2) != pages.end(); }());
    const QRectF annotationRect = annotations(controller, 2).front()->getRectangle();
    bool visible = false;
    for (const auto& item : drawProxy->getSnapshot().items)
        if (item.pageIndex == 2)
            visible = widget->getDrawWidget()->getWidget()->rect().intersects(item.pageToDeviceMatrix.mapRect(annotationRect).toAlignedRect());
    QVERIFY(visible);

    controller->performSave();
    controller->closeDocument();
    QCOMPARE(tree->model()->rowCount(), 0);
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(annotations(controller, 0).size(), 1);
    QCOMPARE(annotations(controller, 2).size(), 1);
    const QString copyPath = m_temp.filePath(QString("markup-save-as-%1.pdf").arg(type));
    QVERIFY(annotationSaveAs(controller, &editor, copyPath));
    QVERIFY(QFile::exists(copyPath));
    controller->closeDocument();
    controller->openDocument(copyPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(int(annotations(controller).front()->getType()), type);

    // A real drag selects only the requested part of two lines, even over an existing mark.
    drawProxy->goToPage(0);
    editor.findChild<QAction*>("actionFitPage")->trigger();
    editor.findChild<QAction*>("actionSelectText")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(drawProxy->getTextLayoutCompiler()->isTextLayoutReady(), 15000);
    QTRY_VERIFY(!drawProxy->getSnapshot().items.empty());
    const auto matrix = drawProxy->getSnapshot().items.front().pageToDeviceMatrix;
    auto* draw = widget->getDrawWidget()->getWidget();
    const QPoint start = matrix.map(QPointF(60, 541)).toPoint();
    const QPoint end = matrix.map(QPointF(180, 521)).toPoint();
    QTest::mousePress(draw, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(draw, end);
    QTest::mouseRelease(draw, Qt::LeftButton, Qt::NoModifier, end);
    const auto partialSelection = controller->getToolManager()->getSelectedText();
    QVERIFY(!partialSelection.isEmpty());
    auto layout = drawProxy->getTextLayoutCompiler()->getTextLayoutLazy(0);
    QPolygonF expectedQuads;
    pdf::PDFTextSelectionPainter selectionPainter(&partialSelection);
    selectionPainter.prepareGeometry(0, layout, QTransform(), &expectedQuads);
    controller->createSelectionMarkup(pdf::AnnotationType(type));
    QCOMPARE(annotations(controller).size(), 2);
    QCOMPARE(annotations(controller, 1).size(), 1);
    const auto partialItems = annotations(controller);
    const auto* partial = dynamic_cast<const pdf::PDFHighlightAnnotation*>(partialItems.back().data());
    QVERIFY(partial);
    QCOMPARE(partial->getHiglightArea().getQuadrilaterals().size(), size_t(expectedQuads.size() / 4));
    for (size_t i = 0; i < partial->getHiglightArea().getQuadrilaterals().size(); ++i)
        for (int corner = 0; corner < 4; ++corner)
            QCOMPARE(partial->getHiglightArea().getQuadrilaterals()[i][corner], expectedQuads[int(i) * 4 + corner]);
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(annotations(controller).size(), 1);
    controller->performSave();

    pdfviewer::PDFViewerMainWindow viewer;
    viewer.show();
    auto* reader = viewer.getProgramController();
    reader->openDocument(copyPath);
    QTRY_VERIFY_WITH_TIMEOUT(reader->getDocument(), 15000);
    auto* manager = reader->getPdfWidget()->getDrawWidgetProxy()->getAnnotationManager();
    QVERIFY(!manager->isEditingEnabled());
    bool editFound = true;
    annotationMenu(manager, annotations(reader).front()->getSelfReference(), reader->getDocument()->getCatalog()->getPage(0)->getPageReference(), &viewer, "editAnnotation", editFound);
    QVERIFY(!editFound);
    bool deleteFound = true;
    annotationMenu(manager, annotations(reader).front()->getSelfReference(), reader->getDocument()->getCatalog()->getPage(0)->getPageReference(), &viewer, "deleteAnnotation", deleteFound);
    QVERIFY(!deleteFound);
    QMimeData mime;
    QVERIFY(!manager->canAcceptAnnotationDrag(&mime));
    QVERIFY(!manager->handleAnnotationDrop(&mime, QPoint(), Qt::MoveAction));
    reader->createSelectionMarkup(pdf::AnnotationType(type));
    QCOMPARE(annotations(reader).size(), 1);
    QTRY_COMPARE(reader->getPdfWidget()->getPageRenderingErrorCount(), 0);
    reader->closeDocument();
    controller->closeDocument();
}

void ViewerContextMenuTest::annotationNoteWorkflow()
{
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    const QString path = m_temp.filePath("note-workflow.pdf");
    QVERIFY(writePdfFixture(path, 3));
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* drawProxy = widget->getDrawWidgetProxy();
    auto* manager = drawProxy->getAnnotationManager();
    auto* tree = editor.findChild<QTreeView*>("notesTreeView");
    auto* filter = editor.findChild<QLineEdit*>("notesSearchLineEdit");
    const QString contents = QString::fromUtf8("中文註解 English comment\n第二行");
    editor.findChild<QAction*>("actionFitPage")->trigger();
    QTRY_VERIFY(!drawProxy->getSnapshot().items.empty());
    const QPoint point = drawProxy->getSnapshot().items.front().pageToDeviceMatrix.map(QPointF(100, 300)).toPoint();
    bool entered = false;
    QTimer inputTimer;
    connect(&inputTimer, &QTimer::timeout, &editor, [&]() {
        if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
        {
            inputTimer.stop(); entered = true;
            dialog->setTextValue(contents);
            dialog->accept();
        }
    });
    editor.findChild<QAction*>("actionStickyNoteNote")->trigger();
    inputTimer.start(10);
    QTest::mouseMove(widget->getDrawWidget()->getWidget(), point);
    QTest::mouseClick(widget->getDrawWidget()->getWidget(), Qt::LeftButton, Qt::NoModifier, point);
    QVERIFY(entered);
    QCOMPARE(annotations(controller).size(), 1);
    QCOMPARE(annotations(controller).front()->getType(), pdf::AnnotationType::Text);
    QCOMPARE(annotations(controller).front()->getContents(), contents);
    QCOMPARE(tree->model()->rowCount(), 1);
    controller->performSave();
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(annotations(controller).front()->getContents(), contents);
    filter->setText(QString::fromUtf8("中文"));
    QCOMPARE(tree->model()->rowCount(), 1);
    filter->setText("ENGLISH");
    QCOMPARE(tree->model()->rowCount(), 1);
    filter->setText("absent");
    QCOMPARE(tree->model()->rowCount(), 0);
    filter->clear();

    const auto reference = annotations(controller).front()->getSelfReference();
    const auto page = controller->getDocument()->getCatalog()->getPage(0)->getPageReference();
    const QString changed = QString::fromUtf8("修改後 Updated comment");
    bool edited = false;
    QTimer editTimer;
    connect(&editTimer, &QTimer::timeout, &editor, [&]() {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        {
            for (auto* browser : dialog->findChildren<QTextBrowser*>())
                if (browser->toPlainText() == contents)
                {
                    editTimer.stop();
                    QVERIFY(!browser->isReadOnly());
                    browser->setPlainText(changed);
                    edited = true;
                    dialog->accept();
                    return;
                }
        }
    });
    editTimer.start(10);
    bool found = false;
    annotationMenu(manager, reference, page, &editor, "editAnnotation", found);
    editTimer.stop();
    QVERIFY(found && edited);
    QCOMPARE(annotations(controller).front()->getContents(), changed);
    filter->setText(QString::fromUtf8("修改後"));
    QCOMPARE(tree->model()->rowCount(), 1);
    filter->clear();
    controller->performSave();
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(annotations(controller).front()->getContents(), changed);
    annotationMenu(manager, annotations(controller).front()->getSelfReference(), page, &editor, "deleteAnnotation", found);
    QVERIFY(found);
    QCOMPARE(tree->model()->rowCount(), 0);
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(annotations(controller).front()->getContents(), changed);
    QCOMPARE(tree->model()->rowCount(), 1);
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(annotations(controller).size(), 0);
    controller->performSave();
    controller->closeDocument();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(annotations(controller).size(), 0);

    // Unsupported and malformed annotation entries cannot poison the list.
    pdf::PDFDocumentModifier modifier(controller->getDocument());
    auto* builder = modifier.getBuilder();
    const auto valid = builder->createAnnotationText(page, QRectF(50, 50, 24, 24), pdf::TextAnnotationIcon::Note, QString(), QString(), contents, false);
    const auto invalid = builder->createAnnotationText(page, QRectF(90, 50, 24, 24), pdf::TextAnnotationIcon::Note, QString(), QString(), QString(), false);
    builder->setObject(invalid, pdf::PDFObject());
    const auto unsupported = builder->createAnnotationText(page, QRectF(130, 50, 24, 24), pdf::TextAnnotationIcon::Note, QString(), QString(), QString(), false);
    pdf::PDFObjectFactory factory;
    factory.beginDictionary(); factory.beginDictionaryItem("Subtype"); factory << pdf::PDFObject::createName("UnsupportedV5"); factory.endDictionaryItem(); factory.endDictionary();
    builder->mergeTo(unsupported, factory.takeObject());
    Q_UNUSED(valid);
    modifier.markAnnotationsChanged();
    QVERIFY(modifier.finalize());
    controller->onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    QCOMPARE(tree->model()->rowCount(), 1);
    filter->setText("absent");
    controller->closeDocument();
    QCOMPARE(tree->model()->rowCount(), 0);
    QVERIFY(filter->text().isEmpty());
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(tree->model()->rowCount(), 0);
    controller->closeDocument();
}


void ViewerContextMenuTest::annotationListLargeDocument()
{
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    const QString path = m_temp.filePath("annotations-500-pages.pdf");
    QVERIFY(writePdfFixture(path, 500));
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    pdf::PDFDocumentModifier modifier(controller->getDocument());
    for (int page = 0; page < 500; page += 5)
        modifier.getBuilder()->createAnnotationText(controller->getDocument()->getCatalog()->getPage(page)->getPageReference(),
            QRectF(50, 50, 24, 24), pdf::TextAnnotationIcon::Comment, QString(), QString(),
            QString::fromUtf8("中文 Comment %1").arg(page), false);
    modifier.markAnnotationsChanged();
    QVERIFY(modifier.finalize());
    QElapsedTimer timer;
    timer.start();
    controller->onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    auto* tree = editor.findChild<QTreeView*>("notesTreeView");
    QCOMPARE(tree->model()->rowCount(), 100);
    qInfo() << "500-page / 100-note list refresh ms:" << timer.elapsed();
    timer.restart();
    editor.findChild<QLineEdit*>("notesSearchLineEdit")->setText("Comment 495");
    QCOMPARE(tree->model()->rowCount(), 1);
    qInfo() << "Annotation filter ms:" << timer.elapsed();
    controller->closeDocument();
}

namespace {
bool writeWorkflowForm(const QString& path)
{
    if (!writePdfFixture(path, 2)) return false;
    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    auto original = reader.readFromFile(path);
    pdf::PDFDocumentBuilder builder(&original);
    const auto first = original.getCatalog()->getPage(0)->getPageReference();
    const auto second = original.getCatalog()->getPage(1)->getPageReference();
    auto text = [&](QString name, QString value, pdf::PDFFormField::FieldFlags flags, int y, bool page2 = false) {
        auto field = builder.createFormFieldText(name, value, flags, 12);
        builder.createFormFieldWidget(field, page2 ? second : first, QRectF(40, y, 220, 40), "/Helv 12 Tf 0 g");
        builder.appendAcroFormField(field);
    };
    text("name", "", pdf::PDFFormField::Required, 450);
    text("readonly", "locked", pdf::PDFFormField::ReadOnly, 390);
    text("multiline", "", pdf::PDFFormField::Multiline, 320);
    text("password", "", pdf::PDFFormField::Password, 260);
    auto check = builder.createFormFieldCheckBox("agree", false, pdf::PDFFormField::None);
    builder.createFormFieldWidget(check, first, QRectF(40, 210, 22, 22), QByteArray());
    builder.appendAcroFormField(check);
    auto radio = builder.createFormFieldRadioGroup("radio", "a", pdf::PDFFormField::None);
    builder.createFormFieldRadioWidget(radio, first, QRectF(40, 170, 22, 22), "a", true);
    builder.createFormFieldRadioWidget(radio, first, QRectF(90, 170, 22, 22), "b", false);
    builder.appendAcroFormField(radio);
    auto combo = builder.createFormFieldChoice("combo", {{"one", "First"}, {"two", "Second"}}, {0}, pdf::PDFFormField::FieldFlags{pdf::PDFFormField::Combo, pdf::PDFFormField::Edit});
    builder.createFormFieldWidget(combo, first, QRectF(40, 110, 220, 30), "/Helv 12 Tf 0 g");
    builder.appendAcroFormField(combo);
    auto list = builder.createFormFieldChoice("list", {{"one", "First"}, {"two", "Second"}}, {0}, pdf::PDFFormField::None);
    builder.createFormFieldWidget(list, second, QRectF(40, 350, 220, 80), "/Helv 12 Tf 0 g");
    builder.appendAcroFormField(list);
    text("second", "", pdf::PDFFormField::None, 450, true);
    auto document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    return bool(writer.write(path, &document, true));
}
pdf::PDFFormField* workflowField(pdf::PDFWidgetFormManager* manager, const QString& name)
{
    pdf::PDFFormField* found = nullptr;
    manager->modify([&](pdf::PDFFormField* field) {
        if (field->getName(pdf::PDFFormField::FullyQualified) == name && !field->getWidgets().empty()) found = field;
    });
    return found;
}
QString workflowValue(pdf::PDFWidgetFormManager* manager, const QString& name)
{
    const auto* field = workflowField(manager, name);
    if (!field) return QString();
    const auto object = manager->getDocument()->getObject(field->getValue());
    if (object.isName()) return QString::fromLatin1(object.getString());
    return pdf::PDFDocumentDataLoaderDecorator(manager->getDocument()).readTextString(object, QString());
}
}

void ViewerContextMenuTest::formWorkflow()
{
    const QString path = m_temp.filePath("forms.pdf");
    QVERIFY(writeWorkflowForm(path));
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900); editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* manager = widget->getFormManager();
    auto* draw = widget->getDrawWidget()->getWidget();
    QVERIFY(manager);
    QCOMPARE(manager->getMissingRequiredFields(), QStringList{"name"});
    const auto originalContents = controller->getDocument()->getCatalog()->getPage(0)->getContents();
    auto enter = [&](const QString& name, const QString& value) {
        auto* field = workflowField(manager, name);
        manager->setFocusToEditor(manager->getEditor(field));
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, value);
        manager->keyPressEvent(draw, &event);
    };
    enter("readonly", "changed"); manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "readonly"), "locked");
    enter("name", "Form value");
    // Exercise the actual shortcut while the form owns keyboard focus.
    draw->setFocus();
    QTest::keyClick(draw, Qt::Key_S, Qt::ControlModifier);
    QCOMPARE(workflowValue(manager, "name"), "Form value");
    QVERIFY(!editor.windowTitle().contains('*'));
    // Save while still typing must flush the active editor, including its appearance.
    controller->performSave();
    QCOMPARE(workflowValue(manager, "name"), "Form value");
    QVERIFY(manager->getMissingRequiredFields().isEmpty());
    auto appearance = [&]() {
        const auto reference = workflowField(manager, "name")->getWidgets().front().getWidget();
        const auto* doc = controller->getDocument();
        const auto* dict = doc->getDictionaryFromObject(doc->getObjectByReference(reference));
        const auto* ap = doc->getDictionaryFromObject(dict->get("AP"));
        return ap ? doc->getObject(ap->get("N")) : pdf::PDFObject();
    };
    QVERIFY(appearance().isStream());
    QVERIFY(!pdf::PDFForm::parse(controller->getDocument(), controller->getDocument()->getCatalog()->getFormObject()).isAppearanceUpdateNeeded());
    const auto savedAppearance = appearance();
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "");
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "Form value");
    QCOMPARE(appearance(), savedAppearance);
    // Menu/toolbar actions must commit pending input before selecting history.
    enter("name", "pending");
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "Form value");
    QCOMPARE(appearance(), savedAppearance);
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "pending");
    editor.findChild<QAction*>("actionUndo")->trigger();
    enter("name", "new branch");
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "new branch");
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(workflowValue(manager, "name"), "Form value");
    enter("name", "123456789012345"); manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "name").size(), 12);
    enter("multiline", "line1\nline2"); manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "multiline"), "line1\nline2");
    for (const QString name : {QString("agree"), QString("radio")})
    {
        auto* field = workflowField(manager, name);
        auto* input = manager->getEditor(field);
        manager->setFocusToEditor(input);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        input->keyPressEvent(draw, &event);
        manager->setFocusToEditor(nullptr);
        const auto changed = workflowValue(manager, name);
        QCOMPARE(changed, name == "agree" ? "Yes" : "b");
        editor.findChild<QAction*>("actionUndo")->trigger();
        QVERIFY(workflowValue(manager, name) != changed);
        editor.findChild<QAction*>("actionRedo")->trigger();
        QCOMPARE(workflowValue(manager, name), changed);
    }
    enter("combo", "Second"); manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "combo"), "two");
    editor.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(workflowValue(manager, "combo"), "one");
    editor.findChild<QAction*>("actionRedo")->trigger();
    QCOMPARE(workflowValue(manager, "combo"), "two");
    manager->setFocusToEditor(manager->getEditor(workflowField(manager, "list")));
    QKeyEvent down(QEvent::KeyPress, Qt::Key_End, Qt::NoModifier);
    manager->keyPressEvent(draw, &down); manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "list"), "two");
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getContents(), originalContents);
    controller->performSave();
    const QString copyPath = m_temp.filePath("forms-save-as.pdf");
    QVERIFY(annotationSaveAs(controller, &editor, copyPath));
    controller->closeDocument();
    controller->openDocument(copyPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(workflowValue(manager, "name"), "123456789012");
    QCOMPARE(workflowValue(manager, "multiline"), "line1\nline2");
    QCOMPARE(workflowValue(manager, "agree"), "Yes");
    QCOMPARE(workflowValue(manager, "radio"), "b");
    QCOMPARE(workflowValue(manager, "combo"), "two");
    QCOMPARE(workflowValue(manager, "list"), "two");
    QVERIFY(appearance().isStream());
    QTRY_COMPARE(widget->getPageRenderingErrorCount(), 0);
    const QString artifact = qEnvironmentVariable("FAMILYPDF_V6_FIXTURE");
    if (!artifact.isEmpty()) QVERIFY(QFile::copy(copyPath, artifact));
    controller->closeDocument();
}

void ViewerContextMenuTest::formNavigation()
{
    const QString path = m_temp.filePath("navigation.pdf");
    QVERIFY(writeWorkflowForm(path));
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900); editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* manager = widget->getFormManager();
    auto* proxy = widget->getDrawWidgetProxy();
    const auto zoom = proxy->getZoom();
    QVERIFY(manager->focusNextPrevFormField(true));
    QVERIFY(manager->isFocused(workflowField(manager, "name")->getWidgets().front().getWidget()));
    QTest::keyClick(widget, Qt::Key_Tab);
    QVERIFY(manager->isFocused(workflowField(manager, "multiline")->getWidgets().front().getWidget()));
    QTest::keyClick(widget, Qt::Key_Backtab, Qt::ShiftModifier);
    QVERIFY(manager->isFocused(workflowField(manager, "name")->getWidgets().front().getWidget()));
    QTest::keyClick(widget, Qt::Key_Backtab, Qt::ShiftModifier);
    const auto* last = workflowField(manager, "second");
    QVERIFY(manager->isFocused(last->getWidgets().front().getWidget()));
    QCOMPARE(proxy->getZoom(), zoom);
    QTRY_VERIFY([&] { const auto pages = widget->getDrawWidget()->getCurrentPages(); return std::find(pages.begin(), pages.end(), 1) != pages.end(); }());
    manager->setFocusToEditor(nullptr);
    controller->closeDocument();
}

void ViewerContextMenuTest::signaturePresentation()
{
    const QString path = m_temp.filePath("signature.pdf");
    QVERIFY(writePdfFixture(path, 2));
    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    auto document = reader.readFromFile(path);
    pdf::PDFDocumentBuilder builder(&document);
    auto signature = builder.createFormFieldSignature("signature", {}, {});
    builder.createFormFieldWidget(signature, document.getCatalog()->getPage(1)->getPageReference(), QRectF(40, 400, 200, 50), QByteArray());
    builder.appendAcroFormField(signature);
    builder.setFormFieldValue(signature, pdf::PDFObjectFactory::createTextString("malformed"));
    document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(path, &document, true));
    pdfviewer::PDFViewerMainWindow viewer;
    viewer.show();
    auto* controller = viewer.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QVERIFY(!controller->getPdfWidget()->getFormManager());
    auto* tree = viewer.findChild<QTreeWidget*>("signatureTreeWidget");
    QVERIFY(tree);
    pdf::PDFSignatureVerificationResult result(pdf::PDFSignature::Type::Sig, signature, "signature");
    result.setFlag(pdf::PDFSignatureVerificationResult::Signature_OK, true);
    result.addCertificateSelfSignedError();
    controller->setDocument(pdf::PDFModifiedDocument(controller->getDocument(), nullptr), {result}, true);
    QCOMPARE(tree->topLevelItemCount(), 1);
    auto* row = tree->topLevelItem(0);
    QVERIFY(row->text(0).contains("Valid / Untrusted"));
    QVERIFY(!row->text(0).contains("Invalid"));
    Q_EMIT tree->itemClicked(row, 0);
    QTRY_VERIFY([&] { const auto pages = controller->getPdfWidget()->getDrawWidget()->getCurrentPages(); return std::find(pages.begin(), pages.end(), 1) != pages.end(); }());
    result.setFlag(pdf::PDFSignatureVerificationResult::Signature_OK, false);
    result.addSignatureDigestFailureError();
    controller->setDocument(pdf::PDFModifiedDocument(controller->getDocument(), nullptr), {result}, true);
    QVERIFY(tree->topLevelItem(0)->text(0).contains("Invalid / Untrusted"));
    for (const auto flag : {
        pdf::PDFSignatureVerificationResult::Error_Signature_Invalid,
        pdf::PDFSignatureVerificationResult::Error_Signature_SourceCertificateMissing,
        pdf::PDFSignatureVerificationResult::Error_Signature_NoSignaturesFound,
        pdf::PDFSignatureVerificationResult::Error_Signature_DataOther,
        pdf::PDFSignatureVerificationResult::Error_Signature_DataCoveredBySignatureMissing})
    {
        pdf::PDFSignatureVerificationResult failed(pdf::PDFSignature::Type::Sig, signature, "signature");
        failed.setFlag(flag, true);
        controller->setDocument(pdf::PDFModifiedDocument(controller->getDocument(), nullptr), {failed}, true);
        QVERIFY(tree->topLevelItem(0)->text(0).contains("Invalid / Unknown"));
    }
    pdf::PDFSignatureVerificationResult unknown(pdf::PDFSignature::Type::Invalid, signature, "signature");
    unknown.addNoHandlerError("unsupported");
    controller->setDocument(pdf::PDFModifiedDocument(controller->getDocument(), nullptr), {unknown}, true);
    QVERIFY(tree->topLevelItem(0)->text(0).contains("Unknown / Unknown"));
    controller->closeDocument();
    QCOMPARE(tree->topLevelItemCount(), 0);
}

void ViewerContextMenuTest::signatureVerificationWorkflow()
{
    const QString path = QFINDTESTDATA("fixtures/pyhanko-signed.pdf");
    QVERIFY(!path.isEmpty());
    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    auto document = reader.readFromFile(path);
    QVERIFY(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK);
    auto form = pdf::PDFForm::parse(&document, document.getCatalog()->getFormObject());
    pdf::PDFCertificateStore store;
    pdf::PDFSignatureHandler::Parameters parameters;
    parameters.store = &store;
    parameters.useSystemCertificateStore = false;
    parameters.ignoreExpirationDate = true; // Fixture certificates are historical.
    const QByteArray original = reader.getSource();
    auto results = pdf::PDFSignatureHandler::verifySignatures(form, original, parameters);
    QCOMPARE(results.size(), size_t(2));
    for (const auto& result : results)
    {
        QVERIFY(result.isSignatureValid());
        QVERIFY(!result.isCertificateValid());
    }
    QByteArray tampered = original;
    // Alter a comment byte covered by both signatures, preserving PDF structure.
    const int comment = tampered.indexOf('%', 1);
    QVERIFY(comment > 0);
    tampered[comment + 1] = 'X';
    auto invalid = pdf::PDFSignatureHandler::verifySignatures(form, tampered, parameters);
    QCOMPARE(invalid.size(), results.size());
    for (const auto& result : invalid)
        QVERIFY(!result.isSignatureValid());

    pdfviewer::PDFViewerMainWindow viewer;
    viewer.show();
    auto* controller = viewer.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* tree = viewer.findChild<QTreeWidget*>("signatureTreeWidget");
    QCOMPARE(tree->topLevelItemCount(), 2);
    QVERIFY(tree->topLevelItem(0)->text(0).contains("Valid / Untrusted"));
    controller->closeDocument();

    // Malformed /V remains a readable document with a visible unknown/invalid result.
    pdf::PDFDocumentBuilder builder(&document);
    form.apply([&](const pdf::PDFFormField* field) {
        if (field->getFieldType() == pdf::PDFFormField::FieldType::Signature)
            builder.setFormFieldValue(field->getSelfReference(), pdf::PDFObjectFactory::createTextString("malformed"));
    });
    document = builder.build();
    form = pdf::PDFForm::parse(&document, document.getCatalog()->getFormObject());
    results = pdf::PDFSignatureHandler::verifySignatures(form, original, parameters);
    QCOMPARE(results.size(), size_t(2));
    for (const auto& result : results) QVERIFY(!result.isSignatureValid());
    const QString malformed = m_temp.filePath("malformed-signature.pdf");
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(malformed, &document, true));
    pdfviewer::PDFEditorMainWindow editor;
    editor.show();
    auto* edit = editor.getProgramController();
    edit->openDocument(malformed);
    QTRY_VERIFY_WITH_TIMEOUT(edit->getDocument(), 15000);
    QCOMPARE(editor.findChild<QTreeWidget*>("signatureTreeWidget")->topLevelItemCount(), 2);
    edit->closeDocument();
}

void ViewerContextMenuTest::formValidationAndMalformed()
{
    const QString path = m_temp.filePath("form-validation.pdf");
    QVERIFY(writeWorkflowForm(path));
    pdfviewer::PDFEditorMainWindow editor;
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* manager = controller->getPdfWidget()->getFormManager();
    bool prompted = false;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        {
            timer.stop(); prompted = true;
            QVERIFY(box->text().contains("name"));
            box->button(QMessageBox::Save)->click();
        }
    });
    timer.start(10);
    controller->performSave();
    QVERIFY(prompted);
    // Passwords use the existing masked editor and are deliberately not persisted.
    auto* field = workflowField(manager, "password");
    QVERIFY(field->getFlags().testFlag(pdf::PDFFormField::Password));
    manager->setFocusToEditor(manager->getEditor(field));
    auto* draw = controller->getPdfWidget()->getDrawWidget()->getWidget();
    QKeyEvent text(QEvent::KeyPress, 0, Qt::NoModifier, QString(6, QChar('x')));
    manager->keyPressEvent(draw, &text); manager->setFocusToEditor(nullptr);
    QVERIFY(workflowField(manager, "password")->getValue().isNull()
        || workflowValue(manager, "password").isEmpty());
    // Unsupported field type with a widget should be ignored, not asserted.
    pdf::PDFDocumentBuilder builder(controller->getDocument());
    const auto unknown = workflowField(manager, "readonly")->getSelfReference();
    pdf::PDFObjectFactory factory;
    factory.beginDictionary(); factory.beginDictionaryItem("FT"); factory << pdf::WrapName("Unsupported"); factory.endDictionaryItem(); factory.endDictionary();
    builder.mergeTo(unknown, factory.takeObject());
    auto malformed = builder.build();
    controller->closeDocument();
    pdf::PDFDocumentWriter writer(nullptr);
    const QString malformedPath = m_temp.filePath("malformed-form.pdf");
    QVERIFY(writer.write(malformedPath, &malformed, true));
    controller->openDocument(malformedPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QVERIFY(manager->focusNextPrevFormField(true));
    controller->closeDocument();
}

void ViewerContextMenuTest::formAppearanceFallback()
{
    const QString path = m_temp.filePath("appearance-fallback.pdf");
    QVERIFY(writeWorkflowForm(path));
    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    auto document = reader.readFromFile(path);
    const auto form = pdf::PDFForm::parse(&document, document.getCatalog()->getFormObject());
    pdf::PDFDocumentBuilder builder(&document);
    form.apply([&](const pdf::PDFFormField* field) {
        if (field->getName(pdf::PDFFormField::FullyQualified) != "second") return;
        pdf::PDFObjectFactory factory;
        factory.beginDictionary(); factory.beginDictionaryItem("Rect"); factory << QRectF(); factory.endDictionaryItem(); factory.endDictionary();
        builder.mergeTo(field->getSelfReference(), factory.takeObject());
    });
    document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    QVERIFY(writer.write(path, &document, true));
    pdfviewer::PDFEditorMainWindow editor;
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* manager = controller->getPdfWidget()->getFormManager();
    auto* field = workflowField(manager, "name");
    manager->setFocusToEditor(manager->getEditor(field));
    QKeyEvent text(QEvent::KeyPress, 0, Qt::NoModifier, "filled");
    manager->keyPressEvent(controller->getPdfWidget()->getDrawWidget()->getWidget(), &text);
    manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "name"), "filled");
    // A stale AP left behind by a failed generator must not clear the request.
    QVERIFY(pdf::PDFForm::parse(controller->getDocument(), controller->getDocument()->getCatalog()->getFormObject()).isAppearanceUpdateNeeded());
    auto normalAppearance = [&](const QString& name) {
        const auto* doc = controller->getDocument();
        const auto reference = workflowField(manager, name)->getWidgets().front().getWidget();
        const auto* dictionary = doc->getDictionaryFromObject(doc->getObjectByReference(reference));
        const auto* ap = doc->getDictionaryFromObject(dictionary->get("AP"));
        return ap ? ap->get("N") : pdf::PDFObject();
    };
    const auto unchangedAppearance = normalAppearance("multiline");
    QVERIFY(unchangedAppearance.isReference());
    for (const QString& value : {QString("next"), QString("last")})
    {
        // Compare allocation against the existing single-field setter on an
        // independent form model: the global refresh must add no second AP.
        pdf::PDFWidgetFormManager baseline(controller->getPdfWidget()->getDrawWidgetProxy(), nullptr);
        baseline.setAnnotationManager(manager->getAnnotationManager());
        baseline.setAppearanceFlags(manager->getAppearanceFlags());
        baseline.setDocument(pdf::PDFModifiedDocument(controller->getDocument(), nullptr, pdf::PDFModifiedDocument::Reset));
        auto* baselineField = baseline.getFormFieldForWidget(workflowField(manager, "name")->getWidgets().front().getWidget());
        QVERIFY(baselineField);
        pdf::PDFDocumentModifier baselineModifier(controller->getDocument());
        baselineModifier.getBuilder()->setFormManager(&baseline);
        pdf::PDFFormField::SetValueParameters parameters;
        parameters.formManager = &baseline;
        parameters.modifier = &baselineModifier;
        parameters.invokingFormField = baselineField;
        parameters.invokingWidget = baselineField->getWidgets().front().getWidget();
        parameters.scope = pdf::PDFFormField::SetValueParameters::Scope::User;
        parameters.value = pdf::PDFObjectFactory::createTextString(value);
        QVERIFY(baselineField->setValue(parameters));
        const auto expectedObjectCount = baselineModifier.getBuilder()->getStorage()->getObjects().size();
        manager->setFocusToEditor(manager->getEditor(workflowField(manager, "name")));
        QKeyEvent input(QEvent::KeyPress, 0, Qt::NoModifier, value);
        manager->keyPressEvent(controller->getPdfWidget()->getDrawWidget()->getWidget(), &input);
        manager->setFocusToEditor(nullptr);
        QCOMPARE(workflowValue(manager, "name"), value);
        QCOMPARE(controller->getDocument()->getStorage().getObjects().size(), expectedObjectCount);
        // No new streams/resources for unrelated widgets on each failed retry.
        QCOMPARE(normalAppearance("multiline"), unchangedAppearance);
        editor.findChild<QAction*>("actionUndo")->trigger();
        editor.findChild<QAction*>("actionRedo")->trigger();
        QCOMPARE(normalAppearance("multiline"), unchangedAppearance);
    }
    controller->performSave();
    controller->closeDocument();
}

QTEST_MAIN(ViewerContextMenuTest)
#include "tst_viewercontextmenutest.moc"
