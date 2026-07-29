// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfbookmarkmanager.h"
#include "pdfbookmarkmodel.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfoutline.h"
#include "pdfrecoverymanager.h"
#include "pdfsafesaveservice.h"
#include "pdfsessionmanager.h"
#include "pdffamilypdfpaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class BookmarkManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void legacyBookmarksAreMigrated();
    void foldersAndColorsRoundTrip();
    void bookmarkFolderEditingPreservesBookmarks();
    void treeModelExposesFoldersAsParents();
    void sameSizeTimestampExternalChangeIsDetected();
    void safeCommitKeepsLatestThreeBackups();
    void preCommitFailureKeepsOriginalAndCandidate();
    void postCommitFailureKeepsRecoverableBackup();
    void recoveryMetadataRoundTripsWithoutSecrets();
    void sessionPathsAreNormalizedAndMissingFilesAreSkipped();
    void portableModeUsesApplicationDataDirectory();
    void newDestinationCommitIsValidatedAndDurable();
    void embeddedOutlineIsWrittenWithHierarchyColorAndDestinations();
    void standardAnnotationsAreWrittenWithColorsAndText();
};

void BookmarkManagerTest::legacyBookmarksAreMigrated()
{
    QJsonObject legacyBookmark{
        { "isAuto", false },
        { "name", QStringLiteral("第一章") },
        { "pageIndex", 4 }
    };
    QJsonObject legacyDocument{
        { "bookmarks", QJsonArray{ legacyBookmark } }
    };

    const pdfviewer::PDFBookmarkManager::Bookmarks bookmarks =
        pdfviewer::PDFBookmarkManager::bookmarksFromJson(legacyDocument);

    QCOMPARE(bookmarks.folders.size(), std::size_t(0));
    QCOMPARE(bookmarks.bookmarks.size(), std::size_t(1));
    QCOMPARE(bookmarks.bookmarks.front().name, QStringLiteral("第一章"));
    QCOMPARE(bookmarks.bookmarks.front().pageIndex, pdf::PDFInteger(4));
    QVERIFY(!bookmarks.bookmarks.front().id.isEmpty());
    QVERIFY(bookmarks.bookmarks.front().folderId.isEmpty());
    QVERIFY(bookmarks.bookmarks.front().color.isValid());
}

void BookmarkManagerTest::foldersAndColorsRoundTrip()
{
    pdfviewer::PDFBookmarkManager::Bookmarks source;

    pdfviewer::PDFBookmarkManager::BookmarkFolder folder;
    folder.id = QStringLiteral("folder-reading");
    folder.name = QStringLiteral("閱讀進度");
    folder.color = QColor(QStringLiteral("#3366cc"));
    source.folders.push_back(folder);

    pdfviewer::PDFBookmarkManager::Bookmark bookmark;
    bookmark.id = QStringLiteral("bookmark-page-8");
    bookmark.name = QStringLiteral("重要段落");
    bookmark.pageIndex = 7;
    bookmark.color = QColor(QStringLiteral("#cc3366"));
    bookmark.folderId = folder.id;
    source.bookmarks.push_back(bookmark);

    const QJsonObject json = pdfviewer::PDFBookmarkManager::bookmarksToJson(source);
    const pdfviewer::PDFBookmarkManager::Bookmarks restored =
        pdfviewer::PDFBookmarkManager::bookmarksFromJson(json);

    QCOMPARE(restored.folders.size(), std::size_t(1));
    QCOMPARE(restored.folders.front().id, folder.id);
    QCOMPARE(restored.folders.front().name, folder.name);
    QCOMPARE(restored.folders.front().color, folder.color);
    QCOMPARE(restored.bookmarks.size(), std::size_t(1));
    QCOMPARE(restored.bookmarks.front().id, bookmark.id);
    QCOMPARE(restored.bookmarks.front().name, bookmark.name);
    QCOMPARE(restored.bookmarks.front().pageIndex, bookmark.pageIndex);
    QCOMPARE(restored.bookmarks.front().color, bookmark.color);
    QCOMPARE(restored.bookmarks.front().folderId, folder.id);
}

void BookmarkManagerTest::bookmarkFolderEditingPreservesBookmarks()
{
    pdfviewer::PDFBookmarkManager manager(nullptr);
    manager.toggleBookmark(7);

    const QString folderId =
        manager.addFolder(QStringLiteral("待閱讀"), QColor(QStringLiteral("#2255aa")));
    QVERIFY(!folderId.isEmpty());
    QCOMPARE(manager.getFolderCount(), 1);

    const QString bookmarkId = manager.getBookmark(0).id;
    QVERIFY(manager.updateBookmark(bookmarkId,
                                   QStringLiteral("重要段落"),
                                   QColor(QStringLiteral("#cc3366")),
                                   folderId));
    QCOMPARE(manager.getBookmark(0).name, QStringLiteral("重要段落"));
    QCOMPARE(manager.getBookmark(0).color, QColor(QStringLiteral("#cc3366")));
    QCOMPARE(manager.getBookmark(0).folderId, folderId);

    QVERIFY(manager.updateFolder(folderId,
                                 QStringLiteral("稍後閱讀"),
                                 QColor(QStringLiteral("#3366cc"))));
    QCOMPARE(manager.getFolder(0).name, QStringLiteral("稍後閱讀"));

    QVERIFY(manager.removeFolder(folderId));
    QCOMPARE(manager.getFolderCount(), 0);
    QCOMPARE(manager.getBookmarkCount(), 1);
    QVERIFY(manager.getBookmark(0).folderId.isEmpty());
}

void BookmarkManagerTest::treeModelExposesFoldersAsParents()
{
    pdfviewer::PDFBookmarkManager manager(nullptr);
    manager.toggleBookmark(2);
    manager.toggleBookmark(8);
    const QString folderId =
        manager.addFolder(QStringLiteral("重要"), QColor(QStringLiteral("#3366cc")));
    QVERIFY(manager.updateBookmark(manager.getBookmark(1).id,
                                   QStringLiteral("第九頁"),
                                   QColor(QStringLiteral("#cc3366")),
                                   folderId));

    pdfviewer::PDFBookmarkItemModel model(&manager, nullptr);
    QCOMPARE(model.rowCount(QModelIndex()), 2);

    const QModelIndex folderIndex = model.index(0, 0, QModelIndex());
    QVERIFY(model.isFolder(folderIndex));
    QCOMPARE(folderIndex.data(Qt::DisplayRole).toString(), QStringLiteral("重要"));
    QCOMPARE(model.rowCount(folderIndex), 1);

    const QModelIndex childBookmark = model.index(0, 0, folderIndex);
    QCOMPARE(model.getBookmarkIndex(childBookmark), 1);
    QCOMPARE(model.parent(childBookmark), folderIndex);

    const QModelIndex rootBookmark = model.index(1, 0, QModelIndex());
    QCOMPARE(model.getBookmarkIndex(rootBookmark), 0);
    QVERIFY(!model.parent(rootBookmark).isValid());
}

void BookmarkManagerTest::sameSizeTimestampExternalChangeIsDetected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("來源.pdf"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("AAAA", 4), qint64(4));
    source.close();

    const pdfviewer::PDFSafeSaveService::Baseline baseline =
        pdfviewer::PDFSafeSaveService::captureBaseline(sourcePath);
    QVERIFY(baseline.isValid);

    QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(source.write("BBBB", 4), qint64(4));
    source.close();
    QVERIFY(source.open(QIODevice::ReadWrite));
    QVERIFY(source.setFileTime(baseline.lastModifiedUtc, QFileDevice::FileModificationTime));
    source.close();

    QString error;
    QVERIFY(!pdfviewer::PDFSafeSaveService::sourceMatchesBaseline(sourcePath, baseline, &error));
    QVERIFY(error.contains(QStringLiteral("SHA-256")));
}

void BookmarkManagerTest::safeCommitKeepsLatestThreeBackups()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("家庭文件.pdf"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("version-0"), qint64(9));
    source.close();

    auto validator = [](const QString& fileName, QString* error)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly) || file.readAll().isEmpty())
        {
            if (error)
            {
                *error = QStringLiteral("candidate is empty");
            }
            return false;
        }
        return true;
    };

    for (int version = 1; version <= 5; ++version)
    {
        const pdfviewer::PDFSafeSaveService::Baseline baseline =
            pdfviewer::PDFSafeSaveService::captureBaseline(sourcePath);
        QVERIFY(baseline.isValid);

        const QString candidatePath = directory.filePath(
            QStringLiteral(".familypdf-save-%1.tmp").arg(version));
        QFile candidate(candidatePath);
        QVERIFY(candidate.open(QIODevice::WriteOnly));
        const QByteArray content = QStringLiteral("version-%1").arg(version).toUtf8();
        QCOMPARE(candidate.write(content), qint64(content.size()));
        candidate.close();

        const pdfviewer::PDFSafeSaveService::Result result =
            pdfviewer::PDFSafeSaveService::commitCandidate(sourcePath,
                                                            candidatePath,
                                                            baseline,
                                                            validator);
        QCOMPARE(result.status, pdfviewer::PDFSafeSaveService::Status::Success);
    }

    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), QByteArray("version-5"));
    source.close();

    QDir backupDirectory(directory.filePath(QStringLiteral(".FamilyPDFBackup")));
    const QFileInfoList backups = backupDirectory.entryInfoList(
        QStringList{ QStringLiteral("家庭文件.pdf.*.bak") },
        QDir::Files,
        QDir::Time);
    QCOMPARE(backups.size(), 3);
}

void BookmarkManagerTest::preCommitFailureKeepsOriginalAndCandidate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.pdf"));
    const QString candidatePath = directory.filePath(QStringLiteral(".candidate.tmp"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("original"), qint64(8));
    source.close();
    QFile candidate(candidatePath);
    QVERIFY(candidate.open(QIODevice::WriteOnly));
    QCOMPARE(candidate.write("invalid"), qint64(7));
    candidate.close();

    const auto baseline = pdfviewer::PDFSafeSaveService::captureBaseline(sourcePath);
    const auto result = pdfviewer::PDFSafeSaveService::commitCandidate(
        sourcePath,
        candidatePath,
        baseline,
        [](const QString&, QString* error)
        {
            if (error)
            {
                *error = QStringLiteral("validation failed");
            }
            return false;
        });

    QCOMPARE(result.status, pdfviewer::PDFSafeSaveService::Status::CandidateInvalid);
    QVERIFY(QFileInfo::exists(candidatePath));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), QByteArray("original"));
}

void BookmarkManagerTest::postCommitFailureKeepsRecoverableBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.pdf"));
    const QString candidatePath = directory.filePath(QStringLiteral(".candidate.tmp"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("original"), qint64(8));
    source.close();
    QFile candidate(candidatePath);
    QVERIFY(candidate.open(QIODevice::WriteOnly));
    QCOMPARE(candidate.write("replacement"), qint64(11));
    candidate.close();

    int validationCount = 0;
    const auto baseline = pdfviewer::PDFSafeSaveService::captureBaseline(sourcePath);
    const auto result = pdfviewer::PDFSafeSaveService::commitCandidate(
        sourcePath,
        candidatePath,
        baseline,
        [&validationCount](const QString&, QString* error)
        {
            ++validationCount;
            if (validationCount == 1)
            {
                return true;
            }
            if (error)
            {
                *error = QStringLiteral("post-commit validation failed");
            }
            return false;
        });

    QCOMPARE(result.status,
             pdfviewer::PDFSafeSaveService::Status::PostCommitValidationFailed);
    QVERIFY(QFileInfo::exists(result.backupPath));
    QFile backup(result.backupPath);
    QVERIFY(backup.open(QIODevice::ReadOnly));
    QCOMPARE(backup.readAll(), QByteArray("original"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), QByteArray("replacement"));
}

void BookmarkManagerTest::recoveryMetadataRoundTripsWithoutSecrets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("家人文件.pdf"));
    const QString recoveryRoot = directory.filePath(QStringLiteral("recovery"));
    const QString snapshotPath =
        pdfviewer::PDFRecoveryManager::snapshotPath(sourcePath, recoveryRoot);

    QDir().mkpath(QFileInfo(snapshotPath).absolutePath());
    QFile snapshot(snapshotPath);
    QVERIFY(snapshot.open(QIODevice::WriteOnly));
    QCOMPARE(snapshot.write("snapshot"), qint64(8));
    snapshot.close();

    QVERIFY(pdfviewer::PDFRecoveryManager::writeMetadata(sourcePath,
                                                         snapshotPath,
                                                         12,
                                                         recoveryRoot));
    const QList<pdfviewer::PDFRecoveryManager::Record> records =
        pdfviewer::PDFRecoveryManager::findRecords(recoveryRoot);
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.front().sourcePath, QFileInfo(sourcePath).absoluteFilePath());
    QCOMPARE(records.front().snapshotPath, QFileInfo(snapshotPath).absoluteFilePath());
    QCOMPARE(records.front().pageCount, 12);

    QFile metadata(records.front().metadataPath);
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const QByteArray bytes = metadata.readAll();
    QVERIFY(!bytes.contains("password"));
    QVERIFY(!bytes.contains("secret"));

    pdfviewer::PDFRecoveryManager::removeRecord(sourcePath, recoveryRoot);
    QVERIFY(pdfviewer::PDFRecoveryManager::findRecords(recoveryRoot).isEmpty());
    QVERIFY(!QFileInfo::exists(snapshotPath));
}

void BookmarkManagerTest::sessionPathsAreNormalizedAndMissingFilesAreSkipped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first.pdf"));
    const QString secondPath = directory.filePath(QStringLiteral("second.pdf"));
    const QString missingPath = directory.filePath(QStringLiteral("missing.pdf"));
    const QString sessionPath = directory.filePath(QStringLiteral("session.json"));

    for (const QString& path : { firstPath, secondPath })
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("%PDF-1.7\n"), qint64(9));
    }

    QVERIFY(pdfviewer::PDFSessionManager::savePaths(
        { firstPath, secondPath, firstPath, missingPath },
        sessionPath));
    const QStringList restored =
        pdfviewer::PDFSessionManager::loadPaths(sessionPath);

    QCOMPARE(restored,
             QStringList({ QFileInfo(firstPath).absoluteFilePath(),
                           QFileInfo(secondPath).absoluteFilePath() }));
}

void BookmarkManagerTest::portableModeUsesApplicationDataDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString applicationDirectory = directory.filePath(QStringLiteral("app"));
    const QString configDirectory = directory.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(applicationDirectory));

    QCOMPARE(pdfviewer::PDFFamilyPDFPaths::resolveDataRoot(applicationDirectory,
                                                          configDirectory),
             QDir(configDirectory).filePath(QStringLiteral("FamilyPDF")));

    QFile marker(QDir(applicationDirectory).filePath(QStringLiteral("portable.mode")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    QCOMPARE(pdfviewer::PDFFamilyPDFPaths::resolveDataRoot(applicationDirectory,
                                                          configDirectory),
             QDir(applicationDirectory).filePath(QStringLiteral("data")));
}

void BookmarkManagerTest::newDestinationCommitIsValidatedAndDurable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString destinationPath = directory.filePath(QStringLiteral("new.pdf"));
    const QString candidatePath = directory.filePath(QStringLiteral(".new.tmp"));
    QFile candidate(candidatePath);
    QVERIFY(candidate.open(QIODevice::WriteOnly));
    QCOMPARE(candidate.write("validated"), qint64(9));
    candidate.close();

    int validationCount = 0;
    const auto result = pdfviewer::PDFSafeSaveService::commitNewCandidate(
        destinationPath,
        candidatePath,
        [&validationCount](const QString& path, QString*)
        {
            ++validationCount;
            QFile file(path);
            return file.open(QIODevice::ReadOnly) && file.readAll() == QByteArray("validated");
        });

    QCOMPARE(result.status, pdfviewer::PDFSafeSaveService::Status::Success);
    QCOMPARE(validationCount, 2);
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(!QFileInfo::exists(candidatePath));
}

void BookmarkManagerTest::embeddedOutlineIsWrittenWithHierarchyColorAndDestinations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // Use UTF-16 escapes so this interoperability fixture is deterministic
    // even when MSVC runs under a non-UTF-8 Windows system code page.
    const QString folderTitle =
        QString::fromUtf16(u"\u95B1\u8B80\u8CC7\u6599\u593E");
    const QString firstChapterTitle =
        QString::fromUtf16(u"\u7B2C\u4E00\u7AE0");
    const QString secondChapterTitle =
        QString::fromUtf16(u"\u7B2C\u4E8C\u7AE0");

    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference firstPage = builder.appendPage(QRectF(0, 0, 595, 842));
    const pdf::PDFObjectReference secondPage = builder.appendPage(QRectF(0, 0, 595, 842));

    pdf::PDFOutlineItem root;
    QSharedPointer<pdf::PDFOutlineItem> folder(new pdf::PDFOutlineItem());
    folder->setTitle(folderTitle);
    folder->setTextColor(QColor(QStringLiteral("#3366cc")));
    folder->setFontBold(true);

    QSharedPointer<pdf::PDFOutlineItem> firstChapter(new pdf::PDFOutlineItem());
    firstChapter->setTitle(firstChapterTitle);
    firstChapter->setTextColor(QColor(QStringLiteral("#cc3366")));
    firstChapter->setAction(pdf::PDFActionPtr(new pdf::PDFActionGoTo(
        pdf::PDFDestination::createFit(firstPage),
        pdf::PDFDestination())));
    folder->addChild(firstChapter);
    root.addChild(folder);

    QSharedPointer<pdf::PDFOutlineItem> secondChapter(new pdf::PDFOutlineItem());
    secondChapter->setTitle(secondChapterTitle);
    secondChapter->setTextColor(QColor(QStringLiteral("#228833")));
    secondChapter->setAction(pdf::PDFActionPtr(new pdf::PDFActionGoTo(
        pdf::PDFDestination::createFit(secondPage),
        pdf::PDFDestination())));
    root.addChild(secondChapter);
    builder.setOutline(&root);

    pdf::PDFDocument document = builder.build();
    QString outputPath = qEnvironmentVariable("FAMILYPDF_OUTLINE_FIXTURE");
    if (outputPath.isEmpty())
    {
        outputPath = directory.filePath(QStringLiteral("outline-interop.pdf"));
    }
    else
    {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    }

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(writeResult), qPrintable(writeResult.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    pdf::PDFDocument restored = reader.readFromFile(outputPath);
    QCOMPARE(reader.getReadingResult(), pdf::PDFDocumentReader::Result::OK);
    const pdf::PDFOutlineItem* restoredRoot =
        restored.getCatalog()->getOutlineRootPtr().data();
    QVERIFY(restoredRoot);
    QCOMPARE(restoredRoot->getChildCount(), std::size_t(2));

    const pdf::PDFOutlineItem* restoredFolder = restoredRoot->getChild(0);
    QCOMPARE(restoredFolder->getTitle(), folderTitle);
    QCOMPARE(restoredFolder->getTextColor(), QColor(QStringLiteral("#3366cc")));
    QVERIFY(restoredFolder->isFontBold());
    QCOMPARE(restoredFolder->getChildCount(), std::size_t(1));

    const pdf::PDFOutlineItem* restoredFirstChapter = restoredFolder->getChild(0);
    QCOMPARE(restoredFirstChapter->getTitle(), firstChapterTitle);
    QCOMPARE(restoredFirstChapter->getTextColor(), QColor(QStringLiteral("#cc3366")));
    const auto* firstAction =
        dynamic_cast<const pdf::PDFActionGoTo*>(restoredFirstChapter->getAction());
    QVERIFY(firstAction);
    QCOMPARE(firstAction->getDestination().getPageReference(), firstPage);

    const pdf::PDFOutlineItem* restoredSecondChapter = restoredRoot->getChild(1);
    QCOMPARE(restoredSecondChapter->getTitle(), secondChapterTitle);
    QCOMPARE(restoredSecondChapter->getTextColor(), QColor(QStringLiteral("#228833")));
    const auto* secondAction =
        dynamic_cast<const pdf::PDFActionGoTo*>(restoredSecondChapter->getAction());
    QVERIFY(secondAction);
    QCOMPARE(secondAction->getDestination().getPageReference(), secondPage);
}

void BookmarkManagerTest::standardAnnotationsAreWrittenWithColorsAndText()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));

    QVERIFY(builder.createAnnotationHighlight(page,
                                              QRectF(50, 700, 180, 24),
                                              QColor(QStringLiteral("#ffff00"))).isValid());
    QVERIFY(builder.createAnnotationUnderline(page,
                                              QRectF(50, 650, 180, 24),
                                              QColor(QStringLiteral("#00aa44"))).isValid());
    QVERIFY(builder.createAnnotationStrikeout(page,
                                              QRectF(50, 600, 180, 24),
                                              QColor(QStringLiteral("#dd2233"))).isValid());
    QVERIFY(builder.createAnnotationSquare(page,
                                           QRectF(40, 500, 220, 70),
                                           2.0,
                                           QColor(QStringLiteral("#ddeeff")),
                                           QColor(QStringLiteral("#3366cc")),
                                           QStringLiteral("FamilyPDF"),
                                           QStringLiteral("Box"),
                                           QStringLiteral("Box annotation")).isValid());
    QVERIFY(builder.createAnnotationFreeText(page,
                                             QRectF(40, 400, 260, 60),
                                             QStringLiteral("FamilyPDF"),
                                             QStringLiteral("Typed text"),
                                             QStringLiteral("Editable typed annotation"),
                                             Qt::AlignLeft).isValid());
    QVERIFY(builder.createAnnotationText(page,
                                         QRectF(40, 340, 24, 24),
                                         pdf::TextAnnotationIcon::Note,
                                         QStringLiteral("FamilyPDF"),
                                         QStringLiteral("Note"),
                                         QStringLiteral("Sticky note annotation"),
                                         false).isValid());

    pdf::PDFDocument document = builder.build();
    QString outputPath = qEnvironmentVariable("FAMILYPDF_ANNOTATION_FIXTURE");
    if (outputPath.isEmpty())
    {
        outputPath = directory.filePath(QStringLiteral("annotation-interop.pdf"));
    }
    else
    {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    }

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(writeResult), qPrintable(writeResult.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    const pdf::PDFDocument restored = reader.readFromFile(outputPath);
    QVERIFY2(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK,
             qPrintable(reader.getErrorMessage()));
    QCOMPARE(restored.getCatalog()->getPageCount(), pdf::PDFInteger(1));
}

QTEST_MAIN(BookmarkManagerTest)

#include "tst_bookmarkmanagertest.moc"
