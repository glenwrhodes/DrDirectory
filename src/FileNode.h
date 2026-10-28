#pragma once
#include <QString>
#include <QtGlobal>
#include <memory>
#include <vector>

// One entry in the scanned tree. Directories own their children.
struct FileNode
{
    enum Kind : quint8 { File, Dir, Drive, Root, FreeSpace, Unknown };

    QString name;
    FileNode* parent = nullptr;
    std::vector<std::unique_ptr<FileNode>> children;

    qint64 size = 0;       // bytes; recursive for containers
    qint64 fileCount = 0;  // recursive file count (containers)
    qint64 dirCount = 0;   // recursive sub-folder count (containers)
    qint64 lastWrite = 0;  // msecs since epoch; newest in subtree for containers
    quint32 attributes = 0;
    int indexInParent = 0;
    int extId = -1;        // index into ExtensionTable (files only)
    Kind kind = File;
    bool accessDenied = false;
    bool reparsePoint = false; // junction / symlink folder (not followed)

    bool isFile() const { return kind == File; }
    bool isContainer() const { return kind == Dir || kind == Drive || kind == Root; }
    bool isSpecial() const { return kind == FreeSpace || kind == Unknown; }
    bool isDeletable() const;
    bool hasRealPath() const { return kind == File || kind == Dir || kind == Drive; }

    QString fullPath() const;
    QString extension() const; // lower-case, no dot; empty if none
    int depth() const;
    bool isAncestorOf(const FileNode* other) const; // true for other == this too
    void renumberChildren(int from = 0);
};

// Recomputes size / counts / newest-date bottom-up and sorts children by size (desc).
void finalizeTree(FileNode* root);

// Non-recursive pre-order traversal.
template <typename F>
void forEachNode(FileNode* root, F&& fn)
{
    if (!root)
        return;
    std::vector<FileNode*> stack{root};
    while (!stack.empty()) {
        FileNode* n = stack.back();
        stack.pop_back();
        fn(n);
        for (auto& c : n->children)
            stack.push_back(c.get());
    }
}
