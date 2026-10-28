#pragma once
#include "FileNode.h"
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QToolButton;

// Clickable path from the scan root to the current treemap zoom level.
class BreadcrumbBar : public QWidget
{
    Q_OBJECT
public:
    explicit BreadcrumbBar(QWidget* parent = nullptr);
    void setNodes(FileNode* top, FileNode* zoom);

signals:
    void nodeClicked(FileNode* node);
    void zoomOutClicked();

private:
    QHBoxLayout* m_crumbs;
    QLabel* m_info;
    QToolButton* m_up;
};
