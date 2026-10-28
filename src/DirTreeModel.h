#pragma once
#include "FileNode.h"
#include <QAbstractItemModel>
#include <QIcon>
#include <memory>

// Tree model over a FileNode hierarchy. The model does not own the tree.
class DirTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Column { ColName, ColPercent, ColSize, ColItems, ColFiles, ColFolders, ColModified, ColAttributes, ColCount };

    explicit DirTreeModel(QObject* parent = nullptr);

    void setRoot(FileNode* root);
    FileNode* root() const { return m_root; }
    FileNode* nodeFromIndex(const QModelIndex& index) const;
    QModelIndex indexForNode(const FileNode* node, int column = 0) const;

    // Mutations that keep views and aggregate sizes consistent.
    void removeNode(FileNode* node);                                      // destroys node
    void insertNode(FileNode* parent, std::unique_ptr<FileNode> child);   // inserted in sort order
    void replaceChildren(FileNode* target, std::unique_ptr<FileNode> fresh);

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void sort(int column, Qt::SortOrder order) override;

private:
    bool lessThan(const FileNode* a, const FileNode* b) const;
    void sortChildren(FileNode* node, bool recursive);
    void adjustAncestors(FileNode* from, qint64 dSize, qint64 dFiles, qint64 dDirs);
    QIcon iconFor(const FileNode* n) const;

    FileNode* m_root = nullptr;
    int m_sortColumn = ColSize;
    Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
    QIcon m_folderIcon, m_driveIcon, m_computerIcon, m_specialIcon, m_linkIcon;
};
