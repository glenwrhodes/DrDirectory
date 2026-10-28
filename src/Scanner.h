#pragma once
#include "FileNode.h"
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <atomic>
#include <memory>
#include <thread>

class QThreadPool;

// Scans one or more folders/drives on a background thread using a pool of workers.
class Scanner : public QObject
{
    Q_OBJECT
public:
    explicit Scanner(QObject* parent = nullptr);
    ~Scanner() override;

    void start(const QStringList& paths, int threads);
    void cancel();
    bool isRunning() const { return m_running.load(); }
    std::unique_ptr<FileNode> takeResult();
    qint64 elapsedMs() const { return m_elapsed.load(); }

    static bool isVolumeRoot(const QString& path); // "C:\\", "/", "/Volumes/X", a mount point
    static QString normalizePath(const QString& path);

signals:
    void progress(qint64 files, qint64 dirs, qint64 bytes, const QString& currentPath);
    void finished(bool cancelled);

private:
    void run(QStringList paths, int threads);
    void scanDirectory(FileNode* node, const QString& path, QThreadPool* pool);
    std::unique_ptr<FileNode> makeTopNode(const QString& path);
    void emitProgress();

    std::thread m_thread;
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_running{false};
    std::atomic<qint64> m_files{0};
    std::atomic<qint64> m_dirs{0};
    std::atomic<qint64> m_bytes{0};
    std::atomic<qint64> m_elapsed{0};
    std::atomic<quint32> m_tick{0};
    QMutex m_mutex;
    QSet<QString> m_skipPaths; // never descend (e.g. /System/Volumes on macOS when scanning /)
    QString m_currentPath;
    std::unique_ptr<FileNode> m_result;
    QElapsedTimer m_timer;
};
