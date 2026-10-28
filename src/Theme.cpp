#include "Theme.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace Theme {

QFont displayFont(qreal pointSize, int weight)
{
    static const bool hasBahn = QFontDatabase::hasFamily(QStringLiteral("Bahnschrift"));
    QFont f(hasBahn ? QStringLiteral("Bahnschrift") : QStringLiteral("Segoe UI"));
    f.setPointSizeF(pointSize);
    f.setWeight(QFont::Weight(weight));
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

static const char* kStyleSheet = R"QSS(
QMainWindow, QDialog, QMessageBox { background: #0D131A; }
QToolTip { background: #0F1922; color: #D6E1EC; border: 1px solid #1F8FA3; padding: 5px 7px; }

QMenuBar { background: #090E14; border-bottom: 1px solid #18232E; padding: 2px 4px; }
QMenuBar::item { background: transparent; padding: 5px 11px; border-radius: 5px; color: #A9B9C9; }
QMenuBar::item:selected { background: #13202B; color: #22D3EE; }
QMenu { background: #0F1720; border: 1px solid #22303E; padding: 5px; }
QMenu::item { padding: 6px 30px 6px 26px; border-radius: 5px; color: #CBD6E2; }
QMenu::item:selected { background: rgba(34, 211, 238, 0.13); color: #EAFBFF; }
QMenu::item:disabled { color: #435160; }
QMenu::separator { height: 1px; background: #1C2834; margin: 5px 8px; }
QMenu::icon { padding-left: 6px; }

QToolBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #101923, stop:1 #0B1118);
           border: none; border-bottom: 1px solid #18232E; padding: 5px 8px; spacing: 3px; }
QToolBar::separator { width: 1px; background: #1C2834; margin: 7px 7px; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 7px;
              padding: 5px 9px; color: #A9B9C9; }
QToolButton:hover { background: rgba(34, 211, 238, 0.08); border-color: rgba(34, 211, 238, 0.38); color: #EAFBFF; }
QToolButton:pressed { background: rgba(34, 211, 238, 0.18); }
QToolButton:disabled { color: #3A4755; }

QHeaderView { background: #0E161F; border: none; }
QHeaderView::section { background: #0E161F; color: #6F849A; border: none; border-bottom: 1px solid #1C2A38;
                       border-right: 1px solid #141F2A; padding: 6px 8px; font-family: "Bahnschrift"; font-size: 9pt; }
QHeaderView::section:hover { color: #22D3EE; }
QHeaderView::up-arrow, QHeaderView::down-arrow { width: 0px; height: 0px; }

QTreeView, QTableView, QTreeWidget { background: #101821; alternate-background-color: #121C26;
        border: 1px solid #18232E; gridline-color: #18232E; outline: 0; }
QTreeView::item, QTableView::item, QTreeWidget::item { padding: 1px 2px; }

QTabWidget::pane { border: none; }
QTabBar::tab { background: transparent; color: #6F849A; padding: 8px 18px; border: none;
               border-bottom: 2px solid transparent; font-family: "Bahnschrift"; font-size: 10pt; }
QTabBar::tab:selected { color: #EAFBFF; border-bottom: 2px solid #22D3EE; }
QTabBar::tab:hover:!selected { color: #B6C6D6; }

QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }
QScrollBar::handle:vertical { background: #243241; min-height: 32px; border-radius: 4px; margin: 2px 3px; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 0; }
QScrollBar::handle:horizontal { background: #243241; min-width: 32px; border-radius: 4px; margin: 3px 2px; }
QScrollBar::handle:hover { background: #1E6F84; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSplitter::handle { background: #090E14; }
QSplitter::handle:hover { background: #155E70; }

QStatusBar { background: #090E14; border-top: 1px solid #18232E; color: #8597AA; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: #8597AA; padding: 0 6px; }
QLabel#HoverLabel { color: #5F7489; }
QLabel#AdminPill { color: #FBBF24; border: 1px solid rgba(251, 191, 36, 0.35); border-radius: 9px; padding: 1px 10px; margin: 2px 4px; }
QLabel#AdminPillOn { color: #5EEAD4; border: 1px solid rgba(94, 234, 212, 0.35); border-radius: 9px; padding: 1px 10px; margin: 2px 4px; }

QProgressBar { background: #0E1720; border: 1px solid #22303E; border-radius: 5px; text-align: center;
               color: #D6E1EC; min-height: 14px; max-height: 14px; }
QProgressBar::chunk { border-radius: 4px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0E9FB3, stop:1 #3B82F6); }

QLineEdit { background: #0E161F; border: 1px solid #22303E; border-radius: 7px; padding: 6px 9px;
            selection-background-color: #0E6A85; color: #D6E1EC; }
QLineEdit:focus { border-color: #22D3EE; }

QPushButton { background: #131E29; border: 1px solid #263646; border-radius: 7px; padding: 7px 18px; color: #D6E1EC; }
QPushButton:hover { border-color: #22D3EE; color: #EAFBFF; }
QPushButton:pressed { background: #0E4F61; }
QPushButton:default { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0E7490, stop:1 #1D4ED8);
                      border: 1px solid #22D3EE; color: #FFFFFF; }

QRadioButton, QCheckBox { spacing: 9px; color: #CBD6E2; }
QRadioButton::indicator { width: 12px; height: 12px; border-radius: 7px; border: 1px solid #3A4B5C; background: #0E161F; }
QRadioButton::indicator:hover { border-color: #22D3EE; }
QRadioButton::indicator:checked { border: 1px solid #22D3EE;
    background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 #22D3EE, stop:0.5 #22D3EE, stop:0.62 #0E161F, stop:1 #0E161F); }

QWidget#Breadcrumb { background: #0B1219; border-top: 1px solid #18232E; border-bottom: 1px solid #18232E; }
QWidget#Breadcrumb QToolButton { color: #8FB4C8; padding: 3px 7px; font-family: "Bahnschrift"; font-size: 10pt; }
QWidget#Breadcrumb QToolButton:hover { color: #22D3EE; }
QLabel#CrumbSep { color: #33495C; font-size: 11pt; }
QLabel#CrumbInfo { color: #6F849A; font-family: "Bahnschrift"; font-size: 9.5pt; }
QLabel#BrandText { color: #EAF6FF; }
)QSS";

void apply(QApplication& app)
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark); // dark Windows title bar

    QPalette p;
    p.setColor(QPalette::Window, Window);
    p.setColor(QPalette::WindowText, Text);
    p.setColor(QPalette::Base, Panel);
    p.setColor(QPalette::AlternateBase, PanelAlt);
    p.setColor(QPalette::ToolTipBase, QColor(15, 25, 34));
    p.setColor(QPalette::ToolTipText, Text);
    p.setColor(QPalette::PlaceholderText, QColor(88, 105, 122));
    p.setColor(QPalette::Text, Text);
    p.setColor(QPalette::Button, QColor(19, 30, 41));
    p.setColor(QPalette::ButtonText, Text);
    p.setColor(QPalette::BrightText, Accent);
    p.setColor(QPalette::Highlight, QColor(14, 104, 132));
    p.setColor(QPalette::HighlightedText, QColor(240, 251, 255));
    p.setColor(QPalette::Link, Accent);
    p.setColor(QPalette::Light, QColor(42, 58, 74));
    p.setColor(QPalette::Midlight, QColor(30, 44, 58));
    p.setColor(QPalette::Mid, QColor(38, 54, 70));
    p.setColor(QPalette::Dark, QColor(8, 12, 17));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0));
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(70, 84, 98));
    p.setColor(QPalette::Inactive, QPalette::Highlight, QColor(17, 74, 95));
    app.setPalette(p);

    QFont f(QStringLiteral("Segoe UI"));
    f.setPointSizeF(9.5);
    f.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(f);
    app.setStyleSheet(QString::fromLatin1(kStyleSheet));
}

} // namespace Theme
