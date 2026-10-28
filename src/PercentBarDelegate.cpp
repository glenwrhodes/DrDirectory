#include "PercentBarDelegate.h"
#include "Format.h"
#include "Roles.h"
#include <QApplication>
#include <QLinearGradient>
#include <QPainter>

QColor PercentBarDelegate::depthColor(int depth)
{
    static const int hues[] = {215, 280, 170, 30, 120, 340, 195, 55};
    return QColor::fromHsv(hues[depth % 8], 150, 225);
}

void PercentBarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    opt.text.clear();
    opt.icon = QIcon();
    const QWidget* w = opt.widget;
    QStyle* style = w ? w->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, w);

    const QVariant fv = index.data(Roles::Fraction);
    if (!fv.isValid())
        return;
    const double frac = qBound(0.0, fv.toDouble(), 1.0);
    const int depth = index.data(Roles::Depth).toInt();

    painter->save();
    const QRect r = opt.rect.adjusted(3, 3, -3, -3);
    painter->setPen(opt.palette.color(QPalette::Mid));
    painter->setBrush(opt.palette.color(QPalette::Base));
    painter->drawRect(r.adjusted(0, 0, -1, -1));

    QRect fr = r.adjusted(1, 1, -1, -1);
    fr.setWidth(int(fr.width() * frac + 0.5));
    if (fr.width() > 0) {
        const QColor c = depthColor(depth);
        QLinearGradient g(fr.topLeft(), fr.bottomLeft());
        g.setColorAt(0.0, c.lighter(125));
        g.setColorAt(1.0, c.darker(110));
        painter->fillRect(fr, g);
    }
    // Text: palette colour over the empty part, dark over the (light) bar so it stays readable when selected.
    const QString txt = Fmt::percent(frac);
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(r, Qt::AlignCenter, txt);
    if (fr.width() > 0) {
        painter->setClipRect(fr);
        painter->setPen(QColor(15, 15, 15));
        painter->drawText(r, Qt::AlignCenter, txt);
    }
    painter->restore();
}

QSize PercentBarDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(qMax(s.width(), 90));
    return s;
}
