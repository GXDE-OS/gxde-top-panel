//
// Created by septemberhx on 2020/5/22.
//

#include <DApplication>
#include <DGuiApplicationHelper>
#include <QDBusMetaType>
#include <QMap>
#include <QMargins>
#include <QEvent>
#include <QWindow>
#include <unistd.h>
#include <iostream>
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
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

    const bool waylandSession = !qgetenv("WAYLAND_DISPLAY").isEmpty();

    // If a single Wayland display is detected, then it IS wayland.
    // Under wayland, we enforce QT_QPA_PLATFORM to be wayland, otherwise
    // laytershell will MALFUNCTION!!
    if (waylandSession) {
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
        // qputenv("QT_QPA_PLATFORM", "xcb");
        qputenv("QT_QPA_PLATFORM", "dxcb"); // 修复在 x11 下特效丢失的问题
    }

    DApplication app(argc, argv);

    if (waylandSession && !settingsMode) {
        class PopupLayerShellPatcher : public QObject {
        public:
            using QObject::QObject;

        protected:
            bool eventFilter(QObject *object, QEvent *event) override {
                QWidget *target = qobject_cast<QWidget *>(object);
                if (target && target->windowType() == Qt::Popup
                        && event->type() == QEvent::Show) {
                    fixPopupLayerShell(target);
                }
                return QObject::eventFilter(object, event);
            }

        private:
            static void fixPopupLayerShell(QWidget *popup) {
                popup->createWinId();
                QWindow *window = popup->windowHandle();
                if (!window) {
                    return;
                }

                LayerShellQt::Window *layer = LayerShellQt::Window::get(window);
                if (!layer) {
                    return;
                }

                const QPoint pos = popup->pos();
                LayerShellQt::Window::Anchors anchors;
                anchors |= LayerShellQt::Window::AnchorTop;
                anchors |= LayerShellQt::Window::AnchorLeft;
                layer->setAnchors(anchors);
                layer->setMargins(QMargins(pos.x(), pos.y(), 0, 0));
                layer->setLayer(LayerShellQt::Window::LayerOverlay);
                layer->setExclusiveZone(0);

                const bool acceptsKeyboard = window->transientParent() == nullptr;
                layer->setKeyboardInteractivity(acceptsKeyboard
                    ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                    : LayerShellQt::Window::KeyboardInteractivityNone);
            }
        };

        app.installEventFilter(new PopupLayerShellPatcher(&app));
    }

    QString locale = QLocale::system().name();

    QTranslator translator;
    const QString translationName = "gxde-top-panel_" + locale + ".qm";
    if (!translator.load(translationName, app.applicationDirPath() + "/../translations")) {
        translator.load(translationName, "/usr/share/gxde-top-panel/translations");
    }
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

    // Apply the saved application theme before creating any plugin widgets.
    CustomSettings::instance();

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
