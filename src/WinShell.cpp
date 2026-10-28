#include "WinShell.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QUrl>
#include <QWidget>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace {
const wchar_t* wstr(const QString& s) { return reinterpret_cast<const wchar_t*>(s.utf16()); }
} // namespace

namespace WinShell {

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
        const QString fake = QStringLiteral("file.") + ext;
        SHFILEINFOW sfi{};
        if (SHGetFileInfoW(wstr(fake), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                           SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
            name = QString::fromWCharArray(sfi.szTypeName);
        if (name.isEmpty())
            name = ext.toUpper() + QStringLiteral(" File");
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
    const QString fake = ext.isEmpty() ? QStringLiteral("file") : QStringLiteral("file.") + ext;
    SHFILEINFOW sfi{};
    if (SHGetFileInfoW(wstr(fake), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                       SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)
        && sfi.hIcon) {
        const QImage img = QImage::fromHICON(sfi.hIcon);
        DestroyIcon(sfi.hIcon);
        if (!img.isNull())
            icon = QIcon(QPixmap::fromImage(img));
    }
    if (icon.isNull())
        icon = QFileIconProvider().icon(QAbstractFileIconProvider::File);
    cache.insert(ext, icon);
    return icon;
}

QString attributesString(quint32 a)
{
    QString s;
    if (a & FILE_ATTRIBUTE_READONLY) s += QLatin1Char('R');
    if (a & FILE_ATTRIBUTE_HIDDEN) s += QLatin1Char('H');
    if (a & FILE_ATTRIBUTE_SYSTEM) s += QLatin1Char('S');
    if (a & FILE_ATTRIBUTE_ARCHIVE) s += QLatin1Char('A');
    if (a & FILE_ATTRIBUTE_COMPRESSED) s += QLatin1Char('C');
    if (a & FILE_ATTRIBUTE_ENCRYPTED) s += QLatin1Char('E');
    if (a & FILE_ATTRIBUTE_SPARSE_FILE) s += QLatin1Char('P');
    if (a & FILE_ATTRIBUTE_REPARSE_POINT) s += QLatin1Char('L');
    if (a & FILE_ATTRIBUTE_OFFLINE) s += QLatin1Char('O');
    return s;
}

void openItem(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void showInExplorer(const QString& path)
{
    const QString native = QDir::toNativeSeparators(path);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(wstr(native));
    if (pidl) {
        const HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        if (SUCCEEDED(hr))
            return;
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + native});
}

void showProperties(QWidget* parent, const QString& path)
{
    const QString native = QDir::toNativeSeparators(path);
    HWND hwnd = parent ? reinterpret_cast<HWND>(parent->window()->winId()) : nullptr;
    SHObjectProperties(hwnd, SHOP_FILEPATH, wstr(native), nullptr);
}

void openTerminal(const QString& dir)
{
    const QString native = QDir::toNativeSeparators(dir);
    const QString wtArgs = QStringLiteral("-d \"") + native + QStringLiteral("\"");
    HINSTANCE r = ShellExecuteW(nullptr, L"open", L"wt.exe", wstr(wtArgs), wstr(native), SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) > 32)
        return;
    ShellExecuteW(nullptr, L"open", L"cmd.exe", nullptr, wstr(native), SW_SHOWNORMAL);
}

bool isElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation{};
    DWORD len = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &len);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

bool restartElevated(const QStringList& args)
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QStringList quoted;
    for (QString a : args) {
        if (a.endsWith(QLatin1Char('\\')))
            a += QLatin1Char('\\'); // keep trailing backslash from escaping the closing quote
        quoted << QLatin1Char('"') + a + QLatin1Char('"');
    }
    const QString params = quoted.join(QLatin1Char(' '));
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = wstr(exe);
    sei.lpParameters = params.isEmpty() ? nullptr : wstr(params);
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei);
}

static bool shellDelete(const QString& native, bool recycle)
{
    IFileOperation* op = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&op))))
        return false;
    DWORD flags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOFX_NOMINIMIZEBOX;
    if (recycle)
        flags |= FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE | FOF_WANTNUKEWARNING;
    op->SetOperationFlags(flags);
    IShellItem* item = nullptr;
    bool ok = false;
    if (SUCCEEDED(SHCreateItemFromParsingName(wstr(native), nullptr, IID_PPV_ARGS(&item)))) {
        if (SUCCEEDED(op->DeleteItem(item, nullptr)))
            ok = SUCCEEDED(op->PerformOperations());
        item->Release();
    }
    op->Release();
    return ok;
}

static void qtDelete(const QString& path)
{
    QFileInfo fi(path);
    if (fi.isDir() && !fi.isSymLink() && !fi.isJunction()) {
        QDir(path).removeRecursively();
    } else {
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteUser);
        if (!QFile::remove(path) && fi.isDir())
            QDir().rmdir(path);
    }
}

QVector<bool> deletePaths(const QStringList& paths, bool toRecycleBin)
{
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    QVector<bool> result;
    result.reserve(paths.size());
    for (const QString& p : paths) {
        const QString native = QDir::toNativeSeparators(p);
        shellDelete(native, toRecycleBin);
        bool exists = QFileInfo::exists(native) || QFileInfo(native).isSymLink();
        if (exists && !toRecycleBin) {
            qtDelete(native); // fallback for very long paths the shell cannot handle
            exists = QFileInfo::exists(native) || QFileInfo(native).isSymLink();
        }
        result.push_back(!exists);
    }
    if (SUCCEEDED(co))
        CoUninitialize();
    return result;
}

} // namespace WinShell
