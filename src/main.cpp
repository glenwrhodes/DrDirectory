#include "MainWindow.h"
#include "Theme.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Tiny Moosh Games"));
    QApplication::setApplicationName(QStringLiteral("DrDirectory"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    Theme::apply(app);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/DrDirectory.ico")));

    MainWindow w;
    w.show();
    w.startup(app.arguments().mid(1));
    return app.exec();
}
