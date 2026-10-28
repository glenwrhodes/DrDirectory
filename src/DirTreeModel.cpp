#include "DirTreeModel.h"
#include "Format.h"
#include "Roles.h"
#include "WinShell.h"
#include <QApplication>
#include <QColor>
#include <QFileIconProvider>
#include <QFont>
#include <QStyle>
#include <algorithm>

DirTreeModel::DirTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    QFileIconProvider p;
    m_folderIcon = p.icon(QAbstractFileIconProvider::Folder);
    m_driveIcon = p.icon(QAbstractFileIconProvider::Drive);
    m_computerIcon = p.icon(QAbstractFileIconProvider::Computer);
    m_specialIcon = QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon);
    m_linkIcon = QApplication::style()->standardIcon(QStyle::SP_DirLinkIcon);
}

void DirTreeModel::setRoot(FileNode* root)
{
    beginResetModel();
    m_root = root;
    endResetModel();
}

FileNode* DirTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<FileNode*>(index.internalPointer()) : nullptr;
}

QModelIndex DirTreeModel::indexForNode(const FileNode* node, int column) const
{
    if (!node || !m_root)
        return {};
    if (node == m_root)
        return createIndex(0, column, const_cast<FileNode*>(node));
    return createIndex(node->indexInParent, column, const_cast<FileNode*>(node));
}

QModelIndex DirTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= ColCount)
        return {};
    if (!parent.isValid())
        return (m_root && row == 0) ? createIndex(0, column, m_root) : QModelIndex();
    FileNode* p = nodeFromIndex(parent);
    if (!p || row >= int(p->children.size()))
        return {};
    return createIndex(row, column, p->children[size_t(row)].get());
}

QModelIndex DirTreeModel::parent(const QModelIndex& index) const
{
    FileNode* n = nodeFromIndex(index);
    if (!n || n == m_root || !n->parent)
        return {};
    return indexForNode(n->parent, 0);
}

int DirTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    if (!parent.isValid())
        return m_root ? 1 : 0;
    FileNode* n = nodeFromIndex(parent);
    return n ? int(n->children.size()) : 0;
}

int DirTreeModel::columnCount(const QModelIndex&) const
{
    return ColCount;
}

QIcon DirTreeModel::iconFor(const FileNode* n) const
{
    switch (n->kind) {
    case FileNode::Root: return m_computerIcon;
    case FileNode::Drive: return m_driveIcon;
    case FileNode::Dir: return n->reparsePoint ? m_linkIcon : m_folderIcon;
    case FileNode::File: return WinShell::iconForExtension(n->extension());
    default: return m_specialIcon;
    }
}

QVariant DirTreeModel::data(const QModelIndex& index, int role) const
{
    const FileNode* n = nodeFromIndex(index);
    if (!n)
        return {};
    const int col = index.column();
    const double fraction = (n->parent && n->parent->size > 0) ? double(n->size) / double(n->parent->size) : 1.0;

    switch (role) {
    case Qt::DisplayRole:
        switch (col) {
        case ColName: return n->name;
        case ColPercent: return Fmt::percent(fraction);
        case ColSize: return Fmt::size(n->size);
        case ColItems: return n->isContainer() ? Fmt::count(n->fileCount + n->dirCount) : QString();
        case ColFiles: return n->isContainer() ? Fmt::count(n->fileCount) : QString();
        case ColFolders: return n->isContainer() ? Fmt::count(n->dirCount) : QString();
        case ColModified: return Fmt::dateTime(n->lastWrite);
        case ColAttributes: return WinShell::attributesString(n->attributes);
        }
        break;
    case Qt::DecorationRole:
        if (col == ColName)
            return iconFor(n);
        break;
    case Qt::TextAlignmentRole:
        if (col >= ColSize && col <= ColFolders)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Qt::ForegroundRole:
        if (n->isSpecial() || n->reparsePoint)
            return QColor(128, 128, 128);
        if (n->accessDenied)
            return QColor(214, 64, 64);
        if (col == ColName && (n->attributes & 0x800 /*COMPRESSED*/))
            return QColor(40, 110, 230);
        break;
    case Qt::FontRole:
        if (n->isSpecial() || n->reparsePoint) {
            QFont f;
            f.setItalic(true);
            return f;
        }
        break;
    case Qt::ToolTipRole: {
        QString t = n->hasRealPath() ? n->fullPath() : n->name;
        if (n->accessDenied)
            t += tr("\nAccess denied: contents could not be read (try running as Administrator).");
        if (n->reparsePoint)
            t += tr("\nJunction / symbolic link: not followed.");
        if (n->kind == FileNode::Unknown)
            t += tr("\nUsed space not found by the scan: system files, other users' folders or areas without permission.");
        return t;
    }
    case Roles::Fraction: return fraction;
    case Roles::Depth: return n->depth();
    case Roles::Node: return QVariant::fromValue(static_cast<void*>(const_cast<FileNode*>(n)));
    }
    return {};
}

QVariant DirTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::DisplayRole) {
        switch (section) {
        case ColName: return tr("Name");
        case ColPercent: return tr("% of Parent");
        case ColSize: return tr("Size");
        case ColItems: return tr("Items");
        case ColFiles: return tr("Files");
        case ColFolders: return tr("Folders");
        case ColModified: return tr("Last Modified");
        case ColAttributes: return tr("Attributes");
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (section >= ColSize && section <= ColFolders)
            return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    return {};
}

Qt::ItemFlags DirTreeModel::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

bool DirTreeModel::lessThan(const FileNode* a, const FileNode* b) const
{
    auto cmp = [this](const FileNode* x, const FileNode* y) -> int {
        switch (m_sortColumn) {
        case ColName: return x->name.compare(y->name, Qt::CaseInsensitive);
        case ColItems: {
            const qint64 xi = x->fileCount + x->dirCount, yi = y->fileCount + y->dirCount;
            return xi < yi ? -1 : (xi > yi ? 1 : 0);
        }
        case ColFiles: return x->fileCount < y->fileCount ? -1 : (x->fileCount > y->fileCount ? 1 : 0);
        case ColFolders: return x->dirCount < y->dirCount ? -1 : (x->dirCount > y->dirCount ? 1 : 0);
        case ColModified: return x->lastWrite < y->lastWrite ? -1 : (x->lastWrite > y->lastWrite ? 1 : 0);
        case ColAttributes:
            return WinShell::attributesString(x->attributes).compare(WinShell::attributesString(y->attributes));
        default: return x->size < y->size ? -1 : (x->size > y->size ? 1 : 0);
        }
    };
    int c = cmp(a, b);
    if (c == 0)
        c = a->name.compare(b->name, Qt::CaseInsensitive);
    return m_sortOrder == Qt::AscendingOrder ? c < 0 : c > 0;
}

void DirTreeModel::sortChildren(FileNode* node, bool recursive)
{
    auto sortOne = [this](FileNode* n) {
        std::stable_sort(n->children.begin(), n->children.end(),
                         [this](const std::unique_ptr<FileNode>& a, const std::unique_ptr<FileNode>& b) {
                             return lessThan(a.get(), b.get());
                         });
        n->renumberChildren();
    };
    if (!recursive) {
        sortOne(node);
        return;
    }
    forEachNode(node, [&](FileNode* n) {
        if (n->children.size() > 1)
            sortOne(n);
    });
}

void DirTreeModel::sort(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    if (!m_root)
        return;
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    const QModelIndexList oldList = persistentIndexList();
    QVector<QPair<FileNode*, int>> saved;
    saved.reserve(oldList.size());
    for (const QModelIndex& i : oldList)
        saved.append({nodeFromIndex(i), i.column()});
    sortChildren(m_root, true);
    QModelIndexList newList;
    newList.reserve(saved.size());
    for (const auto& s : saved)
        newList.append(indexForNode(s.first, s.second));
    changePersistentIndexList(oldList, newList);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void DirTreeModel::adjustAncestors(FileNode* from, qint64 dSize, qint64 dFiles, qint64 dDirs)
{
    for (FileNode* x = from; x; x = x->parent) {
        x->size += dSize;
        x->fileCount += dFiles;
        x->dirCount += dDirs;
        if (!x->children.empty()) {
            // sibling percentages change too
            const QModelIndex first = indexForNode(x->children.front().get(), ColPercent);
            const QModelIndex last = indexForNode(x->children.back().get(), ColPercent);
            emit dataChanged(first, last, {Qt::DisplayRole, Roles::Fraction});
        }
        emit dataChanged(indexForNode(x, ColName), indexForNode(x, ColAttributes));
    }
}

void DirTreeModel::removeNode(FileNode* node)
{
    FileNode* p = node ? node->parent : nullptr;
    if (!p)
        return;
    const int row = node->indexInParent;
    const qint64 dSize = node->size;
    const qint64 dFiles = node->isFile() ? 1 : (node->isContainer() ? node->fileCount : 0);
    const qint64 dDirs = node->isContainer() ? node->dirCount + 1 : 0;

    beginRemoveRows(indexForNode(p), row, row);
    p->children.erase(p->children.begin() + row);
    p->renumberChildren(row);
    endRemoveRows();
    adjustAncestors(p, -dSize, -dFiles, -dDirs);
}

void DirTreeModel::insertNode(FileNode* parent, std::unique_ptr<FileNode> child)
{
    if (!parent || !child)
        return;
    const qint64 dSize = child->size;
    const qint64 dFiles = child->isFile() ? 1 : (child->isContainer() ? child->fileCount : 0);
    const qint64 dDirs = child->isContainer() ? child->dirCount + 1 : 0;

    auto pos = std::upper_bound(parent->children.begin(), parent->children.end(), child.get(),
                                [this](const FileNode* v, const std::unique_ptr<FileNode>& e) {
                                    return lessThan(v, e.get());
                                });
    const int row = int(pos - parent->children.begin());
    child->parent = parent;
    beginInsertRows(indexForNode(parent), row, row);
    parent->children.insert(pos, std::move(child));
    parent->renumberChildren(row);
    endInsertRows();
    adjustAncestors(parent, dSize, dFiles, dDirs);
}

void DirTreeModel::replaceChildren(FileNode* target, std::unique_ptr<FileNode> fresh)
{
    if (!target || !fresh)
        return;
    const QModelIndex tidx = indexForNode(target);
    const qint64 oldSize = target->size, oldFiles = target->fileCount, oldDirs = target->dirCount;

    if (!target->children.empty()) {
        beginRemoveRows(tidx, 0, int(target->children.size()) - 1);
        target->children.clear();
        endRemoveRows();
    }
    sortChildren(fresh.get(), true);
    const int m = int(fresh->children.size());
    if (m > 0) {
        beginInsertRows(tidx, 0, m - 1);
        target->children = std::move(fresh->children);
        for (auto& c : target->children)
            c->parent = target;
        target->renumberChildren();
        endInsertRows();
    }
    target->size = fresh->size;
    target->fileCount = fresh->fileCount;
    target->dirCount = fresh->dirCount;
    target->lastWrite = fresh->lastWrite;
    target->accessDenied = fresh->accessDenied;
    if (fresh->attributes)
        target->attributes = fresh->attributes;
    emit dataChanged(indexForNode(target, ColName), indexForNode(target, ColAttributes));
    adjustAncestors(target->parent, target->size - oldSize, target->fileCount - oldFiles, target->dirCount - oldDirs);
}
