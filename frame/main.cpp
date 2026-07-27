//
// Created by septemberhx on 2020/5/22.
//

#include <DApplication>
#include <DGuiApplicationHelper>
#include <QDBusMetaType>
#include <QMap>
#include <unistd.h>
#include <iostream>
#include <LayerShellQt/Shell>
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

    bool settingsMode = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--settings") == 0) {
            settingsMode = true;
            break;
        }
    }

    // If a single Wayland display is detected, then it IS wayland.
    // Under wayland, we enforce QT_QPA_PLATFORM to be wayland, otherwise
    // laytershell will MALFUNCTION!!
    if (!qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "wayland");
        // LayerShellQt replaces the shell integration for every top-level
        // window in this process.
        // Make a exception for settings page...
        if (settingsMode) {
            qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
        } else {
            LayerShellQt::Shell::useLayerShell();
        }
    } else {
        // Otherwise DXCB is good to go.
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

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
    QByteArray displayName = qgetenv("WAYLAND_DISPLAY");
    if (displayName.isEmpty()) {
        displayName = qgetenv("DISPLAY");
    }
    const QString instanceKey = QStringLiteral("gxde-top-panel%1_%2_%3")
                                    .arg(settingsMode ? QStringLiteral("-settings") : QString())
                                    .arg(getuid())
                                    .arg(QString::fromLatin1(displayName.toHex()));
    if (!app.setSingleInstance(instanceKey)) {
        return 0;
    }

    if (settingsMode) {
        MainSettingWidget settingsWidget;
        QObject::connect(&app, &DApplication::newInstanceStarted,
                         &settingsWidget, [&settingsWidget] {
            settingsWidget.show();
            settingsWidget.raise();
            settingsWidget.activateWindow();
        });
        settingsWidget.show();
        return app.exec();
    }

    TopPanelLauncher launcher;

    return app.exec();
}
