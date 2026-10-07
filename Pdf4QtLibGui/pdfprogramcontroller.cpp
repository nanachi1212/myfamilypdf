// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfprogramcontroller.h"
#include "pdfdrawwidget.h"
#include "pdfannotation.h"
#include "pdfcompiler.h"
#include "pdfform.h"
#include "pdfdocumentwriter.h"
#include "pdfadvancedtools.h"
#include "pdfdrawspacecontroller.h"
#include "pdfwidgetutils.h"
#include "pdfconstants.h"
#include "pdfdocumentbuilder.h"
#include "pdfcertificatemanagerdialog.h"
#include "pdfwidgetutils.h"

#include "pdfviewersettings.h"
#include "pdfundoredomanager.h"
#include "pdfrendertoimagesdialog.h"
#include "pdfprintdialog.h"
#include "pdfexportimagesdialog.h"
#include "pdfinsertpagesdialog.h"
#include "pdfmergepdfsdialog.h"
#include "pdfpageoutput.h"
#include "pdfpagereorder.h"
#include "pdfpageinserter.h"
#include "pdfjpegimage.h"
#include "pdfoptimizedocumentdialog.h"
#include "pdfoptimizeimagesdialog.h"
#include "pdfsanitizedocumentdialog.h"
#include "pdfcreatebitonaldocumentdialog.h"
#include "pdfviewersettingsdialog.h"
#include "pdfaboutdialog.h"
#include "pdfrenderingerrorswidget.h"
#include "pdfpagegeometrydialog.h"
#include "pdfsendmail.h"
#include "pdfrecentfilemanager.h"
#include "pdftexttospeech.h"
#include "pdfencryptionsettingsdialog.h"
#include "pdfwidgetannotation.h"
#include "pdfwidgetformmanager.h"
#include "pdfactioncombobox.h"
#include "pdffullscreenwidget.h"
#include "pdfpagegeometry.h"
#include "pdfdocumentmanipulator.h"

#include <cstdio>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>
#include <memory>

#include <QMenu>
#include <QPrinter>
#include <QProgressDialog>
#include <QScopeGuard>
#include <QMessageBox>
#include <QProcess>
#include <QDesktopServices>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrent>
#include <QInputDialog>
#include <QMainWindow>
#include <QToolBar>
#include <QXmlStreamWriter>
#include <QMenuBar>
#include <QComboBox>
#include <QCryptographicHash>
#include <QScrollBar>
#include <QTimer>

#include "pdfdbgheap.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "config.h"

#if defined(PDF4QT_USE_PRAGMA_LIB)
#pragma comment(lib, "Shell32")
#endif

namespace pdfviewer
{

PDFActionManager::PDFActionManager(QObject* parent) :
    BaseClass(parent),
    m_actions(),
    m_actionGroups()
{

}

QToolButton* PDFActionManager::createToolButtonForActionGroup(ActionGroup group, QWidget* parent) const
{
    QActionGroup* actionGroup = getActionGroup(group);

    if (!actionGroup)
    {
        return nullptr;
    }

    QToolButton* toolButton = new QToolButton(parent);
    toolButton->setPopupMode(QToolButton::MenuButtonPopup);
    toolButton->setMenu(new QMenu(toolButton));
    QList<QAction*> actions = actionGroup->actions();
    auto onInsertStickyNotesActionTriggered = [toolButton](QAction* action)
    {
        if (action)
        {
            toolButton->setDefaultAction(action);
        }
    };
    connect(actionGroup, &QActionGroup::triggered, toolButton, onInsertStickyNotesActionTriggered);
    for (QAction* action : actions)
    {
        toolButton->menu()->addAction(action);
    }
    toolButton->setDefaultAction(actions.front());

    return toolButton;
}

void PDFActionManager::setShortcut(Action type, QKeySequence sequence)
{
    if (QAction* action = getAction(type))
    {
        action->setShortcut(sequence);
    }
}

void PDFActionManager::setUserData(Action type, QVariant userData)
{
    if (QAction* action = getAction(type))
    {
        action->setData(userData);
    }
}

void PDFActionManager::setEnabled(Action type, bool enabled)
{
    if (QAction* action = getAction(type))
    {
        action->setEnabled(enabled);
    }
}

void PDFActionManager::setChecked(PDFActionManager::Action type, bool checked)
{
    if (QAction* action = getAction(type))
    {
        action->setChecked(checked);
    }
}

std::vector<QAction*> PDFActionManager::getRenderingOptionActions() const
{
    return getActionList({
         RenderOptionAntialiasing,
         RenderOptionTextAntialiasing,
         RenderOptionSmoothPictures,
         RenderOptionIgnoreOptionalContentSettings,
         RenderOptionDisplayRenderTimes,
         RenderOptionDisplayAnnotations,
         RenderOptionInvertColors,
         RenderOptionGrayscale,
         RenderOptionBitonal,
         RenderOptionHighContrast,
         RenderOptionCustomColors,
         RenderOptionShowTextBlocks,
         RenderOptionShowTextLines});
}

std::vector<QAction*> PDFActionManager::getActions() const
{
    std::vector<QAction*> result;
    result.reserve(LastAction + m_additionalActions.size());

    for (int i = 0; i < LastAction; ++i)
    {
        if (QAction* action = getAction(Action(i)))
        {
            result.push_back(action);
        }
    }

    result.insert(result.cend(), m_additionalActions.cbegin(), m_additionalActions.cend());
    return result;
}

void PDFActionManager::addAdditionalAction(QAction* action)
{
    m_additionalActions.push_back(action);
}

void PDFActionManager::initActions(QSize iconSize, bool initializeStampActions)
{
    setShortcut(Open, QKeySequence::Open);
#ifdef Q_OS_WIN
    setShortcut(Close, QKeyCombination(Qt::CTRL, Qt::Key_W));
    setShortcut(Quit, QKeyCombination(Qt::CTRL, Qt::Key_F4));
#else
    setShortcut(Close, QKeySequence::Close);
    setShortcut(Quit, QKeySequence::Quit);
#endif
    setShortcut(ZoomIn, QKeySequence::ZoomIn);
    setShortcut(ZoomOut, QKeySequence::ZoomOut);
    setShortcut(Find, QKeySequence::Find);
    setShortcut(FindPrevious, QKeySequence::FindPrevious);
    setShortcut(FindNext, QKeySequence::FindNext);
    setShortcut(SelectTextAll, QKeySequence::SelectAll);
    setShortcut(DeselectText, QKeySequence::Deselect);
    setShortcut(CopyText, QKeySequence::Copy);
    setShortcut(RotateRight, QKeySequence("Ctrl+Shift++"));
    setShortcut(RotateLeft, QKeySequence("Ctrl+Shift+-"));
    setShortcut(Print, QKeySequence::Print);
    setShortcut(Undo, QKeySequence::Undo);
    setShortcut(Redo, QKeySequence::Redo);
    setShortcut(Save, QKeySequence::Save);
    setShortcut(SaveAs, QKeySequence::SaveAs);
    setShortcut(GoToDocumentStart, QKeySequence::MoveToStartOfDocument);
    setShortcut(GoToDocumentEnd, QKeySequence::MoveToEndOfDocument);
    setShortcut(GoToNextPage, QKeySequence::MoveToNextPage);
    setShortcut(GoToPreviousPage, QKeySequence::MoveToPreviousPage);
    setShortcut(GoToNextLine, QKeySequence::MoveToNextLine);
    setShortcut(GoToPreviousLine, QKeySequence::MoveToPreviousLine);
    setShortcut(BookmarkPage, QKeySequence("Ctrl+M"));
    setShortcut(BookmarkGoToNext, QKeySequence("Ctrl+."));
    setShortcut(BookmarkGoToPrevious, QKeySequence("Ctrl+,"));
    setShortcut(FullscreenMode, QKeySequence("Ctrl+L"));
    setShortcut(PageGeometry, QKeySequence("Ctrl+Shift+R"));

    if (hasActions({ CreateStickyNoteComment, CreateStickyNoteHelp, CreateStickyNoteInsert, CreateStickyNoteKey, CreateStickyNoteNewParagraph, CreateStickyNoteNote, CreateStickyNoteParagraph }))
    {
        m_actionGroups[CreateStickyNoteGroup] = new QActionGroup(this);
        m_actionGroups[CreateStickyNoteGroup]->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteComment));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteHelp));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteInsert));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteKey));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteNewParagraph));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteNote));
        m_actionGroups[CreateStickyNoteGroup]->addAction(getAction(CreateStickyNoteParagraph));

        getAction(CreateStickyNoteComment)->setData(int(pdf::TextAnnotationIcon::Comment));
        getAction(CreateStickyNoteHelp)->setData(int(pdf::TextAnnotationIcon::Help));
        getAction(CreateStickyNoteInsert)->setData(int(pdf::TextAnnotationIcon::Insert));
        getAction(CreateStickyNoteKey)->setData(int(pdf::TextAnnotationIcon::Key));
        getAction(CreateStickyNoteNewParagraph)->setData(int(pdf::TextAnnotationIcon::NewParagraph));
        getAction(CreateStickyNoteNote)->setData(int(pdf::TextAnnotationIcon::Note));
        getAction(CreateStickyNoteParagraph)->setData(int(pdf::TextAnnotationIcon::Paragraph));

        getAction(CreateStickyNoteComment)->setIcon(pdf::PDFTextAnnotation::createIcon("Comment", iconSize));
        getAction(CreateStickyNoteHelp)->setIcon(pdf::PDFTextAnnotation::createIcon("Help", iconSize));
        getAction(CreateStickyNoteInsert)->setIcon(pdf::PDFTextAnnotation::createIcon("Insert", iconSize));
        getAction(CreateStickyNoteKey)->setIcon(pdf::PDFTextAnnotation::createIcon("Key", iconSize));
        getAction(CreateStickyNoteNewParagraph)->setIcon(pdf::PDFTextAnnotation::createIcon("NewParagraph", iconSize));
        getAction(CreateStickyNoteNote)->setIcon(pdf::PDFTextAnnotation::createIcon("Note", iconSize));
        getAction(CreateStickyNoteParagraph)->setIcon(pdf::PDFTextAnnotation::createIcon("Paragraph", iconSize));
    }

    if (hasActions({ CreateTextHighlight, CreateTextUnderline, CreateTextStrikeout, CreateTextSquiggly }))
    {
        m_actionGroups[CreateTextHighlightGroup] = new QActionGroup(this);
        m_actionGroups[CreateTextHighlightGroup]->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        m_actionGroups[CreateTextHighlightGroup]->addAction(getAction(CreateTextHighlight));
        m_actionGroups[CreateTextHighlightGroup]->addAction(getAction(CreateTextUnderline));
        m_actionGroups[CreateTextHighlightGroup]->addAction(getAction(CreateTextStrikeout));
        m_actionGroups[CreateTextHighlightGroup]->addAction(getAction(CreateTextSquiggly));

        getAction(CreateTextHighlight)->setData(int(pdf::AnnotationType::Highlight));
        getAction(CreateTextUnderline)->setData(int(pdf::AnnotationType::Underline));
        getAction(CreateTextStrikeout)->setData(int(pdf::AnnotationType::StrikeOut));
        getAction(CreateTextSquiggly)->setData(int(pdf::AnnotationType::Squiggly));
    }

    if (hasActions({ CreateHyperlinkToThisPDFFit, CreateHyperlinkToThisPDFFitH, CreateHyperlinkToThisPDFFitV, CreateHyperlinkToThisPDFFitR,
                     CreateHyperlinkToThisPDFFitB, CreateHyperlinkToThisPDFFitBH, CreateHyperlinkToThisPDFFitBV, CreateHyperlinkToThisPDFXYZ,
                     CreateHyperlinkToThisPDFXYZInheritZoom }))
    {
        m_actionGroups[CreateInDocumentHyperlinkGroup] = new QActionGroup(this);
        m_actionGroups[CreateInDocumentHyperlinkGroup]->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFit));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitH));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitV));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitR));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitB));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitBH));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFFitBV));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFXYZ));
        m_actionGroups[CreateInDocumentHyperlinkGroup]->addAction(getAction(CreateHyperlinkToThisPDFXYZInheritZoom));

        getAction(CreateHyperlinkToThisPDFFit)->setData(int(pdf::DestinationType::Fit));
        getAction(CreateHyperlinkToThisPDFFitH)->setData(int(pdf::DestinationType::FitH));
        getAction(CreateHyperlinkToThisPDFFitV)->setData(int(pdf::DestinationType::FitV));
        getAction(CreateHyperlinkToThisPDFFitR)->setData(int(pdf::DestinationType::FitR));
        getAction(CreateHyperlinkToThisPDFFitB)->setData(int(pdf::DestinationType::FitB));
        getAction(CreateHyperlinkToThisPDFFitBH)->setData(int(pdf::DestinationType::FitBH));
        getAction(CreateHyperlinkToThisPDFFitBV)->setData(int(pdf::DestinationType::FitBV));
        getAction(CreateHyperlinkToThisPDFXYZ)->setData(int(pdf::DestinationType::XYZ));
        getAction(CreateHyperlinkToThisPDFXYZInheritZoom)->setData(int(pdf::DestinationType::XYZ));
        getAction(CreateHyperlinkToThisPDFXYZInheritZoom)->setProperty("inheritZoom", true);
    }

    setUserData(RenderOptionAntialiasing, pdf::PDFRenderer::Antialiasing);
    setUserData(RenderOptionTextAntialiasing, pdf::PDFRenderer::TextAntialiasing);
    setUserData(RenderOptionSmoothPictures, pdf::PDFRenderer::SmoothImages);
    setUserData(RenderOptionIgnoreOptionalContentSettings, pdf::PDFRenderer::IgnoreOptionalContent);
    setUserData(RenderOptionDisplayRenderTimes, pdf::PDFRenderer::DisplayTimes);
    setUserData(RenderOptionDisplayAnnotations, pdf::PDFRenderer::DisplayAnnotations);
    setUserData(RenderOptionInvertColors, pdf::PDFRenderer::ColorAdjust_Invert);
    setUserData(RenderOptionGrayscale, pdf::PDFRenderer::ColorAdjust_Grayscale);
    setUserData(RenderOptionBitonal, pdf::PDFRenderer::ColorAdjust_Bitonal);
    setUserData(RenderOptionHighContrast, pdf::PDFRenderer::ColorAdjust_HighContrast);
    setUserData(RenderOptionCustomColors, pdf::PDFRenderer::ColorAdjust_CustomColors);
    setUserData(RenderOptionShowTextBlocks, pdf::PDFRenderer::DebugTextBlocks);
    setUserData(RenderOptionShowTextLines, pdf::PDFRenderer::DebugTextLines);

    if (initializeStampActions)
    {
        m_actionGroups[CreateStampGroup] = new QActionGroup(this);
        m_actionGroups[CreateStampGroup]->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

        auto createCreateStampAction = [this](Action actionType, pdf::Stamp stamp)
        {
            QString text = pdf::PDFStampAnnotation::getText(stamp, true);
            QAction* action = new QAction(text, this);
            action->setObjectName(QString("actionCreateStamp_%1").arg(int(stamp)));
            action->setData(int(stamp));
            action->setCheckable(true);
            m_actions[actionType] = action;
            m_actionGroups[CreateStampGroup]->addAction(action);
        };

        createCreateStampAction(CreateStampApproved, pdf::Stamp::Approved);
        createCreateStampAction(CreateStampAsIs, pdf::Stamp::AsIs);
        createCreateStampAction(CreateStampConfidential, pdf::Stamp::Confidential);
        createCreateStampAction(CreateStampDepartmental, pdf::Stamp::Departmental);
        createCreateStampAction(CreateStampDraft, pdf::Stamp::Draft);
        createCreateStampAction(CreateStampExperimental, pdf::Stamp::Experimental);
        createCreateStampAction(CreateStampExpired, pdf::Stamp::Expired);
        createCreateStampAction(CreateStampFinal, pdf::Stamp::Final);
        createCreateStampAction(CreateStampForComment, pdf::Stamp::ForComment);
        createCreateStampAction(CreateStampForPublicRelease, pdf::Stamp::ForPublicRelease);
        createCreateStampAction(CreateStampNotApproved, pdf::Stamp::NotApproved);
        createCreateStampAction(CreateStampNotForPublicRelease, pdf::Stamp::NotForPublicRelease);
        createCreateStampAction(CreateStampSold, pdf::Stamp::Sold);
        createCreateStampAction(CreateStampTopSecret, pdf::Stamp::TopSecret);
    }

    m_iconSize = iconSize;
}

void PDFActionManager::styleActions()
{
    if (pdf::PDFWidgetUtils::isDarkTheme())
    {
        qreal devicePixelRatio = qGuiApp->devicePixelRatio();

        // Convert icons to dark theme icons
        pdf::PDFWidgetUtils::convertActionsForDarkTheme(m_actions, m_iconSize, devicePixelRatio);
        pdf::PDFWidgetUtils::convertActionsForDarkTheme(m_additionalActions, m_iconSize, devicePixelRatio);
    }
}

bool PDFActionManager::hasActions(const std::initializer_list<Action>& actionTypes) const
{
    for (Action actionType : actionTypes)
    {
        if (!getAction(actionType))
        {
            return false;
        }
    }

    return true;
}

std::vector<QAction*> PDFActionManager::getActionList(const std::initializer_list<PDFActionManager::Action>& actionTypes) const
{
    std::vector<QAction*> result;
    result.reserve(actionTypes.size());

    for (const Action actionType : actionTypes)
    {
        if (QAction* action = getAction(actionType))
        {
            result.push_back(action);
        }
    }

    return result;
}

PDFProgramController::PDFProgramController(QObject* parent) :
    BaseClass(parent),
    m_actionManager(nullptr),
    m_mainWindow(nullptr),
    m_mainWindowInterface(nullptr),
    m_pdfWidget(nullptr),
    m_settings(new PDFViewerSettings(this)),
    m_undoRedoManager(nullptr),
    m_recentFileManager(new PDFRecentFileManager(this)),
    m_optionalContentActivity(nullptr),
    m_textToSpeech(nullptr),
    m_isDocumentSetInProgress(false),
    m_futureWatcher(nullptr),
    m_recoveryTimer(new QTimer(this)),
    m_recoveryWatcher(new QFutureWatcher<QString>(this)),
    m_recoveryPending(false),
    m_openedRecoverySourcePath(),
    m_CMSManager(new pdf::PDFCMSManager(this)),
    m_toolManager(nullptr),
    m_annotationManager(nullptr),
    m_formManager(nullptr),
    m_bookmarkManager(nullptr),
    m_actionComboBox(nullptr),
    m_isBusy(false),
    m_isFactorySettingsBeingRestored(false),
    m_progress(nullptr),
    m_loadAllPlugins(false),
    m_isFullscreenMode(false),
    m_fullscreenWidget(nullptr)
{
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &PDFProgramController::onFileChanged);
    m_recoveryTimer->setSingleShot(true);
    m_recoveryTimer->setInterval(5000);
    connect(m_recoveryTimer, &QTimer::timeout, this, &PDFProgramController::startRecoverySnapshot);
    connect(m_recoveryWatcher, &QFutureWatcher<QString>::finished, this, [this]()
    {
        const QString recoveredSource = m_recoveryWatcher->result();
        const bool currentDocumentIsSaved =
            m_undoRedoManager && m_undoRedoManager->isCurrentSaved();
        if (!recoveredSource.isEmpty() &&
            (m_fileInfo.absoluteFilePath.compare(recoveredSource, Qt::CaseInsensitive) != 0 ||
             currentDocumentIsSaved))
        {
            PDFRecoveryManager::removeRecord(recoveredSource);
        }

        if (m_recoveryPending)
        {
            m_recoveryPending = false;
            scheduleRecoverySnapshot();
        }
    });
}

PDFProgramController::~PDFProgramController()
{
    delete m_formManager;
    m_formManager = nullptr;

    delete m_annotationManager;
    m_annotationManager = nullptr;

    delete m_bookmarkManager;
    m_bookmarkManager = nullptr;
}

void PDFProgramController::initializeAnnotationManager()
{
    m_annotationManager = new pdf::PDFWidgetAnnotationManager(m_pdfWidget->getDrawWidgetProxy(), this);
    connect(m_annotationManager, &pdf::PDFWidgetAnnotationManager::actionTriggered, this, &PDFProgramController::onActionTriggered);
    connect(m_annotationManager, &pdf::PDFWidgetAnnotationManager::documentModified, this, &PDFProgramController::onDocumentModified);
    m_pdfWidget->setAnnotationManager(m_annotationManager);
}

void PDFProgramController::initializeFormManager()
{
    m_formManager = new pdf::PDFWidgetFormManager(m_pdfWidget->getDrawWidgetProxy(), this);
    m_formManager->setAnnotationManager(m_annotationManager);
    m_formManager->setAppearanceFlags(m_settings->getSettings().m_formAppearanceFlags);
    m_annotationManager->setFormManager(m_formManager);
    m_pdfWidget->setFormManager(m_formManager);
    connect(m_formManager, &pdf::PDFFormManager::actionTriggered, this, &PDFProgramController::onActionTriggered);
    connect(m_formManager, &pdf::PDFFormManager::documentModified, this, &PDFProgramController::onDocumentModified);
}

void PDFProgramController::initializeBookmarkManager()
{
    m_bookmarkManager = new PDFBookmarkManager(this);
    connect(m_bookmarkManager, &PDFBookmarkManager::bookmarkActivated, this, &PDFProgramController::onBookmarkActivated);
    updateBookmarkSettings();
}

void PDFProgramController::initialize(Features features,
                                      QMainWindow* mainWindow,
                                      IMainWindow* mainWindowInterface,
                                      PDFActionManager* actionManager,
                                      pdf::PDFProgress* progress)
{
    Q_ASSERT(!m_actionManager);
    m_actionManager = actionManager;
    m_progress = progress;
    m_mainWindow = mainWindow;
    m_mainWindowInterface = mainWindowInterface;

    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToDocumentStart))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToDocumentStartTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToDocumentEnd))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToDocumentEndTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToNextPage))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToNextPageTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToPreviousPage))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToPreviousPageTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToNextLine))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToNextLineTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GoToPreviousLine))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGoToPreviousLineTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ZoomIn))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionZoomIn);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ZoomOut))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionZoomOut);
    }
    for (QAction* action : m_actionManager->getRenderingOptionActions())
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRenderingOptionTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Print))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::performPrint);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Save))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::performSave);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::SaveAs))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::performSaveAs);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::RotateLeft))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRotateLeftTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::RotateRight))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRotateRightTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Properties))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPropertiesTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::About))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionAboutTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::SendByMail))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionSendByEMailTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::RenderToImages))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRenderToImagesTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ExportPageImages))
    {
        connect(action, &QAction::triggered, this, [this]() { exportPagesAsImages(); });
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::MergePdfs))
    {
        connect(action, &QAction::triggered, this, [this]() { mergePdfs(); });
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Optimize))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionOptimizeTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::OptimizeImages))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionOptimizeImagesTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Sanitize))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionSanitizeTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::RemoveExternalLinks))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRemoveExternalLinksTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageGeometry))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPageGeometryTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateBitonalDocument))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionCreateBitonalDocumentTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Encryption))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionEncryptionTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::FitPage))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFitPageTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::FitWidth))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFitWidthTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::FitHeight))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFitHeightTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ShowRenderingErrors))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionRenderingErrorsTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageLayoutSinglePage))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPageLayoutSinglePageTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageLayoutContinuous))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPageLayoutContinuousTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageLayoutTwoPages))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPageLayoutTwoPagesTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageLayoutTwoColumns))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionPageLayoutTwoColumnsTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::FullscreenMode))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFullscreenModeTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::PageLayoutFirstPageOnRightSide))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFirstPageOnRightSideTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Find))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionFindTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Options))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionOptionsTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ResetToFactorySettings))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::resetSettings);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::ClearRecentFileHistory))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::clearRecentFileHistory);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CertificateManager))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionCertificateManagerTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Open))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionOpenTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::Close))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionCloseTriggered);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::GetSource))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionGetSource);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BecomeSponsor))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBecomeSponsor);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::AutomaticDocumentRefresh))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionAutomaticDocumentRefresh);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkPage))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkPage);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkGoToNext))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkGoToNext);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkGoToPrevious))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkGoToPrevious);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkExport))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkExport);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkImport))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkImport);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkGenerateAutomatically))
    {
        connect(action, &QAction::triggered, this, &PDFProgramController::onActionBookmarkGenerateAutomatically);
    }

    if (m_recentFileManager)
    {
        connect(m_recentFileManager, &PDFRecentFileManager::fileOpenRequest, this, &PDFProgramController::openDocument);

        for (QAction* action : m_recentFileManager->getActions())
        {
            m_actionManager->addAdditionalAction(action);
        }
    }

    readSettings(Settings(GeneralSettings | PluginsSettings | RecentFileSettings | CertificateSettings));

    m_pdfWidget = new pdf::PDFWidget(m_CMSManager, m_settings->getRendererEngine(), m_mainWindow);
    m_pdfWidget->setObjectName("pdfWidget");
    m_pdfWidget->updateCacheLimits(qsizetype(m_settings->getCompiledPageCacheLimit() * 1024LL), m_settings->getThumbnailsCacheLimit(), m_settings->getFontCacheLimit(), m_settings->getInstancedFontCacheLimit());
    m_pdfWidget->getDrawWidgetProxy()->setProgress(m_progress);

    connect(this, &PDFProgramController::queryPasswordRequest, this, &PDFProgramController::onQueryPasswordRequest, Qt::BlockingQueuedConnection);
    connect(m_pdfWidget->getDrawWidgetProxy(), &pdf::PDFDrawWidgetProxy::drawSpaceChanged, this, &PDFProgramController::onDrawSpaceChanged);
    connect(m_pdfWidget->getDrawWidgetProxy(), &pdf::PDFDrawWidgetProxy::pageLayoutChanged, this, &PDFProgramController::onPageLayoutChanged);
    connect(m_pdfWidget, &pdf::PDFWidget::pageRenderingErrorsChanged, this, &PDFProgramController::onPageRenderingErrorsChanged, Qt::QueuedConnection);
    connect(m_settings, &PDFViewerSettings::settingsChanged, this, &PDFProgramController::onViewerSettingsChanged);
    connect(m_CMSManager, &pdf::PDFCMSManager::colorManagementSystemChanged, this, &PDFProgramController::onColorManagementSystemChanged);

    if (features.testFlag(TextToSpeech))
    {
        m_textToSpeech = new PDFTextToSpeech(this);
        m_textToSpeech->setProxy(m_pdfWidget->getDrawWidgetProxy());
        m_textToSpeech->setSettings(m_settings);
    }

    initializeAnnotationManager();
    m_annotationManager->setEditingEnabled(features.testFlag(UndoRedo));
    initializeBookmarkManager();

    if (features.testFlag(Forms))
    {
        initializeFormManager();
    }

    if (features.testFlag(Tools))
    {
        initializeToolManager();
    }

    if (features.testFlag(UndoRedo))
    {
        // Connect undo/redo manager
        m_undoRedoManager = new PDFUndoRedoManager(this);
        connect(m_undoRedoManager, &PDFUndoRedoManager::undoRedoStateChanged, this, &PDFProgramController::updateUndoRedoActions);
        connect(m_undoRedoManager, &PDFUndoRedoManager::documentChangeRequest, this, &PDFProgramController::onDocumentUndoRedo);
        connect(m_actionManager->getAction(PDFActionManager::Undo), &QAction::triggered, this, [this]() {
            if (m_formManager) m_formManager->setFocusToEditor(nullptr);
            m_undoRedoManager->doUndo();
        });
        connect(m_actionManager->getAction(PDFActionManager::Redo), &QAction::triggered, this, [this]() {
            if (m_formManager) m_formManager->setFocusToEditor(nullptr);
            m_undoRedoManager->doRedo();
        });
        updateUndoRedoSettings();
    }

    if (features.testFlag(Plugins))
    {
        loadPlugins();
    }
}

void PDFProgramController::initActionComboBox(PDFActionComboBox* comboBox)
{
    m_actionComboBox = comboBox;

    if (m_actionComboBox)
    {
        bool updatesEnabled = m_actionComboBox->updatesEnabled();
        m_actionComboBox->setUpdatesEnabled(false);

        for (QAction* action : m_actionManager->getActions())
        {
            m_actionComboBox->addQuickFindAction(action);
        }

        m_actionComboBox->setUpdatesEnabled(updatesEnabled);
    }
}

void PDFProgramController::finishInitialization()
{
    readSettings(Settings(WindowSettings | ActionSettings));

    if (m_textToSpeech)
    {
        m_textToSpeech->setSettings(m_settings);
    }

    updatePageLayoutActions();
    m_mainWindowInterface->updateUI(true);
    onViewerSettingsChanged();
    updateActionsAvailability();
}

void PDFProgramController::performPrint()
{
    runPrintWorkflow(m_mainWindowInterface->getSelectedPages(), false);
}

void PDFProgramController::printPages(const std::vector<pdf::PDFInteger>& pageIndices)
{
    runPrintWorkflow(pageIndices, true);
}

void PDFProgramController::runPrintWorkflow(const std::vector<pdf::PDFInteger>& selectedPages, bool preferSelectedPages)
{
    if (!m_pdfDocument)
    {
        return;
    }

    // Printing and exporting keep pointers into the current document (also in their worker threads),
    // so automatic reload must not replace it while the workflow is open.
    const pdf::PDFDocumentPointer documentKeepAlive = m_pdfDocument;
    m_isOutputWorkflowActive = true;
    const auto outputWorkflowGuard = qScopeGuard([this]() { m_isOutputWorkflowActive = false; });

    // Are we allowed to print in high resolution? If yes, then print in high resolution,
    // otherwise print in low resolution. If this action is triggered, then print operation
    // should be allowed (at least print in low resolution).
    const pdf::PDFObjectStorage& storage = m_pdfDocument->getStorage();
    const pdf::PDFSecurityHandler* securityHandler = storage.getSecurityHandler();
    QPrinter::PrinterMode printerMode = QPrinter::HighResolution;
    if (!securityHandler->isAllowed(pdf::PDFSecurityHandler::Permission::PrintHighResolution))
    {
        printerMode = QPrinter::ScreenResolution;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    PDFPrintDialog::PageSelectionInfo pageInfo;
    pageInfo.pageCount = pageCount;
    // The current page is the first visible page (continuous layouts show several).
    pageInfo.currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (pageInfo.currentPages.size() > 1)
    {
        pageInfo.currentPages.resize(1);
    }
    for (const pdf::PDFInteger pageIndex : selectedPages)
    {
        if (pageIndex >= 0 && pageIndex < pageCount)
        {
            pageInfo.selectedPages.push_back(pageIndex);
        }
    }
    pageInfo.preferSelectedPages = preferSelectedPages;

    // The dialog only enumerates printers and renders a single preview page,
    // so opening it does not depend on the size of the document.
    const PDFPageOutputContext outputContext = PDFPageOutputContext::fromProxy(m_pdfDocument.data(), m_pdfWidget->getDrawWidgetProxy());
    PDFPrintDialog printDialog(outputContext, pageInfo, QFileInfo(getOriginalFileName()).fileName(), printerMode, m_mainWindow);
    if (printDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const PDFPrintOptions options = printDialog.getOptions();
    std::unique_ptr<QPrinter> printer = printDialog.takePrinter();
    if (!printer || options.pageIndices.empty())
    {
        return;
    }

    QProgressDialog progressDialog(tr("Printing document"), tr("Cancel"), 0, int(options.pageIndices.size()), m_mainWindow);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);
    progressDialog.setValue(0);

    auto onProgress = [&progressDialog](int done, int total)
    {
        progressDialog.setMaximum(total);
        progressDialog.setValue(done);
        QCoreApplication::processEvents();
        return !progressDialog.wasCanceled();
    };
    const PDFPrintResult result = PDFPageOutput::print(printer.get(), outputContext, options, onProgress);
    progressDialog.close();

    if (result.cancelled)
    {
        m_mainWindowInterface->setStatusBarMessage(tr("Printing was cancelled."), 5000);
        return;
    }

    if (!result.completed)
    {
        QMessageBox::warning(m_mainWindow, tr("Print"), result.errorMessage.isEmpty() ? tr("Printing failed.") : result.errorMessage);
        return;
    }

    if (!result.renderWarnings.isEmpty())
    {
        QMessageBox::warning(m_mainWindow, tr("Print"), tr("The document was printed, but some content could not be rendered:\n%1").arg(result.renderWarnings.join(QLatin1Char('\n'))));
        return;
    }

    m_mainWindowInterface->setStatusBarMessage(tr("%1 page(s) were sent to the printer.").arg(result.pagesPrinted), 5000);
}

void PDFProgramController::exportPagesAsImages()
{
    runExportImagesWorkflow(m_mainWindowInterface->getSelectedPages(), false);
}

void PDFProgramController::exportPagesAsImages(const std::vector<pdf::PDFInteger>& pageIndices)
{
    runExportImagesWorkflow(pageIndices, true);
}

void PDFProgramController::runExportImagesWorkflow(const std::vector<pdf::PDFInteger>& selectedPages, bool preferSelectedPages)
{
    if (!m_pdfDocument)
    {
        return;
    }

    // Printing and exporting keep pointers into the current document (also in their worker threads),
    // so automatic reload must not replace it while the workflow is open.
    const pdf::PDFDocumentPointer documentKeepAlive = m_pdfDocument;
    m_isOutputWorkflowActive = true;
    const auto outputWorkflowGuard = qScopeGuard([this]() { m_isOutputWorkflowActive = false; });

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    PDFExportImagesDialog::Request request;
    request.outputContext = PDFPageOutputContext::fromProxy(m_pdfDocument.data(), m_pdfWidget->getDrawWidgetProxy());
    request.documentFileName = getOriginalFileName();
    request.pageCount = pageCount;
    // The current page is the first visible page (continuous layouts show several).
    request.currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (request.currentPages.size() > 1)
    {
        request.currentPages.resize(1);
    }
    for (const pdf::PDFInteger pageIndex : selectedPages)
    {
        if (pageIndex >= 0 && pageIndex < pageCount)
        {
            request.selectedPages.push_back(pageIndex);
        }
    }
    request.preferSelectedPages = preferSelectedPages;

    PDFExportImagesDialog dialog(request, m_mainWindow);
    dialog.exec();
}

void PDFProgramController::exportSelectionAsImage()
{
    if (!m_pdfDocument || !m_toolManager)
    {
        return;
    }

    // Printing and exporting keep pointers into the current document (also in their worker threads),
    // so automatic reload must not replace it while the workflow is open.
    const pdf::PDFDocumentPointer documentKeepAlive = m_pdfDocument;
    m_isOutputWorkflowActive = true;
    const auto outputWorkflowGuard = qScopeGuard([this]() { m_isOutputWorkflowActive = false; });

    const pdf::PDFTextSelection selection = m_toolManager->getSelectedText();
    if (selection.isEmpty())
    {
        m_mainWindowInterface->setStatusBarMessage(tr("Select some text first, then export the selection as an image."), 5000);
        return;
    }

    // The area of the selection on each page is the bounding box of the selected text,
    // the same geometry the text markup tools use.
    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    std::map<pdf::PDFInteger, QRectF> regions;
    for (auto it = selection.begin(); it != selection.end(); )
    {
        const auto end = selection.nextPageRange(it);
        const pdf::PDFInteger pageIndex = it->start.pageIndex;
        if (pageIndex >= 0 && pageIndex < pageCount)
        {
            auto layoutGetter = m_pdfWidget->getDrawWidgetProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
            QPolygonF quads;
            pdf::PDFTextSelectionPainter painter(&selection);
            const QPainterPath path = painter.prepareGeometry(pageIndex, layoutGetter, QTransform(), &quads);
            if (!path.isEmpty() && !quads.isEmpty())
            {
                regions[pageIndex] = quads.boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0);
            }
        }
        it = end;
    }

    if (regions.empty())
    {
        m_mainWindowInterface->setStatusBarMessage(tr("The selection has no visible area to export."), 5000);
        return;
    }

    PDFExportImagesDialog::Request request;
    request.outputContext = PDFPageOutputContext::fromProxy(m_pdfDocument.data(), m_pdfWidget->getDrawWidgetProxy());
    request.documentFileName = getOriginalFileName();
    request.pageCount = pageCount;
    request.selectionRegions = qMove(regions);

    PDFExportImagesDialog dialog(request, m_mainWindow);
    dialog.exec();
}

void PDFProgramController::onActionTriggered(const pdf::PDFAction* action)
{
    Q_ASSERT(action);

    for (const pdf::PDFAction* currentAction : action->getActionList())
    {
        switch (currentAction->getType())
        {
            case pdf::ActionType::GoTo:
            {
                const pdf::PDFActionGoTo* typedAction = dynamic_cast<const pdf::PDFActionGoTo*>(currentAction);
                pdf::PDFDestination destination = typedAction->getDestination();
                if (destination.getDestinationType() == pdf::DestinationType::Named)
                {
                    const QByteArray destinationName = destination.getName();
                    if (const pdf::PDFDestination* targetDestination = m_pdfDocument->getCatalog()->getNamedDestination(destination.getName()))
                    {
                        destination = *targetDestination;
                    }
                    else
                    {
                        QMessageBox::critical(m_mainWindow, tr("Go to action"), tr("Failed to go to destination '%1'. Destination wasn't found.").arg(pdf::PDFEncoding::convertTextString(destinationName)));
                        destination = pdf::PDFDestination();
                    }
                }

                if (destination.getDestinationType() != pdf::DestinationType::Invalid)
                {
                    m_pdfWidget->getDrawWidgetProxy()->goToDestination(destination);
                }

                break;
            }

            case pdf::ActionType::Launch:
            {
                if (!m_settings->getSettings().m_allowLaunchApplications)
                {
                    // Launching of applications is disabled -> continue to next action
                    continue;
                }

                const pdf::PDFActionLaunch* typedAction = dynamic_cast<const pdf::PDFActionLaunch*>(currentAction);
#ifdef Q_OS_WIN
                const pdf::PDFActionLaunch::Win& winSpecification = typedAction->getWinSpecification();
                if (!winSpecification.file.isEmpty())
                {
                    QString message = tr("Would you like to launch application '%1' in working directory '%2' with parameters '%3'?").arg(QString::fromLatin1(winSpecification.file), QString::fromLatin1(winSpecification.directory), QString::fromLatin1(winSpecification.parameters));
                    if (QMessageBox::question(m_mainWindow, tr("Launch application"), message) == QMessageBox::Yes)
                    {
                        auto getStringOrNULL = [](const QByteArray& array) -> LPCSTR
                        {
                            if (!array.isEmpty())
                            {
                                return array.data();
                            }
                            return NULL;
                        };

                        const HINSTANCE result = ::ShellExecuteA(NULL, getStringOrNULL(winSpecification.operation), getStringOrNULL(winSpecification.file), getStringOrNULL(winSpecification.parameters), getStringOrNULL(winSpecification.directory), SW_SHOWNORMAL);
                        if (result <= HINSTANCE(32))
                        {
                            // Error occured
                            QMessageBox::warning(m_mainWindow, tr("Launch application"), tr("Executing application failed. Error code is %1.").arg(reinterpret_cast<intptr_t>(result)));
                        }
                    }

                    // Continue next action
                    continue;
                }

                const pdf::PDFFileSpecification& fileSpecification = typedAction->getFileSpecification();
                QString plaftormFileName = fileSpecification.getPlatformFileName();
                if (!plaftormFileName.isEmpty())
                {
                    QString message = tr("Would you like to launch application '%1'?").arg(plaftormFileName);
                    if (QMessageBox::question(m_mainWindow, tr("Launch application"), message) == QMessageBox::Yes)
                    {
                        const HINSTANCE result = ::ShellExecuteW(NULL, NULL, plaftormFileName.toStdWString().c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if (result <= HINSTANCE(32))
                        {
                            // Error occured
                            QMessageBox::warning(m_mainWindow, tr("Launch application"), tr("Executing application failed. Error code is %1.").arg(reinterpret_cast<intptr_t>(result)));
                        }
                    }

                    // Continue next action
                    continue;
                }
#endif
                break;
            }

            case pdf::ActionType::URI:
            {
                if (!m_settings->getSettings().m_allowLaunchURI)
                {
                    // Launching of URI is disabled -> continue to next action
                    continue;
                }

                const pdf::PDFActionURI* typedAction = dynamic_cast<const pdf::PDFActionURI*>(currentAction);
                QByteArray URI = m_pdfDocument->getCatalog()->getBaseURI() + typedAction->getURI();
                QString urlString = QString::fromUtf8(URI);
                QString message = tr("Would you like to open URL '%1'?").arg(urlString);
                if (QMessageBox::question(m_mainWindow, tr("Open URL"), message) == QMessageBox::Yes)
                {
                    if (!QDesktopServices::openUrl(QUrl(urlString)))
                    {
                        // Error occured
                        QMessageBox::warning(m_mainWindow, tr("Open URL"), tr("Opening url '%1' failed.").arg(urlString));
                    }
                }

                break;
            }

            case pdf::ActionType::Named:
            {
                const pdf::PDFActionNamed* typedAction = dynamic_cast<const pdf::PDFActionNamed*>(currentAction);
                switch (typedAction->getNamedActionType())
                {
                    case pdf::PDFActionNamed::NamedActionType::NextPage:
                        m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateNextPage);
                        break;

                    case pdf::PDFActionNamed::NamedActionType::PrevPage:
                        m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigatePreviousPage);
                        break;

                    case pdf::PDFActionNamed::NamedActionType::FirstPage:
                        m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateDocumentStart);
                        break;

                    case pdf::PDFActionNamed::NamedActionType::LastPage:
                        m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateDocumentEnd);
                        break;

                    default:
                        break;
                }

                break;
            }

            case pdf::ActionType::SetOCGState:
            {
                const pdf::PDFActionSetOCGState* typedAction = dynamic_cast<const pdf::PDFActionSetOCGState*>(currentAction);
                const pdf::PDFActionSetOCGState::StateChangeItems& stateChanges = typedAction->getStateChangeItems();
                const bool isRadioButtonsPreserved = typedAction->isRadioButtonsPreserved();

                if (m_optionalContentActivity)
                {
                    for (const pdf::PDFActionSetOCGState::StateChangeItem& stateChange : stateChanges)
                    {
                        pdf::OCState newState = pdf::OCState::Unknown;
                        switch (stateChange.first)
                        {
                            case pdf::PDFActionSetOCGState::SwitchType::ON:
                                newState = pdf::OCState::ON;
                                break;

                            case pdf::PDFActionSetOCGState::SwitchType::OFF:
                                newState = pdf::OCState::OFF;
                                break;

                            case pdf::PDFActionSetOCGState::SwitchType::Toggle:
                            {
                                pdf::OCState oldState = m_optionalContentActivity->getState(stateChange.second);
                                switch (oldState)
                                {
                                    case pdf::OCState::ON:
                                        newState = pdf::OCState::OFF;
                                        break;

                                    case pdf::OCState::OFF:
                                        newState = pdf::OCState::ON;
                                        break;

                                    case pdf::OCState::Unknown:
                                        break;

                                    default:
                                        Q_ASSERT(false);
                                        break;
                                }

                                break;
                            }

                            default:
                                Q_ASSERT(false);
                        }

                        if (newState != pdf::OCState::Unknown)
                        {
                            m_optionalContentActivity->setState(stateChange.second, newState, isRadioButtonsPreserved);
                        }
                    }
                }

                break;
            }

            case pdf::ActionType::ResetForm:
            {
                m_formManager->performResetAction(dynamic_cast<const pdf::PDFActionResetForm*>(action));
                break;
            }

            default:
                break;
        }
    }
}

void PDFProgramController::initializeToolManager()
{
    // Initialize tools
    pdf::PDFToolManager::Actions actions;
    actions.findPrevAction = m_actionManager->getAction(PDFActionManager::FindPrevious);
    actions.findNextAction = m_actionManager->getAction(PDFActionManager::FindNext);
    actions.selectTextToolAction = m_actionManager->getAction(PDFActionManager::ToolSelectText);
    actions.selectTableToolAction = m_actionManager->getAction(PDFActionManager::ToolSelectTable);
    actions.selectAllAction = m_actionManager->getAction(PDFActionManager::SelectTextAll);
    actions.deselectAction = m_actionManager->getAction(PDFActionManager::DeselectText);
    actions.copyTextAction = m_actionManager->getAction(PDFActionManager::CopyText);
    actions.magnifierAction = m_actionManager->getAction(PDFActionManager::ToolMagnifier);
    actions.screenshotToolAction = m_actionManager->getAction(PDFActionManager::ToolScreenshot);
    actions.extractImageAction = m_actionManager->getAction(PDFActionManager::ToolExtractImage);
    m_toolManager = new pdf::PDFToolManager(m_pdfWidget->getDrawWidgetProxy(), actions, this, m_mainWindow);
    m_pdfWidget->setToolManager(m_toolManager);
    updateMagnifierToolSettings();
    connect(m_toolManager, &pdf::PDFToolManager::documentModified, this, &PDFProgramController::onDocumentModified);

    // Add special tools
    if (QActionGroup* stickyNoteGroup = m_actionManager->getActionGroup(PDFActionManager::CreateStickyNoteGroup))
    {
        pdf::PDFCreateStickyNoteTool* createStickyNoteTool = new pdf::PDFCreateStickyNoteTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, stickyNoteGroup, this);
        m_toolManager->addTool(createStickyNoteTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateHyperlink))
    {
        pdf::PDFCreateHyperlinkTool* createHyperlinkTool = new pdf::PDFCreateHyperlinkTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, action, this);
        m_toolManager->addTool(createHyperlinkTool);
    }
    if (QActionGroup* inDocumentHyperlinkGroup = m_actionManager->getActionGroup(PDFActionManager::CreateInDocumentHyperlinkGroup))
    {
        pdf::PDFCreateInDocumentHyperlinkTool* createInDocumentHyperlinkTool = new pdf::PDFCreateInDocumentHyperlinkTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, inDocumentHyperlinkGroup, this);
        m_toolManager->addTool(createInDocumentHyperlinkTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::InsertPageNumbers))
    {
        pdf::PDFCreateInsertPageNumbersTool* createInsertPageNumbersTool = new pdf::PDFCreateInsertPageNumbersTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, action, this);
        m_toolManager->addTool(createInsertPageNumbersTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateInlineText))
    {
        pdf::PDFCreateFreeTextTool* createFreeTextTool = new pdf::PDFCreateFreeTextTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, action, this);
        m_toolManager->addTool(createFreeTextTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateStraightLine))
    {
        pdf::PDFCreateLineTypeTool* createStraightLineTool = new pdf::PDFCreateLineTypeTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, pdf::PDFCreateLineTypeTool::Type::Line, action, this);
        m_toolManager->addTool(createStraightLineTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreatePolyline))
    {
        pdf::PDFCreateLineTypeTool* createPolylineTool = new pdf::PDFCreateLineTypeTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, pdf::PDFCreateLineTypeTool::Type::PolyLine, action, this);
        m_toolManager->addTool(createPolylineTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateRectangle))
    {
        pdf::PDFCreateLineTypeTool* createRectangleTool = new pdf::PDFCreateLineTypeTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, pdf::PDFCreateLineTypeTool::Type::Rectangle, action, this);
        m_toolManager->addTool(createRectangleTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreatePolygon))
    {
        pdf::PDFCreateLineTypeTool* createPolygonTool = new pdf::PDFCreateLineTypeTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, pdf::PDFCreateLineTypeTool::Type::Polygon, action, this);
        m_toolManager->addTool(createPolygonTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateEllipse))
    {
        pdf::PDFCreateEllipseTool* createEllipseTool = new pdf::PDFCreateEllipseTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, action, this);
        m_toolManager->addTool(createEllipseTool);
    }
    if (QAction* action = m_actionManager->getAction(PDFActionManager::CreateFreehandCurve))
    {
        pdf::PDFCreateFreehandCurveTool* createFreehandCurveTool = new pdf::PDFCreateFreehandCurveTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, action, this);
        m_toolManager->addTool(createFreehandCurveTool);
    }
    if (QActionGroup* stampGroup = m_actionManager->getActionGroup(PDFActionManager::CreateStampGroup))
    {
        pdf::PDFCreateStampTool* createStampTool = new pdf::PDFCreateStampTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, stampGroup, this);
        m_toolManager->addTool(createStampTool);
    }
    if (QActionGroup* highlightGroup = m_actionManager->getActionGroup(PDFActionManager::CreateTextHighlightGroup))
    {
        pdf::PDFCreateHighlightTextTool* createHighlightTextTool = new pdf::PDFCreateHighlightTextTool(m_pdfWidget->getDrawWidgetProxy(), m_toolManager, highlightGroup, this);
        m_toolManager->addTool(createHighlightTextTool);
    }
}

void PDFProgramController::onActionGoToDocumentStartTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateDocumentStart);
}

void PDFProgramController::onActionGoToDocumentEndTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateDocumentEnd);
}

void PDFProgramController::onActionGoToNextPageTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateNextPage);
}

void PDFProgramController::onActionGoToPreviousPageTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigatePreviousPage);
}

void PDFProgramController::onActionGoToNextLineTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigateNextStep);
}

void PDFProgramController::onActionGoToPreviousLineTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::NavigatePreviousStep);
}

void PDFProgramController::onActionZoomIn()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::ZoomIn);
}

void PDFProgramController::onActionZoomOut()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::ZoomOut);
}

void PDFProgramController::onActionRenderingOptionTriggered(bool checked)
{
    QAction* action = qobject_cast<QAction*>(sender());
    Q_ASSERT(action);

    pdf::PDFRenderer::Features features = m_settings->getFeatures();
    pdf::PDFRenderer::Feature affectedFeature = static_cast<pdf::PDFRenderer::Feature>(action->data().toInt());
    pdf::PDFRenderer::Features colorFeatures = pdf::PDFRenderer::getColorFeatures();

    if (colorFeatures.testFlag(affectedFeature) && checked)
    {
        features = features & ~colorFeatures;
    }

    features.setFlag(affectedFeature, checked);
    m_settings->setFeatures(features);
    updateRenderingOptionActions();
}

void PDFProgramController::performSaveAs()
{
    QFileInfo fileInfo(m_fileInfo.originalFileName);
    QString saveFileName = QFileDialog::getSaveFileName(m_mainWindow, tr("Save As"), fileInfo.dir().absoluteFilePath(m_fileInfo.originalFileName), tr("Portable Document (*.pdf);;All files (*.*)"));
    if (!saveFileName.isEmpty())
    {
        saveDocument(saveFileName);
    }
}

void PDFProgramController::performSave()
{
    if (!m_openedRecoverySourcePath.isEmpty())
    {
        performSaveAs();
        return;
    }
    saveDocument(m_fileInfo.originalFileName);
}

void PDFProgramController::saveDocument(const QString& fileName)
{
    if (m_formManager)
    {
        m_formManager->setFocusToEditor(nullptr);
        const QStringList missing = m_formManager->getMissingRequiredFields();
        if (!missing.isEmpty() && QMessageBox::warning(m_mainWindow, tr("Required form fields"),
                tr("Required fields are empty: %1\nSave the incomplete form anyway?").arg(missing.join(", ")),
                QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Save)
            return;
    }

    updateFileWatcher(true);

    const QString previousSourcePath = m_fileInfo.absoluteFilePath;
    const QFileInfo destinationInfo(fileName);
    const QString destinationPath = destinationInfo.absoluteFilePath();
    const bool isInPlaceSave =
        !m_fileInfo.absoluteFilePath.isEmpty() &&
        destinationPath.compare(m_fileInfo.absoluteFilePath, Qt::CaseInsensitive) == 0;

    if (isInPlaceSave && !m_safeSaveBaseline.isValid)
    {
        updateFileWatcher();
        QMessageBox::information(m_mainWindow,
                                 tr("Safe save"),
                                 tr("FamilyPDF is still establishing the safe-save baseline. "
                                    "Please use Save As or try again after the document finishes loading."));
        return;
    }

    QDir destinationDirectory(destinationInfo.absolutePath());
    const QString candidatePath = destinationDirectory.filePath(
        QStringLiteral(".%1.familypdf-%2.tmp")
            .arg(destinationInfo.fileName(),
                 QUuid::createUuid().toString(QUuid::WithoutBraces)));

    pdf::PDFDocumentWriter writer(nullptr);
    pdf::PDFOperationResult writeResult =
        writer.write(candidatePath, m_pdfDocument.data(), true);
    if (!writeResult)
    {
        updateFileWatcher();
        QMessageBox::critical(m_mainWindow,
                              tr("Safe save failed"),
                              tr("%1\n\nThe original document was not changed.\n"
                                 "Diagnostic temporary file: %2")
                                  .arg(writeResult.getErrorMessage(), candidatePath));
        return;
    }

    const size_t expectedPageCount = m_pdfDocument->getCatalog()->getPageCount();
    auto validator = [expectedPageCount](const QString& path, QString* errorMessage)
    {
        auto queryPassword = [](bool* ok)
        {
            *ok = false;
            return QString();
        };
        pdf::PDFDocumentReader reader(nullptr, qMove(queryPassword), true, false);
        const pdf::PDFDocument document = reader.readFromFile(path);
        if (reader.getReadingResult() != pdf::PDFDocumentReader::Result::OK)
        {
            if (errorMessage)
            {
                *errorMessage = reader.getErrorMessage();
            }
            return false;
        }
        if (document.getCatalog()->getPageCount() != expectedPageCount)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr("Round-trip validation found a different page count.");
            }
            return false;
        }
        return true;
    };

    PDFSafeSaveService::Result safeSaveResult;
    if (isInPlaceSave)
    {
        safeSaveResult =
            PDFSafeSaveService::commitCandidate(destinationPath,
                                                candidatePath,
                                                m_safeSaveBaseline,
                                                validator);
    }
    else if (QFileInfo::exists(destinationPath))
    {
        const PDFSafeSaveService::Baseline destinationBaseline =
            PDFSafeSaveService::captureBaseline(destinationPath);
        safeSaveResult =
            PDFSafeSaveService::commitCandidate(destinationPath,
                                                candidatePath,
                                                destinationBaseline,
                                                validator);
    }
    else
    {
        safeSaveResult =
            PDFSafeSaveService::commitNewCandidate(destinationPath,
                                                   candidatePath,
                                                   validator);
    }

    if (safeSaveResult.status == PDFSafeSaveService::Status::Success)
    {
        if (m_undoRedoManager)
        {
            m_undoRedoManager->setIsCurrentSaved(true);
        }

        updateFileInfo(destinationPath);
        m_safeSaveBaseline = PDFSafeSaveService::captureBaseline(destinationPath);
        if (m_bookmarkManager)
        {
            m_bookmarkManager->setProperty("familyPdfDocumentPath", m_fileInfo.absoluteFilePath);
        }
        updateTitle();

        if (m_recentFileManager)
        {
            m_recentFileManager->addRecentFile(destinationPath);
        }

        if (!previousSourcePath.isEmpty())
        {
            PDFRecoveryManager::removeRecord(previousSourcePath);
        }
        if (!m_openedRecoverySourcePath.isEmpty())
        {
            PDFRecoveryManager::removeRecord(m_openedRecoverySourcePath);
            m_openedRecoverySourcePath.clear();
        }
    }
    else if (safeSaveResult.status == PDFSafeSaveService::Status::SourceChanged)
    {
        updateFileWatcher();

        QMessageBox dialog(QMessageBox::Warning,
                           tr("Source document changed"),
                           tr("Another program changed this PDF after FamilyPDF opened it. "
                              "The source was not overwritten."),
                           QMessageBox::Cancel,
                           m_mainWindow);
        QPushButton* reloadButton = dialog.addButton(tr("Reload"), QMessageBox::AcceptRole);
        QPushButton* saveAsButton = dialog.addButton(tr("Save As"), QMessageBox::ActionRole);
        dialog.setInformativeText(tr("Choose Reload to discard FamilyPDF's unsaved changes, "
                                     "or Save As to keep them in a separate PDF."));
        dialog.exec();
        if (dialog.clickedButton() == reloadButton)
        {
            openDocument(destinationPath);
        }
        else if (dialog.clickedButton() == saveAsButton)
        {
            performSaveAs();
        }
        return;
    }
    else if (safeSaveResult.status == PDFSafeSaveService::Status::UnsupportedVolume)
    {
        updateFileWatcher();
        QMessageBox::information(m_mainWindow,
                                 tr("Save As required"),
                                 tr("%1\n\nFor safety, FamilyPDF will not replace this source in place.")
                                     .arg(safeSaveResult.errorMessage));
        performSaveAs();
        return;
    }
    else
    {
        QString details = safeSaveResult.errorMessage;
        if (!safeSaveResult.backupPath.isEmpty())
        {
            details += tr("\nBackup: %1").arg(safeSaveResult.backupPath);
        }
        if (!safeSaveResult.candidatePath.isEmpty())
        {
            details += tr("\nDiagnostic temporary file: %1").arg(safeSaveResult.candidatePath);
        }
        if (safeSaveResult.status == PDFSafeSaveService::Status::PostCommitValidationFailed)
        {
            details += tr("\n\nThe replacement completed but final validation failed. "
                          "Stop editing this document and recover from the backup to a new path.");
        }
        QMessageBox::critical(m_mainWindow, tr("Safe save failed"), details);
    }

    updateFileWatcher();
}

void PDFProgramController::savePageLayoutPerDocument()
{
    if (m_pdfDocument && !m_fileInfo.absoluteFilePath.isEmpty())
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
        const pdf::PageLayout pageLayout = m_pdfWidget->getDrawWidgetProxy()->getPageLayout();
        settings.beginGroup("PageLayoutPerDocumentSettings");
        settings.setValue(m_fileInfo.absoluteFilePath, pdf::PDFPageLayoutUtils::convertPageLayoutToString(pageLayout));
        settings.endGroup();
    }
}

QString PDFProgramController::getDocumentViewStateKey() const
{
    if (m_fileInfo.absoluteFilePath.isEmpty())
    {
        return QString();
    }

    QFileInfo fileInfo(m_fileInfo.absoluteFilePath);
    QString stablePath = fileInfo.canonicalFilePath();
    if (stablePath.isEmpty())
    {
        stablePath = fileInfo.absoluteFilePath();
    }
    stablePath = QDir::cleanPath(stablePath);
#ifdef Q_OS_WIN
    stablePath = stablePath.toCaseFolded();
#endif
    return QString::fromLatin1(QCryptographicHash::hash(stablePath.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void PDFProgramController::saveDocumentViewState()
{
    if (!m_pdfDocument || m_fileInfo.absoluteFilePath.isEmpty())
    {
        return;
    }

    const std::vector<pdf::PDFInteger> pages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (pages.empty())
    {
        return;
    }

    const QString stateKey = getDocumentViewStateKey();
    if (stateKey.isEmpty())
    {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup(QStringLiteral("DocumentViewStates"));
    settings.beginGroup(stateKey);
    settings.setValue(QStringLiteral("version"), 1);
    settings.setValue(QStringLiteral("path"), m_fileInfo.absoluteFilePath);
    settings.setValue(QStringLiteral("page"), qlonglong(pages.front()));
    settings.setValue(QStringLiteral("zoom"), m_pdfWidget->getDrawWidgetProxy()->getZoom());

    const auto saveScrollPosition = [&settings](const QString& name, const QScrollBar* scrollBar)
    {
        const int span = scrollBar->maximum() - scrollBar->minimum();
        if (span > 0)
        {
            const qreal position = qreal(scrollBar->value() - scrollBar->minimum()) / qreal(span);
            settings.setValue(name, position);
        }
        else
        {
            settings.remove(name);
        }
    };

    if (!m_pdfWidget->getDrawWidgetProxy()->isBlockMode())
    {
        saveScrollPosition(QStringLiteral("horizontalPosition"), m_pdfWidget->getHorizontalScrollbar());
        saveScrollPosition(QStringLiteral("verticalPosition"), m_pdfWidget->getVerticalScrollbar());
    }
    else
    {
        settings.remove(QStringLiteral("horizontalPosition"));
        settings.remove(QStringLiteral("verticalPosition"));
    }
    settings.endGroup();
    settings.endGroup();
}

void PDFProgramController::restoreDocumentViewState()
{
    if (!m_pdfDocument || m_fileInfo.absoluteFilePath.isEmpty())
    {
        return;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    if (pageCount <= 0)
    {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QString stateKey = getDocumentViewStateKey();
    settings.beginGroup(QStringLiteral("DocumentViewStates"));
    settings.beginGroup(stateKey);
    const bool hasSavedState = settings.value(QStringLiteral("version")).toInt() == 1;
    const QVariant pageValue = settings.value(QStringLiteral("page"));
    const QVariant zoomValue = settings.value(QStringLiteral("zoom"));
    const QVariant horizontalValue = settings.value(QStringLiteral("horizontalPosition"));
    const QVariant verticalValue = settings.value(QStringLiteral("verticalPosition"));
    settings.endGroup();
    settings.endGroup();

    QVariant effectivePageValue = pageValue;
    if (!hasSavedState)
    {
        settings.beginGroup(QStringLiteral("LastOpenedDocumentPages"));
        effectivePageValue = settings.value(m_fileInfo.absoluteFilePath);
        settings.endGroup();
    }

    bool pageOk = false;
    const qlonglong savedPage = effectivePageValue.toLongLong(&pageOk);
    if (!pageOk)
    {
        return;
    }

    const pdf::PDFInteger pageIndex = pdf::PDFInteger(std::clamp<qlonglong>(savedPage, 0, pageCount - 1));
    const bool pageWasClamped = savedPage != pageIndex;
    pdf::PDFDrawWidgetProxy* proxy = m_pdfWidget->getDrawWidgetProxy();
    proxy->goToPage(pageIndex);

    bool zoomOk = false;
    const qreal zoom = zoomValue.toDouble(&zoomOk);
    if (hasSavedState && zoomOk && std::isfinite(zoom) &&
        zoom >= pdf::PDFDrawWidgetProxy::getMinZoom() &&
        zoom <= pdf::PDFDrawWidgetProxy::getMaxZoom())
    {
        proxy->zoom(zoom);
    }

    const auto validPosition = [](const QVariant& value, qreal* position)
    {
        bool ok = false;
        const qreal parsed = value.toDouble(&ok);
        if (!ok || !std::isfinite(parsed) || parsed < 0.0 || parsed > 1.0)
        {
            return false;
        }
        *position = parsed;
        return true;
    };

    qreal horizontalPosition = 0.0;
    qreal verticalPosition = 0.0;
    const bool restoreHorizontal = hasSavedState && !pageWasClamped && validPosition(horizontalValue, &horizontalPosition);
    const bool restoreVertical = hasSavedState && !pageWasClamped && validPosition(verticalValue, &verticalPosition);
    if (restoreHorizontal || restoreVertical)
    {
        QTimer::singleShot(0, this, [this, stateKey, restoreHorizontal, restoreVertical, horizontalPosition, verticalPosition]()
        {
            if (!m_pdfDocument || getDocumentViewStateKey() != stateKey || m_pdfWidget->getDrawWidgetProxy()->isBlockMode())
            {
                return;
            }

            const auto restoreScrollPosition = [](QScrollBar* scrollBar, qreal position)
            {
                const int span = scrollBar->maximum() - scrollBar->minimum();
                if (span > 0)
                {
                    scrollBar->setValue(scrollBar->minimum() + qRound(position * span));
                }
            };
            if (restoreHorizontal)
            {
                restoreScrollPosition(m_pdfWidget->getHorizontalScrollbar(), horizontalPosition);
            }
            if (restoreVertical)
            {
                restoreScrollPosition(m_pdfWidget->getVerticalScrollbar(), verticalPosition);
            }
        });
    }
}

bool PDFProgramController::isFactorySettingsBeingRestored() const
{
    return m_isFactorySettingsBeingRestored;
}

bool PDFProgramController::getIsBusy() const
{
    return m_isBusy;
}

void PDFProgramController::setIsBusy(bool isBusy)
{
    m_isBusy = isBusy;
}

bool PDFProgramController::canClose() const
{
    return !(m_futureWatcher && m_futureWatcher->isRunning()) || !m_isBusy;
}

bool PDFProgramController::askForSaveDocumentBeforeClose()
{
    if (m_formManager)
        m_formManager->setFocusToEditor(nullptr);

    if (!m_pdfDocument)
    {
        // Nothing to be done
        return true;
    }

    if (m_undoRedoManager && !m_undoRedoManager->isCurrentSaved())
    {
        QString title = tr("Save Document");
        QString message = tr("Do you wish to save modified document before it is closed?");
        switch (QMessageBox::question(m_mainWindow, title, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel))
        {
            case QMessageBox::Yes:
            {
                performSave();
                return m_undoRedoManager->isCurrentSaved();
            }

            case QMessageBox::No:
                return true;

            case QMessageBox::Cancel:
                return false;

            default:
                Q_ASSERT(false);
                return true;
        }
    }

    return true;
}

void PDFProgramController::createSelectionMarkup(pdf::AnnotationType type)
{
    if (!m_pdfDocument || !m_annotationManager->isEditingEnabled() ||
        !m_pdfDocument->getStorage().getSecurityHandler()->isAllowed(pdf::PDFSecurityHandler::Permission::ModifyInteractiveItems))
        return;
    if (type != pdf::AnnotationType::Highlight && type != pdf::AnnotationType::Underline && type != pdf::AnnotationType::StrikeOut)
        return;

    const pdf::PDFTextSelection selection = m_toolManager ? m_toolManager->getSelectedText() : pdf::PDFTextSelection();
    if (selection.isEmpty())
        return;

    pdf::PDFDocumentModifier modifier(m_pdfDocument.data());
    auto* builder = modifier.getBuilder();
    bool created = false;
    // Select All may span pages. Each page gets its own standard annotation,
    // while the entire operation remains a single undoable document change.
    for (auto it = selection.begin(); it != selection.end(); )
    {
        const auto end = selection.nextPageRange(it);
        const pdf::PDFInteger pageIndex = it->start.pageIndex;
        if (pageIndex >= 0 && pageIndex < m_pdfDocument->getCatalog()->getPageCount())
        {
            auto layoutGetter = m_pdfWidget->getDrawWidgetProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
            QPolygonF quads;
            pdf::PDFTextSelectionPainter painter(&selection);
            const QPainterPath path = painter.prepareGeometry(pageIndex, layoutGetter, QTransform(), &quads);
            if (!path.isEmpty() && !quads.isEmpty())
            {
                const auto page = m_pdfDocument->getCatalog()->getPage(pageIndex)->getPageReference();
                pdf::PDFObjectReference annotation;
                switch (type)
                {
                    case pdf::AnnotationType::Highlight:
                        annotation = builder->createAnnotationHighlight(page, quads, Qt::yellow);
                        builder->setAnnotationOpacity(annotation, 0.2);
                        break;
                    case pdf::AnnotationType::Underline:
                        annotation = builder->createAnnotationUnderline(page, quads, Qt::red);
                        break;
                    case pdf::AnnotationType::StrikeOut:
                        annotation = builder->createAnnotationStrikeout(page, quads, Qt::red);
                        break;
                    default: break;
                }
                const pdf::PDFTextLayout layout = layoutGetter;
                builder->setAnnotationContents(annotation, layout.getTextFromSelection(it, end, pageIndex));
                builder->updateAnnotationAppearanceStreams(annotation);
                created = true;
            }
        }
        it = end;
    }
    if (created)
    {
        modifier.markAnnotationsChanged();
        if (modifier.finalize())
            onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }
}

QString PDFProgramController::getOriginalFileName() const
{
    return m_fileInfo.originalFileName;
}

pdf::PDFTextSelection PDFProgramController::getSelectedText() const
{
    return m_mainWindowInterface->getSelectedText();
}

QMainWindow* PDFProgramController::getMainWindow() const
{
    return m_mainWindow;
}

pdf::IPluginDataExchange::VoiceSettings PDFProgramController::getVoiceSettings() const
{
    VoiceSettings voiceSettings;

    const PDFViewerSettings::Settings& settings = m_settings->getSettings();
    voiceSettings.directory = m_settings->getDirectory();
    voiceSettings.voiceName = settings.m_speechVoice;
    voiceSettings.pitch = settings.m_speechPitch;
    voiceSettings.rate = settings.m_speechRate;
    voiceSettings.volume = settings.m_speechVolume;

    return voiceSettings;
}

void PDFProgramController::onActionRotateRightTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::RotateRight);
}

void PDFProgramController::onActionRotateLeftTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::RotateLeft);
}


void PDFProgramController::onActionPropertiesTriggered()
{
    Q_ASSERT(m_pdfDocument);

    // Document information is editable only in the editor (it has undo/redo) and when modification is permitted
    const bool canEditInfo = m_undoRedoManager &&
                             m_pdfDocument->getStorage().getSecurityHandler()->isAllowed(pdf::PDFSecurityHandler::Permission::Modify);
    PDFDocumentPropertiesDialog documentPropertiesDialog(m_pdfDocument.data(), &m_fileInfo, m_mainWindow, canEditInfo);
    if (documentPropertiesDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const bool isXMPMetadataModified = documentPropertiesDialog.isXMPMetadataModified();
    const std::vector<std::pair<QByteArray, QString>> infoEntries = documentPropertiesDialog.getModifiedInfoEntries();
    if (isXMPMetadataModified || !infoEntries.empty())
    {
        pdf::PDFDocumentModifier modifier(m_pdfDocument.data());
        pdf::PDFDocumentBuilder* builder = modifier.getBuilder();
        if (isXMPMetadataModified)
        {
            builder->setCatalogMetadata(documentPropertiesDialog.getXMPMetadata());
        }

        for (const auto& [key, value] : infoEntries)
        {
            if (value.isEmpty())
            {
                // setDocumentTitle("") etc. would store an empty string; a null value removes the entry
                pdf::PDFObjectFactory factory;
                factory.beginDictionary();
                factory.beginDictionaryItem(key);
                factory << nullptr;
                factory.endDictionaryItem();
                factory.endDictionary();
                builder->updateDocumentInfo(factory.takeObject());
            }
            else if (key == "Title")
            {
                builder->setDocumentTitle(value);
            }
            else if (key == "Author")
            {
                builder->setDocumentAuthor(value);
            }
            else if (key == "Subject")
            {
                builder->setDocumentSubject(value);
            }
            else if (key == "Keywords")
            {
                builder->setDocumentKeywords(value);
            }
            else if (key == "Creator")
            {
                builder->setDocumentCreator(value);
            }
        }

        modifier.markReset();
        if (modifier.finalize())
        {
            // Reset alone clears the undo history; metadata changes must be undoable
            pdf::PDFModifiedDocument::ModificationFlags flags = modifier.getFlags();
            flags.setFlag(pdf::PDFModifiedDocument::PreserveUndoRedo);
            pdf::PDFModifiedDocument document(modifier.getDocument(), m_optionalContentActivity, flags);
            onDocumentModified(qMove(document));
        }
    }
}

void PDFProgramController::onActionAboutTriggered()
{
    PDFAboutDialog dialog(m_mainWindow);
    dialog.exec();
}

void PDFProgramController::onActionSendByEMailTriggered()
{
    Q_ASSERT(m_pdfDocument);

    QString subject = m_pdfDocument->getInfo()->title;
    if (subject.isEmpty())
    {
        subject = m_fileInfo.fileName;
    }

    if (!PDFSendMail::sendMail(m_mainWindow, subject, m_fileInfo.originalFileName))
    {
        QMessageBox::critical(m_mainWindow, tr("Error"), tr("Error while starting email client occured!"));
    }
}

void PDFProgramController::onActionRenderToImagesTriggered()
{
    PDFRenderToImagesDialog dialog(m_pdfDocument.data(), m_pdfWidget->getDrawWidgetProxy(), m_progress, m_mainWindow);
    dialog.exec();
}

void PDFProgramController::onActionOptimizeTriggered()
{
    PDFOptimizeDocumentDialog dialog(m_pdfDocument.data(), m_mainWindow);

    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(dialog.takeOptimizedDocument()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::ModificationFlags(pdf::PDFModifiedDocument::Reset | pdf::PDFModifiedDocument::PreserveUndoRedo));
        onDocumentModified(qMove(document));
    }
}

void PDFProgramController::onActionOptimizeImagesTriggered()
{
    PDFOptimizeImagesDialog dialog(m_pdfDocument.data(), m_progress, m_mainWindow);

    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(dialog.takeOptimizedDocument()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::ModificationFlags(pdf::PDFModifiedDocument::Reset | pdf::PDFModifiedDocument::PreserveUndoRedo));
        onDocumentModified(qMove(document));
    }
}

void PDFProgramController::onActionSanitizeTriggered()
{
    PDFSanitizeDocumentDialog dialog(m_pdfDocument.data(), m_mainWindow);

    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(dialog.takeSanitizedDocument()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::ModificationFlags(pdf::PDFModifiedDocument::Reset | pdf::PDFModifiedDocument::PreserveUndoRedo));
        onDocumentModified(qMove(document));
    }
}

void PDFProgramController::onActionRemoveExternalLinksTriggered()
{
    if (!m_pdfDocument)
    {
        return;
    }

    pdf::PDFDocumentModifier modifier(m_pdfDocument.data());
    pdf::PDFDocumentBuilder* builder = modifier.getBuilder();
    builder->flattenPageTree();

    const pdf::PDFObjectStorage* storage = builder->getStorage();
    pdf::PDFDocumentDataLoaderDecorator loader(storage);

    std::vector<std::pair<pdf::PDFObjectReference, pdf::PDFObjectReference>> annotationsToRemove;
    std::vector<pdf::PDFObjectReference> pageReferences = builder->getPages();

    for (const pdf::PDFObjectReference pageReference : pageReferences)
    {
        const pdf::PDFObject& pageObject = storage->getObjectByReference(pageReference);
        const pdf::PDFDictionary* pageDictionary = storage->getDictionaryFromObject(pageObject);
        if (!pageDictionary)
        {
            continue;
        }

        std::vector<pdf::PDFObjectReference> annotationReferences = loader.readReferenceArrayFromDictionary(pageDictionary, "Annots");
        for (const pdf::PDFObjectReference& annotationReference : annotationReferences)
        {
            pdf::PDFAnnotationPtr annotation = pdf::PDFAnnotation::parse(storage, annotationReference);
            if (pdf::PDFAnnotation::isExternalLinkAnnotation(annotation.data()))
            {
                annotationsToRemove.emplace_back(pageReference, annotationReference);
            }
        }
    }

    if (annotationsToRemove.empty())
    {
        QMessageBox::information(m_mainWindow, QApplication::applicationDisplayName(), tr("No external link annotations found."));
        m_mainWindowInterface->setStatusBarMessage(tr("No external link annotations found."), 4000);
        return;
    }

    for (const auto& item : annotationsToRemove)
    {
        builder->removeAnnotation(item.first, item.second);
    }
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        pdf::PDFModifiedDocument document(modifier.getDocument(), m_optionalContentActivity, modifier.getFlags());
        onDocumentModified(qMove(document));
    }

    QMessageBox::information(m_mainWindow, QApplication::applicationDisplayName(),
                             tr("External link annotations removed: %1.").arg(annotationsToRemove.size()));
    m_mainWindowInterface->setStatusBarMessage(tr("External link annotations removed: %1.").arg(annotationsToRemove.size()), 4000);
}

void PDFProgramController::onActionPageGeometryTriggered()
{
    if (!m_pdfDocument)
    {
        return;
    }

    pdf::PDFPageGeometryDialog dialog(m_mainWindow);
    dialog.setPageCount(m_pdfDocument->getCatalog()->getPageCount());

    std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (!currentPages.empty())
    {
        pdf::PDFPageGeometrySettings settings = dialog.getSettings();
        settings.pageRange = QString::number(currentPages.front() + 1);
        dialog.setSettings(settings);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFDocument updatedDocument = *m_pdfDocument;
        pdf::PDFModifiedDocument::ModificationFlags flags;
        const pdf::PDFOperationResult result = pdf::PDFPageGeometry::apply(&updatedDocument, dialog.getSettings(), &flags);
        if (!result)
        {
            QMessageBox::critical(m_mainWindow, tr("Error"), result.getErrorMessage());
            return;
        }

        if (updatedDocument != *m_pdfDocument)
        {
            pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(std::move(updatedDocument)));
            onDocumentModified(pdf::PDFModifiedDocument(std::move(pointer), m_optionalContentActivity, flags));
        }
    }
}

void PDFProgramController::onActionCreateBitonalDocumentTriggered()
{
    auto cms = m_CMSManager->getCurrentCMS();
    PDFCreateBitonalDocumentDialog dialog(m_pdfDocument.data(), cms.data(), m_progress, m_mainWindow);

    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(dialog.takeBitonaldDocument()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::ModificationFlags(pdf::PDFModifiedDocument::Reset | pdf::PDFModifiedDocument::PreserveUndoRedo));
        onDocumentModified(qMove(document));
    }
}

void PDFProgramController::onActionEncryptionTriggered()
{
    auto queryPassword = [this](bool* ok)
    {
        QString result;
        *ok = false;
        onQueryPasswordRequest(&result, ok);
        return result;
    };

    // Check that we have owner access to the document
    const pdf::PDFSecurityHandler* securityHandler =  m_pdfDocument->getStorage().getSecurityHandler();
    pdf::PDFSecurityHandler::AuthorizationResult authorizationResult = securityHandler->getAuthorizationResult();
    if (authorizationResult != pdf::PDFSecurityHandler::AuthorizationResult::OwnerAuthorized &&
        authorizationResult != pdf::PDFSecurityHandler::AuthorizationResult::NoAuthorizationRequired)
    {
        // Jakub Melka: we must authorize as owner, otherwise we can't continue,
        // because we don't have sufficient permissions.
        pdf::PDFSecurityHandlerPointer clonedSecurityHandler(securityHandler->clone());
        authorizationResult = clonedSecurityHandler->authenticate(queryPassword, true);

        if (authorizationResult != pdf::PDFSecurityHandler::AuthorizationResult::OwnerAuthorized)
        {
            QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), tr("Permission to change document security is denied."));
            return;
        }

        pdf::PDFObjectStorage storage = m_pdfDocument->getStorage();
        storage.setSecurityHandler(qMove(clonedSecurityHandler));

        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(qMove(storage), m_pdfDocument->getInfo()->version, QByteArray()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::Authorization);
        onDocumentModified(qMove(document));
    }

    PDFEncryptionSettingsDialog dialog(m_pdfDocument->getIdPart(0), m_mainWindow);
    if (dialog.exec() == QDialog::Accepted)
    {
        pdf::PDFSecurityHandlerPointer updatedSecurityHandler = dialog.getUpdatedSecurityHandler();

        if (!updatedSecurityHandler)
        {
            QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), tr("Failed to create security handler."));
            return;
        }

        // Jakub Melka: If we changed encryption (password), recheck, that user doesn't
        // forgot (or accidentally entered wrong) password. So, we require owner authentization
        // to continue.
        switch (updatedSecurityHandler->getMode())
        {
            case pdf::EncryptionMode::Standard:
            {
                if (updatedSecurityHandler->authenticate(queryPassword, true) != pdf::PDFSecurityHandler::AuthorizationResult::OwnerAuthorized)
                {
                    QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), tr("Reauthorization is required to change document encryption."));
                    return;
                }

                break;
            }

            case pdf::EncryptionMode::PublicKey:
            {
                if (updatedSecurityHandler->authenticate(queryPassword, false) != pdf::PDFSecurityHandler::AuthorizationResult::UserAuthorized)
                {
                    QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), tr("Reauthorization is required to change document encryption."));
                    return;
                }

                break;
            }

            case pdf::EncryptionMode::None:
                break;

            default:
                Q_ASSERT(false);
                break;
        }

        pdf::PDFDocumentBuilder builder(m_pdfDocument.data());
        builder.setSecurityHandler(qMove(updatedSecurityHandler));

        pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(builder.build()));
        pdf::PDFModifiedDocument document(qMove(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::Reset);
        onDocumentModified(qMove(document));
    }
}

void PDFProgramController::onActionFitPageTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::ZoomFit);
}

void PDFProgramController::onActionFitWidthTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::ZoomFitWidth);
}

void PDFProgramController::onActionFitHeightTriggered()
{
    m_pdfWidget->getDrawWidgetProxy()->performOperation(pdf::PDFDrawWidgetProxy::ZoomFitHeight);
}

void PDFProgramController::onActionRenderingErrorsTriggered()
{
    pdf::PDFRenderingErrorsWidget renderingErrorsDialog(m_mainWindow, m_pdfWidget);
    renderingErrorsDialog.exec();
}

void PDFProgramController::updateMagnifierToolSettings()
{
    if (m_toolManager)
    {
        pdf::PDFMagnifierTool* magnifierTool = m_toolManager->getMagnifierTool();
        magnifierTool->setMagnifierSize(pdf::PDFWidgetUtils::scaleDPI_x(m_mainWindow, m_settings->getSettings().m_magnifierSize));
        magnifierTool->setMagnifierZoom(m_settings->getSettings().m_magnifierZoom);
    }
}

void PDFProgramController::updateUndoRedoSettings()
{
    if (m_undoRedoManager)
    {
        const PDFViewerSettings::Settings& settings = m_settings->getSettings();
        m_undoRedoManager->setMaximumSteps(settings.m_maximumUndoSteps, settings.m_maximumRedoSteps);
    }
}

void PDFProgramController::updateUndoRedoActions()
{
    if (m_undoRedoManager)
    {
        const bool isBusy = (m_futureWatcher && m_futureWatcher->isRunning()) || m_isBusy;
        const bool canUndo = !isBusy && m_undoRedoManager->canUndo();
        const bool canRedo = !isBusy && m_undoRedoManager->canRedo();

        m_actionManager->setEnabled(PDFActionManager::Undo, canUndo);
        m_actionManager->setEnabled(PDFActionManager::Redo, canRedo);
    }
    else
    {
        m_actionManager->setEnabled(PDFActionManager::Undo, false);
        m_actionManager->setEnabled(PDFActionManager::Redo, false);
    }
}

void PDFProgramController::onQueryPasswordRequest(QString* password, bool* ok)
{
    *password = QInputDialog::getText(m_mainWindow, tr("Encrypted document"), tr("Enter password to access document content"), QLineEdit::Password, QString(), ok);
}

void PDFProgramController::onActionPageLayoutSinglePageTriggered()
{
    setPageLayout(pdf::PageLayout::SinglePage);
}

void PDFProgramController::onActionPageLayoutContinuousTriggered()
{
    setPageLayout(pdf::PageLayout::OneColumn);
}

void PDFProgramController::onActionPageLayoutTwoPagesTriggered()
{
    setPageLayout(m_actionManager->getAction(PDFActionManager::PageLayoutFirstPageOnRightSide)->isChecked() ? pdf::PageLayout::TwoPagesRight : pdf::PageLayout::TwoPagesLeft);
}

void PDFProgramController::onActionPageLayoutTwoColumnsTriggered()
{
    setPageLayout(m_actionManager->getAction(PDFActionManager::PageLayoutFirstPageOnRightSide)->isChecked() ? pdf::PageLayout::TwoColumnRight : pdf::PageLayout::TwoColumnLeft);
}

void PDFProgramController::onActionFullscreenModeTriggered(bool checked)
{
    if (checked)
    {
        enterFullscreenMode();
    }
    else
    {
        leaveFullscreenMode();
    }
}

void PDFProgramController::onActionFirstPageOnRightSideTriggered()
{
    switch (m_pdfWidget->getDrawWidgetProxy()->getPageLayout())
    {
        case pdf::PageLayout::SinglePage:
        case pdf::PageLayout::OneColumn:
            break;

        case pdf::PageLayout::TwoColumnLeft:
        case pdf::PageLayout::TwoColumnRight:
            onActionPageLayoutTwoColumnsTriggered();
            break;

        case pdf::PageLayout::TwoPagesLeft:
        case pdf::PageLayout::TwoPagesRight:
            onActionPageLayoutTwoPagesTriggered();
            break;

        default:
            Q_ASSERT(false);
    }
}

void PDFProgramController::onActionFindTriggered()
{
    if (m_toolManager)
    {
        auto* findTool = m_toolManager->getFindTextTool();
        if (!findTool->isActive()) m_toolManager->setActiveTool(findTool);
        findTool->focusSearch();
    }
}

// Bumped when the default toolbar layout changes, so stale saved layouts are ignored once.
static constexpr int WINDOW_STATE_VERSION = 1;

void PDFProgramController::readSettings(Settings settingsFlags)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());

    if (settingsFlags.testFlag(WindowSettings))
    {
        QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
        if (geometry.isEmpty())
        {
            QRect availableGeometry = QApplication::primaryScreen()->availableGeometry();
            QRect windowRect(0, 0, availableGeometry.width() / 2, availableGeometry.height() / 2);
            windowRect = windowRect.translated(availableGeometry.center() - windowRect.center());
            m_mainWindow->setGeometry(windowRect);
        }
        else
        {
            m_mainWindow->restoreGeometry(geometry);
        }

        QByteArray state = settings.value("windowState", QByteArray()).toByteArray();
        if (!state.isEmpty())
        {
            m_mainWindow->restoreState(state, WINDOW_STATE_VERSION);
        }
    }

    if (settingsFlags.testFlag(GeneralSettings))
    {
        m_settings->readSettings(settings, m_CMSManager->getDefaultSettings());
        m_CMSManager->setSettings(m_settings->getColorManagementSystemSettings());

        if (m_textToSpeech)
        {
            m_textToSpeech->setSettings(m_settings);
        }

        if (m_formManager)
        {
            m_formManager->setAppearanceFlags(m_settings->getSettings().m_formAppearanceFlags);
        }

        updateBookmarkSettings();
    }

    if (settingsFlags.testFlag(PluginsSettings))
    {
        // Load allowed plugins
        settings.beginGroup("Plugins");
        m_loadAllPlugins = !settings.contains("EnabledPlugins");
        m_enabledPlugins = settings.value("EnabledPlugins").toStringList();

        // FamilyPDF features are delivered as editor plugins. Existing PDF4QT
        // profiles can contain an empty EnabledPlugins value, which otherwise
        // makes these built-in features look as if they were missing after an
        // upgrade. Enable each FamilyPDF plugin once while preserving the
        // user's choices on subsequent runs.
        constexpr int familyPdfPluginDefaultsVersion = 2;
        const int appliedDefaultsVersion = settings.value("FamilyPDFDefaultsVersion", 0).toInt();
        if (appliedDefaultsVersion < familyPdfPluginDefaultsVersion)
        {
            const QStringList familyPdfPlugins = {
                QStringLiteral("Document Edit"),
                QStringLiteral("Editor"),
                QStringLiteral("Forms"),
                QStringLiteral("FamilyPDF Office Export"),
                QStringLiteral("Redact"),
                QStringLiteral("Signature")
            };

            for (const QString& pluginName : familyPdfPlugins)
            {
                if (!m_enabledPlugins.contains(pluginName))
                {
                    m_enabledPlugins.append(pluginName);
                }
            }

            settings.setValue("EnabledPlugins", m_enabledPlugins);
            settings.setValue("FamilyPDFDefaultsVersion", familyPdfPluginDefaultsVersion);
        }
        settings.endGroup();
    }

    // Load action shortcuts
    if (settingsFlags.testFlag(ActionSettings))
    {
        settings.beginGroup("Actions");
        for (QAction* action : m_actionManager->getActions())
        {
            QString name = action->objectName();
            if (!name.isEmpty() && settings.contains(name))
            {
                QKeySequence sequence = QKeySequence::fromString(settings.value(name, action->shortcut().toString(QKeySequence::PortableText)).toString(), QKeySequence::PortableText);
                action->setShortcut(sequence);
            }
        }
        settings.endGroup();
    }

    if (settingsFlags.testFlag(RecentFileSettings) && m_recentFileManager)
    {
        // Load recent files
        settings.beginGroup("RecentFiles");
        m_recentFileManager->setRecentFilesLimit(settings.value("MaximumRecentFilesCount", PDFRecentFileManager::getDefaultRecentFiles()).toInt());
        m_recentFileManager->setRecentFiles(settings.value("RecentFileList", QStringList()).toStringList());
        settings.endGroup();
    }

    if (settingsFlags.testFlag(CertificateSettings))
    {
        // Load trusted certificates
        m_certificateStore.loadDefaultUserCertificates();
    }
}

void PDFProgramController::setPageLayout(pdf::PageLayout pageLayout)
{
    m_pdfWidget->getDrawWidgetProxy()->setPageLayout(pageLayout);
}

void PDFProgramController::updateActionsAvailability()
{
    const bool isBusy = (m_futureWatcher && m_futureWatcher->isRunning()) || m_isBusy;
    const bool hasDocument = m_pdfDocument != nullptr;
    const bool hasValidDocument = !isBusy && hasDocument;
    bool canPrint = false;
    bool canModify = false;
    if (m_pdfDocument)
    {
        const pdf::PDFObjectStorage& storage = m_pdfDocument->getStorage();
        const pdf::PDFSecurityHandler* securityHandler = storage.getSecurityHandler();
        canPrint = securityHandler->isAllowed(pdf::PDFSecurityHandler::Permission::PrintLowResolution) ||
                   securityHandler->isAllowed(pdf::PDFSecurityHandler::Permission::PrintHighResolution);
        canModify = securityHandler->isAllowed(pdf::PDFSecurityHandler::Permission::Modify) ||
                    securityHandler->isAllowed(pdf::PDFSecurityHandler::Permission::Assemble);
    }

    m_actionManager->setEnabled(PDFActionManager::Open, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::Close, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Quit, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::MergePdfs, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::Options, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::ResetToFactorySettings, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::About, !isBusy);
    m_actionManager->setEnabled(PDFActionManager::FitPage, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::FitWidth, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::FitHeight, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::ShowRenderingErrors, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Find, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Print, hasValidDocument && canPrint);
    m_actionManager->setEnabled(PDFActionManager::RenderToImages, hasValidDocument && canPrint);
    m_actionManager->setEnabled(PDFActionManager::ExportPageImages, hasValidDocument && canPrint);
    m_actionManager->setEnabled(PDFActionManager::Optimize, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::OptimizeImages, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Sanitize, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::RemoveExternalLinks, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::PageGeometry, hasValidDocument && canModify);
    m_actionManager->setEnabled(PDFActionManager::CreateBitonalDocument, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Encryption, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Save,
                                hasValidDocument && m_safeSaveBaseline.isValid);
    m_actionManager->setEnabled(PDFActionManager::SaveAs, hasValidDocument);
    m_actionManager->setEnabled(PDFActionManager::Properties, hasDocument);
    m_actionManager->setEnabled(PDFActionManager::SendByMail, hasDocument);
    m_actionManager->setEnabled(PDFActionManager::FullscreenMode, hasValidDocument);
    m_mainWindow->setEnabled(!isBusy);
    updateUndoRedoActions();
}

void PDFProgramController::onViewerSettingsChanged()
{
    m_pdfWidget->updateRenderer(m_settings->getRendererEngine());
    m_pdfWidget->updateCacheLimits(qsizetype(m_settings->getCompiledPageCacheLimit() * 1024LL), m_settings->getThumbnailsCacheLimit(), m_settings->getFontCacheLimit(), m_settings->getInstancedFontCacheLimit());
    m_pdfWidget->setSmoothWheelScrolling(m_settings->getSettings().m_smoothWheelScrolling);
    m_pdfWidget->setWheelScrollSpeed(m_settings->getSettings().m_wheelScrollHorizontalSpeedPercent, m_settings->getSettings().m_wheelScrollVerticalSpeedPercent);
    m_pdfWidget->getDrawWidgetProxy()->setFeatures(m_settings->getFeatures());
    m_pdfWidget->getDrawWidgetProxy()->setPreferredMeshResolutionRatio(m_settings->getPreferredMeshResolutionRatio());
    m_pdfWidget->getDrawWidgetProxy()->setMinimalMeshResolutionRatio(m_settings->getMinimalMeshResolutionRatio());
    m_pdfWidget->getDrawWidgetProxy()->setColorTolerance(m_settings->getColorTolerance());
    m_annotationManager->setFeatures(m_settings->getFeatures());
    m_annotationManager->setMeshQualitySettings(m_pdfWidget->getDrawWidgetProxy()->getMeshQualitySettings());
    pdf::PDFExecutionPolicy::setStrategy(m_settings->getMultithreadingStrategy());

    updateRenderingOptionActions();
}

void PDFProgramController::onColorManagementSystemChanged()
{
    m_settings->setColorManagementSystemSettings(m_CMSManager->getSettings());
}

void PDFProgramController::onFileChanged(const QString& fileName)
{
    QAction* autoRefreshDocumentAction = m_actionManager->getAction(PDFActionManager::AutomaticDocumentRefresh);

    if (m_isOutputWorkflowActive || // Print or export is using the current document
        !autoRefreshDocumentAction || // We do not have action
        !autoRefreshDocumentAction->isChecked() || // Auto refresh is not enabled
        m_fileInfo.originalFileName != fileName) // File is different
    {
        return;
    }

    if (m_undoRedoManager && !m_undoRedoManager->isCurrentSaved())
    {
        // If document is modified, we do not reload it
        return;
    }

    QFile file(fileName);
    if (file.open(QFile::ReadOnly))
    {
        QByteArray data = file.readAll();
        file.close();

        QByteArray hash = pdf::PDFDocumentReader::hash(data);
        if (m_pdfDocument && m_pdfDocument->getSourceDataHash() != hash)
        {
            auto queryPassword = [](bool* ok)
            {
                *ok = false;
                return QString();
            };

            // Try to open a new document
            pdf::PDFDocumentReader reader(m_progress, qMove(queryPassword), true, false);
            pdf::PDFDocument document = reader.readFromFile(fileName);

            if (reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK)
            {
                pdf::PDFDocumentPointer pointer(new pdf::PDFDocument(std::move(document)));
                pdf::PDFModifiedDocument modifiedDocument(std::move(pointer), m_optionalContentActivity, pdf::PDFModifiedDocument::ModificationFlags(pdf::PDFModifiedDocument::Reset | pdf::PDFModifiedDocument::PreserveView));
                onDocumentModified(std::move(modifiedDocument));
                m_undoRedoManager->setIsCurrentSaved();
            }
        }
    }
}

void PDFProgramController::onBookmarkActivated(int index, PDFBookmarkManager::Bookmark bookmark)
{
    Q_UNUSED(index);

    m_pdfWidget->getDrawWidgetProxy()->goToPage(bookmark.pageIndex);
}

void PDFProgramController::updateFileInfo(const QString& fileName)
{
    QFileInfo fileInfo(fileName);
    m_fileInfo.originalFileName = fileName;
    m_fileInfo.fileName = fileInfo.fileName();
    m_fileInfo.path = fileInfo.path();
    m_fileInfo.fileSize = fileInfo.size();
    m_fileInfo.writable = fileInfo.isWritable();
    m_fileInfo.creationTime = fileInfo.birthTime();
    m_fileInfo.lastModifiedTime = fileInfo.lastModified();
    m_fileInfo.lastReadTime = fileInfo.lastRead();
    m_fileInfo.absoluteFilePath = fileInfo.absoluteFilePath();

    updateFileWatcher(false);
    Q_EMIT documentPathChanged(m_fileInfo.absoluteFilePath);
}

void PDFProgramController::updateFileWatcher(bool forceDisable)
{
    QStringList oldFiles = m_fileWatcher.files();
    QStringList newFiles;

    QAction* action = m_actionManager->getAction(PDFActionManager::AutomaticDocumentRefresh);
    if (!forceDisable && !m_fileInfo.absoluteFilePath.isEmpty() && action && action->isChecked())
    {
        newFiles << m_fileInfo.absoluteFilePath;
    }

    if (oldFiles != newFiles)
    {
        m_fileWatcher.removePaths(oldFiles);
        m_fileWatcher.addPaths(newFiles);
    }
}

void PDFProgramController::scheduleRecoverySnapshot()
{
    if (!m_pdfDocument ||
        m_fileInfo.absoluteFilePath.isEmpty() ||
        !m_undoRedoManager ||
        m_undoRedoManager->isCurrentSaved())
    {
        return;
    }

    if (m_recoveryWatcher->isRunning())
    {
        m_recoveryPending = true;
        return;
    }
    m_recoveryTimer->start();
}

void PDFProgramController::startRecoverySnapshot()
{
    if (!m_pdfDocument ||
        m_fileInfo.absoluteFilePath.isEmpty() ||
        !m_undoRedoManager ||
        m_undoRedoManager->isCurrentSaved())
    {
        return;
    }
    if (m_recoveryWatcher->isRunning())
    {
        m_recoveryPending = true;
        return;
    }

    const pdf::PDFDocumentPointer documentSnapshot = m_pdfDocument;
    const QString sourcePath = m_fileInfo.absoluteFilePath;
    const int pageCount =
        static_cast<int>(documentSnapshot->getCatalog()->getPageCount());
    const QFuture<QString> future = QtConcurrent::run(
        [documentSnapshot, sourcePath, pageCount]() -> QString
        {
            const QString recoveryRoot = PDFRecoveryManager::defaultRoot();
            if (!QDir().mkpath(recoveryRoot))
            {
                return QString();
            }

            const QString finalSnapshot =
                PDFRecoveryManager::snapshotPath(sourcePath, recoveryRoot);
            const QString temporarySnapshot =
                finalSnapshot + QStringLiteral(".tmp-") +
                QUuid::createUuid().toString(QUuid::WithoutBraces);

            pdf::PDFDocumentWriter writer(nullptr);
            const pdf::PDFOperationResult writeResult =
                writer.write(temporarySnapshot, documentSnapshot.data(), true);
            if (!writeResult)
            {
                return QString();
            }

#ifdef Q_OS_WIN
            const QString nativeFinal =
                QDir::toNativeSeparators(QFileInfo(finalSnapshot).absoluteFilePath());
            const QString nativeTemporary =
                QDir::toNativeSeparators(QFileInfo(temporarySnapshot).absoluteFilePath());
            bool committed = false;
            if (QFileInfo::exists(finalSnapshot))
            {
                committed = ReplaceFileW(reinterpret_cast<LPCWSTR>(nativeFinal.utf16()),
                                         reinterpret_cast<LPCWSTR>(nativeTemporary.utf16()),
                                         nullptr,
                                         REPLACEFILE_WRITE_THROUGH,
                                         nullptr,
                                         nullptr);
            }
            else
            {
                committed = MoveFileExW(reinterpret_cast<LPCWSTR>(nativeTemporary.utf16()),
                                        reinterpret_cast<LPCWSTR>(nativeFinal.utf16()),
                                        MOVEFILE_WRITE_THROUGH);
            }
            if (!committed)
            {
                return QString();
            }
#else
            const QByteArray nativeFinal = QFile::encodeName(
                QFileInfo(finalSnapshot).absoluteFilePath());
            const QByteArray nativeTemporary = QFile::encodeName(
                QFileInfo(temporarySnapshot).absoluteFilePath());
            if (std::rename(nativeTemporary.constData(), nativeFinal.constData()) != 0)
            {
                return QString();
            }
#endif

            if (!PDFRecoveryManager::writeMetadata(sourcePath,
                                                   finalSnapshot,
                                                   pageCount,
                                                   recoveryRoot))
            {
                return QString();
            }
            return sourcePath;
        });
    m_recoveryWatcher->setFuture(future);
}

void PDFProgramController::openDocument(const QString& fileName)
{
    if (m_pdfDocument &&
        QFileInfo(fileName).absoluteFilePath().compare(
            m_fileInfo.absoluteFilePath, Qt::CaseInsensitive) != 0)
    {
        m_toolManager->getFindTextTool()->setActive(false);
        Q_EMIT openDocumentInNewTabRequested(fileName);
        return;
    }

    // First close old document
    closeDocument();

    updateFileInfo(fileName);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto readDocument = [this, fileName]() -> AsyncReadingResult
    {
        AsyncReadingResult result;

        auto queryPassword = [this](bool* ok)
        {
            QString result;
            *ok = false;
            Q_EMIT queryPasswordRequest(&result, ok);
            return result;
        };

        // Try to open a new document
        pdf::PDFDocumentReader reader(m_progress, qMove(queryPassword), true, false);
        pdf::PDFDocument document = reader.readFromFile(fileName);

        result.errorMessage = reader.getErrorMessage();
        result.result = reader.getReadingResult();
        if (result.result == pdf::PDFDocumentReader::Result::OK)
        {
            // Verify signatures
            pdf::PDFSignatureHandler::Parameters parameters;
            parameters.store = &m_certificateStore;
            parameters.dss = &document.getCatalog()->getDocumentSecurityStore();
            parameters.enableVerification = m_settings->getSettings().m_signatureVerificationEnabled;
            parameters.ignoreExpirationDate = m_settings->getSettings().m_signatureIgnoreCertificateValidityTime;
            parameters.useSystemCertificateStore = m_settings->getSettings().m_signatureUseSystemStore;

            pdf::PDFForm form = pdf::PDFForm::parse(&document, document.getCatalog()->getFormObject());
            result.signatures = pdf::PDFSignatureHandler::verifySignatures(form, reader.getSource(), parameters);
            result.document.reset(new pdf::PDFDocument(qMove(document)));
            result.safeSaveBaseline = PDFSafeSaveService::captureBaseline(fileName);
        }

        return result;
    };
    m_future = QtConcurrent::run(readDocument);
    m_futureWatcher = new QFutureWatcher<AsyncReadingResult>();
    connect(m_futureWatcher, &QFutureWatcher<AsyncReadingResult>::finished, this, &PDFProgramController::onDocumentReadingFinished);
    m_futureWatcher->setFuture(m_future);
    updateActionsAvailability();
}

void PDFProgramController::openRecoveryDocument(const QString& snapshotPath,
                                                const QString& originalSourcePath)
{
    openDocument(snapshotPath);
    m_openedRecoverySourcePath = QFileInfo(originalSourcePath).absoluteFilePath();
}

void PDFProgramController::onDocumentReadingFinished()
{
    QApplication::restoreOverrideCursor();

    AsyncReadingResult result = m_future.result();
    m_future = QFuture<AsyncReadingResult>();
    m_futureWatcher->deleteLater();
    m_futureWatcher = nullptr;

    switch (result.result)
    {
        case pdf::PDFDocumentReader::Result::OK:
        {
            m_safeSaveBaseline = result.safeSaveBaseline;
            // Mark current directory as this
            QFileInfo fileInfo(m_fileInfo.originalFileName);
            m_settings->setDirectory(fileInfo.dir().absolutePath());

            // We add file to recent files only, if we have successfully read the document
            m_recentFileManager->addRecentFile(m_fileInfo.originalFileName);

            m_pdfDocument = qMove(result.document);
            m_signatures = qMove(result.signatures);
            pdf::PDFModifiedDocument document(m_pdfDocument.data(), m_optionalContentActivity);
            setDocument(document, m_signatures, true);

            if (m_formManager)
            {
                m_formManager->performPaging();
            }

            pdf::PDFDocumentRequirements requirements = pdf::PDFDocumentRequirements::parse(&m_pdfDocument->getStorage(), m_pdfDocument->getCatalog()->getRequirements());
            constexpr pdf::PDFDocumentRequirements::Requirements requirementFlags = pdf::PDFDocumentRequirements::Requirements(pdf::PDFDocumentRequirements::OCInteract |
                                                                                                                               pdf::PDFDocumentRequirements::OCAutoStates |
                                                                                                                               pdf::PDFDocumentRequirements::Navigation |
                                                                                                                               pdf::PDFDocumentRequirements::Attachment |
                                                                                                                               pdf::PDFDocumentRequirements::DigSigValidation |
                                                                                                                               pdf::PDFDocumentRequirements::Encryption);
            pdf::PDFDocumentRequirements::ValidationResult requirementResult = requirements.validate(requirementFlags);
            if (requirementResult.isError())
            {
                QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), requirementResult.message);
            }
            else if (requirementResult.isWarning())
            {
                QMessageBox::warning(m_mainWindow, QApplication::applicationDisplayName(), requirementResult.message);
            }

            restoreDocumentViewState();

            m_mainWindowInterface->setStatusBarMessage(tr("Document '%1' was successfully loaded!").arg(m_fileInfo.fileName), 4000);
            break;
        }

        case pdf::PDFDocumentReader::Result::Failed:
        {
            m_openedRecoverySourcePath.clear();
            QMessageBox::critical(m_mainWindow, QApplication::applicationDisplayName(), tr("Document read error: %1").arg(result.errorMessage));
            break;
        }

        case pdf::PDFDocumentReader::Result::Cancelled:
            m_openedRecoverySourcePath.clear();
            break; // Do nothing, user cancelled the document reading
    }
    updateActionsAvailability();
}

void PDFProgramController::onDocumentModified(pdf::PDFModifiedDocument document)
{
    // We will create undo/redo step from old document, with flags from the new,
    // because new document is modification of old document with flags.

    pdf::PDFBoolGuard guard(m_isDocumentSetInProgress);

    if (m_undoRedoManager && m_pdfDocument)
    {
        m_undoRedoManager->createUndo(document, m_pdfDocument);
    }

    // Retain pointer on old document, because during the update,
    // old pointer must be valid, because some widgets holds raw
    // pointer.
    pdf::PDFDocumentPointer oldDocument = std::move(m_pdfDocument);
    Q_UNUSED(oldDocument);

    m_pdfDocument = document;
    document.setOptionalContentActivity(m_optionalContentActivity);
    setDocument(document, {}, false);
    scheduleRecoverySnapshot();
}

void PDFProgramController::onDocumentUndoRedo(pdf::PDFModifiedDocument document)
{
    m_pdfDocument = document;
    document.setOptionalContentActivity(m_optionalContentActivity);
    setDocument(document, {}, false);
    scheduleRecoverySnapshot();
}

void PDFProgramController::setDocument(pdf::PDFModifiedDocument document, std::vector<pdf::PDFSignatureVerificationResult> signatureVerificationResult, bool isCurrentSaved)
{
    // Results describe the exact bytes opened. Edits and Undo/Redo must not
    // retain a previously verified status for a different document revision.
    m_signatures = signatureVerificationResult;
    if (document.hasReset())
    {
        if (m_optionalContentActivity)
        {
            // We use deleteLater, because we want to avoid consistency problem with model
            // (we set document to the model before activity).
            m_optionalContentActivity->deleteLater();
            m_optionalContentActivity = nullptr;
        }

        if (document)
        {
            m_optionalContentActivity = new pdf::PDFOptionalContentActivity(document, pdf::OCUsage::View, this);
        }

        if (m_undoRedoManager && !document.hasFlag(pdf::PDFModifiedDocument::PreserveUndoRedo))
        {
            m_undoRedoManager->clear();
        }
    }
    else if (m_optionalContentActivity)
    {
        Q_ASSERT(document);
        m_optionalContentActivity->setDocument(document);
    }

    document.setOptionalContentActivity(m_optionalContentActivity);

    if (m_annotationManager)
    {
        m_annotationManager->setDocument(document);
    }

    if (m_bookmarkManager)
    {
        m_bookmarkManager->setProperty("familyPdfDocumentPath", document ? m_fileInfo.absoluteFilePath : QString());
        m_bookmarkManager->setDocument(document);
    }

    if (m_formManager)
    {
        m_formManager->setDocument(document);
    }

    if (m_toolManager)
    {
        if (document.hasReset() || document.hasPageContentsChanged())
            m_toolManager->getFindTextTool()->setActive(false);
        m_toolManager->setDocument(document);
    }

    if (m_textToSpeech)
    {
        m_textToSpeech->setDocument(document);
    }

    if (m_undoRedoManager)
    {
        m_undoRedoManager->setIsCurrentSaved(isCurrentSaved);
    }

    std::vector<pdf::PDFSignatureVerificationResult> fullscreenSignatureVerificationResult = signatureVerificationResult;
    m_pdfWidget->setDocument(document, std::move(signatureVerificationResult));
    if (m_fullscreenWidget)
    {
        m_fullscreenWidget->setDocument(document, std::move(fullscreenSignatureVerificationResult));
    }
    m_mainWindowInterface->setDocument(document);
    m_CMSManager->setDocument(document);

    updateTitle();
    m_mainWindowInterface->updateUI(true);

    for (const auto& plugin : m_loadedPlugins)
    {
        plugin.second->setDocument(document);
    }

    if (m_pdfDocument && document.hasReset() && !document.hasPreserveView())
    {
        const pdf::PDFCatalog* catalog = m_pdfDocument->getCatalog();

        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.beginGroup("PageLayoutPerDocumentSettings");
        QString pageLayoutStored = settings.value(m_fileInfo.absoluteFilePath, pdf::PDFPageLayoutUtils::convertPageLayoutToString(catalog->getPageLayout())).toString();
        settings.endGroup();

        const pdf::PageLayout pageLayout = pdf::PDFPageLayoutUtils::convertStringToPageLayout(pageLayoutStored, catalog->getPageLayout());
        setPageLayout(pageLayout);
        updatePageLayoutActions();

        if (const pdf::PDFAction* action = catalog->getOpenAction())
        {
            onActionTriggered(action);
        }
    }

    updateActionsAvailability();
}

void PDFProgramController::closeDocument()
{
    const QString closingSourcePath = m_fileInfo.absoluteFilePath;
    const QString closingRecoverySourcePath = m_openedRecoverySourcePath;
    m_openedRecoverySourcePath.clear();
    m_recoveryTimer->stop();
    m_recoveryPending = false;

    if (m_isFullscreenMode)
    {
        leaveFullscreenMode();
    }

    if (m_pdfDocument && !m_fileInfo.absoluteFilePath.isEmpty())
    {
        saveDocumentViewState();
        savePageLayoutPerDocument();
    }

    m_signatures.clear();
    m_safeSaveBaseline = PDFSafeSaveService::Baseline();
    setDocument(pdf::PDFModifiedDocument(), {}, true);
    m_pdfDocument.reset();
    updateActionsAvailability();
    updateTitle();
    updateFileInfo(QString());

    if (!closingSourcePath.isEmpty())
    {
        PDFRecoveryManager::removeRecord(closingSourcePath);
    }
    if (!closingRecoverySourcePath.isEmpty())
    {
        PDFRecoveryManager::removeRecord(closingRecoverySourcePath);
    }
}

void PDFProgramController::updateRenderingOptionActions()
{
    const pdf::PDFRenderer::Features features = m_settings->getFeatures();
    for (QAction* action : m_actionManager->getRenderingOptionActions())
    {
        action->setChecked(features.testFlag(static_cast<pdf::PDFRenderer::Feature>(action->data().toInt())));
    }
}

void PDFProgramController::updateBookmarkSettings()
{
    const bool enable = m_settings->getSettings().m_autoGenerateBookmarks;

    if (m_bookmarkManager)
    {
        m_bookmarkManager->setGenerateBookmarksAutomatically(enable);
    }

    QAction* action = m_actionManager->getAction(PDFActionManager::BookmarkGenerateAutomatically);
    if (action)
    {
        action->setChecked(enable);
    }
}

void PDFProgramController::updateTitle()
{
    if (m_pdfDocument)
    {
        QString title = m_pdfDocument->getInfo()->title;

        if (title.isEmpty())
        {
            title = m_fileInfo.fileName;
        }

        if (m_undoRedoManager && !m_undoRedoManager->isCurrentSaved())
        {
            title += "*";
        }

        m_mainWindow->setWindowTitle(tr("%1 - %2").arg(title, QApplication::applicationDisplayName()));
    }
    else
    {
        m_mainWindow->setWindowTitle(QApplication::applicationDisplayName());
    }
}

void PDFProgramController::updatePageLayoutActions()
{
    for (PDFActionManager::Action action : { PDFActionManager::PageLayoutSinglePage, PDFActionManager::PageLayoutContinuous,
                                             PDFActionManager::PageLayoutTwoPages, PDFActionManager::PageLayoutTwoColumns })
    {
        m_actionManager->setChecked(action, false);
    }

    const pdf::PageLayout pageLayout = m_pdfWidget->getDrawWidgetProxy()->getPageLayout();
    switch (pageLayout)
    {
        case pdf::PageLayout::SinglePage:
            m_actionManager->setChecked(PDFActionManager::PageLayoutSinglePage, true);
            break;

        case pdf::PageLayout::OneColumn:
            m_actionManager->setChecked(PDFActionManager::PageLayoutContinuous, true);
            break;

        case pdf::PageLayout::TwoColumnLeft:
        case pdf::PageLayout::TwoColumnRight:
            m_actionManager->setChecked(PDFActionManager::PageLayoutTwoColumns, true);
            m_actionManager->setChecked(PDFActionManager::PageLayoutFirstPageOnRightSide, pageLayout == pdf::PageLayout::TwoPagesRight);
            break;

        case pdf::PageLayout::TwoPagesLeft:
        case pdf::PageLayout::TwoPagesRight:
            m_actionManager->setChecked(PDFActionManager::PageLayoutTwoPages, true);
            m_actionManager->setChecked(PDFActionManager::PageLayoutFirstPageOnRightSide, pageLayout == pdf::PageLayout::TwoPagesRight);
            break;

        default:
            Q_ASSERT(false);
    }

    m_actionManager->setChecked(PDFActionManager::FullscreenMode, m_isFullscreenMode);
}

void PDFProgramController::enterFullscreenMode()
{
    if (m_isFullscreenMode || !m_pdfDocument)
    {
        return;
    }

    m_isFullscreenMode = true;

    m_fullscreenWidget = new PDFFullscreenWidget(m_CMSManager, m_settings->getRendererEngine(), m_mainWindow);
    connect(m_fullscreenWidget, &PDFFullscreenWidget::exitRequested, this, [this]()
    {
        if (QAction* action = m_actionManager->getAction(PDFActionManager::FullscreenMode))
        {
            action->setChecked(false);
        }
        leaveFullscreenMode();
    });

    pdf::PDFWidget* fullscreenPdfWidget = m_fullscreenWidget->getPdfWidget();
    fullscreenPdfWidget->updateCacheLimits(qsizetype(m_settings->getCompiledPageCacheLimit() * 1024LL),
                                           m_settings->getThumbnailsCacheLimit(),
                                           m_settings->getFontCacheLimit(),
                                           m_settings->getInstancedFontCacheLimit());
    fullscreenPdfWidget->setSmoothWheelScrolling(m_settings->getSettings().m_smoothWheelScrolling);
    fullscreenPdfWidget->setWheelScrollSpeed(m_settings->getSettings().m_wheelScrollHorizontalSpeedPercent, m_settings->getSettings().m_wheelScrollVerticalSpeedPercent);
    fullscreenPdfWidget->getDrawWidgetProxy()->setProgress(m_progress);
    fullscreenPdfWidget->getDrawWidgetProxy()->setFeatures(m_settings->getFeatures());
    fullscreenPdfWidget->getDrawWidgetProxy()->setPreferredMeshResolutionRatio(m_settings->getPreferredMeshResolutionRatio());
    fullscreenPdfWidget->getDrawWidgetProxy()->setMinimalMeshResolutionRatio(m_settings->getMinimalMeshResolutionRatio());
    fullscreenPdfWidget->getDrawWidgetProxy()->setColorTolerance(m_settings->getColorTolerance());
    fullscreenPdfWidget->setDocument(pdf::PDFModifiedDocument(m_pdfDocument, m_optionalContentActivity), m_signatures);

    const std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (!currentPages.empty())
    {
        fullscreenPdfWidget->getDrawWidgetProxy()->goToPage(currentPages.front());
    }
    fullscreenPdfWidget->getDrawWidgetProxy()->zoom(m_pdfWidget->getDrawWidgetProxy()->getZoom());

    fullscreenPdfWidget->getDrawWidgetProxy()->setPageLayout(pdf::PageLayout::SinglePage);

    m_fullscreenWidget->showFullScreen();
    fullscreenPdfWidget->setFocus();
    updatePageLayoutActions();
    updateActionsAvailability();
}

void PDFProgramController::leaveFullscreenMode()
{
    if (!m_isFullscreenMode)
    {
        return;
    }

    m_isFullscreenMode = false;

    pdf::PDFInteger targetPage = -1;
    pdf::PDFReal targetZoom = m_pdfWidget->getDrawWidgetProxy()->getZoom();
    if (m_fullscreenWidget)
    {
        if (pdf::PDFWidget* fullscreenPdfWidget = m_fullscreenWidget->getPdfWidget())
        {
            const std::vector<pdf::PDFInteger> currentPages = fullscreenPdfWidget->getDrawWidget()->getCurrentPages();
            if (!currentPages.empty())
            {
                targetPage = currentPages.front();
            }
            targetZoom = fullscreenPdfWidget->getDrawWidgetProxy()->getZoom();
        }

        m_fullscreenWidget->deleteLater();
        m_fullscreenWidget = nullptr;
    }

    if (targetPage >= 0)
    {
        m_pdfWidget->getDrawWidgetProxy()->goToPage(targetPage);
    }
    m_pdfWidget->getDrawWidgetProxy()->zoom(targetZoom);
    updatePageLayoutActions();
    updateActionsAvailability();
    m_pdfWidget->setFocus();
}

void PDFProgramController::loadPlugins()
{
    QStringList availablePlugins;
    QDir directory(QApplication::applicationDirPath() + "/" PDF4QT_PLUGINS_RELATIVE_PATH);
#if defined(Q_OS_WIN)
    availablePlugins = directory.entryList(QStringList("*.dll"));
#elif defined(Q_OS_UNIX)
    availablePlugins = directory.entryList(QStringList("*.so"));
#else
    static_assert(false, "Implement this for another OS!");
#endif

    for (const QString& availablePlugin : availablePlugins)
    {
        QString pluginFileName = directory.absoluteFilePath(availablePlugin);
        QPluginLoader loader(pluginFileName);
        if (loader.load())
        {
            QJsonObject metaData = loader.metaData();
            m_plugins.emplace_back(pdf::PDFPluginInfo::loadFromJson(&metaData));
            m_plugins.back().pluginFile = availablePlugin;
            m_plugins.back().pluginFileWithPath = pluginFileName;

            QString pluginName = m_plugins.back().name;
            if (!m_enabledPlugins.contains(pluginName) && !m_loadAllPlugins)
            {
                loader.unload();
                continue;
            }

            if (m_loadAllPlugins)
            {
                m_enabledPlugins << pluginName;
            }

            pdf::PDFPlugin* plugin = qobject_cast<pdf::PDFPlugin*>(loader.instance());
            if (plugin)
            {
                m_loadedPlugins.push_back(std::make_pair(m_plugins.back(), plugin));
            }
        }
    }
    m_loadAllPlugins = false;

    auto comparator = [](const std::pair<pdf::PDFPluginInfo, pdf::PDFPlugin*>& l, const std::pair<pdf::PDFPluginInfo, pdf::PDFPlugin*>& r)
    {
        return l.first.name < r.first.name;
    };
    std::sort(m_loadedPlugins.begin(), m_loadedPlugins.end(), comparator);

    // Plugin toolbars go to their own row, so they do not squeeze the main toolbar
    // (page number, zoom) out of sight.
    m_mainWindow->addToolBarBreak();

    for (const auto& plugin : m_loadedPlugins)
    {
        plugin.second->setDataExchangeInterface(this);
        plugin.second->setWidget(m_pdfWidget);
        plugin.second->setCMSManager(m_CMSManager);
        std::vector<QAction*> actions = plugin.second->getActions();

        if (!actions.empty())
        {
            QToolBar* toolBar = m_mainWindow->addToolBar(plugin.first.name);
            toolBar->setObjectName(QString("Plugin_Toolbar_%1").arg(plugin.first.name));
            m_mainWindowInterface->adjustToolbar(toolBar);
            QMenu* menu = m_mainWindowInterface->addToolMenu(plugin.second->getPluginMenuName());
            for (QAction* action : actions)
            {
                if (!action)
                {
                    menu->addSeparator();
                    toolBar->addSeparator();
                    continue;
                }

                m_actionManager->addAdditionalAction(action);

                menu->addAction(action);
                toolBar->addAction(action);
            }
        }
    }
}

void PDFProgramController::writeSettings()
{
    Q_ASSERT(!m_isFactorySettingsBeingRestored);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", m_mainWindow->saveGeometry());
    settings.setValue("windowState", m_mainWindow->saveState(WINDOW_STATE_VERSION));

    m_settings->writeSettings(settings);

    // Save action shortcuts
    settings.beginGroup("Actions");
    for (QAction* action : m_actionManager->getActions())
    {
        QString name = action->objectName();
        if (!name.isEmpty())
        {
            QString accelerator = action->shortcut().toString(QKeySequence::PortableText);
            settings.setValue(name, accelerator);
        }
    }
    settings.endGroup();

    // Save recent files
    settings.beginGroup("RecentFiles");
    settings.setValue("MaximumRecentFilesCount", m_recentFileManager->getRecentFilesLimit());
    settings.setValue("RecentFileList", m_recentFileManager->getRecentFiles());
    settings.endGroup();

    // Save allowed plugins
    settings.beginGroup("Plugins");
    settings.setValue("EnabledPlugins", m_enabledPlugins);
    settings.endGroup();

    // Save trusted certificates
    m_certificateStore.saveDefaultUserCertificates();
}

void PDFProgramController::resetSettings()
{
    if (!canClose())
    {
        return;
    }

    if (QMessageBox::question(m_mainWindow, tr("Reset Settings"), tr("Do you wish to restore the default factory settings of the program? All settings changed by the user will be deleted. Application will be closed."), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        closeDocument();

        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.clear();

        QMessageBox::information(m_mainWindow, tr("Reset Settings"), tr("Default factory settings were restored. Application will be now closed."));
        m_isFactorySettingsBeingRestored = true;
        m_mainWindow->close();
    }
}

void PDFProgramController::clearRecentFileHistory()
{
    m_recentFileManager->clearRecentFiles();
}

void PDFProgramController::onActionOptionsTriggered()
{
    PDFViewerSettingsDialog::OtherSettings otherSettings;
    otherSettings.maximumRecentFileCount = m_recentFileManager->getRecentFilesLimit();

    PDFViewerSettingsDialog dialog(m_settings->getSettings(), m_settings->getColorManagementSystemSettings(),
                                   otherSettings, m_certificateStore, m_actionManager->getActions(), m_CMSManager,
                                   m_enabledPlugins, m_plugins, m_mainWindow);
    if (dialog.exec() == QDialog::Accepted)
    {
        const bool pluginsChanged = m_enabledPlugins != dialog.getEnabledPlugins();

        m_settings->setSettings(dialog.getSettings());
        m_settings->setColorManagementSystemSettings(dialog.getCMSSettings());
        m_CMSManager->setSettings(m_settings->getColorManagementSystemSettings());
        if (m_recentFileManager)
        {
            m_recentFileManager->setRecentFilesLimit(dialog.getOtherSettings().maximumRecentFileCount);
        }
        if (m_textToSpeech)
        {
            m_textToSpeech->setSettings(m_settings);
        }
        if (m_formManager)
        {
            m_formManager->setAppearanceFlags(m_settings->getSettings().m_formAppearanceFlags);
        }
        m_certificateStore = dialog.getCertificateStore();
        m_enabledPlugins = dialog.getEnabledPlugins();
        updateMagnifierToolSettings();
        updateUndoRedoSettings();

        if (pluginsChanged)
        {
            QMessageBox::information(m_mainWindow, tr("Plugins"), tr("Plugin on/off state has been changed. Please restart application to apply settings."));
        }
    }
}

void PDFProgramController::onActionCertificateManagerTriggered()
{
    pdf::PDFCertificateManagerDialog dialog(getMainWindow());
    dialog.exec();
}

void PDFProgramController::onDrawSpaceChanged()
{
    if (!m_isDocumentSetInProgress)
    {
        m_mainWindowInterface->updateUI(false);
    }
}

void PDFProgramController::onPageLayoutChanged()
{
    m_mainWindowInterface->updateUI(false);
    updatePageLayoutActions();
}

void PDFProgramController::onActionOpenTriggered()
{
    QString fileName = QFileDialog::getOpenFileName(m_mainWindow, tr("Select PDF document"), m_settings->getDirectory(), tr("PDF document (*.pdf)"));
    if (!fileName.isEmpty())
    {
        openDocument(fileName);
    }
}

void PDFProgramController::extractPages()
{
    const pdf::PDFDocument* document = getDocument();
    if (!document)
    {
        return;
    }

    const pdf::PDFInteger pageCount = document->getCatalog()->getPageCount();
    const std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    bool ok = false;
    const QString rangeText = QInputDialog::getText(m_mainWindow,
                                                    tr("Extract Pages"),
                                                    tr("Pages to extract, for example 1-3,8,10-12 (document has %1 pages):").arg(pageCount)
                                                        + '\n' + tr("Repeated pages are included once, in document order."),
                                                    QLineEdit::Normal,
                                                    currentPages.empty() ? QString() : QString::number(currentPages.front() + 1),
                                                    &ok);
    if (!ok)
    {
        return;
    }

    QString errorMessage;
    const pdf::PDFClosedIntervalSet pageNumbers = pdf::PDFClosedIntervalSet::parsePageSelection(pageCount, rangeText, &errorMessage);
    if (!errorMessage.isEmpty() || pageNumbers.isEmpty())
    {
        QMessageBox::critical(m_mainWindow, tr("Extract Pages"), errorMessage.isEmpty() ? tr("No pages selected.") : errorMessage);
        return;
    }

    std::vector<pdf::PDFInteger> pageIndices;
    for (const pdf::PDFInteger pageNumber : pageNumbers.unfold())
    {
        pageIndices.push_back(pageNumber - 1);
    }
    extractPages(pageIndices);
}

void PDFProgramController::extractPages(const std::vector<pdf::PDFInteger>& pageIndices)
{
    const pdf::PDFDocument* document = getDocument();
    if (!document)
    {
        return;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(document->getCatalog()->getPageCount());
    std::vector<pdf::PDFInteger> selectedPages = pageIndices;
    selectedPages.erase(std::remove_if(selectedPages.begin(), selectedPages.end(), [pageCount](pdf::PDFInteger pageIndex)
    {
        return pageIndex < 0 || pageIndex >= pageCount;
    }), selectedPages.end());
    std::sort(selectedPages.begin(), selectedPages.end());
    selectedPages.erase(std::unique(selectedPages.begin(), selectedPages.end()), selectedPages.end());
    if (selectedPages.empty())
    {
        QMessageBox::critical(m_mainWindow, tr("Extract Pages"), tr("No pages selected."));
        return;
    }

    const QFileInfo sourceInfo(getOriginalFileName());
    const QString baseName = sourceInfo.completeBaseName().isEmpty() ? tr("document") : sourceInfo.completeBaseName();
    const QString suggestedDirectory = sourceInfo.absolutePath().isEmpty() ? m_settings->getDirectory() : sourceInfo.absolutePath();
    QStringList selectedPageNumbers;
    selectedPageNumbers.reserve(selectedPages.size());
    for (const pdf::PDFInteger pageIndex : selectedPages)
    {
        selectedPageNumbers.push_back(QString::number(pageIndex + 1));
    }
    const QString suggestedFile = QDir(suggestedDirectory).filePath(QString("%1_p%2.pdf").arg(baseName, selectedPageNumbers.join('_')));
    const QString fileName = QFileDialog::getSaveFileName(m_mainWindow, tr("Save Extracted Pages"), suggestedFile, tr("PDF document (*.pdf)"));
    if (fileName.isEmpty())
    {
        return;
    }

    pdf::PDFDocumentManipulator::AssembledPages assembledPages;
    const pdf::PDFDocumentManipulator::AssembledPages allPages = pdf::PDFDocumentManipulator::createAllDocumentPages(0, document);
    for (const pdf::PDFInteger pageIndex : selectedPages)
    {
        assembledPages.push_back(allPages[pageIndex]);
    }

    pdf::PDFDocumentManipulator manipulator;
    manipulator.setOutlineMode(pdf::PDFDocumentManipulator::OutlineMode::NoOutline);
    manipulator.addDocument(0, document);
    pdf::PDFOperationResult result = manipulator.assemble(assembledPages);
    if (result)
    {
        pdf::PDFDocumentWriter writer(nullptr);
        // Commit only a complete PDF. QSaveFile's direct-write fallback stays disabled.
        QSaveFile output(fileName);
        if (!output.open(QIODevice::WriteOnly))
        {
            result = output.errorString();
        }
        else
        {
            result = writer.write(&output, &manipulator.getAssembledDocument());
            if (result && !output.commit()) result = output.errorString();
            if (!result) output.cancelWriting();
        }
    }

    if (!result)
    {
        QMessageBox::critical(m_mainWindow, tr("Extract Pages"), result.getErrorMessage());
        return;
    }

    QMessageBox::information(m_mainWindow, tr("Extract Pages"), tr("Saved %1 pages to %2.").arg(assembledPages.size()).arg(QDir::toNativeSeparators(fileName)));
}

void PDFProgramController::mergePdfs()
{
    QString outputToOpen;
    {
        // The dialog keeps the open document (when it is added to the list), so automatic reload must not
        // replace it while the dialog is open.
        const pdf::PDFDocumentPointer documentKeepAlive = m_pdfDocument;
        m_isOutputWorkflowActive = true;
        const auto outputWorkflowGuard = qScopeGuard([this]() { m_isOutputWorkflowActive = false; });

        PDFMergePdfsDialog::Request request;
        const QFileInfo sourceInfo(getOriginalFileName());
        request.directory = sourceInfo.absolutePath().isEmpty() ? m_settings->getDirectory() : sourceInfo.absolutePath();
        if (m_pdfDocument)
        {
            request.currentDocument = pdf::PDFDocumentMerger::createSource(getOriginalFileName(),
                                                                           sourceInfo.fileName().isEmpty() ? tr("Untitled") : sourceInfo.fileName(),
                                                                           m_pdfDocument);
        }

        PDFMergePdfsDialog dialog(request, m_mainWindow);
        dialog.exec();
        if (dialog.isOpenOutputRequested())
        {
            outputToOpen = dialog.getOutputFile();
        }
    }

    if (!outputToOpen.isEmpty())
    {
        // With a document already open this opens the merged PDF in a new tab; nothing is closed.
        openDocument(outputToOpen);
    }
}

void PDFProgramController::deletePages(const std::vector<pdf::PDFInteger>& pageIndices)
{
    if (!m_pdfDocument)
    {
        return;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    std::vector<pdf::PDFInteger> selectedPages = pageIndices;
    selectedPages.erase(std::remove_if(selectedPages.begin(), selectedPages.end(), [pageCount](pdf::PDFInteger pageIndex)
    {
        return pageIndex < 0 || pageIndex >= pageCount;
    }), selectedPages.end());
    std::sort(selectedPages.begin(), selectedPages.end());
    selectedPages.erase(std::unique(selectedPages.begin(), selectedPages.end()), selectedPages.end());
    if (selectedPages.empty())
    {
        return;
    }
    if (pdf::PDFInteger(selectedPages.size()) >= pageCount)
    {
        QMessageBox::warning(m_mainWindow, tr("Delete Pages"), tr("A PDF document must contain at least one page. Select fewer pages and try again."));
        return;
    }

    const std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    const pdf::PDFInteger oldCurrentPage = currentPages.empty() ? selectedPages.front() : currentPages.front();

    pdf::PDFDocumentModifier modifier(m_pdfDocument.data());
    pdf::PDFDocumentBuilder* builder = modifier.getBuilder();
    std::vector<pdf::PDFObjectReference> pages = builder->getPages();
    std::vector<pdf::PDFObjectReference> remainingPages;
    remainingPages.reserve(pages.size() - selectedPages.size());
    for (pdf::PDFInteger pageIndex = 0; pageIndex < pdf::PDFInteger(pages.size()); ++pageIndex)
    {
        if (!std::binary_search(selectedPages.cbegin(), selectedPages.cend(), pageIndex))
        {
            remainingPages.push_back(pages[pageIndex]);
        }
    }
    builder->setPages(remainingPages);
    modifier.markReset();
    if (!modifier.finalize())
    {
        return;
    }

    pdf::PDFModifiedDocument::ModificationFlags flags = modifier.getFlags();
    flags.setFlag(pdf::PDFModifiedDocument::PreserveUndoRedo);
    onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), m_optionalContentActivity, flags));

    const pdf::PDFInteger pagesBeforeCurrent = pdf::PDFInteger(std::count_if(selectedPages.cbegin(), selectedPages.cend(), [oldCurrentPage](pdf::PDFInteger pageIndex)
    {
        return pageIndex < oldCurrentPage;
    }));
    const pdf::PDFInteger newCurrentPage = std::clamp(oldCurrentPage - pagesBeforeCurrent,
                                                      pdf::PDFInteger(0),
                                                      pdf::PDFInteger(remainingPages.size() - 1));
    m_pdfWidget->getDrawWidgetProxy()->goToPage(newCurrentPage);
}

bool PDFProgramController::reorderPages(const std::vector<pdf::PDFInteger>& newPageOrder)
{
    if (!m_pdfDocument)
    {
        return false;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    if (!PDFPageReorder::isPermutation(newPageOrder, pageCount))
    {
        qWarning() << "Page reorder rejected: the new order is not a permutation of" << pageCount << "pages.";
        return false;
    }
    if (PDFPageReorder::isIdentity(newPageOrder))
    {
        return false;
    }

    const std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    const pdf::PDFInteger oldCurrentPage = currentPages.empty() ? 0 : currentPages.front();

    pdf::PDFDocumentModifier modifier(m_pdfDocument.data());
    pdf::PDFDocumentBuilder* builder = modifier.getBuilder();
    std::vector<pdf::PDFObjectReference> pages = builder->getPages();
    if (pdf::PDFInteger(pages.size()) != pageCount)
    {
        // Nested page tree. Flattening copies the inherited attributes into the pages.
        builder->flattenPageTree();
        pages = builder->getPages();
    }

    // The page tree must list exactly the pages the user sees, in the same order.
    bool pageTreeMatches = pdf::PDFInteger(pages.size()) == pageCount;
    for (pdf::PDFInteger pageIndex = 0; pageTreeMatches && pageIndex < pageCount; ++pageIndex)
    {
        pageTreeMatches = pages[size_t(pageIndex)] == m_pdfDocument->getCatalog()->getPage(size_t(pageIndex))->getPageReference();
    }
    if (!pageTreeMatches)
    {
        qWarning() << "Page reorder rejected: the page tree does not match the page list.";
        return false;
    }

    std::vector<pdf::PDFObjectReference> reorderedPages;
    reorderedPages.reserve(pages.size());
    for (const pdf::PDFInteger oldIndex : newPageOrder)
    {
        reorderedPages.push_back(pages[size_t(oldIndex)]);
    }
    builder->setPages(reorderedPages);
    modifier.markReset();
    if (!modifier.finalize())
    {
        return false;
    }

    pdf::PDFModifiedDocument::ModificationFlags flags = modifier.getFlags();
    flags.setFlag(pdf::PDFModifiedDocument::PreserveUndoRedo);
    onDocumentModified(pdf::PDFModifiedDocument(modifier.getDocument(), m_optionalContentActivity, flags));

    // Keep showing the page that was being read, wherever it went.
    const auto newCurrentPosition = std::find(newPageOrder.cbegin(), newPageOrder.cend(), oldCurrentPage);
    if (newCurrentPosition != newPageOrder.cend())
    {
        m_pdfWidget->getDrawWidgetProxy()->goToPage(pdf::PDFInteger(newCurrentPosition - newPageOrder.cbegin()));
    }
    return true;
}

namespace
{

/// Anchor pages for an insertion: the selected thumbnails, or the current page when nothing is selected.
std::vector<pdf::PDFInteger> getInsertAnchorPages(std::vector<pdf::PDFInteger> anchorPages, const std::vector<pdf::PDFInteger>& currentPages, pdf::PDFInteger pageCount)
{
    anchorPages.erase(std::remove_if(anchorPages.begin(), anchorPages.end(), [pageCount](pdf::PDFInteger pageIndex)
    {
        return pageIndex < 0 || pageIndex >= pageCount;
    }), anchorPages.end());
    std::sort(anchorPages.begin(), anchorPages.end());
    anchorPages.erase(std::unique(anchorPages.begin(), anchorPages.end()), anchorPages.end());
    if (anchorPages.empty())
    {
        anchorPages.push_back(currentPages.empty() || pageCount < 1 ? 0 : std::clamp(currentPages.front(), pdf::PDFInteger(0), pageCount - 1));
    }
    return anchorPages;
}

}   // namespace

void PDFProgramController::insertBlankPage(const std::vector<pdf::PDFInteger>& anchorPages)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return;     // the Viewer is read-only
    }

    const QStringList blockers = pdf::PDFPageInserter::checkTarget(m_pdfDocument.data(), false);
    if (!blockers.isEmpty())
    {
        QMessageBox::warning(m_mainWindow, tr("Insert Blank Page"), blockers.join('\n'));
        return;
    }

    const pdf::PDFDocumentPointer document = m_pdfDocument;
    const pdf::PDFInteger pageCount = pdf::PDFInteger(document->getCatalog()->getPageCount());
    PDFInsertPagesDialog::Request request;
    request.mode = PDFInsertPagesDialog::Mode::BlankPage;
    request.document = document.data();
    request.anchorPages = getInsertAnchorPages(anchorPages, m_pdfWidget->getDrawWidget()->getCurrentPages(), pageCount);
    PDFInsertPagesDialog dialog(request, m_mainWindow);
    if (dialog.exec() != QDialog::Accepted || m_pdfDocument != document)
    {
        return;
    }

    QRectF mediaBox(0, 0, 595.276, 841.89);     // A4 in points
    QRectF cropBox;
    pdf::PageRotation rotation = pdf::PageRotation::None;
    const pdf::PDFInteger sizePage = dialog.getSizePage();
    if (sizePage >= 0 && sizePage < pageCount)
    {
        const pdf::PDFPage* page = document->getCatalog()->getPage(size_t(sizePage));
        mediaBox = page->getMediaBox();
        cropBox = page->getCropBox();
        rotation = page->getPageRotation();
    }
    insertBlankPageAt(dialog.getInsertIndex(), mediaBox, cropBox, rotation);
}

void PDFProgramController::insertPagesFromPdf(const std::vector<pdf::PDFInteger>& anchorPages)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return;     // the Viewer is read-only
    }

    const QStringList blockers = pdf::PDFPageInserter::checkTarget(m_pdfDocument.data(), true);
    if (!blockers.isEmpty())
    {
        QMessageBox::warning(m_mainWindow, tr("Insert Pages from PDF"), blockers.join('\n'));
        return;
    }

    const pdf::PDFDocumentPointer document = m_pdfDocument;
    PDFInsertPagesDialog::Request request;
    request.mode = PDFInsertPagesDialog::Mode::PagesFromPdf;
    request.document = document.data();
    request.anchorPages = getInsertAnchorPages(anchorPages, m_pdfWidget->getDrawWidget()->getCurrentPages(), pdf::PDFInteger(document->getCatalog()->getPageCount()));
    const QFileInfo sourceInfo(getOriginalFileName());
    request.directory = sourceInfo.absolutePath().isEmpty() ? m_settings->getDirectory() : sourceInfo.absolutePath();
    PDFInsertPagesDialog dialog(request, m_mainWindow);
    if (dialog.exec() != QDialog::Accepted || m_pdfDocument != document)
    {
        return;
    }
    insertPagesAt(dialog.getInsertIndex(), dialog.getSource(), dialog.getSourcePages());
}

void PDFProgramController::insertJpegPage(const std::vector<pdf::PDFInteger>& anchorPages)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return;     // the Viewer is read-only
    }

    const QFileInfo sourceInfo(getOriginalFileName());
    const QString directory = sourceInfo.absolutePath().isEmpty() ? m_settings->getDirectory() : sourceInfo.absolutePath();
    const QString fileName = QFileDialog::getOpenFileName(m_mainWindow,
                                                          tr("Insert Page from JPEG"),
                                                          directory,
                                                          tr("JPEG image (*.jpg *.jpeg)"));
    if (!fileName.isEmpty())
    {
        insertJpegPageFile(fileName, anchorPages);
    }
}

bool PDFProgramController::insertJpegPageFile(const QString& fileName, const std::vector<pdf::PDFInteger>& anchorPages)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return false;   // the Viewer is read-only
    }

    const QStringList blockers = pdf::PDFPageInserter::checkTarget(m_pdfDocument.data(), true);
    if (!blockers.isEmpty())
    {
        QMessageBox::warning(m_mainWindow, tr("Insert Page from JPEG"), blockers.join('\n'));
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(m_mainWindow,
                              tr("Insert Page from JPEG"),
                              tr("Cannot open JPEG file '%1'.").arg(QFileInfo(fileName).fileName()));
        return false;
    }

    const QByteArray bytes = file.readAll();
    pdf::PDFDocument imageDocument;
    QString errorMessage;
    if (!pdf::PDFJpegImage::createDocument(bytes, &imageDocument, &errorMessage))
    {
        QMessageBox::critical(m_mainWindow, tr("Insert Page from JPEG"), errorMessage);
        return false;
    }

    const pdf::PDFDocumentPointer target = m_pdfDocument;
    const pdf::PDFInteger pageCount = pdf::PDFInteger(target->getCatalog()->getPageCount());
    const std::vector<pdf::PDFInteger> pages = getInsertAnchorPages(anchorPages,
                                                                      m_pdfWidget->getDrawWidget()->getCurrentPages(),
                                                                      pageCount);
    const pdf::PDFDocumentPointer sourceDocument(new pdf::PDFDocument(std::move(imageDocument)));
    const pdf::PDFDocumentMerger::Source source = pdf::PDFDocumentMerger::createSource(fileName,
                                                                                         QFileInfo(fileName).fileName(),
                                                                                         sourceDocument);
    return insertPagesAt(pages.back() + 1, source, { 0 }, tr("Insert Page from JPEG"));
}

bool PDFProgramController::insertBlankPageAt(pdf::PDFInteger insertIndex, const QRectF& mediaBox, const QRectF& cropBox, pdf::PageRotation rotation)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return false;   // the Viewer is read-only
    }

    pdf::PDFDocumentPointer document;
    const pdf::PDFOperationResult result = pdf::PDFPageInserter::insertBlankPage(m_pdfDocument.data(), insertIndex, mediaBox, cropBox, rotation, &document);
    if (!result)
    {
        QMessageBox::critical(m_mainWindow, tr("Insert Blank Page"), result.getErrorMessage());
        return false;
    }
    publishInsertedPages(document, insertIndex, 1);
    return true;
}

bool PDFProgramController::insertPagesAt(pdf::PDFInteger insertIndex, const pdf::PDFDocumentMerger::Source& source, const std::vector<pdf::PDFInteger>& pages, const QString& title)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return false;   // the Viewer is read-only
    }

    // The import runs on copies; the open document changes only in publishInsertedPages.
    const QString messageTitle = title.isEmpty() ? tr("Insert Pages from PDF") : title;
    const pdf::PDFDocumentPointer target = m_pdfDocument;
    pdf::PDFDocumentPointer document;
    QStringList warnings;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const pdf::PDFOperationResult result = pdf::PDFPageInserter::insertPages(target.data(), insertIndex, source, pages, &document, &warnings);
    QApplication::restoreOverrideCursor();
    if (!result)
    {
        QMessageBox::critical(m_mainWindow, messageTitle, result.getErrorMessage());
        return false;
    }
    if (!warnings.isEmpty() &&
        QMessageBox::question(m_mainWindow, messageTitle, warnings.join(QStringLiteral("\n\n")) + QStringLiteral("\n\n") + tr("Insert the pages?"),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
    {
        return false;
    }
    if (m_pdfDocument != target)
    {
        return false;   // the document was reloaded or closed while the warning was open
    }
    publishInsertedPages(document, insertIndex, pdf::PDFInteger(pages.size()));
    return true;
}

bool PDFProgramController::duplicatePages(const std::vector<pdf::PDFInteger>& pageIndices)
{
    if (!m_undoRedoManager || !m_pdfDocument)
    {
        return false;   // the Viewer is read-only
    }

    // The open document is its own source: the copies go through the same restricted import as pages of another PDF.
    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    const std::vector<pdf::PDFInteger> pages = getInsertAnchorPages(pageIndices, m_pdfWidget->getDrawWidget()->getCurrentPages(), pageCount);
    const pdf::PDFDocumentMerger::Source source = pdf::PDFDocumentMerger::createSource(QString(), QFileInfo(getOriginalFileName()).fileName(), m_pdfDocument);
    return insertPagesAt(pages.back() + 1, source, pages, tr("Duplicate Pages"));
}

void PDFProgramController::publishInsertedPages(pdf::PDFDocumentPointer document, pdf::PDFInteger insertIndex, pdf::PDFInteger pageCount)
{
    // One modification, one Undo step. The view keeps its zoom; Fit modes recompute for the new pages as usual.
    const pdf::PDFModifiedDocument::ModificationFlags flags(pdf::PDFModifiedDocument::Reset |
                                                            pdf::PDFModifiedDocument::PreserveUndoRedo |
                                                            pdf::PDFModifiedDocument::PreserveView);
    onDocumentModified(pdf::PDFModifiedDocument(document, m_optionalContentActivity, flags));
    m_pdfWidget->getDrawWidgetProxy()->goToPage(insertIndex);

    std::vector<pdf::PDFInteger> insertedPages(static_cast<size_t>(pageCount));
    std::iota(insertedPages.begin(), insertedPages.end(), insertIndex);
    Q_EMIT pagesInserted(insertedPages);
}

void PDFProgramController::rotatePages(const std::vector<pdf::PDFInteger>& pageIndices, int quarterTurns)
{
    if (!m_pdfDocument || quarterTurns == 0)
    {
        return;
    }

    const pdf::PDFInteger pageCount = pdf::PDFInteger(m_pdfDocument->getCatalog()->getPageCount());
    std::vector<pdf::PDFInteger> selectedPages = pageIndices;
    selectedPages.erase(std::remove_if(selectedPages.begin(), selectedPages.end(), [pageCount](pdf::PDFInteger pageIndex)
    {
        return pageIndex < 0 || pageIndex >= pageCount;
    }), selectedPages.end());
    std::sort(selectedPages.begin(), selectedPages.end());
    selectedPages.erase(std::unique(selectedPages.begin(), selectedPages.end()), selectedPages.end());
    if (selectedPages.empty())
    {
        return;
    }

    QStringList pageNumbers;
    pageNumbers.reserve(selectedPages.size());
    for (const pdf::PDFInteger pageIndex : selectedPages)
    {
        pageNumbers.push_back(QString::number(pageIndex + 1));
    }

    pdf::PDFPageGeometrySettings settings;
    settings.pageRange = pageNumbers.join(',');
    settings.applyMediaBox = false;
    settings.applyCropBox = false;
    settings.rotationQuarterTurns = quarterTurns;

    pdf::PDFDocumentPointer modifiedDocument(new pdf::PDFDocument(*m_pdfDocument));
    pdf::PDFModifiedDocument::ModificationFlags flags;
    const pdf::PDFOperationResult result = pdf::PDFPageGeometry::apply(modifiedDocument.data(), settings, &flags);
    if (!result)
    {
        QMessageBox::critical(m_mainWindow, tr("Rotate Pages"), result.getErrorMessage());
        return;
    }
    if (flags == pdf::PDFModifiedDocument::ModificationFlags())
    {
        return;
    }

    onDocumentModified(pdf::PDFModifiedDocument(modifiedDocument, m_optionalContentActivity, flags));
}

void PDFProgramController::launchOcrPlugin()
{
    const QString inputFile = getOriginalFileName();
    if (inputFile.isEmpty() || !QFileInfo::exists(inputFile))
    {
        QMessageBox::information(m_mainWindow,
                                 tr("FamilyPDF OCR"),
                                 tr("Open a saved PDF before starting OCR."));
        return;
    }

    const QString launcher = QDir(QCoreApplication::applicationDirPath()).filePath("FamilyPDF-OCR.cmd");
    if (!QFileInfo::exists(launcher))
    {
        QMessageBox::information(m_mainWindow,
                                 tr("FamilyPDF OCR"),
                                 tr("The optional FamilyPDF OCR plugin is not installed."));
        return;
    }

    const QFileInfo inputInfo(inputFile);
    const QString suggestedOutput = inputInfo.dir().filePath(inputInfo.completeBaseName() + ".ocr.pdf");
    const QString outputFile = QFileDialog::getSaveFileName(m_mainWindow,
                                                            tr("Save searchable OCR PDF"),
                                                            suggestedOutput,
                                                            tr("PDF document (*.pdf)"));
    if (outputFile.isEmpty())
    {
        return;
    }

#ifdef Q_OS_WIN
    const QString parameters = QString("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(inputFile),
                                                              QDir::toNativeSeparators(outputFile));
    const auto result = reinterpret_cast<qintptr>(
        ShellExecuteW(nullptr,
                      L"open",
                      reinterpret_cast<LPCWSTR>(launcher.utf16()),
                      reinterpret_cast<LPCWSTR>(parameters.utf16()),
                      reinterpret_cast<LPCWSTR>(QCoreApplication::applicationDirPath().utf16()),
                      SW_SHOWNORMAL));
    if (result <= 32)
    {
        QMessageBox::critical(m_mainWindow,
                              tr("FamilyPDF OCR"),
                              tr("Could not start the FamilyPDF OCR plugin."));
    }
#else
    if (!QProcess::startDetached(launcher, {inputFile, outputFile}, QCoreApplication::applicationDirPath()))
    {
        QMessageBox::critical(m_mainWindow,
                              tr("FamilyPDF OCR"),
                              tr("Could not start the FamilyPDF OCR plugin."));
    }
#endif
}

void PDFProgramController::onActionCloseTriggered()
{
    if (askForSaveDocumentBeforeClose())
    {
        closeDocument();
    }
}

void PDFProgramController::onActionGetSource()
{
    QDesktopServices::openUrl(QUrl("https://github.com/nanachi1212/myfamilypdf"));
}

void PDFProgramController::onActionBecomeSponsor()
{
    QDesktopServices::openUrl(QUrl("https://github.com/sponsors/JakubMelka"));
}

void PDFProgramController::onActionAutomaticDocumentRefresh()
{
    updateFileWatcher();
}

void PDFProgramController::onActionBookmarkPage()
{
    std::vector<pdf::PDFInteger> currentPages = m_pdfWidget->getDrawWidget()->getCurrentPages();
    if (!currentPages.empty())
    {
        m_bookmarkManager->toggleBookmark(currentPages.front());
    }
}

void PDFProgramController::onActionBookmarkGoToNext()
{
    m_bookmarkManager->goToNextBookmark();
}

void PDFProgramController::onActionBookmarkGoToPrevious()
{
    m_bookmarkManager->goToPreviousBookmark();
}

void PDFProgramController::onActionBookmarkExport()
{
    if (!m_pdfDocument)
    {
        return;
    }

    QFileInfo fileInfo(m_fileInfo.originalFileName);
    QString saveFileName = QFileDialog::getSaveFileName(m_mainWindow, tr("Export Bookmarks As"), fileInfo.dir().absoluteFilePath(m_fileInfo.originalFileName).replace(".pdf", ".json"), tr("JSON (*.json);;All files (*.*)"));
    if (!saveFileName.isEmpty())
    {
        m_bookmarkManager->saveToFile(saveFileName);
    }
}

void PDFProgramController::onActionBookmarkImport()
{
    if (!m_pdfDocument)
    {
        return;
    }

    QFileInfo fileInfo(m_fileInfo.originalFileName);
    QString fileName = QFileDialog::getOpenFileName(m_mainWindow, tr("Select PDF document"), fileInfo.dir().absolutePath(), tr("JSON (*.json)"));
    if (!fileName.isEmpty())
    {
        m_bookmarkManager->loadFromFile(fileName);
    }
}

void PDFProgramController::onActionBookmarkGenerateAutomatically(bool checked)
{
    auto settings = m_settings->getSettings();
    settings.m_autoGenerateBookmarks = checked;
    m_settings->setSettings(settings);
    m_bookmarkManager->setGenerateBookmarksAutomatically(checked);
}

void PDFProgramController::onPageRenderingErrorsChanged(pdf::PDFInteger pageIndex, int errorsCount)
{
    if (errorsCount > 0)
    {
        m_mainWindowInterface->setStatusBarMessage(tr("Rendering of page %1: %2 errors occured.").arg(pageIndex + 1).arg(errorsCount), 4000);
    }
}

}   // namespace pdfviewer
