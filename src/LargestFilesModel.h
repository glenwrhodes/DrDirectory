#pragma once
#include "FileNode.h"
#include <QAbstractTableModel>
#include <vector>

class LargestFilesModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColName, ColSize, ColFolder, ColModified, ColCount };
    using QAbstractTableModel::QAbstractTableModel;

    void rebuild(FileNode* root, int limit = 2000);
    void clear();
    FileNode* nodeAt(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<FileNode*> m_files;
};
