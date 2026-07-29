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

#include "pdfbookmarkmanager.h"
#include "pdfaction.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QCryptographicHash>

namespace pdfviewer
{

class PDFBookmarkManagerHelper
{
public:
    constexpr PDFBookmarkManagerHelper() = delete;

    static QJsonObject convertBookmarkToJson(const PDFBookmarkManager::Bookmark& bookmark)
    {
        QJsonObject json;
        json["isAuto"] = bookmark.isAuto;
        json["name"] = bookmark.name;
        json["pageIndex"] = static_cast<qint64>(bookmark.pageIndex);
        return json;
    }

    static PDFBookmarkManager::Bookmark convertJsonToBookmark(const QJsonObject& json)
    {
        PDFBookmarkManager::Bookmark bookmark;
        bookmark.isAuto = json["isAuto"].toBool();
        bookmark.name = json["name"].toString();
        bookmark.pageIndex = json["pageIndex"].toInteger();
        return bookmark;
    }

    static QJsonObject convertBookmarksToJson(const PDFBookmarkManager::Bookmarks& bookmarks)
    {
        QJsonArray jsonArray;

        for (const auto& bookmark : bookmarks.bookmarks)
        {
            jsonArray.append(convertBookmarkToJson(bookmark));
        }

        QJsonObject jsonObject;
        jsonObject["bookmarks"] = jsonArray;
        return jsonObject;
    }

    static PDFBookmarkManager::Bookmarks convertBookmarksFromJson(const QJsonObject& object)
    {
        PDFBookmarkManager::Bookmarks bookmarks;

        QJsonArray jsonArray = object["bookmarks"].toArray();

        for (const auto& jsonValue : jsonArray)
        {
            bookmarks.bookmarks.push_back(convertJsonToBookmark(jsonValue.toObject()));
        }

        return bookmarks;
    }

};

PDFBookmarkManager::PDFBookmarkManager(QObject* parent) :
    BaseClass(parent)
{
}

void PDFBookmarkManager::setDocument(const pdf::PDFModifiedDocument& document)
{
    Q_EMIT bookmarksAboutToBeChanged();

    if (document.hasReset())
    {
        savePersistentBookmarks();
        m_document = document.getDocument();
        setProperty("familyPdfDocumentKey", getDocumentKey(m_document));
        m_currentBookmark = -1;

        if (!document.hasPreserveView())
        {
            if (!loadPersistentBookmarks())
            {
                m_bookmarks.bookmarks.clear();
                regenerateAutoBookmarks();
                savePersistentBookmarks();
            }
        }
    }
    else
    {
        m_document = document.getDocument();
    }

    Q_EMIT bookmarksChanged();
}

void PDFBookmarkManager::saveToFile(QString fileName)
{
    QJsonDocument doc(PDFBookmarkManagerHelper::convertBookmarksToJson(m_bookmarks));

    // Příklad zápisu do souboru
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson());
        file.close();
    }
}

bool PDFBookmarkManager::loadFromFile(QString fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly))
    {
        QJsonDocument loadedDoc = QJsonDocument::fromJson(file.readAll());
        file.close();

        Q_EMIT bookmarksAboutToBeChanged();
        m_bookmarks = PDFBookmarkManagerHelper::convertBookmarksFromJson(loadedDoc.object());
        savePersistentBookmarks();
        Q_EMIT bookmarksChanged();
        return true;
    }

    return false;
}

bool PDFBookmarkManager::isEmpty() const
{
    return m_bookmarks.bookmarks.empty();
}

int PDFBookmarkManager::getBookmarkCount() const
{
    return static_cast<int>(m_bookmarks.bookmarks.size());
}

PDFBookmarkManager::Bookmark PDFBookmarkManager::getBookmark(int index) const
{
    return m_bookmarks.bookmarks.at(index);
}

void PDFBookmarkManager::toggleBookmark(pdf::PDFInteger pageIndex)
{
    Q_EMIT bookmarksAboutToBeChanged();

    auto it = std::find_if(m_bookmarks.bookmarks.begin(), m_bookmarks.bookmarks.end(), [pageIndex](const auto& bookmark) { return bookmark.pageIndex == pageIndex; });
    if (it != m_bookmarks.bookmarks.cend())
    {
        Bookmark& bookmark = *it;
        if (bookmark.isAuto)
        {
            bookmark.isAuto = false;
        }
        else
        {
            m_bookmarks.bookmarks.erase(it);
        }
    }
    else
    {
        Bookmark bookmark;
        bookmark.isAuto = false;
        bookmark.name = tr("User bookmark for page %1").arg(pageIndex + 1);
        bookmark.pageIndex = pageIndex;
        m_bookmarks.bookmarks.push_back(bookmark);
        sortBookmarks();
    }

    savePersistentBookmarks();
    Q_EMIT bookmarksChanged();
}

void PDFBookmarkManager::setGenerateBookmarksAutomatically(bool generateBookmarksAutomatically)
{
    if (m_generateBookmarksAutomatically != generateBookmarksAutomatically)
    {
        Q_EMIT bookmarksAboutToBeChanged();
        m_generateBookmarksAutomatically = generateBookmarksAutomatically;
        regenerateAutoBookmarks();
        savePersistentBookmarks();
        Q_EMIT bookmarksChanged();
    }
}

bool PDFBookmarkManager::loadPersistentBookmarks()
{
    const QString documentKey = property("familyPdfDocumentKey").toString();
    if (documentKey.isEmpty())
    {
        return false;
    }

    QFile file(getPersistentBookmarksFileName());
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QJsonObject documents = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    const QJsonValue value = documents.value(documentKey);
    if (!value.isObject())
    {
        return false;
    }

    m_bookmarks = PDFBookmarkManagerHelper::convertBookmarksFromJson(value.toObject());
    return true;
}

void PDFBookmarkManager::savePersistentBookmarks() const
{
    const QString documentKey = property("familyPdfDocumentKey").toString();
    if (documentKey.isEmpty())
    {
        return;
    }

    const QString fileName = getPersistentBookmarksFileName();
    QDir directory;
    if (!directory.mkpath(QFileInfo(fileName).absolutePath()))
    {
        return;
    }

    QJsonObject documents;
    QFile existingFile(fileName);
    if (existingFile.open(QIODevice::ReadOnly))
    {
        documents = QJsonDocument::fromJson(existingFile.readAll()).object();
        existingFile.close();
    }

    documents[documentKey] = PDFBookmarkManagerHelper::convertBookmarksToJson(m_bookmarks);

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(QJsonDocument(documents).toJson(QJsonDocument::Compact));
        file.close();
    }
}

QString PDFBookmarkManager::getDocumentKey(const pdf::PDFDocument* document) const
{
    if (!document)
    {
        return QString();
    }

    QString documentPath = QDir::cleanPath(property("familyPdfDocumentPath").toString());
#ifdef Q_OS_WIN
    documentPath = documentPath.toLower();
#endif
    if (!documentPath.isEmpty())
    {
        return QStringLiteral("path:") +
               QString::fromLatin1(QCryptographicHash::hash(documentPath.toUtf8(), QCryptographicHash::Sha256).toHex());
    }

    // The source hash is populated by the document reader and does not require
    // traversing the trailer while the document-reset notification is active.
    const QByteArray sourceHash = document->getSourceDataHash();
    if (!sourceHash.isEmpty())
    {
        return QStringLiteral("hash:") + QString::fromLatin1(sourceHash.toHex());
    }

    return QString();
}

QString PDFBookmarkManager::getPersistentBookmarksFileName() const
{
    const QString configDirectory = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(configDirectory).filePath(QStringLiteral("FamilyPDF/bookmarks.json"));
}

void PDFBookmarkManager::goToNextBookmark()
{
    if (isEmpty())
    {
        return;
    }

    m_currentBookmark = (m_currentBookmark + 1) % getBookmarkCount();
    goToCurrentBookmark();
}

void PDFBookmarkManager::goToPreviousBookmark()
{
    if (isEmpty())
    {
        return;
    }

    if (m_currentBookmark <= 0)
    {
        m_currentBookmark = getBookmarkCount() - 1;
    }
    else
    {
        --m_currentBookmark;
    }

    goToCurrentBookmark();
}

void PDFBookmarkManager::goToCurrentBookmark()
{
    if (isEmpty())
    {
        return;
    }

    if (m_currentBookmark >= 0 && m_currentBookmark < getBookmarkCount())
    {
        Q_EMIT bookmarkActivated(m_currentBookmark, m_bookmarks.bookmarks.at(m_currentBookmark));
    }
}

void PDFBookmarkManager::goToBookmark(int index, bool force)
{
    if (m_currentBookmark != index || force)
    {
        m_currentBookmark = index;
        goToCurrentBookmark();
    }
}

void PDFBookmarkManager::sortBookmarks()
{
    auto predicate = [](const auto& l, const auto& r)
    {
        return l.pageIndex < r.pageIndex;
    };
    std::sort(m_bookmarks.bookmarks.begin(), m_bookmarks.bookmarks.end(), predicate);
}

void PDFBookmarkManager::regenerateAutoBookmarks()
{
    if (!m_document)
    {
        return;
    }

    // Create bookmarks for all main chapters
    Bookmarks& bookmarks = m_bookmarks;

    for (auto it = bookmarks.bookmarks.begin(); it != bookmarks.bookmarks.end();)
    {
        if (it->isAuto)
        {
            it = bookmarks.bookmarks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (!m_generateBookmarksAutomatically)
    {
        return;
    }

    if (auto outlineRoot = m_document->getCatalog()->getOutlineRootPtr())
    {
        size_t childCount = outlineRoot->getChildCount();
        for (size_t i = 0; i < childCount; ++i)
        {
            Bookmark bookmark;
            bookmark.isAuto = true;
            bookmark.pageIndex = pdf::PDFCatalog::INVALID_PAGE_INDEX;

            const pdf::PDFOutlineItem* child = outlineRoot->getChild(i);
            const pdf::PDFAction* action = child->getAction();

            if (action)
            {
                for (const pdf::PDFAction* currentAction : action->getActionList())
                {
                    if (currentAction->getType() != pdf::ActionType::GoTo)
                    {
                        continue;
                    }

                    const pdf::PDFActionGoTo* typedAction = dynamic_cast<const pdf::PDFActionGoTo*>(currentAction);
                    pdf::PDFDestination destination = typedAction->getDestination();
                    if (destination.getDestinationType() == pdf::DestinationType::Named)
                    {
                        if (const pdf::PDFDestination* targetDestination = m_document->getCatalog()->getNamedDestination(destination.getName()))
                        {
                            destination = *targetDestination;
                        }
                    }

                    if (destination.getDestinationType() != pdf::DestinationType::Invalid &&
                        destination.getPageReference() != pdf::PDFObjectReference())
                    {
                        const size_t pageIndex = m_document->getCatalog()->getPageIndexFromPageReference(destination.getPageReference());
                        if (pageIndex != pdf::PDFCatalog::INVALID_PAGE_INDEX)
                        {
                            bookmark.pageIndex = pageIndex;
                            bookmark.name = child->getTitle();
                        }
                    }
                }
            }

            if (bookmark.pageIndex != pdf::PDFCatalog::INVALID_PAGE_INDEX)
            {
                bookmarks.bookmarks.emplace_back(std::move(bookmark));
            }
        }
    }
}

}   // namespace pdf
