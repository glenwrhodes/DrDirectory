#include "BreadcrumbBar.h"
#include "Format.h"
#include "Icons.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

BreadcrumbBar::BreadcrumbBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Breadcrumb"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto outer = new QHBoxLayout(this);
    outer->setContentsMargins(6, 3, 10, 3);
    outer->setSpacing(2);
    m_up = new QToolButton(this);
    m_up->setIcon(Icons::make(Icons::Up));
    m_up->setToolTip(tr("Zoom out one level (Backspace / mouse wheel down)"));
    m_up->setAutoRaise(true);
    m_up->setEnabled(false);
    connect(m_up, &QToolButton::clicked, this, &BreadcrumbBar::zoomOutClicked);
    outer->addWidget(m_up);
    m_crumbs = new QHBoxLayout;
    m_crumbs->setSpacing(0);
    outer->addLayout(m_crumbs);
    outer->addStretch(1);
    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("CrumbInfo"));
    m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(m_info);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void BreadcrumbBar::setNodes(FileNode* top, FileNode* zoom)
{
    while (QLayoutItem* item = m_crumbs->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_info->clear();
    m_up->setEnabled(top && zoom && zoom != top);
    if (!top || !zoom)
        return;

    QList<FileNode*> chain;
    for (FileNode* n = zoom; n; n = n->parent) {
        chain.prepend(n);
        if (n == top)
            break;
    }
    const int maxShown = 7;
    for (int i = 0; i < chain.size(); ++i) {
        if (chain.size() > maxShown && i > 0 && i < chain.size() - (maxShown - 1)) {
            if (i == 1) {
                auto dots = new QLabel(QStringLiteral(" \u2026 \u203A "), this);
                dots->setObjectName(QStringLiteral("CrumbSep"));
                m_crumbs->addWidget(dots);
            }
            continue;
        }
        FileNode* n = chain[i];
        auto b = new QToolButton(this);
        b->setAutoRaise(true);
        b->setText(n->name);
        b->setToolTip(n->hasRealPath() ? n->fullPath() : n->name);
        if (n == zoom)
            b->setStyleSheet(QStringLiteral("color:#EAFBFF;"));
        connect(b, &QToolButton::clicked, this, [this, n] { emit nodeClicked(n); });
        m_crumbs->addWidget(b);
        if (i + 1 < chain.size())
        {
            auto sep = new QLabel(QStringLiteral("\u203A"), this);
            sep->setObjectName(QStringLiteral("CrumbSep"));
            m_crumbs->addWidget(sep);
        }
    }
    m_info->setText(tr("%1   \u00B7   %2 files   \u00B7   %3 folders")
                        .arg(Fmt::size(zoom->size), Fmt::count(zoom->fileCount), Fmt::count(zoom->dirCount)));
}
