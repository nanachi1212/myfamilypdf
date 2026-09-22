// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfviewermainwindow.h"
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
#include <QMenu>
#include <QPainter>
#include <QPluginLoader>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

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
    QList<QByteArray> objects;
    objects << "<< /Type /Catalog /Pages 2 0 R >>"
            << "<< /Type /Pages /Kids [3 0 R 5 0 R 7 0 R] /Count 3 >>";
    for (int page = 0; page < 3; ++page)
    {
        const int contentObject = 4 + page * 2;
        objects << QByteArray("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 420 595] ")
                       + "/Resources << /Font << /F1 9 0 R >> >> /Contents "
                       + QByteArray::number(contentObject) + " 0 R >>";
        const QByteArray stream = "BT /F1 16 Tf 30 535 Td (FamilyPDF smoke page "
                                + QByteArray::number(page + 1) + ") Tj ET\n";
        objects << QByteArray("<< /Length ") + QByteArray::number(stream.size())
                       + " >>\nstream\n" + stream + "endstream";
    }
    objects << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

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
        pdfData += QByteArray::number(offsets[index]).rightJustified(10, '0') + " 00000 n \n";
    pdfData += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1)
             + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    QFile fixture(m_pdfPath);
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    QCOMPARE(fixture.write(pdfData), pdfData.size());
    fixture.close();
    const QString artifactDirectory = qEnvironmentVariable("FAMILYPDF_TEST_ARTIFACT_DIR");
    if (!artifactDirectory.isEmpty())
    {
        QVERIFY(QDir().mkpath(artifactDirectory));
        QVERIFY(QFile::copy(m_pdfPath, QDir(artifactDirectory).filePath("viewer-smoke.pdf")));
    }
}

void ViewerContextMenuTest::init()
{
    qInfo() << "ViewerContextMenuTest: construct window";
    m_window = std::make_unique<pdfviewer::PDFViewerMainWindow>();
    qInfo() << "ViewerContextMenuTest: show window";
    m_window->resize(1100, 900);
    m_window->show();
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
    QVERIFY(action("actionBookmarkPage")->text().contains(QString::fromUtf8("\xE6\x9B\xB8\xE7\xB1\xA4")));
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

QTEST_MAIN(ViewerContextMenuTest)
#include "tst_viewercontextmenutest.moc"
