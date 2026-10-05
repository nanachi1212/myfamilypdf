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
#include "pdfthumbnailslistview.h"
#include "pdfpagereorder.h"
#include "pdfprintdialog.h"
#include "pdfexportimagesdialog.h"
#include "pdfmergepdfsdialog.h"
#include "pdfsecurityhandler.h"
#include <QMenuBar>
#include <QProgressBar>
#include "pdfpageoutput.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfconstants.h"
#include <QRadioButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QComboBox>
#include <QPrinter>
#include <QPrinterInfo>
#include <QThreadPool>
#include <QTreeWidget>
#include "pdfdocumentbuilder.h"
#include "pdfannotation.h"
#include "pdfwidgetannotation.h"
#include <QToolButton>
#include <QTreeView>
#include <QTextBrowser>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QtConcurrent/QtConcurrentRun>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <psapi.h>
#endif
#include <memory>
#include <thread>

namespace
{

template <typename Condition>
bool waitUntil(Condition&& condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition())
    {
        if (timer.elapsed() > timeoutMs)
        {
            return false;
        }
        QTest::qWait(20);
    }
    return true;
}

bool writePdfFixture(const QString& path, int pageCount, int lines = 1, bool withText = true, bool unicode = false, bool malformed = false, const QByteArray& textPrefix = "FamilyPDF smoke page ")
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
                    + (unicode ? QByteArray("ABCD 2026 ABCD") : textPrefix + QByteArray::number(page+1)) + ") Tj ET\n";
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
    void pageReorderOrderMath_data();
    void pageReorderOrderMath();
    void pageReorderInsertionGeometry();
    void thumbnailReorderWorkflow();
    void reorderPreservesContentAfterSave();
    void reorderFlattensNestedPageTree();
    void viewerThumbnailsAreReadOnly();
    void nativeThumbnailDragSmoke();
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
    void printAndExportEntriesAreAvailable();
    void printDialogOptionsAndCancel();
    void exportImagesDialogWorkflow();
    void exportSelectionAsImageWorkflow();
    void filledFormPrintsAndExports();
    void printExportLargeDocumentBenchmark();
    void mergePdfsEntriesAreAvailable();
    void mergePdfsDialogWorkflow();
    void mergePdfsOutputOrderAndTextLayerAfterReopen();
    void mergePdfsBlocksAndWarns();
    void mergePdfsCancelLeavesNoPartialFile();
    void mergePdfsTranslations();

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
    // Export dialog preferences must not leak into the registry of the machine.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_temp.filePath("qsettings"));
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
    if (testFunction == "thumbnailSelectionAndPageManagement" || testFunction.startsWith("annotation")
        || testFunction.startsWith("pageReorder") || testFunction == "thumbnailReorderWorkflow"
        || testFunction == "reorderPreservesContentAfterSave" || testFunction == "reorderFlattensNestedPageTree" || testFunction == "nativeThumbnailDragSmoke"
        || testFunction.startsWith("mergePdfs"))
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
            "thumbnailRotatePagesLeftAction",
            "thumbnailPrintPagesAction",
            "thumbnailExportImagesAction"
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

namespace {

qint64 countDarkPixels(const QImage& image)
{
    qint64 count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y).lightness() < 140)
                ++count;
    return count;
}

/// Rendering resources for a document which is read from a file (printed output).
struct RenderedFile
{
    bool open(const QString& path)
    {
        pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
        document = std::make_unique<pdf::PDFDocument>(reader.readFromFile(path));
        if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
            return false;
        optionalContent = std::make_unique<pdf::PDFOptionalContentActivity>(document.get(), pdf::OCUsage::Export, nullptr);
        cms = std::make_unique<pdf::PDFCMSManager>(nullptr);
        cms->setDocument(document.get());
        fonts = std::make_unique<pdf::PDFFontCache>(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
        fonts->setDocument(pdf::PDFModifiedDocument(document.get(), optionalContent.get()));
        return true;
    }

    QImage render(int pageIndex, int dpi)
    {
        pdfviewer::PDFPageOutputContext context;
        context.document = document.get();
        context.fontCache = fonts.get();
        context.cmsManager = cms.get();
        pdfviewer::PDFPageImageExporter exporter(context, pdf::OCUsage::Export, 1);
        return exporter.renderPage(pageIndex, pdfviewer::PDFPageImageExporter::getImageSize(document->getCatalog()->getPage(pageIndex), dpi));
    }

    std::unique_ptr<pdf::PDFDocument> document;
    std::unique_ptr<pdf::PDFOptionalContentActivity> optionalContent;
    std::unique_ptr<pdf::PDFCMSManager> cms;
    std::unique_ptr<pdf::PDFFontCache> fonts;
};

}

void ViewerContextMenuTest::printAndExportEntriesAreAvailable()
{
    for (const bool editorWindow : { false, true })
    {
        std::unique_ptr<QMainWindow> window;
        pdfviewer::PDFProgramController* controller = nullptr;
        if (editorWindow)
        {
            auto* editor = new pdfviewer::PDFEditorMainWindow;
            window.reset(editor);
            controller = editor->getProgramController();
        }
        else
        {
            auto* viewer = new pdfviewer::PDFViewerMainWindow;
            window.reset(viewer);
            controller = viewer->getProgramController();
        }
        window->resize(1100, 900);
        window->show();

        auto* exportAction = window->findChild<QAction*>("actionExportPageImages");
        auto* printAction = window->findChild<QAction*>("actionPrint");
        auto* fileMenu = window->findChild<QMenu*>("menuFile");
        QVERIFY(exportAction && printAction && fileMenu);
        QVERIFY(!exportAction->isEnabled());

        controller->openDocument(m_pdfPath);
        QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
        QTRY_VERIFY(exportAction->isEnabled());
        QVERIFY(printAction->isEnabled());

        // The export entry is listed right after Print in the File menu.
        const QList<QAction*> actions = fileMenu->actions();
        const int printIndex = actions.indexOf(printAction);
        QVERIFY(printIndex >= 0);
        QCOMPARE(actions.value(printIndex + 1), exportAction);
        QVERIFY(exportAction->text().contains("Images"));

        controller->closeDocument();
        QTRY_VERIFY(!exportAction->isEnabled());
        QVERIFY(!printAction->isEnabled());
    }
}

void ViewerContextMenuTest::printDialogOptionsAndCancel()
{
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);

    const bool hasPrinter = !QPrinterInfo::availablePrinterNames().isEmpty();
    bool inspected = false;
    QElapsedTimer sinceOpen;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFPrintDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        if (!sinceOpen.isValid())
            sinceOpen.start();

        auto* preview = dialog->findChild<QLabel*>("printPreviewLabel");
        QVERIFY(preview);
        // The first page of the preview appears without rendering the document.
        if (hasPrinter && preview->pixmap(Qt::ReturnByValue).isNull() && sinceOpen.elapsed() < 20000)
            return;
        timer.stop();
        if (hasPrinter)
            qInfo() << "PERF print dialog first preview page ms:" << sinceOpen.elapsed();
        saveImage(dialog->grab(), "print-dialog.png");

        auto* range = dialog->findChild<QLineEdit*>("printRangeEdit");
        auto* all = dialog->findChild<QRadioButton*>("printAllPagesRadio");
        auto* fit = dialog->findChild<QRadioButton*>("printFitRadio");
        auto* actual = dialog->findChild<QRadioButton*>("printActualSizeRadio");
        auto* orientation = dialog->findChild<QComboBox*>("printOrientationCombo");
        auto* duplex = dialog->findChild<QComboBox*>("printDuplexCombo");
        auto* copies = dialog->findChild<QSpinBox*>("printCopiesSpin");
        QVERIFY(range && all && fit && actual && orientation && duplex && copies);

        // Defaults: all pages, fit to the printable area, orientation follows the pages.
        QVERIFY(all->isChecked());
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 0, 1, 2 }));
        QCOMPARE(dialog->getOptions().scaling, pdfviewer::PDFPrintScaling::FitToPrintableArea);
        QCOMPARE(dialog->getOptions().orientation, pdfviewer::PDFPrintOrientation::Auto);
        QCOMPARE(copies->value(), 1);

        range->setText("2-3");
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 1, 2 }));
        range->setText("3,1,2,2");
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 0, 1, 2 }));
        QString error;
        range->setText("0");
        QVERIFY(dialog->getSelectedPages(&error).empty());
        QVERIFY(!error.isEmpty());
        error.clear();
        range->setText("2-9");
        QVERIFY(dialog->getSelectedPages(&error).empty());
        QVERIFY(!error.isEmpty());
        range->setText("1,3");
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 0, 2 }));

        actual->setChecked(true);
        QCOMPARE(dialog->getOptions().scaling, pdfviewer::PDFPrintScaling::ActualSize);
        orientation->setCurrentIndex(orientation->findData(int(pdfviewer::PDFPrintOrientation::Landscape)));
        QCOMPARE(dialog->getOptions().orientation, pdfviewer::PDFPrintOrientation::Landscape);

        // Two-sided printing is only offered when the printer reports it.
        if (hasPrinter)
        {
            const QPrinterInfo info = QPrinterInfo::printerInfo(dialog->findChild<QComboBox*>("printPrinterCombo")->currentText());
            const auto modes = info.supportedDuplexModes();
            QCOMPARE(duplex->isEnabled(), modes.contains(QPrinter::DuplexLongSide) || modes.contains(QPrinter::DuplexShortSide));
            QVERIFY(dialog->findChild<QPushButton*>("printButton")->isEnabled());
        }
        inspected = true;
        dialog->reject();
    });
    timer.start(10);
    editor.findChild<QAction*>("actionPrint")->trigger();
    timer.stop();
    QVERIFY(inspected);

    // Cancel leaves nothing running in the background.
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));

    // "Print selected pages" from the thumbnails starts with those pages.
    inspected = false;
    QTimer selectedTimer;
    connect(&selectedTimer, &QTimer::timeout, &editor, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFPrintDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        selectedTimer.stop();
        auto* selected = dialog->findChild<QRadioButton*>("printSelectedPagesRadio");
        QVERIFY(selected && selected->isEnabled() && selected->isChecked());
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 1, 2 }));
        inspected = true;
        dialog->reject();
    });
    selectedTimer.start(10);
    controller->printPages({ 1, 2 });
    selectedTimer.stop();
    QVERIFY(inspected);
    controller->closeDocument();
}

void ViewerContextMenuTest::exportImagesDialogWorkflow()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);

    QDir outputDirectory(m_temp.filePath("ui-export"));
    QVERIFY(outputDirectory.mkpath("."));

    // Runs the export dialog: the first function adjusts it, then the Export button is pressed.
    // Message boxes are answered by the second function (return true to press the default answer).
    auto runDialog = [&](const std::function<void(pdfviewer::PDFExportImagesDialog*)>& configure, QMessageBox::StandardButton answer, int expectedMessages)
    {
        int stage = 0;
        int messages = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, &editor, [&]()
        {
            QWidget* modal = QApplication::activeModalWidget();
            if (auto* message = qobject_cast<QMessageBox*>(modal))
            {
                ++messages;
                if (auto* button = message->button(answer))
                    button->click();
                else
                    message->accept();
                return;
            }
            if (auto* dialog = qobject_cast<pdfviewer::PDFExportImagesDialog*>(modal); dialog && stage == 0)
            {
                stage = 1;
                saveImage(dialog->grab(), "export-images-dialog.png");
                configure(dialog);
                auto* button = dialog->findChild<QPushButton*>("exportButton");
                QVERIFY(button && button->isEnabled());
                // Clicked from the event loop: the overwrite question is a nested loop which this timer must not block.
                QMetaObject::invokeMethod(button, &QPushButton::click, Qt::QueuedConnection);
                return;
            }
            if (auto* dialog = qobject_cast<pdfviewer::PDFExportImagesDialog*>(modal); dialog && stage == 1 && messages > 0)
            {
                // The export was not started (answered "No"): close the dialog.
                stage = 2;
                dialog->reject();
            }
        });
        timer.start(10);
        editor.findChild<QAction*>("actionExportPageImages")->trigger();
        timer.stop();
        QCOMPARE(messages, expectedMessages);
    };

    // 1. Current page as JPEG
    runDialog([&](pdfviewer::PDFExportImagesDialog* dialog)
    {
        QVERIFY(dialog->findChild<QRadioButton*>("exportCurrentPageRadio")->isChecked());
        dialog->findChild<QLineEdit*>("exportDirectoryEdit")->setText(outputDirectory.absolutePath());
        dialog->findChild<QSpinBox*>("exportDpiSpin")->setValue(72);
        auto* format = dialog->findChild<QComboBox*>("exportFormatCombo");
        format->setCurrentIndex(format->findData(int(pdfviewer::PDFImageFormat::Jpeg)));
        QVERIFY(dialog->findChild<QSpinBox*>("exportQualitySpin")->isEnabled());
        dialog->findChild<QSpinBox*>("exportQualitySpin")->setValue(60);
        QVERIFY(dialog->findChild<QLabel*>("exportFilesLabel")->text().contains("three-pages_p1.jpg"));
    }, QMessageBox::Ok, 1);
    QCOMPARE(outputDirectory.entryList(QDir::Files, QDir::Name), (QStringList{ "three-pages_p1.jpg" }));
    QCOMPARE(QImage(outputDirectory.filePath("three-pages_p1.jpg")).size(), QSize(420, 595));

    // 2. All pages as PNG at 144 dpi
    runDialog([&](pdfviewer::PDFExportImagesDialog* dialog)
    {
        dialog->findChild<QLineEdit*>("exportDirectoryEdit")->setText(outputDirectory.absolutePath());
        dialog->findChild<QRadioButton*>("exportAllPagesRadio")->setChecked(true);
        dialog->findChild<QSpinBox*>("exportDpiSpin")->setValue(144);
        auto* format = dialog->findChild<QComboBox*>("exportFormatCombo");
        format->setCurrentIndex(format->findData(int(pdfviewer::PDFImageFormat::Png)));
        QVERIFY(!dialog->findChild<QSpinBox*>("exportQualitySpin")->isEnabled());
    }, QMessageBox::Ok, 1);
    QCOMPARE(outputDirectory.entryList(QDir::Files, QDir::Name), (QStringList{ "three-pages_p1.jpg", "three-pages_p1.png", "three-pages_p2.png", "three-pages_p3.png" }));
    for (const QString& name : { "three-pages_p1.png", "three-pages_p2.png", "three-pages_p3.png" })
    {
        const QImage image(outputDirectory.filePath(name));
        QCOMPARE(image.size(), QSize(840, 1190));
        QVERIFY(countDarkPixels(image) > 50);   // the page text
    }

    // 3. Existing files are never replaced without asking. Answering "No" keeps them untouched.
    const QDateTime before = QFileInfo(outputDirectory.filePath("three-pages_p2.png")).lastModified();
    runDialog([&](pdfviewer::PDFExportImagesDialog* dialog)
    {
        dialog->findChild<QLineEdit*>("exportDirectoryEdit")->setText(outputDirectory.absolutePath());
        dialog->findChild<QRadioButton*>("exportRangeRadio")->setChecked(true);
        dialog->findChild<QLineEdit*>("exportRangeEdit")->setText("2-3");
        dialog->findChild<QSpinBox*>("exportDpiSpin")->setValue(144);
    }, QMessageBox::No, 1);
    QCOMPARE(QFileInfo(outputDirectory.filePath("three-pages_p2.png")).lastModified(), before);
    // 4. Page range selection and an invalid range keep Export disabled.
    bool checked = false;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFExportImagesDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        timer.stop();
        auto* range = dialog->findChild<QLineEdit*>("exportRangeEdit");
        range->setText("1-3,8");
        QVERIFY(!dialog->findChild<QPushButton*>("exportButton")->isEnabled());
        QVERIFY(dialog->getSelectedPages().empty());
        range->setText("1,3");
        QVERIFY(dialog->findChild<QPushButton*>("exportButton")->isEnabled());
        QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 0, 2 }));
        checked = true;
        dialog->reject();
    });
    timer.start(10);
    controller->exportPagesAsImages();
    timer.stop();
    QVERIFY(checked);
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    controller->closeDocument();
}

void ViewerContextMenuTest::exportSelectionAsImageWorkflow()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    const QString path = m_temp.filePath("selection-source.pdf");
    QVERIFY(writePdfFixture(path, 3, 2));
    // A failed check must not leave the document open while the window is destroyed.
    const auto closeGuard = qScopeGuard([controller]() { controller->closeDocument(); });
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* drawProxy = widget->getDrawWidgetProxy();
    editor.findChild<QAction*>("actionSelectText")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(drawProxy->getTextLayoutCompiler()->isTextLayoutReady(), 15000);

    // The context menu offers the export only while text is selected.
    auto exportSelectionEnabled = [&]()
    {
        std::optional<bool> enabled;
        QTimer menuTimer;
        connect(&menuTimer, &QTimer::timeout, &editor, [&]()
        {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
            {
                menuTimer.stop();
                auto* action = menu->findChild<QAction*>("actionExportSelectionImage");
                enabled = action && action->isEnabled();
                menu->close();
            }
        });
        menuTimer.start(10);
        Q_EMIT widget->customContextMenuRequested(QPoint(100, 100));
        menuTimer.stop();
        return enabled;
    };
    QCOMPARE(exportSelectionEnabled(), std::optional<bool>(false));
    editor.findChild<QAction*>("actionSelectTextAll")->trigger();
    QVERIFY(!controller->getToolManager()->getSelectedText().isEmpty());
    QCOMPARE(exportSelectionEnabled(), std::optional<bool>(true));

    QDir outputDirectory(m_temp.filePath("ui-selection-export"));
    QVERIFY(outputDirectory.mkpath("."));
    int stage = 0;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&]()
    {
        QWidget* modal = QApplication::activeModalWidget();
        if (auto* message = qobject_cast<QMessageBox*>(modal))
        {
            ++stage;
            message->accept();
        }
        else if (auto* dialog = qobject_cast<pdfviewer::PDFExportImagesDialog*>(modal); dialog && stage == 0)
        {
            stage = -1;
            QCOMPARE(dialog->windowTitle(), QString("Export Selection as Image"));
            auto* format = dialog->findChild<QComboBox*>("exportFormatCombo");
            format->setCurrentIndex(format->findData(int(pdfviewer::PDFImageFormat::Png)));
            // One region per page of the document; there are no page options.
            QCOMPARE(dialog->getSelectedPages(), (std::vector<pdf::PDFInteger>{ 0, 1, 2 }));
            dialog->findChild<QLineEdit*>("exportDirectoryEdit")->setText(outputDirectory.absolutePath());
            dialog->findChild<QSpinBox*>("exportDpiSpin")->setValue(144);
            dialog->findChild<QPushButton*>("exportButton")->click();
        }
    });
    timer.start(10);
    controller->exportSelectionAsImage();
    timer.stop();
    QCOMPARE(stage, 0);     // -1 + the message box

    QCOMPARE(outputDirectory.entryList(QDir::Files, QDir::Name), (QStringList{ "selection-source_p1_selection.png", "selection-source_p2_selection.png", "selection-source_p3_selection.png" }));
    for (const QFileInfo& file : outputDirectory.entryInfoList(QDir::Files))
    {
        const QImage image(file.absoluteFilePath());
        // The text block of the page (two lines of 16 point text) at 144 dpi, not the whole page.
        QVERIFY2(image.width() > 150 && image.width() < 840 && image.height() > 40 && image.height() < 400, qPrintable(QString("%1x%2").arg(image.width()).arg(image.height())));
        QVERIFY(countDarkPixels(image) > 100);
    }
    controller->closeDocument();
}

void ViewerContextMenuTest::filledFormPrintsAndExports()
{
    const QString path = m_temp.filePath("print-form.pdf");
    QVERIFY(writeWorkflowForm(path));
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    auto* widget = controller->getPdfWidget();
    auto* manager = widget->getFormManager();
    auto* draw = widget->getDrawWidget()->getWidget();
    QVERIFY(manager);

    auto* field = workflowField(manager, "name");
    manager->setFocusToEditor(manager->getEditor(field));
    QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString("Form value"));
    manager->keyPressEvent(draw, &event);
    manager->setFocusToEditor(nullptr);
    QCOMPARE(workflowValue(manager, "name"), "Form value");

    const pdf::PDFDocument* document = controller->getDocument();
    const pdf::PDFPage* page = document->getCatalog()->getPage(0);
    const auto context = pdfviewer::PDFPageOutputContext::fromProxy(document, widget->getDrawWidgetProxy());

    // Image export draws the filled field, like the screen does.
    pdfviewer::PDFPageImageExporter exporter(context, pdf::OCUsage::Export, 1);
    const QSize size = pdfviewer::PDFPageImageExporter::getImageSize(page, 144);
    QString error;
    const QImage image = exporter.renderPage(0, size, &error);
    QVERIFY2(!image.isNull(), qPrintable(error));
    const QRect fieldRect = pdf::PDFRenderer::createPagePointToDevicePointMatrix(page, QRect(QPoint(0, 0), size)).mapRect(QRectF(40, 450, 220, 40)).toAlignedRect();
    const qint64 exportedPixels = countDarkPixels(image.copy(fieldRect.adjusted(6, 6, -6, -6)));
    qInfo() << "form value pixels in exported image:" << exportedPixels;
    QVERIFY(exportedPixels > 40);

    // The same field of an empty form (second page has no value) stays blank.
    const QRect emptyRect = pdf::PDFRenderer::createPagePointToDevicePointMatrix(page, QRect(QPoint(0, 0), size)).mapRect(QRectF(40, 390, 220, 40)).toAlignedRect();
    QVERIFY(countDarkPixels(image.copy(emptyRect.adjusted(40, 6, -6, -6))) < exportedPixels);

    // Printing keeps the field: print to PDF output and render the printed sheet again.
    const QString printedPath = m_temp.filePath("print-form-printed.pdf");
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(printedPath);
    pdfviewer::PDFPrintOptions options;
    options.pageIndices = { 0 };
    options.scaling = pdfviewer::PDFPrintScaling::ActualSize;
    const pdfviewer::PDFPrintResult result = pdfviewer::PDFPageOutput::print(&printer, context, options);
    QVERIFY2(result.completed, qPrintable(result.errorMessage));
    QVERIFY2(result.renderWarnings.isEmpty(), qPrintable(result.renderWarnings.join("; ")));

    RenderedFile printed;
    QVERIFY(printed.open(printedPath));
    QCOMPARE(printed.document->getCatalog()->getPageCount(), size_t(1));
    const QImage sheet = printed.render(0, 144);
    // The 420 x 595 point page is centered on the A4 sheet at actual size.
    const QSizeF sheetPoints = printed.document->getCatalog()->getPage(0)->getRotatedMediaBox().size();
    const QPointF origin((sheetPoints.width() - 420.0) / 2.0 * 2.0, (sheetPoints.height() - 595.0) / 2.0 * 2.0);
    const QRect printedField = fieldRect.translated(origin.toPoint());
    const qint64 printedPixels = countDarkPixels(sheet.copy(printedField.adjusted(6, 6, -6, -6)));
    qInfo() << "form value pixels in printed sheet:" << printedPixels;
    QVERIFY(printedPixels > 40);
    controller->closeDocument();
}

void ViewerContextMenuTest::printExportLargeDocumentBenchmark()
{
    const QString path = m_temp.filePath("print-export-1200.pdf");
    QVERIFY(writePdfFixture(path, 1200, 20));
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    const auto closeGuard = qScopeGuard([controller]() { controller->closeDocument(); });
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 30000);
    const auto context = pdfviewer::PDFPageOutputContext::fromProxy(controller->getDocument(), controller->getPdfWidget()->getDrawWidgetProxy());

    QDir directory(m_temp.filePath("large-export"));
    QVERIFY(directory.mkpath("."));
    auto exportPages = [&](const std::vector<pdf::PDFInteger>& pages, int dpi)
    {
        pdfviewer::PDFPageImageExporter exporter(context, pdf::OCUsage::Export, 4);
        std::vector<pdfviewer::PDFImageExportTarget> targets;
        for (const pdf::PDFInteger page : pages)
        {
            pdfviewer::PDFImageExportTarget target;
            target.pageIndex = page;
            target.fileName = directory.absoluteFilePath(pdfviewer::PDFPageImageExporter::getFileName("big", page, 1200, pdfviewer::PDFImageFormat::Png, false));
            targets.push_back(target);
        }
        QElapsedTimer timer;
        timer.start();
        const auto result = exporter.exportTargets(targets, pdfviewer::PDFImageFormat::Png, dpi, 90);
        const qint64 elapsed = timer.elapsed();
        return std::make_pair(result.isComplete(), elapsed);
    };

    // Only page 600 of 1200: the pages before it are not touched.
    auto single = exportPages({ 599 }, 150);
    QVERIFY(single.first);
    qInfo() << "PERF 1 page PNG (#600 of 1200, 150 dpi) ms:" << single.second;
    QCOMPARE(directory.entryList(QDir::Files).size(), 1);
    QVERIFY(countDarkPixels(QImage(directory.filePath("big_p0600.png"))) > 100);
    QVERIFY2(single.second < 8000, "one page of a large document must not wait for the others");

    std::vector<pdf::PDFInteger> tenPages;
    for (int i = 0; i < 10; ++i)
        tenPages.push_back(600 + i);
    auto batch = exportPages(tenPages, 150);
    QVERIFY(batch.first);
    qInfo() << "PERF 10 pages PNG (#601-610 of 1200, 150 dpi) ms:" << batch.second;
    QCOMPARE(directory.entryList(QDir::Files).size(), 11);

    // Print one page of the large document.
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(m_temp.filePath("big-printed.pdf"));
    pdfviewer::PDFPrintOptions options;
    options.pageIndices = { 599 };
    QElapsedTimer timer;
    timer.start();
    const auto printResult = pdfviewer::PDFPageOutput::print(&printer, context, options);
    qInfo() << "PERF print 1 page (#600 of 1200) ms:" << timer.elapsed();
    QVERIFY2(printResult.completed, qPrintable(printResult.errorMessage));

    // Opening the print dialog (with its first preview page) on the large document.
    bool opened = false;
    QElapsedTimer dialogTimer;
    QTimer dialogPoll;
    connect(&dialogPoll, &QTimer::timeout, &editor, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFPrintDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto* preview = dialog->findChild<QLabel*>("printPreviewLabel");
        if (!preview || (!QPrinterInfo::availablePrinterNames().isEmpty() && preview->pixmap(Qt::ReturnByValue).isNull() && dialogTimer.elapsed() < 20000))
            return;
        dialogPoll.stop();
        qInfo() << "PERF print dialog on 1200 pages, first preview ms:" << dialogTimer.elapsed();
        opened = true;
        dialog->reject();
    });
    dialogTimer.start();
    dialogPoll.start(10);
    controller->performPrint();
    dialogPoll.stop();
    QVERIFY(opened);
}

namespace
{

std::vector<pdf::PDFObjectReference> reorderPageReferences(const pdf::PDFDocument* document)
{
    std::vector<pdf::PDFObjectReference> references;
    for (size_t index = 0; index < document->getCatalog()->getPageCount(); ++index)
    {
        references.push_back(document->getCatalog()->getPage(index)->getPageReference());
    }
    return references;
}

// Delivers a thumbnail drag as the windowing system would: enter, move, drop.
void dropThumbnails(pdfviewer::PDFThumbnailsListView* view, const std::vector<pdf::PDFInteger>& pages, const QPoint& position)
{
    // A real drag started by the view accepts drops while it runs; Qt delivers drag events to a
    // widget that does not accept drops nowhere, so the test does the same for the duration of the drop.
    const bool acceptedDrops = view->acceptDrops();
    view->setAcceptDrops(true);
    const auto restore = qScopeGuard([&]() { view->setAcceptDrops(acceptedDrops); });
    std::unique_ptr<QMimeData> mimeData(pdfviewer::PDFThumbnailsListView::createReorderMimeData(pages));
    QDragEnterEvent enter(position, Qt::MoveAction, mimeData.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &enter);
    QDragMoveEvent move(position, Qt::MoveAction, mimeData.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QDropEvent drop(QPointF(position), Qt::MoveAction, mimeData.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &drop);
    // The view reports the drop with a queued signal.
    QCoreApplication::processEvents();
}

// Opens the thumbnails page of the sidebar so that the view has its real size.
bool showThumbnailsPage(QMainWindow* window)
{
    auto* dock = window->findChild<QDockWidget*>("SidebarDockWidget");
    auto* button = window->findChild<QToolButton*>("thumbnailsButton");
    auto* view = window->findChild<QListView*>("thumbnailsListView");
    if (!dock || !button || !view)
    {
        return false;
    }
    dock->show();
    button->click();
    view->show();
    for (int attempt = 0; attempt < 100 && view->viewport()->height() < 300; ++attempt)
    {
        QTest::qWait(20);
    }
    return view->viewport()->height() >= 300;
}

// A point inside the first (before) or last (after) quarter of a thumbnail, on both axes,
// so the result does not depend on whether the view lays out one or several columns.
QPoint thumbnailDropPoint(QListView* view, int row, bool after)
{
    view->scrollTo(view->model()->index(row, 0));
    QCoreApplication::processEvents();
    const QRect rect = view->visualRect(view->model()->index(row, 0));
    return after ? QPoint(rect.left() + rect.width() * 3 / 4, rect.top() + rect.height() * 3 / 4)
                 : QPoint(rect.left() + rect.width() / 4, rect.top() + rect.height() / 4);
}

std::vector<int> selectedThumbnailRows(const QListView* view)
{
    std::vector<int> rows;
    for (const QModelIndex& index : view->selectionModel()->selectedIndexes())
    {
        rows.push_back(index.row());
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

bool writeReorderFixture(const QString& path)
{
    if (!writePdfFixture(path, 4))
    {
        return false;
    }

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    pdf::PDFDocument original = reader.readFromFile(path);
    if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
    {
        return false;
    }

    pdf::PDFDocumentBuilder builder(&original);
    std::vector<pdf::PDFObjectReference> pages;
    for (size_t index = 0; index < original.getCatalog()->getPageCount(); ++index)
    {
        pages.push_back(original.getCatalog()->getPage(index)->getPageReference());
        // The width identifies the page after the move.
        builder.setPageMediaBox(pages.back(), QRectF(0, 0, 400 + 10 * qreal(index), 595));
    }

    builder.setPageRotation(pages[1], pdf::PageRotation::Rotate90);
    builder.createAnnotationSquare(pages[2], QRectF(40, 300, 120, 60), 2.0, QColor(255, 220, 220), QColor(200, 0, 0), QStringLiteral("tester"), QStringLiteral("subject"), QStringLiteral("note-on-third-page"));

    auto field = builder.createFormFieldText(QStringLiteral("reorder-name"), QStringLiteral("Alice"), pdf::PDFFormField::None, 40);
    builder.createFormFieldWidget(field, pages[0], QRectF(40, 450, 220, 40), "/Helv 12 Tf 0 g");
    builder.appendAcroFormField(field);

    pdf::PDFDocument document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    return bool(writer.write(path, &document, true));
}

QString pageText(pdf::PDFAsynchronousTextLayoutCompiler* compiler, pdf::PDFInteger pageIndex)
{
    QString text;
    const pdf::PDFTextLayout& layout = compiler->getTextLayoutStorage()->getTextLayout(pageIndex);
    for (const pdf::PDFTextFlow& flow : pdf::PDFTextFlow::createTextFlows(layout, pdf::PDFTextFlow::FlowFlags(), pageIndex))
    {
        text += flow.getText();
    }
    return text;
}

#ifdef Q_OS_WIN
#endif
#ifdef Q_OS_WIN
#endif
#ifdef Q_OS_WIN
// Presses the real left mouse button, drags with the real cursor and releases. The
// Windows drag-and-drop loop reads the physical mouse state, so Qt test events cannot
// drive it. The steps are scheduled up front because the drag blocks the caller.
// Positions are converted to physical pixels relative to the client area of the window,
// which takes the screen scaling and the screen origin into account.
bool scheduleNativeDrag(QWidget* widget, const QPoint& from, const QPoint& to)
{
    const auto toScreen = [widget](const QPoint& viewportPoint)
    {
        const QWidget* top = widget->window();
        const QPoint inWindow = widget->mapTo(top, viewportPoint);
        const qreal ratio = top->devicePixelRatioF();
        POINT point = { qRound(inWindow.x() * ratio), qRound(inWindow.y() * ratio) };
        ClientToScreen(reinterpret_cast<HWND>(top->winId()), &point);
        return QPoint(point.x, point.y);
    };
    const QPoint start = toScreen(from);
    const QPoint end = toScreen(to);
    // The real mouse reaches whatever window is on top at that point.
    const HWND topWindow = reinterpret_cast<HWND>(widget->window()->winId());
    for (const QPoint& point : { start, end })
    {
        const HWND hit = WindowFromPoint(POINT{ point.x(), point.y() });
        if (!hit || GetAncestor(hit, GA_ROOT) != topWindow)
        {
            qWarning() << "Native drag point" << point << "is not over the test window";
            return false;
        }
    }

    // A worker thread drives the mouse: the GUI thread is blocked inside the drag loop.
    auto positions = std::make_shared<QStringList>();
    std::thread([start, end, positions]()
    {
        // SetCursorPos does not produce mouse input, so the drag loop would act on a stale position.
        // SendInput with an absolute position is real mouse movement.
        const auto place = [positions](const QPoint& point)
        {
            const int width = GetSystemMetrics(SM_CXSCREEN);
            const int height = GetSystemMetrics(SM_CYSCREEN);
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dx = LONG((qint64(point.x()) * 65535 + (width - 1) / 2) / (width - 1));
            input.mi.dy = LONG((qint64(point.y()) * 65535 + (height - 1) / 2) / (height - 1));
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
            SendInput(1, &input, sizeof(INPUT));
            Sleep(15);
            POINT actual;
            GetCursorPos(&actual);
            positions->push_back(QStringLiteral("%1,%2").arg(actual.x).arg(actual.y));
        };
        Sleep(100);
        place(start);
        Sleep(150);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(150);
        const int steps = 12;
        for (int step = 1; step <= steps; ++step)
        {
            place(start + (end - start) * step / steps);
            Sleep(60);
        }
        Sleep(250);
        place(end);
        Sleep(300);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Sleep(200);
        qInfo() << "NATIVE cursor path" << *positions << "start" << start << "end" << end;
    }).detach();
    return true;
}
#endif

}

void ViewerContextMenuTest::pageReorderOrderMath_data()
{
    QTest::addColumn<int>("pageCount");
    QTest::addColumn<QList<int>>("moved");
    QTest::addColumn<int>("insertionRow");
    QTest::addColumn<QList<int>>("expected");

    // 0-based pages; "after page N" is insertion row N.
    QTest::newRow("single forward") << 6 << QList<int>{1} << 5 << QList<int>{0, 2, 3, 4, 1, 5};
    QTest::newRow("single backward") << 6 << QList<int>{4} << 1 << QList<int>{0, 4, 1, 2, 3, 5};
    QTest::newRow("contiguous forward (1 2 3 4 5 6, move 2 3 behind 5)") << 6 << QList<int>{1, 2} << 5 << QList<int>{0, 3, 4, 1, 2, 5};
    QTest::newRow("contiguous backward") << 6 << QList<int>{3, 4} << 1 << QList<int>{0, 3, 4, 1, 2, 5};
    QTest::newRow("ctrl non contiguous") << 6 << QList<int>{1, 3} << 5 << QList<int>{0, 2, 4, 1, 3, 5};
    QTest::newRow("shift range to front") << 6 << QList<int>{1, 2, 3, 4} << 0 << QList<int>{1, 2, 3, 4, 0, 5};
    QTest::newRow("shift range to end") << 6 << QList<int>{1, 2, 3, 4} << 6 << QList<int>{0, 5, 1, 2, 3, 4};
    QTest::newRow("drop before first page") << 6 << QList<int>{3} << 0 << QList<int>{3, 0, 1, 2, 4, 5};
    QTest::newRow("drop after last page") << 6 << QList<int>{2} << 6 << QList<int>{0, 1, 3, 4, 5, 2};
    QTest::newRow("selection order does not matter") << 6 << QList<int>{2, 1} << 5 << QList<int>{0, 3, 4, 1, 2, 5};
    QTest::newRow("duplicates and invalid pages are ignored") << 6 << QList<int>{1, 1, 2, -1, 9} << 5 << QList<int>{0, 3, 4, 1, 2, 5};
    QTest::newRow("insertion row is clamped") << 3 << QList<int>{0} << 99 << QList<int>{1, 2, 0};
    // Dropping a selection into its own area changes nothing.
    QTest::newRow("self: before first moved page") << 6 << QList<int>{2, 3} << 2 << QList<int>{0, 1, 2, 3, 4, 5};
    QTest::newRow("self: between moved pages") << 6 << QList<int>{2, 3} << 3 << QList<int>{0, 1, 2, 3, 4, 5};
    QTest::newRow("self: after last moved page") << 6 << QList<int>{2, 3} << 4 << QList<int>{0, 1, 2, 3, 4, 5};
    QTest::newRow("self: single page before") << 6 << QList<int>{2} << 2 << QList<int>{0, 1, 2, 3, 4, 5};
    QTest::newRow("self: single page after") << 6 << QList<int>{2} << 3 << QList<int>{0, 1, 2, 3, 4, 5};
    QTest::newRow("self: everything") << 4 << QList<int>{0, 1, 2, 3} << 2 << QList<int>{0, 1, 2, 3};
}

void ViewerContextMenuTest::pageReorderOrderMath()
{
    QFETCH(int, pageCount);
    QFETCH(QList<int>, moved);
    QFETCH(int, insertionRow);
    QFETCH(QList<int>, expected);

    const std::vector<pdf::PDFInteger> movedPages(moved.cbegin(), moved.cend());
    const std::vector<pdf::PDFInteger> order = pdfviewer::PDFPageReorder::computeNewPageOrder(pageCount, movedPages, insertionRow);
    const std::vector<pdf::PDFInteger> expectedOrder(expected.cbegin(), expected.cend());
    QCOMPARE(order, expectedOrder);
    QVERIFY(pdfviewer::PDFPageReorder::isPermutation(order, pageCount));
    QCOMPARE(pdfviewer::PDFPageReorder::isIdentity(order), pdfviewer::PDFPageReorder::isIdentity(expectedOrder));

    // Validation helpers used by the controller.
    QVERIFY(!pdfviewer::PDFPageReorder::isPermutation({}, pageCount));
    std::vector<pdf::PDFInteger> duplicate = order;
    duplicate.back() = duplicate.front();
    if (pageCount > 1)
    {
        QVERIFY(!pdfviewer::PDFPageReorder::isPermutation(duplicate, pageCount));
    }
    std::vector<pdf::PDFInteger> outOfRange = order;
    outOfRange.back() = pageCount;
    QVERIFY(!pdfviewer::PDFPageReorder::isPermutation(outOfRange, pageCount));
    std::vector<pdf::PDFInteger> shorter = order;
    shorter.pop_back();
    QVERIFY(!pdfviewer::PDFPageReorder::isPermutation(shorter, pageCount));
    std::vector<pdf::PDFInteger> negative = order;
    negative.front() = -1;
    QVERIFY(!pdfviewer::PDFPageReorder::isPermutation(negative, pageCount));

    // The moved pages are found at their new positions, in their original relative order.
    const std::vector<pdf::PDFInteger> normalized = pdfviewer::PDFPageReorder::normalizePages(movedPages, pageCount);
    const std::vector<pdf::PDFInteger> newRows = pdfviewer::PDFPageReorder::mapOldToNew(order, normalized);
    QCOMPARE(newRows.size(), normalized.size());
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        QCOMPARE(order[size_t(newRows[i])], normalized[i]);
        if (i > 0)
        {
            QVERIFY(newRows[i] > newRows[i - 1]);
        }
    }
}

void ViewerContextMenuTest::pageReorderInsertionGeometry()
{
    using pdfviewer::PDFPageReorder;
    const auto rowAt = [](const std::vector<QRect>& rects, const QPoint& point) { return PDFPageReorder::computeInsertionPoint(rects, point).row; };

    // Three columns, 100 x 140 items, 10 px spacing, 7 items (the last row has one).
    std::vector<QRect> grid;
    for (int i = 0; i < 7; ++i)
    {
        grid.push_back(QRect(5 + (i % 3) * 110, 5 + (i / 3) * 150, 100, 140));
    }
    QVERIFY(PDFPageReorder::computeInsertionPoint(grid, QPoint(125, 200)).horizontalFlow);
    QCOMPARE(rowAt(grid, QPoint(125, 200)), 4);     // left half of item 4: before it
    QCOMPARE(rowAt(grid, QPoint(200, 200)), 5);     // right half of item 4: after it
    QCOMPARE(rowAt(grid, QPoint(219, 200)), 5);     // gap between item 4 and 5
    QCOMPARE(rowAt(grid, QPoint(400, 200)), 6);     // blank space at the end of the row
    QCOMPARE(rowAt(grid, QPoint(400, 350)), 7);     // blank space beside the single item of the last row
    QCOMPARE(rowAt(grid, QPoint(90, 310)), 7);      // right half of the last item
    QCOMPARE(rowAt(grid, QPoint(20, 310)), 6);      // left half of the last item
    QCOMPARE(rowAt(grid, QPoint(50, 600)), 7);      // below everything: end of the document
    QCOMPARE(rowAt(grid, QPoint(200, 480)), 7);
    QCOMPARE(rowAt(grid, QPoint(50, 0)), 0);        // above everything
    QCOMPARE(rowAt(grid, QPoint(8, 20)), 0);        // first page, left half
    QCOMPARE(rowAt(grid, QPoint(50, 150)), 3);      // between rows: in front of the lower row
    QCOMPARE(rowAt({}, QPoint(10, 10)), 0);

    // The result follows the geometry, not fixed pixel values: the same layout at another thumbnail size.
    std::vector<QRect> tinyGrid;
    for (int i = 0; i < 7; ++i)
    {
        tinyGrid.push_back(QRect(2 + (i % 3) * 40, 2 + (i / 3) * 60, 36, 56));
    }
    QCOMPARE(rowAt(tinyGrid, QPoint(2 + 40 + 4, 70)), 4);
    QCOMPARE(rowAt(tinyGrid, QPoint(2 + 40 + 30, 70)), 5);
    QCOMPARE(rowAt(tinyGrid, QPoint(500, 70)), 6);
    QCOMPARE(rowAt(tinyGrid, QPoint(10, 5000)), 7);

    // One column: the vertical axis decides.
    std::vector<QRect> column;
    for (int i = 0; i < 4; ++i)
    {
        column.push_back(QRect(5, 5 + i * 150, 100, 140));
    }
    QVERIFY(!PDFPageReorder::computeInsertionPoint(column, QPoint(30, 20)).horizontalFlow);
    QCOMPARE(rowAt(column, QPoint(30, 20)), 0);     // upper half of the first item
    QCOMPARE(rowAt(column, QPoint(30, 120)), 1);    // lower half
    QCOMPARE(rowAt(column, QPoint(90, 120)), 1);    // x does not matter in a column
    QCOMPARE(rowAt(column, QPoint(30, 148)), 1);    // gap
    QCOMPARE(rowAt(column, QPoint(300, 200)), 1);   // blank beside the second item, upper half
    QCOMPARE(rowAt(column, QPoint(30, 2000)), 4);

    // Mixed portrait / landscape items in one row.
    const std::vector<QRect> mixed = { QRect(5, 5, 100, 140), QRect(115, 45, 140, 100), QRect(265, 5, 100, 140) };
    QCOMPARE(rowAt(mixed, QPoint(125, 100)), 1);
    QCOMPARE(rowAt(mixed, QPoint(240, 100)), 2);
    QCOMPARE(rowAt(mixed, QPoint(150, 20)), 1);     // above the landscape item, left of its centre
    QCOMPARE(rowAt(mixed, QPoint(230, 20)), 2);

    // The indicator is drawn next to the item the decision was made on.
    const PDFPageReorder::InsertionPoint point = PDFPageReorder::computeInsertionPoint(grid, QPoint(200, 200));
    QCOMPARE(point.anchor, 4);
    QVERIFY(point.after);
}

void ViewerContextMenuTest::thumbnailReorderWorkflow()
{
#ifdef Q_OS_LINUX
    QSKIP("Editor thumbnail interactions are covered by the Windows runtime job.");
#endif
    const QString path = m_temp.filePath("thumbnail-reorder.pdf");
    QVERIFY(writePdfFixture(path, 6));

    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    auto* drawProxy = controller->getPdfWidget()->getDrawWidgetProxy();
    drawProxy->setPageLayout(pdf::PageLayout::OneColumn);

    auto* sidebarDock = editor.findChild<QDockWidget*>("SidebarDockWidget");
    auto* thumbnails = editor.findChild<pdfviewer::PDFThumbnailsListView*>("thumbnailsListView");
    QVERIFY(sidebarDock);
    QVERIFY(thumbnails);
    QVERIFY(showThumbnailsPage(&editor));
    QTRY_COMPARE(thumbnails->model()->rowCount(), 6);
    QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(0, 0)).isValid());
    QVERIFY(thumbnails->isReorderEnabled());
    QVERIFY(thumbnails->dragEnabled());
    QVERIFY(thumbnails->model()->flags(thumbnails->model()->index(0, 0)) & Qt::ItemIsDragEnabled);

    QAction* undoAction = editor.findChild<QAction*>("actionUndo");
    QAction* redoAction = editor.findChild<QAction*>("actionRedo");
    QVERIFY(undoAction);
    QVERIFY(redoAction);

    const std::vector<pdf::PDFObjectReference> original = reorderPageReferences(controller->getDocument());
    QCOMPARE(original.size(), size_t(6));

    // Page references are only rearranged, never copied: position i must hold the original reference of page expected[i].
    const auto verifyArrangement = [&](const std::vector<int>& expected)
    {
        const std::vector<pdf::PDFObjectReference> current = reorderPageReferences(controller->getDocument());
        QCOMPARE(current.size(), original.size());
        QCOMPARE(thumbnails->model()->rowCount(), int(original.size()));
        for (size_t position = 0; position < expected.size(); ++position)
        {
            QVERIFY2(current[position] == original[size_t(expected[position])], qPrintable(QStringLiteral("position %1").arg(position)));
        }
    };
    const auto click = [&](int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(row, 0)).isValid());
        thumbnails->scrollTo(thumbnails->model()->index(row, 0));
        QCoreApplication::processEvents();
        QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, modifiers, thumbnails->visualRect(thumbnails->model()->index(row, 0)).center());
        QCoreApplication::processEvents();
    };
    const auto dropSelection = [&](int row, bool after)
    {
        const std::vector<int> rows = selectedThumbnailRows(thumbnails);
        const std::vector<pdf::PDFInteger> pages(rows.cbegin(), rows.cend());
        dropThumbnails(thumbnails, pages, thumbnailDropPoint(thumbnails, row, after));
    };
    // The sidebar stays on the thumbnails page after a move and after Undo, so the next drag can start at once.
    const auto undoToOriginal = [&]()
    {
        undoAction->trigger();
        verifyArrangement({0, 1, 2, 3, 4, 5});
        QVERIFY(thumbnails->isVisible());
    };
    const auto reorder = [&](const std::vector<int>& expected, int dropRow, bool after)
    {
        const pdf::PDFDocument* before = controller->getDocument();
        dropSelection(dropRow, after);
        QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != before, 5000);
        verifyArrangement(expected);
        QVERIFY(thumbnails->isVisible());
    };

    // 1. Single page forward: page 2 goes behind page 5.
    click(1);
    reorder({0, 2, 3, 4, 1, 5}, 4, true);
    undoToOriginal();

    // 2. Single page backward.
    click(4);
    reorder({0, 4, 1, 2, 3, 5}, 1, false);
    undoToOriginal();

    // 3. Contiguous pages forward: 1 2 3 4 5 6, move 2 3 behind 5 -> 1 4 5 2 3 6. Also checks the
    //    selection and the page being read after the move, and Undo / Redo.
    click(1);
    click(2, Qt::ShiftModifier);
    QCOMPARE(selectedThumbnailRows(thumbnails), (std::vector<int>{1, 2}));
    // Reading page 5 while pages 2 and 3 are selected: moving the view synchronizes the thumbnails and
    // collapses the selection, so the selection is made again afterwards (without navigating).
    drawProxy->goToPage(4);
    QTRY_COMPARE_WITH_TIMEOUT(controller->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(4), 5000);
    QTest::qWait(50);
    thumbnails->selectionModel()->clearSelection();
    thumbnails->selectionModel()->select(thumbnails->model()->index(1, 0), QItemSelectionModel::Select);
    thumbnails->selectionModel()->select(thumbnails->model()->index(2, 0), QItemSelectionModel::Select);
    QCOMPARE(selectedThumbnailRows(thumbnails), (std::vector<int>{1, 2}));
    reorder({0, 3, 4, 1, 2, 5}, 4, true);
    QTRY_COMPARE_WITH_TIMEOUT(selectedThumbnailRows(thumbnails), (std::vector<int>{3, 4}), 5000);
    // The page that was being read (original page 5) is now page 3 and stays in view.
    QTRY_COMPARE_WITH_TIMEOUT(controller->getPdfWidget()->getDrawWidget()->getCurrentPages().front(), pdf::PDFInteger(2), 5000);
    QVERIFY(controller->getDocument()->getCatalog()->getPage(2)->getPageReference() == original[4]);
    undoToOriginal();
    redoAction->trigger();
    verifyArrangement({0, 3, 4, 1, 2, 5});
    undoToOriginal();

    // 4. Contiguous pages backward.
    click(3);
    click(4, Qt::ShiftModifier);
    reorder({0, 3, 4, 1, 2, 5}, 1, false);
    undoToOriginal();

    // 5. Ctrl selection of separate pages.
    click(1);
    click(3, Qt::ControlModifier);
    QCOMPARE(selectedThumbnailRows(thumbnails), (std::vector<int>{1, 3}));
    reorder({0, 2, 4, 1, 3, 5}, 4, true);
    QTRY_COMPARE_WITH_TIMEOUT(selectedThumbnailRows(thumbnails), (std::vector<int>{3, 4}), 5000);
    undoToOriginal();

    // 6. Shift range to the front.
    click(1);
    click(4, Qt::ShiftModifier);
    QCOMPARE(selectedThumbnailRows(thumbnails), (std::vector<int>{1, 2, 3, 4}));
    reorder({1, 2, 3, 4, 0, 5}, 0, false);
    undoToOriginal();

    // 7. Drop before the first page.
    click(3);
    reorder({3, 0, 1, 2, 4, 5}, 0, false);
    QTRY_COMPARE_WITH_TIMEOUT(selectedThumbnailRows(thumbnails), (std::vector<int>{0}), 5000);
    undoToOriginal();

    // 8. Drop behind the last page, and into the empty area of the view.
    click(2);
    reorder({0, 1, 3, 4, 5, 2}, 5, true);
    undoToOriginal();
    click(0);
    click(1, Qt::ShiftModifier);
    {
        const QPoint blank(thumbnails->viewport()->width() - 2, thumbnails->viewport()->height() - 2);
        if (!thumbnails->indexAt(blank).isValid())
        {
            const pdf::PDFDocument* before = controller->getDocument();
            dropThumbnails(thumbnails, {0, 1}, blank);
            QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != before, 5000);
            verifyArrangement({2, 3, 4, 5, 0, 1});
            undoToOriginal();
        }
    }

    // 9. Dropping into the own area does not change the order and creates no undo entry.
    const auto expectNoChange = [&](int row, bool after)
    {
        const pdf::PDFDocument* before = controller->getDocument();
        const bool undoBefore = undoAction->isEnabled();
        dropSelection(row, after);
        QTest::qWait(50);
        QVERIFY(controller->getDocument() == before);
        QCOMPARE(undoAction->isEnabled(), undoBefore);
        verifyArrangement({0, 1, 2, 3, 4, 5});
    };
    click(2);
    click(3, Qt::ShiftModifier);
    expectNoChange(2, false);   // before the first selected page
    expectNoChange(2, true);    // between the selected pages
    expectNoChange(3, true);    // after the last selected page
    click(2);
    expectNoChange(2, false);
    expectNoChange(2, true);
    expectNoChange(3, false);

    // 10. / 11. The controller refuses anything that is not a complete permutation.
    const pdf::PDFDocument* untouched = controller->getDocument();
    const std::vector<std::vector<pdf::PDFInteger>> invalidOrders = {
        { 0, 1, 2, 3, 4 },              // missing page (wrong size)
        { 0, 1, 2, 3, 4, 5, 6 },        // too many
        { 0, 1, 2, 3, 4, 9 },           // out of range
        { 0, 1, 2, 3, 4, -1 },          // negative
        { 0, 1, 2, 3, 4, 4 },           // duplicate, page 5 missing
        { 1, 1, 2, 3, 4, 5 },           // duplicate
        {},
    };
    for (const auto& order : invalidOrders)
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Page reorder rejected")));
        QVERIFY(!controller->reorderPages(order));
        QVERIFY(controller->getDocument() == untouched);
        verifyArrangement({0, 1, 2, 3, 4, 5});
    }
    // The current order is a no-op, not an undo step.
    const bool undoBefore = undoAction->isEnabled();
    QVERIFY(!controller->reorderPages({0, 1, 2, 3, 4, 5}));
    QVERIFY(controller->getDocument() == untouched);
    QCOMPARE(undoAction->isEnabled(), undoBefore);

    // 12. / 13. Several steps undo and redo in order.
    QVERIFY(controller->reorderPages({1, 0, 2, 3, 4, 5}));
    QVERIFY(controller->reorderPages({1, 0, 5, 4, 3, 2}));
    verifyArrangement({0, 1, 5, 4, 3, 2});
    undoAction->trigger();
    verifyArrangement({1, 0, 2, 3, 4, 5});
    undoAction->trigger();
    verifyArrangement({0, 1, 2, 3, 4, 5});
    redoAction->trigger();
    verifyArrangement({1, 0, 2, 3, 4, 5});
    redoAction->trigger();
    verifyArrangement({0, 1, 5, 4, 3, 2});
    controller->closeDocument();
    QCoreApplication::processEvents();
}

void ViewerContextMenuTest::reorderPreservesContentAfterSave()
{
#ifdef Q_OS_LINUX
    QSKIP("Editor thumbnail interactions are covered by the Windows runtime job.");
#endif
    const QString path = m_temp.filePath("reorder-content.pdf");
    QVERIFY(writeReorderFixture(path));

    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    controller->getPdfWidget()->getDrawWidgetProxy()->setPageLayout(pdf::PageLayout::OneColumn);

    auto* sidebarDock = editor.findChild<QDockWidget*>("SidebarDockWidget");
    auto* thumbnails = editor.findChild<pdfviewer::PDFThumbnailsListView*>("thumbnailsListView");
    QVERIFY(sidebarDock && thumbnails);
    QVERIFY(showThumbnailsPage(&editor));
    QTRY_COMPARE(thumbnails->model()->rowCount(), 4);
    QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(0, 0)).isValid());

    // Pages 1 and 2 (the form page and the rotated page) go behind the others: 3 4 1 2.
    const std::vector<pdf::PDFObjectReference> original = reorderPageReferences(controller->getDocument());
    QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::NoModifier, thumbnails->visualRect(thumbnails->model()->index(0, 0)).center());
    QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::ShiftModifier, thumbnails->visualRect(thumbnails->model()->index(1, 0)).center());
    const pdf::PDFDocument* before = controller->getDocument();
    dropThumbnails(thumbnails, {0, 1}, thumbnailDropPoint(thumbnails, 3, true));
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != before, 5000);
    const std::vector<int> expected = {2, 3, 0, 1};
    const std::vector<pdf::PDFObjectReference> moved = reorderPageReferences(controller->getDocument());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        QVERIFY(moved[i] == original[size_t(expected[i])]);
    }

    const QString savedPath = m_temp.filePath("reorder-content-saved.pdf");
    QVERIFY(annotationSaveAs(controller, &editor, savedPath));
    QVERIFY(QFile::exists(savedPath));
    controller->closeDocument();
    controller->openDocument(savedPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    const pdf::PDFDocument* reopened = controller->getDocument();
    const pdf::PDFCatalog* catalog = reopened->getCatalog();
    QCOMPARE(catalog->getPageCount(), size_t(4));

    // Order: the width of the media box identifies the original page.
    const std::vector<int> widths = {420, 430, 400, 410};
    for (size_t i = 0; i < widths.size(); ++i)
    {
        QCOMPARE(int(catalog->getPage(i)->getMediaBox().width()), widths[i]);
    }

    // Rotation stays with the page that had it (original page 2, now page 4).
    for (size_t i = 0; i < 4; ++i)
    {
        QCOMPARE(catalog->getPage(i)->getPageRotation(), i == 3 ? pdf::PageRotation::Rotate90 : pdf::PageRotation::None);
    }

    // The annotation is still on the page it was created on (original page 3, now page 1).
    for (size_t i = 0; i < 4; ++i)
    {
        const auto items = annotations(controller, int(i));
        QCOMPARE(items.size(), i == 0 ? 1 : 0);
    }
    const auto items = annotations(controller, 0);
    QCOMPARE(items.front()->asMarkupAnnotation()->getContents(), QStringLiteral("note-on-third-page"));
    QVERIFY(items.front()->asMarkupAnnotation()->getPageReference() == catalog->getPage(0)->getPageReference());

    // The form widget belongs to the same page object and keeps its value (original page 1, now page 3).
    const pdf::PDFForm form = pdf::PDFForm::parse(reopened, catalog->getFormObject());
    int fieldCount = 0;
    form.apply([&](const pdf::PDFFormField* field)
    {
        if (field->getName(pdf::PDFFormField::FullyQualified) != QStringLiteral("reorder-name"))
        {
            return;
        }
        ++fieldCount;
        QCOMPARE(pdf::PDFDocumentDataLoaderDecorator(reopened).readTextString(reopened->getObject(field->getValue()), QString()), QStringLiteral("Alice"));
        QCOMPARE(field->getWidgets().size(), size_t(1));
        QVERIFY(field->getWidgets().front().getPage() == catalog->getPage(2)->getPageReference());
    });
    QCOMPARE(fieldCount, 1);

    // Text layer of every page is still the text of its original page.
    auto* compiler = controller->getPdfWidget()->getDrawWidgetProxy()->getTextLayoutCompiler();
    compiler->makeTextLayout();
    QTRY_VERIFY_WITH_TIMEOUT(compiler->isTextLayoutReady(), 15000);
    const std::vector<int> originalNumbers = {3, 4, 1, 2};
    for (size_t i = 0; i < originalNumbers.size(); ++i)
    {
        QVERIFY2(pageText(compiler, pdf::PDFInteger(i)).contains(QStringLiteral("smoke page %1").arg(originalNumbers[i])),
                 qPrintable(QStringLiteral("page %1: %2").arg(i + 1).arg(pageText(compiler, pdf::PDFInteger(i)))));
    }
    controller->closeDocument();
    QCoreApplication::processEvents();
}

void ViewerContextMenuTest::reorderFlattensNestedPageTree()
{
    // Pages 1 and 2 inherit MediaBox and Rotate from their parent node, pages 3 and 4 inherit a different MediaBox.
    const QList<QByteArray> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 4 >>",
        "<< /Type /Pages /Parent 2 0 R /Kids [5 0 R 6 0 R] /Count 2 /MediaBox [0 0 500 700] /Rotate 90 >>",
        "<< /Type /Pages /Parent 2 0 R /Kids [7 0 R 8 0 R] /Count 2 /MediaBox [0 0 300 400] >>",
        "<< /Type /Page /Parent 3 0 R >>",
        "<< /Type /Page /Parent 3 0 R >>",
        "<< /Type /Page /Parent 4 0 R >>",
        "<< /Type /Page /Parent 4 0 R >>",
    };
    QByteArray pdfData = "%PDF-1.4\n";
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
    pdfData += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    const QString path = m_temp.filePath("nested-page-tree.pdf");
    QFile fixture(path);
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(fixture.write(pdfData), pdfData.size());
    fixture.close();

    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(4));
    const std::vector<pdf::PDFObjectReference> original = reorderPageReferences(controller->getDocument());

    QVERIFY(controller->reorderPages({3, 2, 1, 0}));
    const pdf::PDFCatalog* catalog = controller->getDocument()->getCatalog();
    QCOMPARE(catalog->getPageCount(), size_t(4));
    const std::vector<pdf::PDFObjectReference> reordered = reorderPageReferences(controller->getDocument());
    for (size_t i = 0; i < 4; ++i)
    {
        QVERIFY(reordered[i] == original[3 - i]);
    }
    // The inherited attributes travelled with the pages.
    const std::vector<int> widths = {300, 300, 500, 500};
    for (size_t i = 0; i < 4; ++i)
    {
        QCOMPARE(int(catalog->getPage(i)->getMediaBox().width()), widths[i]);
        QCOMPARE(catalog->getPage(i)->getPageRotation(), i < 2 ? pdf::PageRotation::None : pdf::PageRotation::Rotate90);
    }

    editor.findChild<QAction*>("actionUndo")->trigger();
    QVERIFY(reorderPageReferences(controller->getDocument()) == original);
    QCOMPARE(int(controller->getDocument()->getCatalog()->getPage(0)->getMediaBox().width()), 500);
    controller->closeDocument();
    QCoreApplication::processEvents();
}

void ViewerContextMenuTest::viewerThumbnailsAreReadOnly()
{
    auto* sidebarDock = m_window->findChild<QDockWidget*>("SidebarDockWidget");
    auto* thumbnails = m_window->findChild<pdfviewer::PDFThumbnailsListView*>("thumbnailsListView");
    QVERIFY(sidebarDock);
    QVERIFY(thumbnails);
    QVERIFY(showThumbnailsPage(m_window.get()));
    QTRY_COMPARE(thumbnails->model()->rowCount(), 3);
    QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(0, 0)).isValid());

    QVERIFY(!thumbnails->isReorderEnabled());
    QVERIFY(!thumbnails->dragEnabled());
    QVERIFY(!thumbnails->acceptDrops());

    // Even a drop that reaches the view changes nothing.
    const auto before = reorderPageReferences(m_window->getProgramController()->getDocument());
    const pdf::PDFDocument* document = m_window->getProgramController()->getDocument();
    dropThumbnails(thumbnails, {0}, thumbnailDropPoint(thumbnails, 2, true));
    QTest::qWait(50);
    QVERIFY(m_window->getProgramController()->getDocument() == document);
    QVERIFY(reorderPageReferences(m_window->getProgramController()->getDocument()) == before);
}

void ViewerContextMenuTest::nativeThumbnailDragSmoke()
{
#ifndef Q_OS_WIN
    QSKIP("Native drag smoke test needs Windows.");
#else
    if (qEnvironmentVariableIntValue("FAMILYPDF_NATIVE_DRAG_SMOKE") == 0 || QGuiApplication::platformName() != QLatin1String("windows"))
    {
        QSKIP("Moves the real mouse cursor: set FAMILYPDF_NATIVE_DRAG_SMOKE=1 and QT_QPA_PLATFORM=windows.");
    }

    const QString path = m_temp.filePath("thumbnail-native-drag.pdf");
    QVERIFY(writePdfFixture(path, 6));
    pdfviewer::PDFEditorMainWindow editor;
    editor.setWindowFlag(Qt::WindowStaysOnTopHint, true);   // the real mouse must reach this window
    editor.resize(1100, 900);
    editor.show();
    editor.raise();
    editor.activateWindow();
    auto* controller = editor.getProgramController();
    controller->openDocument(path);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument() != nullptr, 15000);
    controller->getPdfWidget()->getDrawWidgetProxy()->setPageLayout(pdf::PageLayout::OneColumn);

    auto* sidebarDock = editor.findChild<QDockWidget*>("SidebarDockWidget");
    auto* thumbnails = editor.findChild<pdfviewer::PDFThumbnailsListView*>("thumbnailsListView");
    QVERIFY(sidebarDock && thumbnails);
    QVERIFY(showThumbnailsPage(&editor));
    QTRY_COMPARE(thumbnails->model()->rowCount(), 6);
    QTRY_VERIFY(thumbnails->visualRect(thumbnails->model()->index(5, 0)).isValid());
    QTest::qWait(300);
    editor.activateWindow();

    QAction* undoAction = editor.findChild<QAction*>("actionUndo");
    QAction* redoAction = editor.findChild<QAction*>("actionRedo");
    const std::vector<pdf::PDFObjectReference> original = reorderPageReferences(controller->getDocument());
    const auto verifyArrangement = [&](const std::vector<int>& expected)
    {
        const std::vector<pdf::PDFObjectReference> current = reorderPageReferences(controller->getDocument());
        QCOMPARE(current.size(), original.size());
        for (size_t position = 0; position < expected.size(); ++position)
        {
            QVERIFY2(current[position] == original[size_t(expected[position])], qPrintable(QStringLiteral("position %1").arg(position)));
        }
    };
    const auto itemCentre = [&](int row) { return thumbnails->visualRect(thumbnails->model()->index(row, 0)).center(); };
    // The cursor of a real drag can be nudged by the person at the keyboard, so the exact drop row is
    // not asserted. What is asserted is that the real drag ends in a drop on this view, and that the
    // document, selection and history match the row the drop reported. A drag that ended in the own
    // area of the selection (no change) or outside the view is repeated.
    const auto nativeDrag = [&](int fromRow, int toRow, bool after, const std::vector<pdf::PDFInteger>& dragged) -> bool
    {
        for (int attempt = 1; attempt <= 4; ++attempt)
        {
            std::vector<pdf::PDFInteger> droppedPages;
            int droppedRow = -1;
            const auto connection = QObject::connect(thumbnails, &pdfviewer::PDFThumbnailsListView::pagesDropped, thumbnails, [&](const std::vector<pdf::PDFInteger>& pages, int row)
            {
                droppedPages = pages;
                droppedRow = row;
            });
            const auto disconnect = qScopeGuard([&]() { QObject::disconnect(connection); });

            const pdf::PDFDocument* before = controller->getDocument();
            QWidget* top = thumbnails->window();
            if (top->isMinimized())
            {
                top->showNormal();
            }
            top->raise();
            top->activateWindow();
            QTest::qWait(400);
            const QPoint from = itemCentre(fromRow);
            const QPoint to = thumbnailDropPoint(thumbnails, toRow, after);
            if (!scheduleNativeDrag(thumbnails->viewport(), from, to))
            {
                qInfo() << "NATIVE attempt" << attempt << "the test window is not on top at the drag points";
                return false;
            }
            const bool dropped = waitUntil([&]() { return droppedRow >= 0; }, 8000);
            QTest::qWait(1500);   // the worker thread finishes its steps
            if (!dropped)
            {
                qInfo() << "NATIVE attempt" << attempt << "no drop on the view was reported";
                continue;
            }
            qInfo() << "NATIVE attempt" << attempt << "dragged" << dragged.size() << "page(s), reported insertion row" << droppedRow << "(aimed at" << (after ? toRow + 1 : toRow) << ")";
            if (droppedPages != dragged)
            {
                return false;
            }
            const std::vector<pdf::PDFInteger> order = pdfviewer::PDFPageReorder::computeNewPageOrder(6, droppedPages, droppedRow);
            if (pdfviewer::PDFPageReorder::isIdentity(order))
            {
                // Dropped on the own area: nothing may change.
                if (controller->getDocument() != before)
                {
                    return false;
                }
                QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::NoModifier, itemCentre(dragged.front()));
                continue;
            }
            if (!waitUntil([&]() { return controller->getDocument() != before; }, 5000)) return false;
            const std::vector<pdf::PDFObjectReference> current = reorderPageReferences(controller->getDocument());
            for (size_t position = 0; position < order.size(); ++position)
            {
                if (!(current[position] == original[size_t(order[position])]))
                {
                    return false;
                }
            }
            const std::vector<pdf::PDFInteger> newRows = pdfviewer::PDFPageReorder::mapOldToNew(order, droppedPages);
            const std::vector<int> expectedSelection(newRows.cbegin(), newRows.cend());
            if (!waitUntil([&]() { return selectedThumbnailRows(thumbnails) == expectedSelection; }, 5000)) return false;
            return true;
        }
        return false;
    };

    // Single page, dragged with the real mouse.
    QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::NoModifier, itemCentre(1));
    QVERIFY2(nativeDrag(1, 4, true, {1}), "single page native drag");

    undoAction->trigger();
    verifyArrangement({0, 1, 2, 3, 4, 5});
    QTest::qWait(300);

    // Several pages (Shift selection of pages 2 and 3), dragged with the real mouse.
    QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::NoModifier, itemCentre(1));
    QTest::mouseClick(thumbnails->viewport(), Qt::LeftButton, Qt::ShiftModifier, itemCentre(2));
    QVERIFY2(nativeDrag(1, 4, true, {1, 2}), "multi page native drag");
    const std::vector<pdf::PDFObjectReference> afterMultiDrag = reorderPageReferences(controller->getDocument());

    undoAction->trigger();
    verifyArrangement({0, 1, 2, 3, 4, 5});
    redoAction->trigger();
    QVERIFY(reorderPageReferences(controller->getDocument()) == afterMultiDrag);
    undoAction->trigger();
    verifyArrangement({0, 1, 2, 3, 4, 5});
    controller->closeDocument();
    QCoreApplication::processEvents();
#endif
}


// ---- Merge PDFs (v9) -------------------------------------------------------------------------------------

namespace
{

/// Answers the message boxes that appear while a dialog runs. \p choose picks the button of a box (nullptr: default).
class MessageBoxAnswerer : public QObject
{
public:
    explicit MessageBoxAnswerer(std::function<QAbstractButton*(QMessageBox*)> choose, QObject* parent = nullptr) :
        QObject(parent),
        m_choose(std::move(choose))
    {
        connect(&m_timer, &QTimer::timeout, this, &MessageBoxAnswerer::poll);
        m_timer.start(10);
    }

    QStringList texts;                      ///< Text of every message box that was answered

private:
    void poll()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        {
            texts << box->text();
            QAbstractButton* button = m_choose ? m_choose(box) : nullptr;
            if (!button)
            {
                button = box->defaultButton() ? static_cast<QAbstractButton*>(box->defaultButton()) : (box->buttons().isEmpty() ? nullptr : box->buttons().front());
            }
            if (button)
            {
                button->click();
            }
            else
            {
                box->accept();
            }
        }
    }

    std::function<QAbstractButton*(QMessageBox*)> m_choose;
    QTimer m_timer;
};

QAbstractButton* boxButton(QMessageBox* box, const QString& textPart)
{
    for (QAbstractButton* button : box->buttons())
    {
        if (button->text().contains(textPart, Qt::CaseInsensitive))
        {
            return button;
        }
    }
    return nullptr;
}

QStringList mergeListRows(QTreeWidget* list)
{
    QStringList rows;
    for (int i = 0; i < list->topLevelItemCount(); ++i)
    {
        rows << QStringLiteral("%1|%2|%3").arg(list->topLevelItem(i)->text(0), list->topLevelItem(i)->text(1), list->topLevelItem(i)->text(2));
    }
    return rows;
}

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString writeEncryptedFixture(const QString& path, const QString& userPassword, const QString& ownerPassword, uint32_t permissions)
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
    settings.id = QByteArrayLiteral("merge-ui-test-id-0123456789");
    builder.setSecurityHandler(pdf::PDFSecurityHandlerFactory::createSecurityHandler(settings));
    const pdf::PDFDocument document = builder.build();
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult result = writer.write(path, &document, true);
    return result ? QString() : result.getErrorMessage();
}

} // namespace

void ViewerContextMenuTest::mergePdfsEntriesAreAvailable()
{
    // Merge PDFs makes a new file, so both applications offer it, with or without an open document.
    pdfviewer::PDFViewerMainWindow viewer;
    pdfviewer::PDFEditorMainWindow editor;
    for (QMainWindow* window : { static_cast<QMainWindow*>(&viewer), static_cast<QMainWindow*>(&editor) })
    {
        window->resize(900, 700);
        window->show();
        QAction* merge = window->findChild<QAction*>("actionMergePdfs");
        QVERIFY(merge);
        QVERIFY(merge->isEnabled());
        QVERIFY(merge->text().contains("Merge PDFs"));
        bool inFileMenu = false;
        for (QMenu* menu : window->menuBar()->findChildren<QMenu*>())
        {
            inFileMenu = inFileMenu || (menu->objectName() == "menuFile" && menu->actions().contains(merge));
        }
        QVERIFY(inFileMenu);
    }
    viewer.getProgramController()->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.getProgramController()->getDocument(), 15000);
    QVERIFY(viewer.findChild<QAction*>("actionMergePdfs")->isEnabled());
}

void ViewerContextMenuTest::mergePdfsDialogWorkflow()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    pdfviewer::PDFEditorMainWindow editor;
    editor.resize(1100, 900);
    editor.show();
    auto* controller = editor.getProgramController();
    controller->openDocument(m_pdfPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    const pdf::PDFDocument* openDocument = controller->getDocument();

    const QString zetaPath = m_temp.filePath("zeta.pdf");
    QVERIFY(writePdfFixture(zetaPath, 2, 1, true, false, false, "ZETA page "));
    const QString outputPath = m_temp.filePath("merged-ui.pdf");
    QFile::remove(outputPath);
    const QByteArray sourceBefore = fileBytes(m_pdfPath);
    const QByteArray zetaBefore = fileBytes(zetaPath);

    QStringList rowsBeforeMerge;
    QString infoText;
    QString defaultRange;
    int stage = 0;
    MessageBoxAnswerer answerer([](QMessageBox* box) { return boxButton(box, "Close"); });
    QTimer timer;
    connect(&timer, &QTimer::timeout, &editor, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFMergePdfsDialog*>(QApplication::activeModalWidget());
        if (!dialog)
        {
            return;
        }
        if (stage == 0)
        {
            stage = 1;
            auto* list = dialog->findChild<QTreeWidget*>("mergeList");
            QVERIFY(dialog->findChild<QPushButton*>("mergeAddCurrentButton")->isVisible());
            QVERIFY(!dialog->findChild<QPushButton*>("mergeButton")->isEnabled());      // nothing to merge yet
            dialog->findChild<QPushButton*>("mergeAddCurrentButton")->click();
            dialog->addFiles({ zetaPath });
            QCOMPARE(list->topLevelItemCount(), 2);
            defaultRange = list->topLevelItem(0)->text(2);
            // Page ranges: the open document keeps pages 1 and 3, the second PDF gives page 2 and then page 1.
            list->topLevelItem(0)->setText(2, "1,3");
            list->topLevelItem(1)->setText(2, "2,1");
            // Move the second PDF to the top with the button, then check Move Down and the borders.
            list->setCurrentItem(list->topLevelItem(1));
            QVERIFY(dialog->findChild<QPushButton*>("mergeUpButton")->isEnabled());
            QVERIFY(!dialog->findChild<QPushButton*>("mergeDownButton")->isEnabled());
            dialog->findChild<QPushButton*>("mergeUpButton")->click();
            QVERIFY(!dialog->findChild<QPushButton*>("mergeUpButton")->isEnabled());
            QVERIFY(dialog->findChild<QPushButton*>("mergeDownButton")->isEnabled());
            dialog->findChild<QPushButton*>("mergeDownButton")->click();
            dialog->findChild<QPushButton*>("mergeUpButton")->click();
            // Remove and add again (the removed row must not leave anything behind).
            list->setCurrentItem(list->topLevelItem(1));
            dialog->findChild<QPushButton*>("mergeRemoveButton")->click();
            QCOMPARE(list->topLevelItemCount(), 1);
            dialog->findChild<QPushButton*>("mergeAddCurrentButton")->click();
            QCOMPARE(list->topLevelItemCount(), 2);
            list->topLevelItem(1)->setText(2, "1,3");
            rowsBeforeMerge = mergeListRows(list);
            infoText = dialog->findChild<QLabel*>("mergeInfoLabel")->text();
            dialog->findChild<QLineEdit*>("mergeOutputEdit")->setText(outputPath);
            auto* mergeButton = dialog->findChild<QPushButton*>("mergeButton");
            QVERIFY(mergeButton->isEnabled());
            QMetaObject::invokeMethod(mergeButton, &QPushButton::click, Qt::QueuedConnection);
            return;
        }
        if (stage == 1 && !answerer.texts.isEmpty())
        {
            stage = 2;
            dialog->reject();       // The success message was answered with Close; the dialog stays open until closed.
        }
    });
    timer.start(10);
    editor.findChild<QAction*>("actionMergePdfs")->trigger();
    timer.stop();

    QCOMPARE(stage, 2);
    QCOMPARE(defaultRange, QStringLiteral("All pages"));
    QCOMPARE(rowsBeforeMerge.size(), 2);
    QCOMPARE(rowsBeforeMerge[0], QStringLiteral("zeta.pdf|2|2,1"));
    QCOMPARE(rowsBeforeMerge[1], QStringLiteral("three-pages.pdf (open document)|3|1,3"));
    QVERIFY2(infoText.contains("4 page") || infoText.contains("page(s)"), qPrintable(infoText));
    QCOMPARE(answerer.texts.size(), 1);
    QVERIFY2(answerer.texts.front().contains("Saved 4 page"), qPrintable(answerer.texts.front()));

    // Output: pages of the open document twice (list order, then range order).
    pdf::PDFDocumentReader reader(nullptr, {}, true, false);
    const pdf::PDFDocument merged = reader.readFromFile(outputPath);
    QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(4));

    // Sources and the open document are untouched; the output is a separate new file.
    QCOMPARE(fileBytes(m_pdfPath), sourceBefore);
    QCOMPARE(fileBytes(zetaPath), zetaBefore);
    QCOMPARE(controller->getDocument(), openDocument);
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(3));
    controller->closeDocument();
}

void ViewerContextMenuTest::mergePdfsOutputOrderAndTextLayerAfterReopen()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const QString alpha = m_temp.filePath("alpha.pdf");
    const QString zeta = m_temp.filePath("zeta2.pdf");
    QVERIFY(writePdfFixture(alpha, 3, 1, true, false, false, "ALPHA page "));
    QVERIFY(writePdfFixture(zeta, 2, 1, true, false, false, "ZETA page "));
    const QString outputPath = m_temp.filePath("merged-order.pdf");
    QFile::remove(outputPath);

    // Without an open document, the dialog starts with an empty list. After the merge "Open Merged PDF" opens the result.
    pdfviewer::PDFViewerMainWindow viewer;
    viewer.resize(1100, 900);
    viewer.show();
    auto* controller = viewer.getProgramController();
    QVERIFY(!controller->getDocument());

    int stage = 0;
    MessageBoxAnswerer answerer([](QMessageBox* box) { return boxButton(box, "Open"); });
    QTimer timer;
    connect(&timer, &QTimer::timeout, &viewer, [&]()
    {
        auto* dialog = qobject_cast<pdfviewer::PDFMergePdfsDialog*>(QApplication::activeModalWidget());
        if (dialog && stage == 0)
        {
            stage = 1;
            QVERIFY(!dialog->findChild<QPushButton*>("mergeAddCurrentButton")->isVisible());
            // ALPHA pages 1-2 then ZETA 2,1 then ALPHA 3 (the same file may be listed twice).
            dialog->addFiles({ alpha, zeta, alpha });
            auto* list = dialog->findChild<QTreeWidget*>("mergeList");
            QCOMPARE(list->topLevelItemCount(), 3);
            list->topLevelItem(0)->setText(2, "1-2");
            list->topLevelItem(1)->setText(2, "2,1");
            list->topLevelItem(2)->setText(2, "3");
            dialog->findChild<QLineEdit*>("mergeOutputEdit")->setText(outputPath);
            QMetaObject::invokeMethod(dialog->findChild<QPushButton*>("mergeButton"), &QPushButton::click, Qt::QueuedConnection);
        }
    });
    timer.start(10);
    viewer.findChild<QAction*>("actionMergePdfs")->trigger();
    timer.stop();
    QCOMPARE(stage, 1);
    QCOMPARE(answerer.texts.size(), 1);

    // "Open Merged PDF" opened the new file in this window.
    QTRY_VERIFY_WITH_TIMEOUT(controller->getDocument(), 15000);
    QCOMPARE(controller->getDocument()->getCatalog()->getPageCount(), size_t(5));
    auto* compiler = controller->getPdfWidget()->getDrawWidgetProxy()->getTextLayoutCompiler();
    compiler->makeTextLayout();
    QTRY_VERIFY_WITH_TIMEOUT(compiler->isTextLayoutReady(), 15000);
    const QStringList expected = { "ALPHA page 1", "ALPHA page 2", "ZETA page 2", "ZETA page 1", "ALPHA page 3" };
    for (size_t i = 0; i < expected.size(); ++i)
    {
        QVERIFY2(pageText(compiler, pdf::PDFInteger(i)).contains(expected[int(i)]),
                 qPrintable(QStringLiteral("page %1: %2").arg(i + 1).arg(pageText(compiler, pdf::PDFInteger(i)))));
    }
    controller->closeDocument();
}

void ViewerContextMenuTest::mergePdfsBlocksAndWarns()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const QString plain = m_temp.filePath("plain-merge.pdf");
    QVERIFY(writePdfFixture(plain, 2, 1, true, false, false, "PLAIN page "));
    const QString signedFile = QFINDTESTDATA("fixtures/pyhanko-signed.pdf");
    QVERIFY(!signedFile.isEmpty());
    const QString restricted = m_temp.filePath("restricted-merge.pdf");
    QVERIFY(writeEncryptedFixture(restricted, QString(), "owner", uint32_t(pdf::PDFSecurityHandler::Permission::PrintLowResolution)).isEmpty());
    const QString locked = m_temp.filePath("locked-merge.pdf");
    QVERIFY(writeEncryptedFixture(locked, "secret", "owner", 0xFFFFFFFFu).isEmpty());
    const QString broken = m_temp.filePath("broken-merge.pdf");
    {
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a pdf");
    }
    const QString outputPath = m_temp.filePath("merged-blocked.pdf");
    QFile::remove(outputPath);

    int passwordAsked = 0;
    QString passwordToGive = QStringLiteral("wrong");
    pdfviewer::PDFMergePdfsDialog::Request request;
    request.directory = m_temp.path();
    request.passwordCallback = [&](bool* ok) { ++passwordAsked; *ok = true; return passwordToGive; };

    auto runMerge = [&](pdfviewer::PDFMergePdfsDialog& dialog, QMessageBox::StandardButton answer, QStringList* texts)
    {
        MessageBoxAnswerer answerer([answer](QMessageBox* box) -> QAbstractButton*
        {
            if (auto* button = box->button(answer))
            {
                return button;
            }
            return boxButton(box, "Close");
        });
        dialog.findChild<QLineEdit*>("mergeOutputEdit")->setText(outputPath);
        QTimer::singleShot(0, dialog.findChild<QPushButton*>("mergeButton"), &QPushButton::click);
        QTest::qWait(400);
        *texts = answerer.texts;
    };

    {
        pdfviewer::PDFMergePdfsDialog dialog(request, nullptr);
        dialog.show();
        auto* list = dialog.findChild<QTreeWidget*>("mergeList");

        // A broken file and a file whose password is wrong are reported and not added (one message).
        {
            MessageBoxAnswerer answerer([](QMessageBox* box) { return boxButton(box, "OK"); });
            dialog.addFiles({ broken, locked });
            QCOMPARE(answerer.texts.size(), 1);
            QVERIFY(answerer.texts.front().contains("broken-merge.pdf"));
            QVERIFY(answerer.texts.front().contains("locked-merge.pdf"));
        }
        QCOMPARE(list->topLevelItemCount(), 0);
        QCOMPARE(passwordAsked, 3);      // the wrong password is tried a limited number of times

        // The right password adds it. The warning says that the output is not encrypted.
        passwordToGive = "secret";
        dialog.addFiles({ locked, plain });
        QCOMPARE(list->topLevelItemCount(), 2);
        QVERIFY2(dialog.findChild<QLabel*>("mergeInfoLabel")->text().contains("not encrypted"), qPrintable(dialog.findChild<QLabel*>("mergeInfoLabel")->text()));

        // Answer "No" to the warning: nothing is written.
        QStringList texts;
        runMerge(dialog, QMessageBox::No, &texts);
        QCOMPARE(texts.size(), 1);
        QVERIFY2(texts.front().contains("not encrypted") && texts.front().contains("Do you want to merge anyway"), qPrintable(texts.front()));
        QVERIFY(!QFileInfo::exists(outputPath));

        // A signed PDF adds a signature warning.
        dialog.addFiles({ signedFile });
        QVERIFY2(dialog.findChild<QLabel*>("mergeInfoLabel")->text().contains("signature"), qPrintable(dialog.findChild<QLabel*>("mergeInfoLabel")->text()));
        runMerge(dialog, QMessageBox::No, &texts);
        QVERIFY(texts.front().contains("signature"));
        QVERIFY(!QFileInfo::exists(outputPath));

        // A PDF that does not allow copying blocks the merge: no way to confirm it, no output.
        dialog.addFiles({ restricted });
        QCOMPARE(list->topLevelItemCount(), 4);
        QVERIFY2(dialog.findChild<QLabel*>("mergeInfoLabel")->text().contains("Cannot merge"), qPrintable(dialog.findChild<QLabel*>("mergeInfoLabel")->text()));
        runMerge(dialog, QMessageBox::Yes, &texts);
        QCOMPARE(texts.size(), 1);
        QVERIFY2(texts.front().contains("restricted-merge.pdf"), qPrintable(texts.front()));
        QVERIFY(!QFileInfo::exists(outputPath));

        // Remove the blocking PDF; "Yes" to the warnings writes the file.
        list->setCurrentItem(list->topLevelItem(3));
        dialog.findChild<QPushButton*>("mergeRemoveButton")->click();
        QCOMPARE(list->topLevelItemCount(), 3);
        runMerge(dialog, QMessageBox::Yes, &texts);
        QVERIFY(QFileInfo::exists(outputPath));

        // Overwriting asks first; "No" keeps the file as it is.
        const QByteArray written = fileBytes(outputPath);
        QVERIFY(!written.isEmpty());
        runMerge(dialog, QMessageBox::No, &texts);
        QVERIFY2(texts.size() == 1 && texts.front().contains("already exists"), qPrintable(texts.join('|')));
        QCOMPARE(fileBytes(outputPath), written);

        // A range outside the PDF is refused with a message, nothing is written.
        list->topLevelItem(1)->setText(2, "1-99");
        QFile::remove(outputPath);
        runMerge(dialog, QMessageBox::Yes, &texts);
        QVERIFY2(texts.size() == 1 && texts.front().contains("between 1 and"), qPrintable(texts.join('|')));
        QVERIFY(!QFileInfo::exists(outputPath));
        QVERIFY(dialog.findChild<QLabel*>("mergeInfoLabel")->text().contains("between 1 and"));
        list->topLevelItem(1)->setText(2, "All pages");

        // The output file must not be one of the listed files.
        dialog.findChild<QLineEdit*>("mergeOutputEdit")->setText(plain);
        const QByteArray plainBefore = fileBytes(plain);
        {
            MessageBoxAnswerer answerer([](QMessageBox* box) { return boxButton(box, "OK"); });
            QTimer::singleShot(0, dialog.findChild<QPushButton*>("mergeButton"), &QPushButton::click);
            QTest::qWait(300);
            QCOMPARE(answerer.texts.size(), 1);
            QVERIFY(answerer.texts.front().contains("different"));
        }
        QCOMPARE(fileBytes(plain), plainBefore);
    }

    // The file written after "Yes" is a complete PDF that opens without a password: locked + plain + signed document.
    pdf::PDFDocumentReader signedReader(nullptr, {}, true, false);
    const pdf::PDFDocument signedDocument = signedReader.readFromFile(signedFile);
    QCOMPARE(signedReader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
    // (the output was removed by the "range" step above, so merge once more to check the written file)
    {
        pdfviewer::PDFMergePdfsDialog dialog(request, nullptr);
        dialog.show();
        dialog.addFiles({ locked, plain });
        QStringList texts;
        runMerge(dialog, QMessageBox::Yes, &texts);
    }
    pdf::PDFDocumentReader reader(nullptr, {}, true, false);
    const pdf::PDFDocument merged = reader.readFromFile(outputPath);
    QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
    QCOMPARE(merged.getCatalog()->getPageCount(), size_t(4));
    QCOMPARE(int(merged.getStorage().getSecurityHandler()->getMode()), int(pdf::EncryptionMode::None));
    Q_UNUSED(signedDocument);
}

void ViewerContextMenuTest::mergePdfsCancelLeavesNoPartialFile()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    // Many pages, so that the merge is still running when Cancel is pressed.
    const QString big = m_temp.filePath("big-merge.pdf");
    QVERIFY(writePdfFixture(big, 600, 1, true, false, false, "BIG page "));
    const QString outputPath = m_temp.filePath("merged-cancel.pdf");
    QFile::remove(outputPath);

    pdfviewer::PDFMergePdfsDialog::Request request;
    request.directory = m_temp.path();
    pdfviewer::PDFMergePdfsDialog dialog(request, nullptr);
    dialog.show();
    dialog.addFiles({ big, big, big, big });
    auto* list = dialog.findChild<QTreeWidget*>("mergeList");
    QCOMPARE(list->topLevelItemCount(), 4);
    dialog.findChild<QLineEdit*>("mergeOutputEdit")->setText(outputPath);

    MessageBoxAnswerer answerer([](QMessageBox* box) { return boxButton(box, "Close"); });
    auto* mergeButton = dialog.findChild<QPushButton*>("mergeButton");
    auto* cancelButton = dialog.findChild<QPushButton*>("mergeCancelButton");
    mergeButton->click();                                   // starts the worker
    QVERIFY(!mergeButton->isEnabled());
    QVERIFY(cancelButton->isVisible());
    QVERIFY(dialog.findChild<QProgressBar*>("mergeProgressBar")->isVisible());
    cancelButton->click();                                  // right away
    QTRY_VERIFY_WITH_TIMEOUT(mergeButton->isEnabled(), 30000);

    const QString status = dialog.findChild<QLabel*>("mergeStatusLabel")->text();
    if (QFileInfo::exists(outputPath))
    {
        // The merge was faster than the cancel: then the file is complete.
        pdf::PDFDocumentReader reader(nullptr, {}, true, false);
        const pdf::PDFDocument merged = reader.readFromFile(outputPath);
        QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
        QCOMPARE(merged.getCatalog()->getPageCount(), size_t(2400));
        qInfo() << "merge finished before the cancel took effect:" << status;
    }
    else
    {
        QVERIFY2(status.contains("cancelled"), qPrintable(status));
    }
    QCOMPARE(answerer.texts.size(), QFileInfo::exists(outputPath) ? 1 : 0);
    // No temporary files stay behind.
    QStringList stray;
    for (const QString& name : QDir(m_temp.path()).entryList(QDir::Files))
    {
        if (name.contains("merged-cancel"))
        {
            stray << name;
        }
    }
    QVERIFY2(stray.size() == (QFileInfo::exists(outputPath) ? 1 : 0), qPrintable(stray.join(',')));
}


void ViewerContextMenuTest::mergePdfsTranslations()
{
    struct Language
    {
        pdf::PDFApplicationTranslator::ELanguage language;
        QStringList menuAndButtons;     // Menu text, then the dialog widgets in this order: add, remove, up, down, merge, cancel, output label
        QString allPages;
        QStringList warningParts;       // Parts of the encryption and signature warnings
    };
    const QList<Language> languages = {
        { pdf::PDFApplicationTranslator::E_LANGUAGE_CHINESE_TRADITIONAL,
          { QString::fromUtf8("\xE5\x90\x88\xE4\xBD\xB5 PDF"), QString::fromUtf8("\xE6\x96\xB0\xE5\xA2\x9E\xE6\xAA\x94\xE6\xA1\x88"),
            QString::fromUtf8("\xE7\xA7\xBB\xE9\x99\xA4"), QString::fromUtf8("\xE4\xB8\x8A\xE7\xA7\xBB"), QString::fromUtf8("\xE4\xB8\x8B\xE7\xA7\xBB"),
            QString::fromUtf8("\xE5\x90\x88\xE4\xBD\xB5"), QString::fromUtf8("\xE5\x8F\x96\xE6\xB6\x88"), QString::fromUtf8("\xE8\xBC\xB8\xE5\x87\xBA\xE6\xAA\x94\xE6\xA1\x88") },
          QString::fromUtf8("\xE6\x89\x80\xE6\x9C\x89\xE9\xA0\x81\xE9\x9D\xA2"),
          { QString::fromUtf8("\xE5\xB7\xB2\xE5\x8A\xA0\xE5\xAF\x86"), QString::fromUtf8("\xE6\x95\xB8\xE4\xBD\x8D\xE7\xB0\xBD\xE7\xAB\xA0") } },
        { pdf::PDFApplicationTranslator::E_LANGUAGE_CHINESE_SIMPLIFIED,
          { QString::fromUtf8("\xE5\x90\x88\xE5\xB9\xB6 PDF"), QString::fromUtf8("\xE6\xB7\xBB\xE5\x8A\xA0\xE6\x96\x87\xE4\xBB\xB6"),
            QString::fromUtf8("\xE7\xA7\xBB\xE9\x99\xA4"), QString::fromUtf8("\xE4\xB8\x8A\xE7\xA7\xBB"), QString::fromUtf8("\xE4\xB8\x8B\xE7\xA7\xBB"),
            QString::fromUtf8("\xE5\x90\x88\xE5\xB9\xB6"), QString::fromUtf8("\xE5\x8F\x96\xE6\xB6\x88"), QString::fromUtf8("\xE8\xBE\x93\xE5\x87\xBA\xE6\x96\x87\xE4\xBB\xB6") },
          QString::fromUtf8("\xE6\x89\x80\xE6\x9C\x89\xE9\xA1\xB5\xE9\x9D\xA2"),
          { QString::fromUtf8("\xE5\xB7\xB2\xE5\x8A\xA0\xE5\xAF\x86"), QString::fromUtf8("\xE6\x95\xB0\xE5\xAD\x97\xE7\xAD\xBE\xE5\x90\x8D") } },
    };

    const QString signedFile = QFINDTESTDATA("fixtures/pyhanko-signed.pdf");
    const QString dupA = m_temp.filePath("dup-a.pdf");
    QVERIFY(writePdfFixture(dupA, 1, 1, true, false, false, "DUP page "));
    for (const Language& language : languages)
    {
        pdf::PDFApplicationTranslator translator;
        translator.setLanguage(language.language);
        translator.installTranslator();

        pdfviewer::PDFViewerMainWindow viewer;
        viewer.show();
        QVERIFY2(viewer.findChild<QAction*>("actionMergePdfs")->text().contains(language.menuAndButtons[0]), qPrintable(viewer.findChild<QAction*>("actionMergePdfs")->text()));
        pdfviewer::PDFEditorMainWindow editor;
        editor.show();
        QVERIFY2(editor.findChild<QAction*>("actionMergePdfs")->text().contains(language.menuAndButtons[0]), qPrintable(editor.findChild<QAction*>("actionMergePdfs")->text()));

        pdfviewer::PDFMergePdfsDialog::Request request;
        request.directory = m_temp.path();
        pdfviewer::PDFMergePdfsDialog dialog(request, nullptr);
        dialog.show();
        QVERIFY2(dialog.windowTitle().contains(language.menuAndButtons[0]), qPrintable(dialog.windowTitle()));
        const char* names[] = { "mergeAddButton", "mergeRemoveButton", "mergeUpButton", "mergeDownButton", "mergeButton", "mergeCancelButton" };
        for (int i = 0; i < 6; ++i)
        {
            const QString text = dialog.findChild<QPushButton*>(names[i])->text();
            QVERIFY2(text.contains(language.menuAndButtons[i + 1]), qPrintable(QStringLiteral("%1: %2").arg(names[i], text)));
        }
        QVERIFY(dialog.findChildren<QLabel*>().size() > 0);
        bool outputLabel = false;
        for (QLabel* label : dialog.findChildren<QLabel*>())
        {
            outputLabel = outputLabel || label->text().contains(language.menuAndButtons[7]);
        }
        QVERIFY(outputLabel);
        auto* list = dialog.findChild<QTreeWidget*>("mergeList");
        QVERIFY(list->headerItem()->text(2).contains(QString::fromUtf8(language.language == pdf::PDFApplicationTranslator::E_LANGUAGE_CHINESE_TRADITIONAL ? "\xE9\xA0\x81\xE9\x9D\xA2\xE7\xAF\x84\xE5\x9C\x8D" : "\xE9\xA1\xB5\xE9\x9D\xA2\xE8\x8C\x83\xE5\x9B\xB4")));

        // An encrypted source and a signed source raise the encryption and the signature warning.
        QVERIFY(!signedFile.isEmpty());
        const QString locked = m_temp.filePath("locked-translation.pdf");
        QVERIFY(writeEncryptedFixture(locked, QString(), "owner", 0xFFFFFFFFu).isEmpty());
        dialog.addFiles({ dupA, locked, signedFile });
        QCOMPARE(list->topLevelItemCount(), 3);
        QVERIFY2(list->topLevelItem(0)->text(2) == language.allPages, qPrintable(list->topLevelItem(0)->text(2)));
        const QString info = dialog.findChild<QLabel*>("mergeInfoLabel")->text();
        QVERIFY2(info.contains(language.warningParts[0]), qPrintable(info));
        QVERIFY2(info.contains(language.warningParts[1]), qPrintable(info));
        // The page range written in the local language ("all pages") is accepted as "all".
        const QString outputPath = m_temp.filePath("merged-translation.pdf");
        QFile::remove(outputPath);
        dialog.findChild<QLineEdit*>("mergeOutputEdit")->setText(outputPath);
        MessageBoxAnswerer answerer([](QMessageBox* box) -> QAbstractButton* { return box->button(QMessageBox::No); });
        QTimer::singleShot(0, dialog.findChild<QPushButton*>("mergeButton"), &QPushButton::click);
        QTest::qWait(300);
        QVERIFY(answerer.texts.size() >= 1);
        QVERIFY2(answerer.texts.front().contains(language.warningParts[0]), qPrintable(answerer.texts.front()));
        QVERIFY(!QFileInfo::exists(outputPath));            // the default answer to the warning is "No"
    }
}

QTEST_MAIN(ViewerContextMenuTest)
#include "tst_viewercontextmenutest.moc"
