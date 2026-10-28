#include "MainWindow.h"
#include "Scanner.h"
#include "Splash.h"
#include "Theme.h"
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QIcon>
#include <QTextStream>

// Headless scan used by CI: DrDirectory --selftest <folder> <report-file>
static int runSelfTest(const QString& folder, const QString& reportPath)
{
    Scanner scanner;
    QEventLoop loop;
    bool cancelled = true;
    QObject::connect(&scanner, &Scanner::finished, &loop, [&](bool c) {
        cancelled = c;
        loop.quit();
    });
    scanner.start({folder}, 4);
    loop.exec();
    const auto root = scanner.takeResult();
    QFile out(reportPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return 2;
    QTextStream ts(&out);
    if (cancelled || !root) {
        ts << "error=scan failed\n";
        return 1;
    }
    ts << "version=" << DRD_VERSION << '\n'
       << "path=" << root->name << '\n'
       << "files=" << root->fileCount << '\n'
       << "dirs=" << root->dirCount << '\n'
       << "bytes=" << root->size << '\n';
    return 0;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Tiny Moosh Games"));
    QApplication::setApplicationName(QStringLiteral("DrDirectory"));
    QApplication::setApplicationVersion(QStringLiteral(DRD_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("drdirectory"));

    const QStringList args = app.arguments();
    if (args.size() >= 4 && args.at(1) == QLatin1String("--selftest"))
        return runSelfTest(args.at(2), args.at(3));

    Theme::apply(app);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/DrDirectory.ico")));

    // Plain arguments are folders to scan. Developer flags (used for the README screenshots):
    //   --capture <png> | --capture-scanning <png>  --size WxH  --zoom <folder>  --highlight <ext>  --tab <n>
    MainWindow::CaptureRequest capture;
    QSize size;
    QStringList paths;
    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args.at(i);
        const QString next = i + 1 < args.size() ? args.at(i + 1) : QString();
        if (a == QLatin1String("--capture") || a == QLatin1String("--capture-scanning")) {
            capture.file = next;
            capture.duringScan = a.endsWith(QLatin1String("scanning"));
            ++i;
        } else if (a == QLatin1String("--size")) {
            const QStringList wh = next.split(QLatin1Char('x'));
            if (wh.size() == 2)
                size = QSize(wh[0].toInt(), wh[1].toInt());
            ++i;
        } else if (a == QLatin1String("--zoom")) {
            capture.zoomPath = next;
            ++i;
        } else if (a == QLatin1String("--highlight")) {
            capture.highlightExt = next;
            ++i;
        } else if (a == QLatin1String("--tab")) {
            capture.tab = next.toInt();
            ++i;
        } else {
            paths << a;
        }
    }

    SplashScreen* splash = nullptr;
    if (capture.file.isEmpty() && !args.contains(QLatin1String("--no-splash"))) {
        splash = new SplashScreen;
        splash->show();
        app.processEvents();
    }
    paths.removeAll(QStringLiteral("--no-splash"));

    MainWindow w;
    w.setCaptureRequest(capture);
    if (size.isValid()) {
        w.showNormal();
        w.resize(size);
    } else {
        w.show();
    }
    if (splash)
        splash->finishAfter(&w, 1800); // click to dismiss sooner
    w.startup(paths, splash ? 2150 : 0);
    return app.exec();
}
