#pragma once
#include <QIcon>
#include <QPixmap>

// Thin-line vector icons drawn at runtime so they match the theme (and stay crisp at any DPI).
namespace Icons {
enum Kind { Scan, Rescan, Refresh, Stop, ZoomIn, ZoomOut, ZoomReset, Folder, Recycle, Delete, Shield,
            Export, Copy, Info, Terminal, Open, Exit, Up, Drive, Computer, FolderLink };
QIcon make(Kind kind);
QIcon tinted(Kind kind, const QColor& color); // single-colour icon for item views
QPixmap logo(int size, qreal dpr);
} // namespace Icons
