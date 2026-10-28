#pragma once
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

// Thin wrappers around Windows shell APIs.
namespace WinShell {
QString typeNameForExtension(const QString& ext);
QIcon iconForExtension(const QString& ext);
QString attributesString(quint32 attributes);

void openItem(const QString& path);
void showInExplorer(const QString& path);
void showProperties(QWidget* parent, const QString& path);
void openTerminal(const QString& dir);

bool isElevated();
bool restartElevated(const QStringList& args);

// Deletes (or recycles) each path. Safe to call from a worker thread. Returns success per path.
QVector<bool> deletePaths(const QStringList& paths, bool toRecycleBin);
} // namespace WinShell
