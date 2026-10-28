#pragma once
#include <QColor>
#include <QFont>

class QApplication;

// Central dark "instrument panel" theme: graphite / navy surfaces, cyan & teal accents.
namespace Theme {
inline const QColor Bg(9, 14, 20);          // treemap canvas, deepest layer
inline const QColor Window(13, 19, 26);
inline const QColor Panel(16, 24, 33);
inline const QColor PanelAlt(19, 28, 38);
inline const QColor Border(34, 48, 62);
inline const QColor Text(214, 225, 236);
inline const QColor TextDim(118, 135, 154);
inline const QColor Accent(34, 211, 238);   // cyan
inline const QColor Accent2(94, 234, 212);  // teal
inline const QColor AccentBlue(59, 130, 246);
inline const QColor Warn(251, 191, 36);
inline const QColor Danger(248, 113, 113);

void apply(QApplication& app);
QFont displayFont(qreal pointSize, int weight = QFont::Normal);
} // namespace Theme
