#include "ExtensionModel.h"
#include "Format.h"
#include "Roles.h"
#include "WinShell.h"
#include <QPainter>

ExtensionModel::ExtensionModel(ExtensionTable* table, QObject* parent)
    : QAbstractTableModel(parent)
    , m_table(table)
{
}

void ExtensionModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    m_total = 0;
    const auto& list = m_table->list();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].files > 0) {
            m_rows.push_back(i);
            m_total += list[i].bytes;
        }
    }
    endResetModel();
}

int ExtensionModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ExtensionModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QPixmap ExtensionModel::swatch(const QColor& c) const
{
    auto it = m_swatches.constFind(c.rgb());
    if (it != m_swatches.cend())
        return it.value();
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QLinearGradient g(0, 0, 14, 14);
    g.setColorAt(0, c.lighter(130));
    g.setColorAt(1, c.darker(115));
    p.setBrush(g);
    p.setPen(c.darker(160));
    p.drawRect(0, 0, 13, 13);
    p.end();
    m_swatches.insert(c.rgb(), pm);
    return pm;
}

QVariant ExtensionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const int id = m_rows[index.row()];
    const ExtInfo& e = m_table->list()[id];
    const double frac = m_total > 0 ? double(e.bytes) / double(m_total) : 0.0;
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColExt: return e.ext.isEmpty() ? tr("(no extension)") : QStringLiteral(".") + e.ext;
        case ColDesc: return WinShell::typeNameForExtension(e.ext);
        case ColBytes: return Fmt::size(e.bytes);
        case ColPercent: return Fmt::percent(frac);
        case ColFiles: return Fmt::count(e.files);
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == ColExt)
            return swatch(e.color);
        if (index.column() == ColDesc)
            return WinShell::iconForExtension(e.ext);
        break;
    case Qt::TextAlignmentRole:
        if (index.column() == ColBytes || index.column() == ColFiles)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Roles::Sort:
        switch (index.column()) {
        case ColExt: return e.ext;
        case ColDesc: return WinShell::typeNameForExtension(e.ext);
        case ColBytes:
        case ColPercent: return e.bytes;
        case ColFiles: return e.files;
        }
        break;
    case Roles::Fraction: return frac;
    case Roles::Depth: return 0;
    case Roles::ExtId: return id;
    }
    return {};
}

QVariant ExtensionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColExt: return tr("Extension");
    case ColDesc: return tr("Description");
    case ColBytes: return tr("Size");
    case ColPercent: return tr("% of Total");
    case ColFiles: return tr("Files");
    }
    return {};
}
