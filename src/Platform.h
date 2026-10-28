#pragma once
#include <QIcon>
#include <QList>
#include <QStorageInfo>
#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

// OS integration layer. Implemented in Platform_win.cpp and Platform_unix.cpp (macOS + Linux).
namespace Platform {
QString typeNameForExtension(const QString& ext);
QIcon iconForExtension(const QString& ext);
QString attributesString(quint32 attributes); // Windows attribute letters, or POSIX permissions

QString fileManagerName(); // "Explorer", "Finder", "File Manager"
QString trashName();       // "Recycle Bin" or "Trash"

void openItem(const QString& path);
void showInFileManager(const QString& path);
void showProperties(QWidget* parent, const QString& path);
void openTerminal(const QString& dir);

bool supportsElevation();
bool restartElevated(const QStringList& args);

// Deletes (or moves to trash) each path. Safe to call from a worker thread. Returns success per path.
QVector<bool> deletePaths(const QStringList& paths, bool toTrash);

QList<QStorageInfo> scannableVolumes(); // real, user-facing volumes for the scan dialog
bool isLocalVolume(const QStorageInfo& volume);
} // namespace Platform
