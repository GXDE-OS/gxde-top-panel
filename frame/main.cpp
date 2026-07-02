//
// Created by septemberhx on 2020/5/22.
//

#include <DApplication>
#include <DGuiApplicationHelper>
#include <QDBusMetaType>
#include <QMap>
#include <unistd.h>
#include <iostream>
#include "window/MainWindow.h"

DWIDGET_USE_NAMESPACE
#ifdef DCORE_NAMESPACE
DCORE_USE_NAMESPACE
#else
DUTIL_USE_NAMESPACE
#endif


int main(int argc, char *argv[]) {
    // 修复无限崩溃被拉回
    qDBusRegisterMetaType<QMap<QString, QString>>();

    DApplication app(argc, argv);

    QString locale = QLocale::system().name();

    QTranslator translator;
    translator.load("/usr/share/gxde-top-panel/translations/gxde-top-panel_"+ locale +".qm");
    app.installTranslator(&translator);

    app.setOrganizationName("GXDE");
    app.setApplicationName("gxde-top-panel");
    app.setApplicationDisplayName("GXDE Top Panel");
    app.setApplicationVersion("0.6.9");
    app.loadTranslator();

//    MainWindow mw(qApp->primaryScreen());
//    mw.loadPlugins();
    if (!app.setSingleInstance(QString("gxde-top-panel_%1").arg(getuid()))) {
        qDebug() << "set single instance failed!!!!";
        return -1;
    }


    TopPanelLauncher launcher;

    return app.exec();
}
