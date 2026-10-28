#include "Scanner.h"
#include <QDir>
#include <QMutexLocker>
#include <QThreadPool>
#include <windows.h>

namespace {

QString longPath(const QString& path)
{
    if (path.startsWith(QLatin1String("\\\\?\\")))
        return path;
    if (path.startsWith(QLatin1String("\\\\")))
        return QStringLiteral("\\\\?\\UNC\\") + path.mid(2);
    return QStringLiteral("\\\\?\\") + path;
}

QString joinPath(const QString& dir, const QString& name)
{
    if (dir.endsWith(QLatin1Char('\\')))
        return dir + name;
    return dir + QLatin1Char('\\') + name;
}

qint64 fileTimeToMs(const FILETIME& ft)
{
    ULARGE_INTEGER v;
    v.LowPart = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    if (v.QuadPart < 116444736000000000ULL)
        return 0;
    return qint64((v.QuadPart - 116444736000000000ULL) / 10000ULL);
}

const wchar_t* wstr(const QString& s) { return reinterpret_cast<const wchar_t*>(s.utf16()); }

} // namespace

Scanner::Scanner(QObject* parent)
    : QObject(parent)
{
}

Scanner::~Scanner()
{
    cancel();
    if (m_thread.joinable())
        m_thread.join();
}

bool Scanner::isDriveRoot(const QString& path)
{
    const QString p = normalizePath(path);
    return p.size() == 3 && p[1] == QLatin1Char(':') && p[2] == QLatin1Char('\\');
}

QString Scanner::normalizePath(const QString& path)
{
    QString p = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (p.size() == 2 && p[1] == QLatin1Char(':'))
        p += QLatin1Char('\\');
    if (p.size() == 3 && p[1] == QLatin1Char(':'))
        p[0] = p[0].toUpper();
    return p;
}

void Scanner::start(const QStringList& paths, int threads)
{
    if (m_running.load())
        return;
    if (m_thread.joinable())
        m_thread.join();
    m_cancel = false;
    m_files = 0;
    m_dirs = 0;
    m_bytes = 0;
    m_elapsed = 0;
    m_result.reset();
    m_running = true;
    m_timer.start();
    m_thread = std::thread(&Scanner::run, this, paths, qMax(1, threads));
}

void Scanner::cancel()
{
    m_cancel = true;
}

std::unique_ptr<FileNode> Scanner::takeResult()
{
    QMutexLocker lock(&m_mutex);
    return std::move(m_result);
}

std::unique_ptr<FileNode> Scanner::makeTopNode(const QString& path)
{
    auto n = std::make_unique<FileNode>();
    n->name = normalizePath(path);
    n->kind = isDriveRoot(n->name) ? FileNode::Drive : FileNode::Dir;
    WIN32_FILE_ATTRIBUTE_DATA ad{};
    const QString lp = longPath(n->name);
    if (GetFileAttributesExW(wstr(lp), GetFileExInfoStandard, &ad)) {
        n->attributes = ad.dwFileAttributes;
        n->lastWrite = fileTimeToMs(ad.ftLastWriteTime);
    }
    return n;
}

void Scanner::emitProgress()
{
    QString path;
    {
        QMutexLocker lock(&m_mutex);
        path = m_currentPath;
    }
    emit progress(m_files.load(), m_dirs.load(), m_bytes.load(), path);
}

void Scanner::run(QStringList paths, int threads)
{
    std::unique_ptr<FileNode> root;
    QThreadPool pool;
    pool.setMaxThreadCount(threads);

    auto launch = [this, &pool](FileNode* node) {
        const QString p = node->name;
        pool.start([this, node, p, &pool] { scanDirectory(node, p, &pool); });
    };

    if (paths.size() == 1) {
        root = makeTopNode(paths.first());
        launch(root.get());
    } else {
        root = std::make_unique<FileNode>();
        root->kind = FileNode::Root;
        root->name = tr("This PC");
        for (const QString& p : paths) {
            auto child = makeTopNode(p);
            child->parent = root.get();
            FileNode* raw = child.get();
            root->children.push_back(std::move(child));
            launch(raw);
        }
    }

    while (!pool.waitForDone(100))
        emitProgress();
    emitProgress();

    const bool cancelled = m_cancel.load();
    if (!cancelled)
        finalizeTree(root.get());
    else
        root.reset();

    {
        QMutexLocker lock(&m_mutex);
        m_result = std::move(root);
    }
    m_elapsed = m_timer.elapsed();
    m_running = false;
    emit finished(cancelled);
}

void Scanner::scanDirectory(FileNode* node, const QString& path, QThreadPool* pool)
{
    if (m_cancel.load())
        return;

    QString search = longPath(path);
    if (!search.endsWith(QLatin1Char('\\')))
        search += QLatin1Char('\\');
    search += QLatin1Char('*');

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(wstr(search), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
            node->accessDenied = true;
        return;
    }

    std::vector<std::pair<FileNode*, QString>> subdirs;
    qint64 files = 0, bytes = 0, dirs = 0;
    do {
        const wchar_t* fn = fd.cFileName;
        if (fn[0] == L'.' && (fn[1] == 0 || (fn[1] == L'.' && fn[2] == 0)))
            continue;
        auto child = std::make_unique<FileNode>();
        child->name = QString::fromWCharArray(fn);
        child->parent = node;
        child->attributes = fd.dwFileAttributes;
        child->lastWrite = fileTimeToMs(fd.ftLastWriteTime);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            child->kind = FileNode::Dir;
            ++dirs;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                child->reparsePoint = true; // never follow junctions / symlinks (avoids loops & double counting)
            else
                subdirs.emplace_back(child.get(), joinPath(path, child->name));
        } else {
            child->kind = FileNode::File;
            child->size = (qint64(fd.nFileSizeHigh) << 32) | qint64(fd.nFileSizeLow);
            ++files;
            bytes += child->size;
        }
        node->children.push_back(std::move(child));
    } while (!m_cancel.load() && FindNextFileW(h, &fd));
    FindClose(h);

    m_files += files;
    m_bytes += bytes;
    m_dirs += dirs;

    if ((m_tick.fetch_add(1) & 31u) == 0) {
        QMutexLocker lock(&m_mutex);
        m_currentPath = path;
    }

    for (auto& [child, p] : subdirs) {
        FileNode* c = child;
        QString cp = p;
        pool->start([this, c, cp, pool] { scanDirectory(c, cp, pool); });
    }
}
