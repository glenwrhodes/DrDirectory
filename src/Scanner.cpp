#include "Scanner.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStorageInfo>
#include <QThreadPool>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(Q_OS_LINUX)
#include <sys/vfs.h>
#else
#include <sys/mount.h>
#include <sys/param.h>
#endif
#endif

namespace {

QString joinPath(const QString& dir, const QString& name)
{
    const QChar sep = QDir::separator();
    if (dir.endsWith(sep))
        return dir + name;
    return dir + sep + name;
}

#ifdef Q_OS_WIN
QString longPath(const QString& path)
{
    if (path.startsWith(QLatin1String("\\\\?\\")))
        return path;
    if (path.startsWith(QLatin1String("\\\\")))
        return QStringLiteral("\\\\?\\UNC\\") + path.mid(2);
    return QStringLiteral("\\\\?\\") + path;
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
#else
// Pseudo / virtual filesystems that should never be counted as disk usage.
bool isPseudoFilesystem(const char* path)
{
    struct statfs sf;
    if (statfs(path, &sf) != 0)
        return false;
#if defined(Q_OS_LINUX)
    switch (static_cast<unsigned long>(sf.f_type) & 0xFFFFFFFFul) {
    case 0x9fa0ul:      // proc
    case 0x62656572ul:  // sysfs
    case 0x01021994ul:  // tmpfs / devtmpfs
    case 0x1cd1ul:      // devpts
    case 0x27e0ebul:    // cgroup
    case 0x63677270ul:  // cgroup2
    case 0x64626720ul:  // debugfs
    case 0x73636673ul:  // securityfs
    case 0x6165676cul:  // pstore
    case 0xcafe4a11ul:  // bpf
    case 0x74726163ul:  // tracefs
    case 0xde5e81e4ul:  // efivarfs
    case 0x62656570ul:  // configfs
    case 0x65735543ul:  // fusectl
    case 0x958458f6ul:  // hugetlbfs
    case 0x19800202ul:  // mqueue
    case 0x42494e4dul:  // binfmt_misc
    case 0x0187ul:      // autofs
    case 0x6e736673ul:  // nsfs
    case 0x73717368ul:  // squashfs (snaps)
    case 0x794c7630ul:  // overlayfs (containers)
    case 0x858458f6ul:  // ramfs
        return true;
    default:
        return false;
    }
#else
    return std::strcmp(sf.f_fstypename, "devfs") == 0 || std::strcmp(sf.f_fstypename, "autofs") == 0
        || std::strcmp(sf.f_fstypename, "nullfs") == 0;
#endif
}

qint64 mtimeMs(const struct stat& st)
{
#if defined(Q_OS_MACOS)
    return qint64(st.st_mtimespec.tv_sec) * 1000 + st.st_mtimespec.tv_nsec / 1000000;
#else
    return qint64(st.st_mtim.tv_sec) * 1000 + st.st_mtim.tv_nsec / 1000000;
#endif
}
#endif

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

QString Scanner::normalizePath(const QString& path)
{
#ifdef Q_OS_WIN
    QString p = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (p.size() == 2 && p[1] == QLatin1Char(':'))
        p += QLatin1Char('\\');
    if (p.size() == 3 && p[1] == QLatin1Char(':'))
        p[0] = p[0].toUpper();
    return p;
#else
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#endif
}

bool Scanner::isVolumeRoot(const QString& path)
{
    const QString p = normalizePath(path);
#ifdef Q_OS_WIN
    return p.size() == 3 && p[1] == QLatin1Char(':') && p[2] == QLatin1Char('\\');
#else
    const QStorageInfo si(p);
    return si.isValid() && QDir::cleanPath(si.rootPath()) == p;
#endif
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
    n->kind = isVolumeRoot(n->name) ? FileNode::Drive : FileNode::Dir;
#ifdef Q_OS_WIN
    WIN32_FILE_ATTRIBUTE_DATA ad{};
    const QString lp = longPath(n->name);
    if (GetFileAttributesExW(wstr(lp), GetFileExInfoStandard, &ad)) {
        n->attributes = ad.dwFileAttributes;
        n->lastWrite = fileTimeToMs(ad.ftLastWriteTime);
    }
#else
    struct stat st{};
    if (stat(QFile::encodeName(n->name).constData(), &st) == 0) {
        n->attributes = quint32(st.st_mode);
        n->lastWrite = mtimeMs(st);
    }
#endif
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

    m_skipPaths.clear();
#ifdef Q_OS_MACOS
    // On APFS the data volume is firmlinked into "/" and also mounted under /System/Volumes;
    // skipping the latter avoids counting user data twice.
    m_skipPaths.insert(QStringLiteral("/System/Volumes"));
#endif

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
#ifdef Q_OS_WIN
        root->name = tr("This PC");
#else
        root->name = tr("Computer");
#endif
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

#ifdef Q_OS_WIN

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

#else // POSIX (macOS, Linux)

void Scanner::scanDirectory(FileNode* node, const QString& path, QThreadPool* pool)
{
    if (m_cancel.load())
        return;

    const QByteArray encoded = QFile::encodeName(path);
    const int dfd = open(encoded.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        if (errno != ENOENT)
            node->accessDenied = true;
        return;
    }
    struct stat dirStat{};
    fstat(dfd, &dirStat);
    DIR* dir = fdopendir(dfd);
    if (!dir) {
        close(dfd);
        node->accessDenied = true;
        return;
    }

    std::vector<std::pair<FileNode*, QString>> subdirs;
    qint64 files = 0, bytes = 0, dirs = 0;
    while (!m_cancel.load()) {
        struct dirent* ent = readdir(dir);
        if (!ent)
            break;
        const char* fn = ent->d_name;
        if (fn[0] == '.' && (fn[1] == 0 || (fn[1] == '.' && fn[2] == 0)))
            continue;
        struct stat st{};
        if (fstatat(dfd, fn, &st, AT_SYMLINK_NOFOLLOW) != 0)
            continue;
        auto child = std::make_unique<FileNode>();
        child->name = QFile::decodeName(fn);
        child->parent = node;
        child->attributes = quint32(st.st_mode);
        child->lastWrite = mtimeMs(st);
        if (S_ISDIR(st.st_mode)) {
            child->kind = FileNode::Dir;
            ++dirs;
            const QString childPath = joinPath(path, child->name);
            bool follow = !m_skipPaths.contains(childPath);
            if (follow && st.st_dev != dirStat.st_dev) // mount point: skip /proc, /sys, snaps, etc.
                follow = !isPseudoFilesystem(QFile::encodeName(childPath).constData());
            if (follow)
                subdirs.emplace_back(child.get(), childPath);
            else
                child->reparsePoint = true;
        } else {
            child->kind = FileNode::File;
            if (S_ISLNK(st.st_mode)) {
                child->reparsePoint = true; // symlinks are never followed and count as 0 bytes
            } else if (S_ISREG(st.st_mode)) {
                child->size = qint64(st.st_size);
                bytes += child->size;
            }
            ++files;
        }
        node->children.push_back(std::move(child));
    }
    closedir(dir);

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

#endif
