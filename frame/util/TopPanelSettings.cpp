//
// Created by septemberhx on 2020/5/23.
//

#include "TopPanelSettings.h"
#include "WaylandMenu.h"
#include "utils.h"
#include <QApplication>
#include <QScreen>
#include <QAction>
#include <QEvent>
#include <DApplication>
#include <iostream>
#include "CustomSettings.h"

#define WINDOW_MAX_SIZE          100

extern const QPoint rawXPosition(const QPoint &scaledPos);


TopPanelSettings::TopPanelSettings(DockItemManager *itemManager, QScreen *screen, QWidget *parent)
        : QObject(parent)
        , m_dockInter(new DBusDock(this))
        , m_dockWindowSize(CustomSettings::instance()->getPanelHeight())
        , m_position(Top)
        , m_displayMode(Dock::Efficient)
        , m_displayInter(new DBusDisplay(this))
        , m_itemManager(itemManager)
        , m_screen(screen)
{
    if (!DockInterface::usingGXDEDock() && qgetenv("WAYLAND_DISPLAY") != "") {
        // 回退到 deepin-daemon 时，Wayland 下因为 deepin-daemon 异常，获取不到数据
        // 直接设置马上超时以避免卡住；gxde-dock-daemon 在 Wayland 下工作正常，无需如此
        m_dockInter->setTimeout(1);
    } else {
        m_dockInter->setTimeout(100);
    }
    m_primaryRawRect = screen->geometry();
    m_primaryRawRect.setHeight(m_primaryRawRect.height() * screen->devicePixelRatio());
    m_primaryRawRect.setWidth(m_primaryRawRect.width() * screen->devicePixelRatio());
    m_screenRawHeight = m_primaryRawRect.height();
    m_screenRawWidth = m_primaryRawRect.width();

    m_hideSubMenu = new QMenu(&m_settingsMenu);
    m_hideSubMenu->setAccessibleName("pluginsmenu");
    m_settingsMenu.installEventFilter(this);
    m_hideSubMenu->installEventFilter(this);
    QAction *hideSubMenuAct = new QAction(tr("Plugins"), this);
    hideSubMenuAct->setMenu(m_hideSubMenu);

    m_settingsMenu.addAction(hideSubMenuAct);
    m_settingsMenu.setTitle("Settings Menu");

    QAction *settingAction = new QAction(tr("Settings"), this);
    m_settingsMenu.addAction(settingAction);

    m_settingsMenu.addSeparator();

    QAction *restartAction = new QAction(tr("Restart"), this);
    m_settingsMenu.addAction(restartAction);

    WaylandMenu::installStyle(&m_settingsMenu);
    WaylandMenu::installStyle(m_hideSubMenu);

    connect(&m_settingsMenu, &QMenu::triggered, this, &TopPanelSettings::menuActionClicked);
    connect(settingAction, &QAction::triggered, this, &TopPanelSettings::settingActionClicked);
    connect(restartAction, &QAction::triggered, this, [this] {
       qApp->exit();
       //QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
    });

    calculateWindowConfig();
}

void TopPanelSettings::moveToScreen(QScreen *screen) {
    this->m_screen = screen;
    m_primaryRawRect = screen->geometry();
    m_primaryRawRect.setHeight(m_primaryRawRect.height() * screen->devicePixelRatio());
    m_primaryRawRect.setWidth(m_primaryRawRect.width() * screen->devicePixelRatio());
    m_screenRawWidth = m_primaryRawRect.width();
    m_screenRawHeight = m_primaryRawRect.height();

    calculateWindowConfig();
}

void TopPanelSettings::showDockSettingsMenu(const QPoint &panelPos)
{
    // exec() 会开嵌套事件循环，菜单还开着时再次右键会重入到这里。那样下面的
    // qDeleteAll 会删掉正在显示的菜单项，并且对同一个 QMenu 重复 exec，直接崩。
    if (m_menuShowing) {
        return;
    }
    m_menuShowing = true;

    // create actions
    QList<QAction *> actions;
    for (auto *p : m_itemManager->pluginList()) {
        if (!p->pluginIsAllowDisable())
            continue;

        const bool enable = !p->pluginIsDisable();
        const QString &name = p->pluginName();
        const QString &display = p->pluginDisplayName();

//        if (!m_trashPluginShow && name == "trash") {
//            continue;
//        }

        QAction *act = new QAction(display, this);
        act->setCheckable(true);
        act->setChecked(enable);
        act->setData(name);

        actions << act;
    }

    // sort by name
    std::sort(actions.begin(), actions.end(), [](QAction * a, QAction * b) -> bool {
        return a->data().toString() > b->data().toString();
    });

    // add actions
    qDeleteAll(m_hideSubMenu->actions());
    for (auto act : actions)
        m_hideSubMenu->addAction(act);

    if (Utils::isWayland()) {
        m_menuLayerPos = QPoint(panelPos.x(), m_mainWindowSize.height());
        WaylandMenu::configure(&m_settingsMenu, m_screen, m_menuLayerPos);
        WaylandMenu::configureSubmenus(&m_settingsMenu, m_screen);
    }

    m_settingsMenu.exec(Utils::isWayland() ? m_menuLayerPos : QCursor::pos());
    m_menuShowing = false;
}

void TopPanelSettings::menuActionClicked(QAction *action)
{
    Q_ASSERT(action);

    // check plugin hide menu.
    const QString &data = action->data().toString();
    if (data.isEmpty())
        return;
    for (auto *p : m_itemManager->pluginList()) {
        if (p->pluginName() == data)
            return p->pluginStateSwitched();
    }
}

bool TopPanelSettings::eventFilter(QObject *watched, QEvent *event)
{
    if (Utils::isWayland()
        && (watched == &m_settingsMenu || watched == m_hideSubMenu)) {
        auto *menu = static_cast<QWidget *>(watched);
        if (event->type() == QEvent::Show) {
            WaylandMenu::updateEffects(static_cast<QMenu *>(menu));
        } else if (event->type() == QEvent::Resize) {
            // Updating the stylesheet from inside Resize can itself trigger a
            // relayout.  Only the compositor blur region depends on size.
            WaylandMenu::updateBlurRegion(static_cast<QMenu *>(menu));
        }
    }

    return QObject::eventFilter(watched, event);
}

void TopPanelSettings::calculateWindowConfig()
{
    if (m_displayMode == Dock::Efficient) {
        if (m_dockWindowSize > WINDOW_MAX_SIZE || m_dockWindowSize < CustomSettings::instance()->getPanelHeight()) {
            m_dockWindowSize = CustomSettings::instance()->getPanelHeight();
        }

        m_mainWindowSize.setHeight(m_dockWindowSize);

        int dockWidth = 0;
        if (!CustomSettings::instance()->isIgnoreDock()
            && !this->m_dockInter->hideMode()
            && this->m_screen == qApp->primaryScreen()
            && (this->m_dockInter->position() == Left || this->m_dockInter->position() == Right)) {

            dockWidth = this->realDDEDockWidth() / qApp->primaryScreen()->devicePixelRatio();
        }
        m_mainWindowSize.setWidth(primaryRect().width() - dockWidth);
    } else {
        Q_ASSERT(false);
    }

    resetFrontendGeometry();
    emit windowRectChanged();
}

const QRect TopPanelSettings::primaryRect() const
{
    QRect rect = m_primaryRawRect;
    qreal scale = this->m_screen->devicePixelRatio();

    rect.setWidth(std::round(qreal(rect.width()) / scale));
    rect.setHeight(std::round(qreal(rect.height()) / scale));

    return rect;
}

void TopPanelSettings::resetFrontendGeometry()
{
    const QRect r = this->windowRect(m_position, false);
    const qreal ratio = dockRatio();
    const QPoint p = rawXPosition(r.topLeft());

    const uint w = r.width() * ratio;
    const uint h = r.height() * ratio;

    // Replacing global X coordinate w/ Dock width drops secondary screen's
    // origin & moves all X11 panels on the top of primary one, causing
    // the top panel looking wired on X11 dual screen.
    m_frontendRect = QRect(p.x(), p.y(), w, h);
    if (CustomSettings::instance()->isIgnoreDock()) {
        m_dockInter->setPosition(Top);
        m_dockInter->setHideMode(KeepShowing);
        m_dockInter->SetFrontendWindowRect(p.x(), p.y(), w, h);
    }
}

const QRect TopPanelSettings::windowRect(const Position position, const bool hide) const
{
    QSize size = m_mainWindowSize;
    if (hide) {
        switch (position) {
            case Top:
            case Bottom:    size.setHeight(2);      break;
            case Left:
            case Right:     size.setWidth(2);       break;
        }
    }

    const QRect primaryRect = this->primaryRect();
    int offsetX = (primaryRect.width() - size.width());

    if (!this->m_dockInter->hideMode()
        && this->m_screen == qApp->primaryScreen()
        && this->m_dockInter->position() == Right) {
        offsetX = 0;
    }

    const int offsetY = (primaryRect.height() - size.height());
    int margin = hide ?  0 : this->dockMargin();
    QPoint p(0, 0);
    switch (position) {
        case Top:
            p = QPoint(offsetX, margin);
            break;
        case Left:
            p = QPoint(margin, offsetY);
            break;
        case Right:
            p = QPoint(primaryRect.width() - size.width() - margin, offsetY);
            break;
        case Bottom:
            p = QPoint(offsetX, primaryRect.height() - size.height() - margin);
            break;
        default: Q_UNREACHABLE();
    }

    return QRect(primaryRect.topLeft() + p, size);
}

const int TopPanelSettings::dockMargin() const
{
    return 0;
}

qreal TopPanelSettings::dockRatio() const
{
    return m_screen ? m_screen->devicePixelRatio() : qApp->devicePixelRatio();
}

void TopPanelSettings::applyCustomSettings(const CustomSettings& customSettings) {
    this->calculateWindowConfig();
}

/**
 * Only work fine for vertical position (LEFT, RIGHT)
 * @return
 */
int TopPanelSettings::realDDEDockWidth() {
    int spacing = 10;
    if (this->m_dockInter->displayMode() == Dock::DisplayMode::Efficient) {
        spacing = 0;
    } else {

    }

    return this->m_dockInter->frontendWindowRect().operator QRect().width() + 2 * 2 * spacing;
}
