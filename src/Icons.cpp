#include "Icons.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace Icons {
namespace {

void drawGlyph(QPainter& p, Kind k)
{
    // All glyphs live in a 24x24 design grid.
    auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    auto poly = [&](std::initializer_list<QPointF> pts) {
        QPainterPath path;
        bool first = true;
        for (const QPointF& pt : pts) {
            if (first) path.moveTo(pt); else path.lineTo(pt);
            first = false;
        }
        p.drawPath(path);
    };
    auto trash = [&] {
        line(4.5, 7, 19.5, 7);
        poly({{9.5, 7}, {9.5, 4.5}, {14.5, 4.5}, {14.5, 7}});
        poly({{6.2, 7}, {7.2, 20}, {16.8, 20}, {17.8, 7}});
    };

    switch (k) {
    case Scan: {
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        p.drawEllipse(QPointF(12, 12), 4.2, 4.2);
        line(12, 12, 18.0, 6.0);
        p.save();
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(15.6, 14.6), 1.3, 1.3);
        p.restore();
        break;
    }
    case Rescan:
    case Refresh: {
        QPainterPath arc;
        const QRectF r(4.5, 4.5, 15, 15);
        arc.arcMoveTo(r, 60);
        arc.arcTo(r, 60, 280);
        p.drawPath(arc);
        poly({{13.2, 3.4}, {16.3, 5.8}, {13.0, 8.6}});
        if (k == Rescan) {
            p.save();
            p.setBrush(p.pen().color());
            p.drawEllipse(QPointF(12, 12), 1.4, 1.4);
            p.restore();
        }
        break;
    }
    case Stop: p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 2.2, 2.2); break;
    case ZoomIn:
    case ZoomOut:
        p.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
        line(15.2, 15.2, 20, 20);
        line(7.8, 10.5, 13.2, 10.5);
        if (k == ZoomIn)
            line(10.5, 7.8, 10.5, 13.2);
        break;
    case ZoomReset:
        poly({{4, 9}, {4, 4}, {9, 4}});
        poly({{15, 4}, {20, 4}, {20, 9}});
        poly({{20, 15}, {20, 20}, {15, 20}});
        poly({{9, 20}, {4, 20}, {4, 15}});
        p.drawRoundedRect(QRectF(9, 9, 6, 6), 1, 1);
        break;
    case Folder: {
        QPainterPath path;
        path.moveTo(3.5, 18.5);
        path.lineTo(3.5, 6.5);
        path.lineTo(9.5, 6.5);
        path.lineTo(11.5, 8.5);
        path.lineTo(20.5, 8.5);
        path.lineTo(20.5, 18.5);
        path.closeSubpath();
        p.drawPath(path);
        line(3.5, 11, 20.5, 11);
        break;
    }
    case Recycle:
        trash();
        line(10.2, 10.5, 10.5, 16.8);
        line(13.8, 10.5, 13.5, 16.8);
        break;
    case Delete:
        trash();
        line(9.6, 11, 14.4, 16);
        line(14.4, 11, 9.6, 16);
        break;
    case Shield: {
        QPainterPath path;
        path.moveTo(12, 3);
        path.lineTo(19, 6);
        path.lineTo(19, 11.5);
        path.quadTo(19, 17.5, 12, 21);
        path.quadTo(5, 17.5, 5, 11.5);
        path.lineTo(5, 6);
        path.closeSubpath();
        p.drawPath(path);
        poly({{9, 12}, {11.2, 14.2}, {15.3, 9.8}});
        break;
    }
    case Export:
        poly({{5, 12}, {5, 19.5}, {19, 19.5}, {19, 12}});
        line(12, 15, 12, 4);
        poly({{8, 8}, {12, 4}, {16, 8}});
        break;
    case Copy:
        p.drawRoundedRect(QRectF(8.5, 8.5, 11, 12), 2, 2);
        poly({{15.5, 5.5}, {15.5, 4.5}, {4.5, 4.5}, {4.5, 16.5}, {5.5, 16.5}});
        break;
    case Info:
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        line(12, 11, 12, 16.5);
        p.save();
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(12, 7.8), 0.9, 0.9);
        p.restore();
        break;
    case Terminal:
        p.drawRoundedRect(QRectF(3, 5, 18, 14), 2.5, 2.5);
        poly({{7, 10}, {10, 12.5}, {7, 15}});
        line(12, 15, 16.5, 15);
        break;
    case Open:
        poly({{11, 5}, {5, 5}, {5, 19}, {19, 19}, {19, 13}});
        poly({{14, 5}, {19, 5}, {19, 10}});
        line(19, 5, 11.5, 12.5);
        break;
    case Exit: {
        QPainterPath arc;
        const QRectF r(5, 5, 14, 14);
        arc.arcMoveTo(r, 125);
        arc.arcTo(r, 125, 290);
        p.drawPath(arc);
        line(12, 3.5, 12, 11);
        break;
    }
    case Up:
        poly({{6, 14}, {12, 8}, {18, 14}});
        break;
    case Drive:
        p.drawRoundedRect(QRectF(3, 7.5, 18, 9), 2.2, 2.2);
        line(6.5, 12, 11, 12);
        p.save();
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(17, 12), 1.1, 1.1);
        p.restore();
        break;
    case Computer:
        p.drawRoundedRect(QRectF(3, 4, 18, 12), 2, 2);
        line(12, 16, 12, 19.5);
        line(8, 20, 16, 20);
        break;
    case FolderLink: {
        QPainterPath path;
        path.moveTo(3.5, 18.5);
        path.lineTo(3.5, 6.5);
        path.lineTo(9.5, 6.5);
        path.lineTo(11.5, 8.5);
        path.lineTo(20.5, 8.5);
        path.lineTo(20.5, 18.5);
        path.closeSubpath();
        p.drawPath(path);
        poly({{9, 16}, {9, 13.5}, {15, 13.5}});
        poly({{13, 11.5}, {15, 13.5}, {13, 15.5}});
        break;
    }
    }
}

QPixmap render(Kind k, int size, const QColor& c)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    QPen pen(c, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    drawGlyph(p, k);
    return pm;
}

} // namespace

QIcon make(Kind k)
{
    QColor normal(160, 183, 204);
    QColor active = Theme::Accent;
    if (k == Delete) {
        normal = QColor(232, 140, 140);
        active = Theme::Danger;
    } else if (k == Shield) {
        normal = QColor(240, 196, 92);
        active = Theme::Warn;
    }
    const QColor disabled(54, 68, 82);
    QIcon icon;
    for (int s : {16, 20, 24, 32, 40, 48}) {
        icon.addPixmap(render(k, s, normal), QIcon::Normal);
        icon.addPixmap(render(k, s, active), QIcon::Active);
        icon.addPixmap(render(k, s, QColor(234, 251, 255)), QIcon::Selected);
        icon.addPixmap(render(k, s, disabled), QIcon::Disabled);
    }
    return icon;
}

QIcon tinted(Kind k, const QColor& color)
{
    QIcon icon;
    for (int s : {16, 20, 24, 32, 48}) {
        icon.addPixmap(render(k, s, color), QIcon::Normal);
        icon.addPixmap(render(k, s, QColor(234, 251, 255)), QIcon::Selected);
    }
    return icon;
}

QPixmap logo(int size, qreal dpr)
{
    const int px = int(std::ceil(size * dpr));
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px / 24.0, px / 24.0);
    QLinearGradient bg(0, 0, 24, 24);
    bg.setColorAt(0, QColor(18, 32, 46));
    bg.setColorAt(1, QColor(8, 14, 22));
    p.setPen(QPen(QColor(34, 211, 238, 200), 1.0));
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0.5, 0.5, 23, 23), 5, 5);
    struct B { QRectF r; QColor c; };
    const B blocks[] = {
        {{3.5, 3.5, 9.5, 10.0}, QColor(34, 150, 230)},  {{14.0, 3.5, 6.5, 6.0}, QColor(26, 188, 180)},
        {{14.0, 10.5, 6.5, 3.0}, QColor(90, 112, 200)}, {{3.5, 14.5, 6.0, 6.0}, QColor(40, 200, 235)},
        {{10.5, 14.5, 4.5, 6.0}, QColor(80, 100, 128)}, {{16.0, 14.5, 4.5, 6.0}, QColor(30, 110, 170)},
    };
    p.setPen(Qt::NoPen);
    for (const B& b : blocks) {
        QRadialGradient g(b.r.center() - QPointF(b.r.width() * 0.2, b.r.height() * 0.2), b.r.width());
        g.setColorAt(0, b.c.lighter(135));
        g.setColorAt(1, b.c.darker(150));
        p.setBrush(g);
        p.drawRoundedRect(b.r, 1.2, 1.2);
    }
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

} // namespace Icons
