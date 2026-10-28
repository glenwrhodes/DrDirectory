#include "TreemapWidget.h"
#include "Format.h"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const QColor kDirColor(140, 140, 140);
const QColor kFreeColor(176, 176, 176);
const QColor kUnknownColor(214, 200, 120);
} // namespace

TreemapWidget::TreemapWidget(ExtensionTable* exts, QWidget* parent)
    : QWidget(parent)
    , m_exts(exts)
{
    setMouseTracking(true);
    setMinimumHeight(120);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_resizeTimer.setSingleShot(true);
    m_resizeTimer.setInterval(120);
    connect(&m_resizeTimer, &QTimer::timeout, this, [this] {
        m_resizePending = false;
        m_dirty = true;
        update();
    });
    const double len = std::sqrt(1.0 + 1.0 + 100.0);
    m_lx = -1.0 / len;
    m_ly = -1.0 / len;
    m_lz = 10.0 / len;
    m_message = tr("Choose a drive or folder to scan (File \u2192 Scan\u2026)");
}

void TreemapWidget::setTopRoot(FileNode* root)
{
    m_top = root;
    m_zoom = root;
    m_selected = nullptr;
    m_hover = nullptr;
    m_highlightExt = -1;
    m_rects.clear();
    m_dirty = true;
    update();
    emit zoomChanged(m_zoom);
}

void TreemapWidget::setZoomRoot(FileNode* node)
{
    if (!node || !m_top || !m_top->isAncestorOf(node) || !node->isContainer())
        return;
    if (node == m_zoom)
        return;
    m_zoom = node;
    m_hover = nullptr;
    m_dirty = true;
    update();
    emit zoomChanged(m_zoom);
}

void TreemapWidget::setSelected(FileNode* node)
{
    if (m_selected == node)
        return;
    m_selected = node;
    update();
}

void TreemapWidget::setHighlightExtension(int extId)
{
    if (m_highlightExt == extId)
        return;
    m_highlightExt = extId;
    update();
}

void TreemapWidget::setOptions(const Options& o)
{
    m_opt = o;
    invalidate();
}

void TreemapWidget::setMessage(const QString& msg)
{
    m_message = msg;
    if (!m_zoom)
        update();
}

void TreemapWidget::invalidate()
{
    m_rects.clear();
    m_dirty = true;
    update();
}

void TreemapWidget::prepareRemoval(FileNode* node, bool keepNode)
{
    if (!node)
        return;
    auto doomed = [&](const FileNode* x) { return x && node->isAncestorOf(x) && !(keepNode && x == node); };
    bool zoomChangedFlag = false;
    if (doomed(m_top)) {
        m_top = m_zoom = nullptr;
        zoomChangedFlag = true;
    } else if (doomed(m_zoom)) {
        m_zoom = keepNode ? node : node->parent;
        zoomChangedFlag = true;
    }
    if (doomed(m_selected))
        m_selected = keepNode ? node : nullptr;
    if (doomed(m_hover))
        m_hover = nullptr;
    m_rects.clear();
    m_dirty = true;
    update();
    if (zoomChangedFlag)
        emit zoomChanged(m_zoom);
}

void TreemapWidget::zoomIn()
{
    zoomToward(m_selected);
}

void TreemapWidget::zoomOut()
{
    if (canZoomOut() && m_zoom->parent)
        setZoomRoot(m_zoom->parent);
}

void TreemapWidget::zoomReset()
{
    if (m_top)
        setZoomRoot(m_top);
}

void TreemapWidget::zoomToward(FileNode* n)
{
    if (!n || !m_zoom || !m_zoom->isAncestorOf(n) || n == m_zoom)
        return;
    FileNode* c = n;
    while (c && c->parent != m_zoom)
        c = c->parent;
    if (c && c->isContainer() && !c->children.empty())
        setZoomRoot(c);
}

QColor TreemapWidget::colorFor(const FileNode* n) const
{
    switch (n->kind) {
    case FileNode::File: return m_exts->color(n->extId);
    case FileNode::FreeSpace: return kFreeColor;
    case FileNode::Unknown: return kUnknownColor;
    default: return kDirColor;
    }
}

// ----------------------------------------------------------------------------------------------- rendering

void TreemapWidget::render()
{
    m_dpr = devicePixelRatioF();
    const QSize px(qMax(1, qRound(width() * m_dpr)), qMax(1, qRound(height() * m_dpr)));
    m_image = QImage(px, QImage::Format_RGB32);
    m_image.fill(palette().color(QPalette::Window).darker(120));
    m_rects.clear();
    if (m_zoom && m_zoom->size > 0) {
        const double s[4] = {0, 0, 0, 0};
        drawNode(m_zoom, QRect(QPoint(0, 0), px), s, m_opt.height, true);
    }
    m_image.setDevicePixelRatio(m_dpr);
    m_dirty = false;
}

void TreemapWidget::addRidge(const QRect& rc, double* s, double h)
{
    const double w = rc.width(), ht = rc.height();
    const double left = rc.left(), right = rc.left() + w;
    const double top = rc.top(), bottom = rc.top() + ht;
    const double h4 = 4.0 * h;
    const double wf = h4 / w;
    s[2] += wf * (right + left);
    s[0] -= wf;
    const double hf = h4 / ht;
    s[3] += hf * (bottom + top);
    s[1] -= hf;
}

void TreemapWidget::squarify(const std::vector<FileNode*>& kids, const QRectF& rc, double total,
                             std::vector<QRectF>& out)
{
    const size_t n = kids.size();
    out.assign(n, QRectF());
    double x = rc.x(), y = rc.y(), w = rc.width(), h = rc.height();
    double remaining = total;
    size_t i = 0;
    while (i < n) {
        if (w <= 0.0 || h <= 0.0 || remaining <= 0.0)
            break;
        const bool column = w >= h; // lay the next row as a column on the left
        const double side = column ? h : w;
        const double scale = (w * h) / remaining; // pixels² per byte
        size_t j = i;
        double rowSum = 0.0;
        double worst = std::numeric_limits<double>::max();
        while (j < n) {
            const double sz = double(kids[j]->size);
            const double newSum = rowSum + sz;
            const double thick = newSum * scale / side;
            const double maxLen = double(kids[i]->size) * scale / thick;
            const double minLen = sz * scale / thick;
            const double r1 = std::max(maxLen / thick, thick / maxLen);
            const double r2 = std::max(minLen / thick, thick / minLen);
            const double newWorst = std::max(r1, r2);
            if (j > i && newWorst > worst)
                break;
            worst = newWorst;
            rowSum = newSum;
            ++j;
        }
        const double thick = rowSum * scale / side;
        double pos = column ? y : x;
        for (size_t k = i; k < j; ++k) {
            double len = double(kids[k]->size) * scale / thick;
            if (k == j - 1)
                len = (column ? y + h : x + w) - pos; // absorb rounding error
            out[k] = column ? QRectF(x, pos, thick, len) : QRectF(pos, y, len, thick);
            pos += len;
        }
        if (column) {
            x += thick;
            w -= thick;
        } else {
            y += thick;
            h -= thick;
        }
        remaining -= rowSum;
        i = j;
    }
}

void TreemapWidget::drawNode(FileNode* node, const QRect& rc, const double* parentSurface, double h, bool isRoot)
{
    m_rects[node] = rc;
    double s[4] = {parentSurface[0], parentSurface[1], parentSurface[2], parentSurface[3]};
    if (m_opt.cushion && !isRoot)
        addRidge(rc, s, h);

    const bool tooSmall = rc.width() < 3 || rc.height() < 3;
    if (!node->isContainer() || node->children.empty() || tooSmall) {
        renderLeaf(rc, s, colorFor(node));
        return;
    }

    std::vector<FileNode*> kids;
    kids.reserve(node->children.size());
    double total = 0;
    for (auto& c : node->children) {
        if (c->size > 0) {
            kids.push_back(c.get());
            total += double(c->size);
        }
    }
    if (kids.empty() || total <= 0) {
        renderLeaf(rc, s, colorFor(node));
        return;
    }
    std::sort(kids.begin(), kids.end(), [](const FileNode* a, const FileNode* b) { return a->size > b->size; });

    std::vector<QRectF> rects;
    squarify(kids, QRectF(rc), total, rects);
    const double childH = h * m_opt.scale;
    for (size_t i = 0; i < kids.size(); ++i) {
        const QRectF& f = rects[i];
        if (f.isEmpty())
            continue;
        const int l = int(std::lround(f.left()));
        const int t = int(std::lround(f.top()));
        const int r = int(std::lround(f.right()));
        const int b = int(std::lround(f.bottom()));
        if (r <= l || b <= t)
            continue;
        drawNode(kids[i], QRect(l, t, r - l, b - t), s, childH, false);
    }
}

void TreemapWidget::renderLeaf(const QRect& rc, const double* s, const QColor& color)
{
    const QRect r = rc.intersected(m_image.rect());
    if (r.isEmpty())
        return;
    const double cr = color.red(), cg = color.green(), cb = color.blue();
    const bool grid = m_opt.grid && rc.width() > 3 && rc.height() > 3;
    const QRgb gridColor = qRgb(int(cr * 0.45), int(cg * 0.45), int(cb * 0.45));

    if (!m_opt.cushion) {
        const QRgb c = qRgb(int(cr), int(cg), int(cb));
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(m_image.scanLine(y));
            std::fill(line + r.left(), line + r.right() + 1, c);
        }
    } else {
        const double ia = m_opt.ambient;
        const double is = 1.0 - ia;
        const double gain = m_opt.brightness;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(m_image.scanLine(y));
            const double ny = -(2.0 * s[1] * (y + 0.5) + s[3]);
            const double nyl = ny * m_ly + m_lz;
            const double ny2 = ny * ny + 1.0;
            for (int x = r.left(); x <= r.right(); ++x) {
                const double nx = -(2.0 * s[0] * (x + 0.5) + s[2]);
                double cosa = (nx * m_lx + nyl) / std::sqrt(nx * nx + ny2);
                if (cosa > 1.0)
                    cosa = 1.0;
                double pixel = is * cosa;
                if (pixel < 0.0)
                    pixel = 0.0;
                pixel = (pixel + ia) * gain;
                line[x] = qRgb(std::min(255, int(cr * pixel)), std::min(255, int(cg * pixel)),
                               std::min(255, int(cb * pixel)));
            }
        }
    }
    if (grid) {
        QRgb* bottom = reinterpret_cast<QRgb*>(m_image.scanLine(r.bottom()));
        std::fill(bottom + r.left(), bottom + r.right() + 1, gridColor);
        for (int y = r.top(); y <= r.bottom(); ++y)
            reinterpret_cast<QRgb*>(m_image.scanLine(y))[r.right()] = gridColor;
    }
}

// ----------------------------------------------------------------------------------------------- painting

QRectF TreemapWidget::toLogical(const QRect& r) const
{
    return QRectF(r.x() / m_dpr, r.y() / m_dpr, r.width() / m_dpr, r.height() / m_dpr);
}

QPoint TreemapWidget::toPixels(const QPointF& p) const
{
    return QPoint(int(p.x() * m_dpr), int(p.y() * m_dpr));
}

void TreemapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (!m_zoom) {
        p.fillRect(rect(), palette().color(QPalette::Base));
        p.setPen(palette().color(QPalette::PlaceholderText));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.2);
        p.setFont(f);
        p.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, m_message);
        return;
    }
    if (m_resizePending && !m_image.isNull()) {
        p.drawImage(QRectF(rect()), m_image);
        return;
    }
    if (m_dirty || m_image.isNull())
        render();
    p.drawImage(QPointF(0, 0), m_image);
    drawOverlays(p);
}

void TreemapWidget::drawOverlays(QPainter& p)
{
    // Highlight every file of the selected extension.
    if (m_highlightExt >= 0) {
        QPen dark(QColor(0, 0, 0, 200), 1);
        QPen light(QColor(255, 255, 255), 1);
        p.setBrush(Qt::NoBrush);
        for (const auto& [node, r] : m_rects) {
            if (node->kind != FileNode::File || node->extId != m_highlightExt)
                continue;
            const QRectF lr = toLogical(r);
            p.setPen(dark);
            p.drawRect(lr.adjusted(0.5, 0.5, -0.5, -0.5));
            if (lr.width() > 4 && lr.height() > 4) {
                p.setPen(light);
                p.drawRect(lr.adjusted(1.5, 1.5, -1.5, -1.5));
            }
        }
    }

    // Labels for the immediate children of the zoom root.
    if (m_opt.labels && m_zoom) {
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.9);
        p.setFont(f);
        const QFontMetrics fm(f);
        const qreal lineH = fm.height();
        for (auto& c : m_zoom->children) {
            auto it = m_rects.find(c.get());
            if (it == m_rects.end())
                continue;
            const QRectF lr = toLogical(it->second);
            if (lr.width() < 70 || lr.height() < 2 * lineH + 12)
                continue;
            const QString t1 = fm.elidedText(c->name, Qt::ElideMiddle, int(lr.width() - 20));
            const QString t2 = Fmt::size(c->size);
            const qreal tw = std::max(fm.horizontalAdvance(t1), fm.horizontalAdvance(t2));
            const QRectF box(lr.left() + 4, lr.top() + 4, std::min(lr.width() - 8, tw + 10), 2 * lineH + 6);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 140));
            p.drawRoundedRect(box, 3, 3);
            p.setPen(Qt::white);
            p.drawText(box.adjusted(5, 3, -5, -3), Qt::AlignLeft | Qt::AlignTop, t1 + QLatin1Char('\n') + t2);
        }
    }

    // Hover outline.
    if (m_hover && m_hover != m_selected) {
        const QRect r = drawnRectFor(m_hover);
        if (!r.isEmpty()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 170), 1));
            p.drawRect(toLogical(r).adjusted(0.5, 0.5, -0.5, -0.5));
        }
    }

    // Selection frame.
    if (m_selected) {
        const QRect r = drawnRectFor(m_selected);
        if (!r.isEmpty()) {
            const QRectF lr = toLogical(r);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 0, 0), 3));
            p.drawRect(lr.adjusted(1.5, 1.5, -1.5, -1.5));
            p.setPen(QPen(QColor(255, 235, 59), 1.6));
            p.drawRect(lr.adjusted(1.5, 1.5, -1.5, -1.5));
        }
    }
}

QRect TreemapWidget::drawnRectFor(const FileNode* n) const
{
    if (!m_zoom || !n || !m_zoom->isAncestorOf(n))
        return {};
    for (const FileNode* x = n; x; x = x->parent) {
        auto it = m_rects.find(x);
        if (it != m_rects.end())
            return it->second;
        if (x == m_zoom)
            break;
    }
    return {};
}

FileNode* TreemapWidget::hitTest(const QPoint& px) const
{
    if (!m_zoom)
        return nullptr;
    auto it = m_rects.find(m_zoom);
    if (it == m_rects.end() || !it->second.contains(px))
        return nullptr;
    FileNode* n = m_zoom;
    while (n->isContainer()) {
        FileNode* next = nullptr;
        for (auto& c : n->children) {
            auto ci = m_rects.find(c.get());
            if (ci != m_rects.end() && ci->second.contains(px)) {
                next = c.get();
                break;
            }
        }
        if (!next)
            break;
        n = next;
    }
    return n;
}

// ----------------------------------------------------------------------------------------------- events

void TreemapWidget::resizeEvent(QResizeEvent*)
{
    m_resizePending = !m_image.isNull();
    m_dirty = true;
    m_resizeTimer.start();
}

void TreemapWidget::mouseMoveEvent(QMouseEvent* e)
{
    FileNode* n = hitTest(toPixels(e->position()));
    if (n != m_hover) {
        m_hover = n;
        update();
        emit nodeHovered(n);
        if (n) {
            QString text = QStringLiteral("<p style='white-space:pre'><b>%1</b><br>%2").arg(n->name.toHtmlEscaped(), Fmt::size(n->size));
            if (m_zoom && m_zoom->size > 0)
                text += QStringLiteral(" (%1 of view)").arg(Fmt::percent(double(n->size) / double(m_zoom->size)));
            if (n->hasRealPath())
                text += QStringLiteral("<br><span style='color:gray'>%1</span>").arg(n->fullPath().toHtmlEscaped());
            text += QStringLiteral("</p>");
            QToolTip::showText(e->globalPosition().toPoint(), text, this);
        } else {
            QToolTip::hideText();
        }
    }
}

void TreemapWidget::leaveEvent(QEvent*)
{
    if (m_hover) {
        m_hover = nullptr;
        update();
        emit nodeHovered(nullptr);
    }
}

void TreemapWidget::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    if (e->button() == Qt::LeftButton) {
        if (FileNode* n = hitTest(toPixels(e->position()))) {
            m_selected = n;
            update();
            emit nodeClicked(n);
        }
    } else if (e->button() == Qt::BackButton) {
        zoomOut();
    } else if (e->button() == Qt::ForwardButton) {
        zoomIn();
    }
}

void TreemapWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        zoomToward(hitTest(toPixels(e->position())));
}

void TreemapWidget::wheelEvent(QWheelEvent* e)
{
    if (!m_zoom)
        return;
    m_wheelAccum += e->angleDelta().y();
    while (m_wheelAccum >= 120) {
        m_wheelAccum -= 120;
        zoomToward(hitTest(toPixels(e->position())));
    }
    while (m_wheelAccum <= -120) {
        m_wheelAccum += 120;
        zoomOut();
    }
    e->accept();
}

void TreemapWidget::contextMenuEvent(QContextMenuEvent* e)
{
    FileNode* n = hitTest(toPixels(QPointF(e->pos())));
    if (!n)
        return;
    m_selected = n;
    update();
    emit nodeClicked(n);
    emit contextMenuRequested(n, e->globalPos());
}

void TreemapWidget::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Backspace: zoomOut(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter: zoomIn(); break;
    case Qt::Key_Home: zoomReset(); break;
    default: QWidget::keyPressEvent(e);
    }
}
