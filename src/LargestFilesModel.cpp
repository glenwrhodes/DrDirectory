#include "LargestFilesModel.h"
#include "Format.h"
#include "Roles.h"
#include "WinShell.h"
#include <algorithm>
#include <queue>

void LargestFilesModel::rebuild(FileNode* root, int limit)
{
    beginResetModel();
    m_files.clear();
    if (root) {
        auto cmp = [](const FileNode* a, const FileNode* b) { return a->size > b->size; };
        std::priority_queue<FileNode*, std::vector<FileNode*>, decltype(cmp)> heap(cmp);
        forEachNode(root, [&](FileNode* n) {
            if (n->kind != FileNode::File)
                return;
            if (int(heap.size()) < limit)
                heap.push(n);
            else if (n->size > heap.top()->size) {
                heap.pop();
                heap.push(n);
            }
        });
        m_files.reserve(heap.size());
        while (!heap.empty()) {
            m_files.push_back(heap.top());
            heap.pop();
        }
        std::reverse(m_files.begin(), m_files.end());
    }
    endResetModel();
}

void LargestFilesModel::clear()
{
    beginResetModel();
    m_files.clear();
    endResetModel();
}

FileNode* LargestFilesModel::nodeAt(int row) const
{
    return (row >= 0 && row < int(m_files.size())) ? m_files[size_t(row)] : nullptr;
}

int LargestFilesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_files.size());
}

int LargestFilesModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant LargestFilesModel::data(const QModelIndex& index, int role) const
{
    FileNode* n = nodeAt(index.row());
    if (!n)
        return {};
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColName: return n->name;
        case ColSize: return Fmt::size(n->size);
        case ColFolder: return n->parent ? n->parent->fullPath() : QString();
        case ColModified: return Fmt::dateTime(n->lastWrite);
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == ColName)
            return WinShell::iconForExtension(n->extension());
        break;
    case Qt::TextAlignmentRole:
        if (index.column() == ColSize)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Qt::ToolTipRole: return n->fullPath();
    case Roles::Sort:
        switch (index.column()) {
        case ColName: return n->name.toLower();
        case ColSize: return n->size;
        case ColFolder: return n->parent ? n->parent->fullPath().toLower() : QString();
        case ColModified: return n->lastWrite;
        }
        break;
    case Roles::Node: return QVariant::fromValue(static_cast<void*>(n));
    }
    return {};
}

QVariant LargestFilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColName: return tr("Name");
    case ColSize: return tr("Size");
    case ColFolder: return tr("Folder");
    case ColModified: return tr("Last Modified");
    }
    return {};
}
