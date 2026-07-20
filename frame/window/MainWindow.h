//
// Created by septemberhx on 2020/5/23.
//

#ifndef DDE_TOP_PANEL_MAINWINDOW_H
#define DDE_TOP_PANEL_MAINWINDOW_H

#include <DBlurEffectWidget>
#include <DPlatformWindowHandle>
#include "../panel/MainPanelControl.h"
#include "util/TopPanelSettings.h"
#include "xcb/xcb_misc.h"
#include "dbus/sni/statusnotifierwatcher_interface.h"
#include "util/CustomSettings.h"
#include "dockinterface.h"
#include "../widgets/mainsettingwidget.h"

namespace LayerShellQt {
class Window;
}

DWIDGET_USE_NAMESPACE

using namespace Dock;
using DBusDock = DockInterface;

class MainWindow : public DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QScreen *screen, bool enableBlacklist, QWidget *parent = 0);
    ~MainWindow() override;

    void loadPlugins();
    void moveToScreen(QScreen *screen);
    void setRaidus(int radius);
    void adjustPanelSize();
    void applyCustomSettings(const CustomSettings& customSettings);

    void adjustPosition();
signals:
    void panelGeometryChanged();
    void settingActionClicked();

private slots:
    void onDbusNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void mousePressEvent(QMouseEvent *e);
    void resizeEvent(QResizeEvent *e) override;
    void resizeMainPanelWindow();
    void clearStrutPartial();
    void setStrutPartial();
    void initLayerShell(QScreen *screen);
    void updateLayerShellExclusiveZone();
    void initConnections();
    void initSNIHost();

private:
    DockItemManager *m_itemManager;
    DBusDock *m_dockInter;
    MainPanelControl *m_mainPanel;
    TopPanelSettings *m_settings;
    XcbMisc *m_xcbMisc;
    bool m_isWayland;
    LayerShellQt::Window *m_layerShell;
    Position m_curDockPos;
    DPlatformWindowHandle m_platformWindowHandle;
    QVBoxLayout *m_layout;
    QDBusConnectionInterface *m_dbusDaemonInterface;
    org::kde::StatusNotifierWatcher *m_sniWatcher;
    QString m_sniHostService;
};

class TopPanelLauncher : public QObject {
    Q_OBJECT

public:
    explicit TopPanelLauncher();

private slots:
    void monitorsChanged();
    void primaryChanged();
    void onScreenRemoved(QScreen *screen);

private:
    MainSettingWidget *m_settingWidget;

    QScreen *primaryScreen;
    DBusDisplay *m_display;
    QMap<QScreen *, MainWindow *> mwMap;
    QTimer *m_rearrangeTimer;
    bool m_isWayland;
    void rearrange();
    MainWindow *createPanel(QScreen *screen);
};


#endif //DDE_TOP_PANEL_MAINWINDOW_H
