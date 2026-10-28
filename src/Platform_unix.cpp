#include "Platform.h"
#include "Format.h"
#include "Icons.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>
#include <sys/stat.h>

namespace Platform {

QString typeNameForExtension(const QString& ext)
{
    static QHash<QString, QString> cache;
    auto it = cache.constFind(ext);
    if (it != cache.cend())
        return it.value();
    QString name;
    if (ext.isEmpty()) {
        name = QStringLiteral("File");
    } else {
        static QMimeDatabase db;
        const QMimeType m = db.mimeTypeForFile(QStringLiteral("file.") + ext, QMimeDatabase::MatchExtension);
        name = (m.isValid() && !m.isDefault()) ? m.comment() : ext.toUpper() + QStringLiteral(" File");
        if (!name.isEmpty())
            name[0] = name[0].toUpper();
    }
    cache.insert(ext, name);
    return name;
}

QIcon iconForExtension(const QString& ext)
{
    static QHash<QString, QIcon> cache;
    auto it = cache.constFind(ext);
    if (it != cache.cend())
        return it.value();
    QIcon icon;
    if (!ext.isEmpty()) {
        static QMimeDatabase db;
        const QMimeType m = db.mimeTypeForFile(QStringLiteral("file.") + ext, QMimeDatabase::MatchExtension);
        if (m.isValid() && !m.isDefault()) {
            icon = QIcon::fromTheme(m.iconName());
            if (icon.isNull())
                icon = QIcon::fromTheme(m.genericIconName());
        }
    }
    if (icon.isNull())
        icon = Icons::tinted(Icons::File, QColor(140, 160, 182));
    cache.insert(ext, icon);
    return icon;
}

QString attributesString(quint32 mode)
{
    QString s(9, QLatin1Char('-'));
    const quint32 bits[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    const char chars[] = "rwxrwxrwx";
    for (int i = 0; i < 9; ++i)
        if (mode & bits[i])
            s[i] = QLatin1Char(chars[i]);
    return s;
}

QString fileManagerName()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("Finder");
#else
    return QStringLiteral("File Manager");
#endif
}

QString trashName() { return QStringLiteral("Trash"); }

void openItem(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void showInFileManager(const QString& path)
{
#ifdef Q_OS_MACOS
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#else
    QProcess p;
    p.start(QStringLiteral("dbus-send"),
            {QStringLiteral("--session"), QStringLiteral("--print-reply"),
             QStringLiteral("--dest=org.freedesktop.FileManager1"), QStringLiteral("--type=method_call"),
             QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
             QStringLiteral("array:string:") + QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded),
             QStringLiteral("string:")});
    if (p.waitForFinished(2000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
        return;
    const QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? path : fi.absolutePath()));
#endif
}

void showProperties(QWidget* parent, const QString& path)
{
    const QFileInfo fi(path);
    QString text = QStringLiteral("<b>%1</b><br><br>").arg(fi.fileName().toHtmlEscaped());
    auto row = [&](const QString& k, const QString& v) {
        text += QStringLiteral("<span style='color:#6F849A'>%1</span>&nbsp;&nbsp;%2<br>").arg(k, v.toHtmlEscaped());
    };
    row(QStringLiteral("Location"), fi.absolutePath());
    row(QStringLiteral("Type"), fi.isSymLink() ? QStringLiteral("Symbolic link") : fi.isDir() ? QStringLiteral("Folder") : typeNameForExtension(fi.suffix().toLower()));
    if (!fi.isDir())
        row(QStringLiteral("Size"), Fmt::size(fi.size()) + QStringLiteral(" (") + Fmt::count(fi.size()) + QStringLiteral(" bytes)"));
    row(QStringLiteral("Modified"), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    row(QStringLiteral("Owner"), fi.owner() + QLatin1Char(':') + fi.group());
    struct stat st{};
    if (lstat(QFile::encodeName(path).constData(), &st) == 0)
        row(QStringLiteral("Permissions"), attributesString(st.st_mode));
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("Properties"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.exec();
}

void openTerminal(const QString& dir)
{
#ifdef Q_OS_MACOS
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-a"), QStringLiteral("Terminal"), dir});
#else
    const QList<QPair<QString, QStringList>> candidates = {
        {QStringLiteral("x-terminal-emulator"), {}},
        {QStringLiteral("gnome-terminal"), {QStringLiteral("--working-directory=") + dir}},
        {QStringLiteral("konsole"), {QStringLiteral("--workdir"), dir}},
        {QStringLiteral("xfce4-terminal"), {QStringLiteral("--working-directory"), dir}},
        {QStringLiteral("kitty"), {QStringLiteral("--directory"), dir}},
        {QStringLiteral("alacritty"), {QStringLiteral("--working-directory"), dir}},
        {QStringLiteral("xterm"), {}},
    };
    for (const auto& [exe, args] : candidates) {
        if (!QStandardPaths::findExecutable(exe).isEmpty() && QProcess::startDetached(exe, args, dir))
            return;
    }
#endif
}

bool supportsElevation() { return false; }
bool restartElevated(const QStringList&) { return false; }

QVector<bool> deletePaths(const QStringList& paths, bool toTrash)
{
    QVector<bool> result;
    result.reserve(paths.size());
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (toTrash) {
            QFile::moveToTrash(p);
        } else if (fi.isDir() && !fi.isSymLink()) {
            QDir(p).removeRecursively();
        } else {
            QFile::remove(p);
        }
        const QFileInfo after(p);
        result.push_back(!after.exists() && !after.isSymLink());
    }
    return result;
}

static bool isPseudoFs(const QByteArray& type)
{
    static const QSet<QByteArray> pseudo = {
        "proc", "sysfs", "tmpfs", "devtmpfs", "devpts", "cgroup", "cgroup2", "debugfs", "securityfs",
        "pstore", "bpf", "tracefs", "efivarfs", "configfs", "fusectl", "hugetlbfs", "mqueue", "binfmt_misc",
        "autofs", "nsfs", "squashfs", "overlay", "ramfs", "rpc_pipefs", "devfs", "nullfs", "fuse.portal",
        "fuse.gvfsd-fuse", "fuse.snapfuse", "tracefs"};
    return pseudo.contains(type);
}

QList<QStorageInfo> scannableVolumes()
{
    QList<QStorageInfo> out;
    for (const QStorageInfo& s : QStorageInfo::mountedVolumes()) {
        if (!s.isValid() || !s.isReady() || s.bytesTotal() <= 0)
            continue;
        if (isPseudoFs(s.fileSystemType()))
            continue;
        const QString root = s.rootPath();
        if (root.startsWith(QLatin1String("/System/Volumes/")) || root.startsWith(QLatin1String("/snap/"))
            || root.startsWith(QLatin1String("/boot")) || root.startsWith(QLatin1String("/private/var/vm"))
            || root.startsWith(QLatin1String("/var/lib/")) || root.startsWith(QLatin1String("/run/"))
            || root.startsWith(QLatin1String("/dev")) || root.startsWith(QLatin1String("/sys"))
            || root.startsWith(QLatin1String("/proc")))
            continue;
        out << s;
    }
    return out;
}

bool isLocalVolume(const QStorageInfo& v)
{
    static const QSet<QByteArray> network = {"nfs", "nfs4", "cifs", "smbfs", "smb3", "afpfs", "webdav",
                                             "fuse.sshfs", "9p", "davfs"};
    return !network.contains(v.fileSystemType());
}

} // namespace Platform
