#pragma once
#include <QStyledItemDelegate>

// Paints a WinDirStat-style percentage bar using Roles::Fraction and Roles::Depth.
class PercentBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    static QColor depthColor(int depth);
};
