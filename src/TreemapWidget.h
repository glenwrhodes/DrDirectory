#pragma once
#include "Extensions.h"
#include "FileNode.h"
#include <QImage>
#include <QTimer>
#include <QWidget>
#include <unordered_map>
#include <vector>

class QPainter;

// Squarified cushion treemap (van Wijk / van de Wetering shading, as popularised by SequoiaView & WinDirStat).
class TreemapWidget : public QWidget
{
    Q_OBJECT
public:
    struct Options
    {
        bool cushion = true;
        bool grid = false;
        bool labels = true;
        double height = 0.38;     // initial cushion height
        double scale = 0.91;      // height falloff per level
        double ambient = 0.15;    // ambient light
        double brightness = 1.08; // overall gain
    };

    explicit TreemapWidget(ExtensionTable* exts, QWidget* parent = nullptr);

    void setTopRoot(FileNode* root);
    FileNode* topRoot() const { return m_top; }
    FileNode* zoomRoot() const { return m_zoom; }
    void setZoomRoot(FileNode* node);
    void setSelected(FileNode* node);
    void setHighlightExtension(int extId);
    void setOptions(const Options& o);
    Options options() const { return m_opt; }
    void setMessage(const QString& msg);
    void invalidate();
    // Call before `node` (or, with keepNode, only its descendants) is destroyed.
    void prepareRemoval(FileNode* node, bool keepNode);
    bool canZoomOut() const { return m_zoom && m_zoom != m_top; }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();

signals:
    void nodeClicked(FileNode* node);
    void nodeHovered(FileNode* node);
    void zoomChanged(FileNode* zoomRoot);
    void contextMenuRequested(FileNode* node, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void render();
    void drawNode(FileNode* node, const QRect& rc, const double* surface, double h, bool isRoot);
    void renderLeaf(const QRect& rc, const double* surface, const QColor& color);
    static void addRidge(const QRect& rc, double* s, double h);
    static void squarify(const std::vector<FileNode*>& kids, const QRectF& rc, double total, std::vector<QRectF>& out);
    QColor colorFor(const FileNode* n) const;
    FileNode* hitTest(const QPoint& px) const;
    QRect drawnRectFor(const FileNode* n) const;
    QRectF toLogical(const QRect& r) const;
    QPoint toPixels(const QPointF& p) const;
    void zoomToward(FileNode* n);
    void drawOverlays(QPainter& p);

    ExtensionTable* m_exts;
    Options m_opt;
    FileNode* m_top = nullptr;
    FileNode* m_zoom = nullptr;
    FileNode* m_selected = nullptr;
    FileNode* m_hover = nullptr;
    int m_highlightExt = -1;
    QString m_message;
    QImage m_image;
    qreal m_dpr = 1.0;
    bool m_dirty = true;
    bool m_resizePending = false;
    int m_wheelAccum = 0;
    QTimer m_resizeTimer;
    std::unordered_map<const FileNode*, QRect> m_rects;
    double m_lx, m_ly, m_lz;
};
