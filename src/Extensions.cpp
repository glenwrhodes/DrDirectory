#include "Extensions.h"
#include <QHash>
#include <algorithm>

QColor ExtensionTable::paletteColor(int rank, const QString& ext)
{
    static const QColor palette[] = {
        QColor(46, 140, 255),  QColor(20, 196, 186),  QColor(96, 110, 240),  QColor(56, 200, 240),
        QColor(116, 136, 166), QColor(28, 120, 200),  QColor(96, 226, 206),  QColor(140, 128, 236),
        QColor(30, 160, 150),  QColor(150, 170, 196), QColor(66, 92, 196),   QColor(44, 176, 220),
        QColor(84, 104, 132),  QColor(120, 196, 236), QColor(24, 104, 128),  QColor(176, 190, 210),
    };
    constexpr int n = int(sizeof(palette) / sizeof(palette[0]));
    if (rank >= 0 && rank < n)
        return palette[rank];
    const int hue = int(qHash(ext) % 360u);
    return QColor::fromHsv(185 + hue % 60, 45 + int(qHash(ext + QLatin1Char('#')) % 50u), 120 + hue % 70);
}

QColor ExtensionTable::color(int extId) const
{
    if (extId >= 0 && extId < m_list.size())
        return m_list[extId].color;
    return QColor(90, 106, 126);
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
