//
// Created by septemberhx on 2020/5/23.
//

#include <xcb/xcb_misc.h>
#include "MainWindow.h"
#include "controller/dockitemmanager.h"
#include "util/utils.h"
#include <DGuiApplicationHelper>
#include <iostream>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <LayerShellQt/Window>

DGUI_USE_NAMESPACE

#define SNI_WATCHER_SERVICE "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_PATH "/StatusNotifierWatcher"

MainWindow::MainWindow(QScreen *screen, bool enableBlacklist, QWidget *parent)
    : DBlurEffectWidget(parent)
    , m_itemManager(new DockItemManager(this))
    , m_dockInter(new DBusDock(this))
    , m_mainPanel(new MainPanelControl(this))
    , m_xcbMisc(XcbMisc::instance())
    , m_isWayland(Utils::isWayland())
    , m_layerShell(nullptr)
    , m_platformWindowHandle(this, this)
    , m_layout(new QVBoxLayout(this))
    , m_dbusDaemonInterface(QDBusConnection::sessionBus().interface())
    , m_sniWatcher(new org::kde::StatusNotifierWatcher(SNI_WATCHER_SERVICE, SNI_WATCHER_PATH, QDBusConnection::sessionBus(), this))
{
    setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    setAccessibleName("dock-top-panel-mainwindow");
    m_mainPanel->setAccessibleName("dock-top-panel-mainpanel");
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setAcceptDrops(true);

    this->setLayout(this->m_layout);
    this->m_layout->addWidget(m_mainPanel);
    this->setFixedHeight(CustomSettings::instance()->getPanelHeight());
    this->m_layout->setContentsMargins(0, 0, 0, 0);
    this->m_layout->setSpacing(0);



    m_settings = new TopPanelSettings(m_itemManager, screen, this);
    if (m_isWayland) {
        initLayerShell(screen);
    } else {
        m_xcbMisc->set_window_type(winId(), XcbMisc::Dock);
    }
    m_mainPanel->setDisplayMode(m_settings->displayMode());
    m_mainPanel->move(0, 0);

    this->initSNIHost();
    this->initConnections();
    this->resizeMainPanelWindow();

    for (auto item : m_itemManager->itemList())
        m_mainPanel->insertItem(-1, item);

    m_curDockPos = m_settings->position();
    setStrutPartial();

//    this->windowHandle()->setScreen(screen);
    this->adjustPosition();

    setVisible(true);
    if (!m_isWayland) {
        // platformwindowhandle only works when the widget is visible...
        m_platformWindowHandle.setEnableBlurWindow(true);
        m_platformWindowHandle.setTranslucentBackground(true);
        m_platformWindowHandle.setWindowRadius(0);  // have no idea why it doesn't work :(
        m_platformWindowHandle.setShadowOffset(QPoint(0, 5));
        m_platformWindowHandle.setShadowColor(QColor(0, 0, 0, 0.3 * 255));
        m_platformWindowHandle.setBorderWidth(1);
    } else {
        setBlurEnabled(true);
    }


    qreal value = CustomSettings::instance()->getPanelOpacity();
    CustomSettings::instance()->setPanelOpacity(value);

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this] () {
        this->applyCustomSettings(*CustomSettings::instance());
    });
}

void MainWindow::resizeMainPanelWindow()
{
    m_settings->calculateWindowConfig();
    if (m_isWayland) {
        setFixedHeight(m_settings->m_mainWindowSize.height());
        m_mainPanel->setFixedSize(size());
        updateLayerShellExclusiveZone();
        return;
    }
    m_mainPanel->setFixedSize(m_settings->m_mainWindowSize);
    std::cout << "+++++++++ " << m_settings->m_frontendRect.topLeft().x() << std::endl;
    this->adjustPosition();
//    switch (m_curDockPos) {
//        case Dock::Top:
//            m_dragWidget->setGeometry(0, height() - DRAG_AREA_SIZE, width(), DRAG_AREA_SIZE);
//            break;
//        case Dock::Bottom:
//            m_dragWidget->setGeometry(0, 0, width(), DRAG_AREA_SIZE);
//            break;
//        case Dock::Left:
//            m_dragWidget->setGeometry(width() - DRAG_AREA_SIZE, 0, DRAG_AREA_SIZE, height());
//            break;
//        case Dock::Right:
//            m_dragWidget->setGeometry(0, 0, DRAG_AREA_SIZE, height());
//            break;
//        default: break;
//    }
}

MainWindow::~MainWindow() {
    // m_xcbMisc 是全局单例，多屏下会被所有顶栏共享，不能在这里释放
}

const QPoint rawXPosition(const QPoint &scaledPos)
{
    QScreen const *screen = Utils::screenAtByScaled(scaledPos);

    return screen ? screen->geometry().topLeft() +
                    (scaledPos - screen->geometry().topLeft()) *
                    screen->devicePixelRatio()
                  : scaledPos;
}

void MainWindow::initLayerShell(QScreen* screen) {
    winId();
    QWindow* win = windowHandle();
    if (!win) {
        qWarning() << "(Wayland) Init: Failed to get window handles!!";
        return;
    }

    if (screen) {
        win->setScreen(screen);
    }

    m_layerShell = LayerShellQt::Window::get(win);
    m_layerShell->setScope(QStringLiteral("dock"));
    m_layerShell->setLayer(LayerShellQt::Window::LayerTop);
    m_layerShell->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    m_layerShell->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    m_layerShell->setScreenConfiguration(
        LayerShellQt::Window::ScreenFromQWindow);
    m_layerShell->setCloseOnDismissed(false);
    updateLayerShellExclusiveZone();
}

void MainWindow::updateLayerShellExclusiveZone() {
    if (!m_layerShell) {
        return;
    }

    m_layerShell->setExclusiveZone(height() + m_settings->dockMargin());
}

void MainWindow::clearStrutPartial() {
    if (m_isWayland)
        return;

    m_xcbMisc->clear_strut_partial(winId());
}

void MainWindow::setStrutPartial() {
    // Exclusize zone handles it under wayland
    if (m_isWayland) {
        updateLayerShellExclusiveZone();
        return;
    }

    // first, clear old strut partial
    clearStrutPartial();

    // reset env
    //resetPanelEnvironment(true);

    const auto ratio = devicePixelRatioF();
    const int maxScreenHeight = m_settings->screenRawHeight();
    const int maxScreenWidth = m_settings->screenRawWidth();
    const Position side = m_curDockPos;
    const QPoint &p = rawXPosition(m_settings->windowRect(Top, false).topLeft());
    const QSize &s = m_settings->windowSize();
    const QRect &primaryRawRect = m_settings->primaryRawRect();

    XcbMisc::Orientation orientation = XcbMisc::OrientationTop;
    uint strut = 0;
    uint strutTop = 0;
    uint strutStart = 0;
    uint strutEnd = 0;

    QRect strutArea(0, 0, maxScreenWidth, maxScreenHeight);
    switch (side) {
        case Position::Top:
            orientation = XcbMisc::OrientationTop;
            strut = p.y() + s.height() * ratio;
            strutTop = p.y();
            strutStart = p.x();
            strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
            strutArea.setTop(strutTop);
            strutArea.setLeft(strutStart);
            strutArea.setRight(strutEnd);
            strutArea.setBottom(strut);
            break;
        case Position::Bottom:
            orientation = XcbMisc::OrientationBottom;
            strut = maxScreenHeight - p.y();
            strutStart = p.x();
            strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
            strutArea.setLeft(strutStart);
            strutArea.setRight(strutEnd);
            strutArea.setTop(p.y());
            break;
        case Position::Left:
            orientation = XcbMisc::OrientationLeft;
            strut = p.x() + s.width() * ratio;
            strutStart = p.y();
            strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
            strutArea.setTop(strutStart);
            strutArea.setBottom(strutEnd);
            strutArea.setRight(strut);
            break;
        case Position::Right:
            orientation = XcbMisc::OrientationRight;
            strut = maxScreenWidth - p.x();
            strutStart = p.y();
            strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
            strutArea.setTop(strutStart);
            strutArea.setBottom(strutEnd);
            strutArea.setLeft(p.x());
            break;
        default:
            Q_ASSERT(false);
    }

    // pass if strut area is intersect with other screen
    int count = 0;
    const QRect pr = m_settings->primaryRect();
    for (auto *screen : qApp->screens()) {
        const QRect sr = screen->geometry();
        if (sr == pr)
            continue;

        if (sr.intersects(strutArea))
            ++count;
    }
    if (count > 0) {
        qWarning() << "strutArea is intersects with another screen.";
        qWarning() << maxScreenHeight << maxScreenWidth << side << p << s;
        return;
    }
    m_xcbMisc->set_strut_partial(winId(), orientation, strut + m_settings->dockMargin() * ratio, strutStart, strutEnd);
}

void MainWindow::initConnections() {
//    connect(m_settings, &DockSettings::windowHideModeChanged, this, &MainWindow::setStrutPartial, Qt::QueuedConnection);
    connect(m_itemManager, &DockItemManager::itemInserted, m_mainPanel, &MainPanelControl::insertItem, Qt::DirectConnection);
    connect(m_itemManager, &DockItemManager::itemUpdated, m_mainPanel, &MainPanelControl::itemUpdated, Qt::DirectConnection);
    connect(m_itemManager, &DockItemManager::itemRemoved, m_mainPanel, &MainPanelControl::removeItem, Qt::DirectConnection);

    connect(m_mainPanel, &MainPanelControl::itemMoved, DockItemManager::instance(), &DockItemManager::itemMoved, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemAdded, DockItemManager::instance(), &DockItemManager::itemAdded, Qt::DirectConnection);

    // 因为 DDE15 与 DDE20 的接口有差异，
    // DDE15 需要通过 FrontendWindowRectChanged 而不是 PositionChanged 检测任务栏的位置和大小变化
    connect(this->m_dockInter, &DBusDock::FrontendWindowRectChanged, this, &MainWindow::resizeMainPanelWindow);
    connect(this->m_dockInter, &DBusDock::PositionChanged, this, &MainWindow::resizeMainPanelWindow);
    connect(this->m_dockInter, &DBusDock::DisplayModeChanged, this, &MainWindow::resizeMainPanelWindow);
    connect(this->m_dockInter, &DBusDock::HideModeChanged, this, &MainWindow::resizeMainPanelWindow);
    connect(this->m_dockInter, &DBusDock::WindowSizeChanged, this, &MainWindow::resizeMainPanelWindow);

    connect(m_dbusDaemonInterface, &QDBusConnectionInterface::serviceOwnerChanged, this, &MainWindow::onDbusNameOwnerChanged);
    connect(m_settings, &TopPanelSettings::settingActionClicked, this, &MainWindow::settingActionClicked);
    connect(CustomSettings::instance(), &CustomSettings::settingsChanged, this, [this]() {
        this->applyCustomSettings(*CustomSettings::instance());
    });
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    e->ignore();
    if (e->button() == Qt::RightButton) {
        m_settings->showDockSettingsMenu();
        return;
    }
}

void MainWindow::loadPlugins() {
    this->m_itemManager->startLoadPlugins();
}

void MainWindow::moveToScreen(QScreen *screen) {
    m_settings->moveToScreen(screen);

    if (m_isWayland) {
        if (QWindow *win = windowHandle()) {
            win->setScreen(screen);
        }
        updateLayerShellExclusiveZone();
        return;
    }

    this->resize(m_settings->m_mainWindowSize);
    m_mainPanel->resize(m_settings->m_mainWindowSize);
    m_mainPanel->adjustSize();
    QThread::msleep(100);  // sleep for a short while to make sure the movement is successful
    this->adjustPosition();
    this->setStrutPartial();
}

void MainWindow::setRaidus(int radius) {
    if (m_isWayland) {
        return;
    }

    m_platformWindowHandle.setWindowRadius(radius);  // have no idea why it doesn't work :(
}

void MainWindow::adjustPanelSize() {
    this->m_mainPanel->adjustSize();
}

void MainWindow::onDbusNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner) {
    Q_UNUSED(oldOwner)

    if (name == SNI_WATCHER_SERVICE && !newOwner.isEmpty()) {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon started, register dock to watcher as SNI Host";
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    }
}

void MainWindow::initSNIHost()
{
    // registor dock as SNI Host on dbus
    QDBusConnection dbusConn = QDBusConnection::sessionBus();
    m_sniHostService = QString("org.kde.StatusNotifierHost-") + QString::number(qApp->applicationPid());
    dbusConn.registerService(m_sniHostService);
    dbusConn.registerObject("/StatusNotifierHost", this);

    if (m_sniWatcher->isValid()) {
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    } else {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon is not exist for now!";
    }
}

void MainWindow::applyCustomSettings(const CustomSettings &customSettings) {
    this->setMaskAlpha(customSettings.getPanelOpacity());
    this->setMaskColor(customSettings.getPanelBgColor());
    this->m_settings->applyCustomSettings(customSettings);
    this->m_mainPanel->applyCustomSettings(customSettings);
}

void MainWindow::adjustPosition() {
    // Under wayland, windows are NOT able to move themselves, the ositioning
    // is handled by layer-shell.
    if (m_isWayland) {
        return;
    }

    std::cout << "++++++++++ " << m_settings->m_frontendRect.topLeft().x() << std::endl;
    this->move(m_settings->m_frontendRect.topLeft() / m_settings->m_screen->devicePixelRatio());
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    DBlurEffectWidget::resizeEvent(e);

    if (m_isWayland) {
        m_mainPanel->setFixedSize(size());
        updateLayerShellExclusiveZone();
    }
}

TopPanelLauncher::TopPanelLauncher()
        : m_display(new DBusDisplay(this))
        , m_rearrangeTimer(new QTimer(this))
        , m_isWayland(Utils::isWayland()) {
    this->m_settingWidget = new MainSettingWidget();

    m_rearrangeTimer->setSingleShot(true);
    m_rearrangeTimer->setInterval(100);
    connect(m_rearrangeTimer, &QTimer::timeout, this,
        &TopPanelLauncher::rearrange);

    // Seems that Wayland lacks deepin-daemon's Display service.
    // We relys on Qt's screen signals instead.
    // For X11 we use both depending who arrives first.
    // TODO(CharOfString): update this once gxde-daemon has display service.
    connect(qApp, &QGuiApplication::screenAdded, this,
        &TopPanelLauncher::monitorsChanged);
    connect(qApp, &QGuiApplication::screenRemoved, this,
        &TopPanelLauncher::onScreenRemoved);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this,
        &TopPanelLauncher::monitorsChanged);
    if (!m_isWayland) {
        connect(m_display, &DBusDisplay::MonitorsChanged, this,
            &TopPanelLauncher::monitorsChanged);
    }

    this->rearrange();
}

void TopPanelLauncher::monitorsChanged() {
    m_rearrangeTimer->start();
}

void TopPanelLauncher::onScreenRemoved(QScreen *screen) {
    if (MainWindow *mw = mwMap.take(screen)) {
        qDebug() << "(Panel) Screen: Panel closed due to closed QScreen"
            << screen->name();
        mw->close();
        mw->deleteLater();
    }

    if (primaryScreen == screen) {
        primaryScreen = nullptr;
    }
    m_rearrangeTimer->start();
}

MainWindow* TopPanelLauncher::createPanel(QScreen* screen) {
    MainWindow* mw = new MainWindow(screen, screen != qApp->primaryScreen());
    connect(mw, &MainWindow::settingActionClicked, this, [this, mw]() {
        QScreen* screen = mw->screen();
        if (screen) {
            this->m_settingWidget->move(screen->geometry().topLeft());
        }
        this->m_settingWidget->show();
    });
    return mw;
}

void TopPanelLauncher::rearrange() {
    this->primaryChanged();

    // Under Wayland + dual screen + screen copy mode, each screen copy needs
    // an individual bar.
    bool ifCopyScreenMode = false;
    if (!m_isWayland) {
        for (auto p_screen : qApp->screens()) {
            if (p_screen != qApp->primaryScreen() && p_screen->geometry() == qApp->primaryScreen()->geometry()) {
                ifCopyScreenMode = true;
                break;
            }
        }
    }
    std::cout << "==============> ifCopyMode:" << ifCopyScreenMode << std::endl;

    const QList<QScreen *> targetScreens = ifCopyScreenMode
            ? QList<QScreen *>{ qApp->primaryScreen() }
            : qApp->screens();

    for (auto p_screen : targetScreens) {
        if (!p_screen)
            continue;

        if (mwMap.contains(p_screen)) {
            // adjust size
            std::cout << "========> Panel exists, resizing..." << std::endl;
            MainWindow *mw = mwMap[p_screen];
            if (m_isWayland) {
                mw->moveToScreen(p_screen);
            } else {
                mw->hide();
                mw->moveToScreen(p_screen);
                mw->show();
                mw->setRaidus(0);
            }
            continue;
        }

        MainWindow *mw = createPanel(p_screen);
        if (!m_isWayland) {
            mw->move(p_screen->geometry().topLeft());
            mw->adjustPosition();
        }

        if (p_screen == qApp->primaryScreen()) {
            mw->loadPlugins();
        }
        mwMap.insert(p_screen, mw);
    }

    for (auto screen : mwMap.keys()) {
        if (targetScreens.contains(screen))
            continue;

        MainWindow *mw = mwMap.take(screen);
        qDebug() << "(Panel) Screen: Closing panel due to closed QScreen"
            << screen->name();
        mw->close();
        mw->deleteLater();
    }
}

void TopPanelLauncher::primaryChanged() {
    QScreen *currPrimaryScreen = qApp->primaryScreen();
    if (currPrimaryScreen == primaryScreen) return;
    if (primaryScreen == nullptr) {
        primaryScreen = currPrimaryScreen;
        return;
    }

    // prevent raw primary screen is destroyed (unplugged)
    bool ifRawPrimaryExists = qApp->screens().contains(primaryScreen);
    bool ifCurrPrimaryExisted = mwMap.contains(currPrimaryScreen);
    bool ifRawPrimaryExisted = mwMap.contains(primaryScreen);

    MainWindow *pMw = nullptr;
    if (ifRawPrimaryExisted) {
        pMw = mwMap[primaryScreen];
        pMw->hide();
    }

    if (ifCurrPrimaryExisted) {
        mwMap[currPrimaryScreen]->hide();
        if (ifRawPrimaryExists) {
            mwMap[currPrimaryScreen]->moveToScreen(primaryScreen);
            mwMap[currPrimaryScreen]->move(primaryScreen->geometry().topLeft());
            mwMap[currPrimaryScreen]->adjustPosition();
            mwMap[primaryScreen] = mwMap[currPrimaryScreen];
        } else {
            mwMap[currPrimaryScreen]->close();
            mwMap.remove(primaryScreen);
        }
    }

    if (ifRawPrimaryExisted) {
        pMw->moveToScreen(currPrimaryScreen);
        if (mwMap.contains(currPrimaryScreen)) {
            mwMap[currPrimaryScreen] = pMw;
        } else {
            mwMap.insert(currPrimaryScreen, pMw);
        }
        pMw->show();
        pMw->setRaidus(0);

        if (ifRawPrimaryExists) {
            mwMap[primaryScreen]->show();
            mwMap[primaryScreen]->setRaidus(0);
        } else {
            mwMap.remove(primaryScreen);
        }

        if (!ifCurrPrimaryExisted) {
            mwMap.remove(primaryScreen);
        }
    }

    this->primaryScreen = currPrimaryScreen;
}
