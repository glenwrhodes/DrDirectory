#include "PercentBarDelegate.h"
#include "Format.h"
#include "Roles.h"
#include <QApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

QColor PercentBarDelegate::depthColor(int depth)
{
    static const int hues[] = {192, 212, 176, 226, 200, 184, 218, 170};
    return QColor::fromHsv(hues[depth % 8], 200, 220);
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
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(opt.rect).adjusted(4, 4.5, -4, -4.5);
    painter->setPen(QPen(QColor(34, 48, 62), 1));
    painter->setBrush(QColor(9, 15, 22));
    painter->drawRoundedRect(r, 3, 3);

    QRectF fr = r.adjusted(1.5, 1.5, -1.5, -1.5);
    fr.setWidth(fr.width() * frac);
    if (fr.width() > 0.6) {
        const QColor c = depthColor(depth);
        QLinearGradient g(fr.topLeft(), fr.topRight());
        g.setColorAt(0.0, c.darker(170));
        g.setColorAt(1.0, c);
        painter->setPen(Qt::NoPen);
        painter->setBrush(g);
        painter->drawRoundedRect(fr, 2, 2);
        QLinearGradient sheen(fr.topLeft(), fr.bottomLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 46));
        sheen.setColorAt(0.5, QColor(255, 255, 255, 0));
        painter->setBrush(sheen);
        painter->drawRoundedRect(fr, 2, 2);
        if (frac < 0.995 && fr.width() > 2) {
            painter->setPen(QPen(c.lighter(160), 1.2));
            painter->drawLine(QPointF(fr.right(), fr.top() + 0.5), QPointF(fr.right(), fr.bottom() - 0.5));
        }
    }
    QFont f = painter->font();
    f.setPointSizeF(f.pointSizeF() * 0.92);
    painter->setFont(f);
    const QString txt = Fmt::percent(frac);
    painter->setPen(QColor(0, 0, 0, 170));
    painter->drawText(r.translated(0, 1), Qt::AlignCenter, txt);
    painter->setPen(QColor(230, 242, 251));
    painter->drawText(r, Qt::AlignCenter, txt);
    painter->restore();
}

QSize PercentBarDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(qMax(s.width(), 90));
    return s;
}
