#include "MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QLocale>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("匠心图文"));
    QCoreApplication::setApplicationName(QStringLiteral("匠心矢量设计"));
    QCoreApplication::setApplicationVersion(QStringLiteral(JX_APP_VERSION));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    MainWindow window;
    window.show();
    return app.exec();
}

