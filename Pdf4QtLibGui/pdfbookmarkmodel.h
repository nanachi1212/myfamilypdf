// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#ifndef PDFBOOKMARKMODEL_H
#define PDFBOOKMARKMODEL_H

#include "pdfbookmarkmanager.h"

#include <QAbstractItemModel>

namespace pdfviewer
{

class PDF4QTLIBGUILIBSHARED_EXPORT PDFBookmarkItemModel : public QAbstractItemModel
{
private:
    using BaseClass = QAbstractItemModel;

public:
    PDFBookmarkItemModel(PDFBookmarkManager* bookmarkManager, QObject* parent);

    virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    bool isFolder(const QModelIndex& index) const;
    int getFolderIndex(const QModelIndex& index) const;
    int getBookmarkIndex(const QModelIndex& index) const;
    QModelIndex getBookmarkModelIndex(int bookmarkIndex) const;

private:
    static constexpr quintptr FOLDER_NODE = 1;
    static constexpr quintptr BOOKMARK_NODE = 2;

    static quintptr getNodeId(quintptr type, int sourceIndex);
    static int getNodeSourceIndex(const QModelIndex& index, quintptr type);
    int getRootBookmarkIndex(int rootBookmarkRow) const;
    int getFolderBookmarkIndex(int folderIndex, int childRow) const;
    int getRootRowForBookmark(int bookmarkIndex) const;

    PDFBookmarkManager* m_bookmarkManager = nullptr;
};

} // namespace pdfviewer

#endif // PDFBOOKMARKMODEL_H
