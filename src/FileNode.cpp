#include "FileNode.h"
#include <algorithm>

bool FileNode::isDeletable() const
{
    return (kind == File || kind == Dir) && parent && parent->kind != Root;
}

QString FileNode::fullPath() const
{
    if (!hasRealPath())
        return QString();
    if (!parent || parent->kind == Root)
        return name;
    QString p = parent->fullPath();
    if (!p.endsWith(QLatin1Char('\\')) && !p.endsWith(QLatin1Char('/')))
        p += QLatin1Char('\\');
    return p + name;
}

QString FileNode::extension() const
{
    if (kind != File)
        return QString();
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot == name.size() - 1)
        return QString();
    return name.mid(dot + 1).toLower();
}

int FileNode::depth() const
{
    int d = 0;
    for (const FileNode* p = parent; p; p = p->parent)
        ++d;
    return d;
}

bool FileNode::isAncestorOf(const FileNode* other) const
{
    for (const FileNode* n = other; n; n = n->parent)
        if (n == this)
            return true;
    return false;
}

void FileNode::renumberChildren(int from)
{
    for (int i = from; i < int(children.size()); ++i)
        children[size_t(i)]->indexInParent = i;
}

static bool bySizeDesc(const std::unique_ptr<FileNode>& a, const std::unique_ptr<FileNode>& b)
{
    if (a->size != b->size)
        return a->size > b->size;
    return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
}

void finalizeTree(FileNode* root)
{
    if (!root)
        return;
    std::vector<std::pair<FileNode*, bool>> stack;
    stack.emplace_back(root, false);
    while (!stack.empty()) {
        auto [n, visited] = stack.back();
        stack.pop_back();
        if (!n->isContainer())
            continue;
        if (!visited) {
            stack.emplace_back(n, true);
            for (auto& c : n->children)
                if (c->isContainer())
                    stack.emplace_back(c.get(), false);
            continue;
        }
        qint64 size = 0, files = 0, dirs = 0, lw = n->lastWrite;
        for (auto& c : n->children) {
            size += c->size;
            if (c->kind == FileNode::File) {
                ++files;
            } else if (c->isContainer()) {
                files += c->fileCount;
                dirs += c->dirCount + 1;
            }
            lw = std::max(lw, c->lastWrite);
        }
        n->size = size;
        n->fileCount = files;
        n->dirCount = dirs;
        n->lastWrite = lw;
        std::sort(n->children.begin(), n->children.end(), bySizeDesc);
        for (auto& c : n->children)
            c->parent = n;
        n->renumberChildren();
    }
}
