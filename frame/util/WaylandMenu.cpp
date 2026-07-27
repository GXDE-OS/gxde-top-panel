#include "WaylandMenu.h"

#include "utils.h"

#include <DPlatformWindowHandle>
#include <KWindowEffects>
#include <LayerShellQt/Window>
#include <QAction>
#include <QMenu>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QRegion>
#include <QScreen>
#include <QWindow>

namespace {

constexpr int MenuRadius = 10;
constexpr auto PlatformHandleName = "wayland-menu-platform-handle";
constexpr auto LayerXProperty = "gxde-wayland-menu-layer-x";
constexpr auto LayerYProperty = "gxde-wayland-menu-layer-y";
constexpr auto SubmenuHookProperty = "gxde-wayland-submenu-hook";

QRegion roundedRegion(const QMenu *menu)
{
    if (!menu || menu->width() <= 0 || menu->height() <= 0) {
        return {};
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(menu->rect()), MenuRadius, MenuRadius);
    return QRegion(path.toFillPolygon().toPolygon());
}

QPoint boundedPosition(QMenu *menu, QScreen *screen, const QPoint &position)
{
    if (!menu || !screen) {
        return position;
    }

    const QSize outputSize = screen->geometry().size();
    return QPoint(qBound(0, position.x(), qMax(0, outputSize.width() - menu->width())),
                  qBound(0, position.y(), qMax(0, outputSize.height() - menu->height())));
}

} // namespace

namespace WaylandMenu {

void updateEffects(QMenu *menu)
{
    if (!menu || !Utils::isWayland()) {
        return;
    }

    menu->setAttribute(Qt::WA_TranslucentBackground);

    const QColor background = menu->palette().color(QPalette::Window);
    const QColor border = menu->palette().color(QPalette::Mid);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background-color: rgba(%1, %2, %3, 220); "
        "border: 1px solid rgba(%4, %5, %6, 110); "
        "border-radius: %7px; padding: 4px; }")
        .arg(background.red()).arg(background.green()).arg(background.blue())
        .arg(border.red()).arg(border.green()).arg(border.blue())
        .arg(MenuRadius));

    QWindow *window = menu->windowHandle();
    if (!window) {
        return;
    }

    auto *handle = menu->findChild<Dtk::Widget::DPlatformWindowHandle *>(
        QString::fromLatin1(PlatformHandleName), Qt::FindDirectChildrenOnly);
    if (!handle) {
        handle = new Dtk::Widget::DPlatformWindowHandle(menu, menu);
        handle->setObjectName(QString::fromLatin1(PlatformHandleName));
    }
    handle->setTranslucentBackground(true);
    handle->setWindowRadius(MenuRadius);

    // The DTK/UKUI protocol blurs the whole surface, so configure() commits
    // the popup's real size before enabling it.  On KWin-compatible
    // compositors the explicit rounded region below narrows it further.
    handle->setEnableBlurWindow(true);
    KWindowEffects::enableBlurBehind(window, true, roundedRegion(menu));
}

void updateBlurRegion(QMenu *menu)
{
    if (menu && menu->windowHandle()) {
        KWindowEffects::enableBlurBehind(menu->windowHandle(), true,
                                         roundedRegion(menu));
    }
}

void configure(QMenu *menu, QScreen *screen, const QPoint &layerPosition)
{
    if (!menu || !Utils::isWayland()) {
        return;
    }

    // Apply the border and padding before asking QMenu for its natural size.
    updateEffects(menu);

    // A layer surface with an implicit 0x0 size can be expanded to the whole
    // output by the compositor.  Commit the menu's natural size before its
    // layer-shell role is created.
    menu->ensurePolished();
    menu->adjustSize();
    const QSize naturalSize = menu->sizeHint().expandedTo(menu->minimumSizeHint());
    if (naturalSize.isValid()) {
        menu->resize(naturalSize);
    }

    menu->winId();
    QWindow *window = menu->windowHandle();
    if (!window) {
        qWarning() << "(Wayland) Menu: failed to get window handle";
        return;
    }

    if (screen) {
        window->setScreen(screen);
    }
    window->resize(menu->size());

    const QPoint position = boundedPosition(menu, screen, layerPosition);
    menu->setProperty(LayerXProperty, position.x());
    menu->setProperty(LayerYProperty, position.y());

    LayerShellQt::Window *layer = LayerShellQt::Window::get(window);
    layer->setScope(QStringLiteral("menu"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    layer->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
    layer->setExclusiveZone(-1);
    layer->setMargins(QMargins(position.x(), position.y(), 0, 0));
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layer->setCloseOnDismissed(true);

    updateEffects(menu);
}

void configureSubmenus(QMenu *menu, QScreen *screen)
{
    if (!menu || !Utils::isWayland()) {
        return;
    }

    for (QAction *action : menu->actions()) {
        QMenu *submenu = action ? action->menu() : nullptr;
        if (!submenu) {
            continue;
        }

        if (!submenu->property(SubmenuHookProperty).toBool()) {
            submenu->setProperty(SubmenuHookProperty, true);
            QPointer<QMenu> parentMenu(menu);
            QPointer<QMenu> childMenu(submenu);
            QPointer<QAction> parentAction(action);
            QPointer<QScreen> targetScreen(screen);
            QObject::connect(submenu, &QMenu::aboutToShow, submenu,
                             [parentMenu, childMenu, parentAction, targetScreen] {
                if (!parentMenu || !childMenu || !parentAction) {
                    return;
                }

                const QRect actionRect = parentMenu->actionGeometry(parentAction);
                const QPoint parentPosition(
                    parentMenu->property(LayerXProperty).toInt(),
                    parentMenu->property(LayerYProperty).toInt());
                configure(childMenu, targetScreen,
                          parentPosition + QPoint(parentMenu->width(), actionRect.top()));
                configureSubmenus(childMenu, targetScreen);
            });
        }

        configureSubmenus(submenu, screen);
    }
}

} // namespace WaylandMenu
