#pragma once
#include "FileNode.h"
#include <QColor>
#include <QString>
#include <QVector>

struct ExtInfo
{
    QString ext;  // lower-case, no dot ("" = no extension)
    qint64 bytes = 0;
    qint64 files = 0;
    QColor color;
};

// Per-extension statistics; ExtInfo index == FileNode::extId.
class ExtensionTable
{
public:
    void clear() { m_list.clear(); }
    void rebuild(FileNode* root);       // recomputes everything and assigns colours by rank
    void subtract(FileNode* subtree);   // keeps colours stable after a deletion
    const QVector<ExtInfo>& list() const { return m_list; }
    QColor color(int extId) const;
    qint64 totalBytes() const;
    static QColor paletteColor(int rank, const QString& ext);

private:
    QVector<ExtInfo> m_list;
};
