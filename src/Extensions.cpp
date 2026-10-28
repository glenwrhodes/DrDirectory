#include "Extensions.h"
#include <QHash>
#include <algorithm>

QColor ExtensionTable::paletteColor(int rank, const QString& ext)
{
    static const QColor palette[] = {
        QColor(60, 125, 255),  QColor(255, 69, 69),   QColor(76, 209, 76),   QColor(255, 210, 46),
        QColor(46, 216, 230),  QColor(224, 76, 224),  QColor(255, 140, 26),  QColor(142, 107, 255),
        QColor(154, 217, 61),  QColor(255, 111, 168), QColor(26, 166, 143),  QColor(200, 155, 90),
        QColor(108, 156, 217), QColor(217, 108, 75),  QColor(176, 176, 48),  QColor(122, 196, 165),
    };
    constexpr int n = int(sizeof(palette) / sizeof(palette[0]));
    if (rank >= 0 && rank < n)
        return palette[rank];
    const int hue = int(qHash(ext) % 360u);
    return QColor::fromHsv(hue, 80, 185);
}

QColor ExtensionTable::color(int extId) const
{
    if (extId >= 0 && extId < m_list.size())
        return m_list[extId].color;
    return QColor(160, 160, 160);
}

qint64 ExtensionTable::totalBytes() const
{
    qint64 t = 0;
    for (const ExtInfo& e : m_list)
        t += e.bytes;
    return t;
}

void ExtensionTable::rebuild(FileNode* root)
{
    m_list.clear();
    if (!root)
        return;
    QHash<QString, int> ids;
    QVector<ExtInfo> tmp;
    forEachNode(root, [&](FileNode* n) {
        if (n->kind != FileNode::File)
            return;
        const QString ext = n->extension();
        auto it = ids.constFind(ext);
        int id;
        if (it == ids.cend()) {
            id = tmp.size();
            ids.insert(ext, id);
            ExtInfo e;
            e.ext = ext;
            tmp.push_back(e);
        } else {
            id = it.value();
        }
        tmp[id].bytes += n->size;
        tmp[id].files += 1;
        n->extId = id;
    });

    QVector<int> order(tmp.size());
    for (int i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (tmp[a].bytes != tmp[b].bytes)
            return tmp[a].bytes > tmp[b].bytes;
        return tmp[a].ext < tmp[b].ext;
    });
    QVector<int> remap(tmp.size());
    m_list.reserve(tmp.size());
    for (int rank = 0; rank < order.size(); ++rank) {
        ExtInfo e = tmp[order[rank]];
        e.color = paletteColor(rank, e.ext);
        remap[order[rank]] = rank;
        m_list.push_back(e);
    }
    forEachNode(root, [&](FileNode* n) {
        if (n->kind == FileNode::File && n->extId >= 0)
            n->extId = remap[n->extId];
    });
}

void ExtensionTable::subtract(FileNode* subtree)
{
    forEachNode(subtree, [&](FileNode* n) {
        if (n->kind == FileNode::File && n->extId >= 0 && n->extId < m_list.size()) {
            m_list[n->extId].bytes -= n->size;
            m_list[n->extId].files -= 1;
        }
    });
}
