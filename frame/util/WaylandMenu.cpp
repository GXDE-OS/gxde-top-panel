#include "WaylandMenu.h"

#include "utils.h"

#include <DPlatformWindowHandle>
#include <KWindowEffects>
#include <LayerShellQt/Window>
#include <QAction>
#include <QCoreApplication>
#include <QApplication>
#include <QMenu>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QRegion>
#include <QScreen>
#include <QWindow>
#include <QPainter>
#include <QEvent>
#include <DGuiApplicationHelper>

DGUI_USE_NAMESPACE

namespace {

constexpr int MenuRadius = 8;
constexpr auto PlatformHandleName = "wayland-menu-platform-handle";
constexpr auto BorderOverlayName = "wayland-menu-border-overlay";
constexpr auto ShadowProperty = "gxde-wayland-menu-shadow";
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

class MenuBorderOverlay : public QWidget {
public:
    explicit MenuBorderOverlay(QMenu *menu)
            : QWidget(menu) {
        setObjectName(QString::fromLatin1(BorderOverlayName));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        menu->installEventFilter(this);
        syncGeometry();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == parent() && (event->type() == QEvent::Resize
                || event->type() == QEvent::Show)) {
            syncGeometry();
        }
        
        if (watched == parent() && event->type() == QEvent::Paint
                && DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType) {
            auto *menu = static_cast<QWidget *>(watched);
            QPainter painter(menu);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(24, 24, 24, 90));
            painter.drawRoundedRect(QRectF(menu->rect()), MenuRadius, MenuRadius);
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override {
        const bool dark = DGuiApplicationHelper::instance()->themeType()
            == DGuiApplicationHelper::DarkType;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 31), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
            MenuRadius - 0.5, MenuRadius - 0.5);
    }

private:
    void syncGeometry() {
        setGeometry(parentWidget()->rect());
        raise();
        update();
    }
};

class MenuShadow : public QWidget {
public:
    static constexpr int Radius = 18;
    static constexpr int OffsetY = 1;

    explicit MenuShadow(QMenu *menu)
            : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowTransparentForInput
                | Qt::WindowDoesNotAcceptFocus | Qt::Tool)
            , m_menu(menu) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        menu->installEventFilter(this);
        QObject::connect(menu, &QObject::destroyed, this, &QObject::deleteLater);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != m_menu) {
            return false;
        }

        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
            if (m_menu->isVisible()) {
                follow();
            }
            break;
        case QEvent::Hide:
            hide();
            break;
        default:
            break;
        }
        return false;
    }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        const QRectF menuArea = QRectF(rect()).adjusted(Radius, Radius - OffsetY,
            -Radius, -Radius - OffsetY);
        QPainterPath clip;
        clip.addRect(QRectF(rect()));
        QPainterPath menuPath;
        menuPath.addRoundedRect(menuArea, MenuRadius, MenuRadius);
        painter.setClipPath(clip.subtracted(menuPath));

        const QRectF core = QRectF(rect()).adjusted(Radius, Radius, -Radius, -Radius);
        for (int i = Radius; i > 0; --i) {
            const qreal t = qreal(Radius - i + 1) / Radius;          // 0..1 由外到内
            painter.setBrush(QColor(0, 0, 0, qRound(6 * t * t)));
            painter.drawRoundedRect(core.adjusted(-i, -i, i, i),
                MenuRadius + i, MenuRadius + i);
        }
    }

private:
    void follow() {
        QScreen *screen = m_menu->screen();
        const QRect menuRect(m_menu->pos(), m_menu->size());
        const QRect shadowRect = menuRect.adjusted(-Radius, -Radius + OffsetY,
            Radius, Radius + OffsetY);
        resize(shadowRect.size());

        if (!m_layerReady) {
            winId();
            if (QWindow *window = windowHandle()) {
                if (screen)
                    window->setScreen(screen);
                LayerShellQt::Window *layer = LayerShellQt::Window::get(window);
                layer->setScope(QStringLiteral("menu-shadow"));
                layer->setLayer(LayerShellQt::Window::LayerTop);
                layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
                layer->setAnchors(LayerShellQt::Window::Anchors(
                    LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
                layer->setExclusiveZone(-1);
                layer->setKeyboardInteractivity(
                    LayerShellQt::Window::KeyboardInteractivityNone);
                m_layerReady = true;
            }
        }

        const QPoint origin = screen ? screen->geometry().topLeft() : QPoint();
        const QPoint pos = shadowRect.topLeft() - origin;
        if (QWindow *window = windowHandle()) {
            LayerShellQt::Window::get(window)->setMargins(
                QMargins(qMax(0, pos.x()), qMax(0, pos.y()), 0, 0));
        }
        show();
        update();
    }

    QMenu *m_menu;
    bool m_layerReady = false;
};

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

    QPalette palette = QApplication::palette(menu);
    if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType) {
        QColor window = palette.color(QPalette::Window);
        window.setAlpha(80);
        palette.setColor(QPalette::Window, window);
    }
    menu->setPalette(palette);

    if (!menu->findChild<QWidget *>(QString::fromLatin1(BorderOverlayName),
            Qt::FindDirectChildrenOnly)) {
        new MenuBorderOverlay(menu);
    }

    if (!menu->property(ShadowProperty).toBool()) {
        menu->setProperty(ShadowProperty, true);
        auto *shadow = new MenuShadow(menu);
        if (menu->isVisible()) {
            QCoreApplication::postEvent(menu, new QEvent(QEvent::Move));
        }
        Q_UNUSED(shadow);
    }

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
    // Keep the Wayland surface tight to the menu; avoid blur outside its bounds.
    handle->setShadowRadius(0);
    handle->setBorderWidth(0);

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
