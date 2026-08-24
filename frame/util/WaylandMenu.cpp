#include "WaylandMenu.h"

#include "utils.h"

#include <DPlatformWindowHandle>
#include <KWindowEffects>
#include <LayerShellQt/Window>
#include <QAction>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QProxyStyle>
#include <QRegion>
#include <QScreen>
#include <QStyleFactory>
#include <QStyleOption>
#include <QWindow>

QT_BEGIN_NAMESPACE
void qt_blurImage(QPainter* painter, QImage& blurImage, qreal radius,
    bool quality, bool alphaOnly, int transposed = 0);
QT_END_NAMESPACE

namespace {

constexpr int MenuRadius = 8;
// The Treeland personalization protocol blurs the complete Wayland surface.
// Keep that surface tight to the menu so the blur cannot extend past it.
constexpr int ShadowMargin = 0;
constexpr int ShadowBlur = 7;
constexpr int ShadowOffsetY = 3;
constexpr qreal MenuBackgroundOpacity = 0.72;
constexpr auto PlatformHandleName = "wayland-menu-platform-handle";
constexpr auto LayerXProperty = "gxde-wayland-menu-layer-x";
constexpr auto LayerYProperty = "gxde-wayland-menu-layer-y";
constexpr auto SubmenuHookProperty = "gxde-wayland-submenu-hook";
constexpr auto StyleInstalledProperty = "gxde-dtk-menu-style-installed";
constexpr auto StyleUnavailableProperty = "gxde-dtk-menu-style-unavailable";

class DtkMenuStyle final : public QProxyStyle {
public:
    explicit DtkMenuStyle(QStyle* baseStyle) : QProxyStyle(baseStyle) {}

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
            QPainter* painter,
                const QWidget* widget = nullptr) const override {
        if (element == PE_PanelMenu) {
            drawMenuPanel(painter, widget);
            return;
        }

        if (element == PE_FrameMenu) {
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

private:
    static void drawMenuPanel(QPainter* painter, const QWidget* widget) {
        if (!painter || !widget) {
            return;
        }

        const QMargins margins = widget->contentsMargins();
        const QRectF panelRect(
            margins.left(), margins.top(),
            widget->width() - margins.left() - margins.right(),
            widget->height() - margins.top() - margins.bottom());

        if (panelRect.isEmpty()) {
            return;
        }

        QPainterPath panelPath;
        panelPath.addRoundedRect(panelRect, MenuRadius, MenuRadius);

        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->fillRect(widget->rect(), Qt::transparent);
        painter->restore();

        QImage shadow(widget->size(), QImage::Format_ARGB32_Premultiplied);
        shadow.fill(Qt::transparent);

        {
            QPainter shadowPainter(&shadow);
            shadowPainter.setRenderHint(QPainter::Antialiasing);
            shadowPainter.fillPath(
                panelPath.translated(0, ShadowOffsetY), QColor(0, 0, 0));
        }

        painter->save();
        QPainterPath outside;
        outside.addRect(widget->rect());
        painter->setClipPath(outside.subtracted(panelPath));
        painter->setOpacity(0.18);
        qt_blurImage(painter, shadow, ShadowBlur * 2.0, true, true);
        painter->restore();

        QColor background = widget->palette().color(QPalette::Window);
        background.setAlphaF(MenuBackgroundOpacity);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillPath(panelPath, background);
        painter->strokePath(panelPath, QPen(QColor(0, 0, 0, 20), 1));
    }
};

QRect panelRect(const QMenu* menu) {
    if (!menu) {
        return {};
    }
    const QMargins margins = menu->contentsMargins();
    return menu->rect().marginsRemoved(margins);
}

QRegion roundedRegion(const QMenu* menu) {
    const QRect rect = panelRect(menu);
    if (rect.isEmpty()) {
        return {};
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(rect), MenuRadius, MenuRadius);
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

bool installStyle(QMenu* menu) {
    if (!menu) {
        return false;
    }
    if (menu->property(StyleInstalledProperty).toBool()) {
        return true;
    }
    if (menu->property(StyleUnavailableProperty).toBool()) {
        return false;
    }

    QStyle *baseStyle = QStyleFactory::create(QStringLiteral("dlight2"));
    if (!baseStyle) {
        menu->setProperty(StyleUnavailableProperty, true);
        return false;
    }

    // Once DTK2 style is redy, use it to override config
    menu->setStyleSheet(QString());
    if (!Utils::isWayland()) {
        baseStyle->setParent(menu);
        menu->setStyle(baseStyle);
        menu->setProperty(StyleInstalledProperty, true);
        return true;
    }

    auto* style = new DtkMenuStyle(baseStyle);
    style->setParent(menu);
    menu->setStyle(style);
    menu->setProperty(StyleInstalledProperty, true);
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setContentsMargins(ShadowMargin, ShadowMargin,
        ShadowMargin, ShadowMargin);
    return true;
}

void updateEffects(QMenu *menu)
{
    if (!installStyle(menu) || !Utils::isWayland()) {
        return;
    }

    menu->setAttribute(Qt::WA_TranslucentBackground);

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
    if (menu && menu->property(StyleInstalledProperty).toBool()
        && menu->windowHandle()) {
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

    const QMargins shadowMargins = menu->contentsMargins();
    const QPoint surfacePosition = layerPosition
        - QPoint(shadowMargins.left(), shadowMargins.top());
    const QPoint position = boundedPosition(menu, screen, surfacePosition);
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

        installStyle(submenu);

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
                const QMargins parentMargins = parentMenu->contentsMargins();
                configure(childMenu, targetScreen,
                          parentPosition
                            + QPoint(parentMenu->width() - parentMargins.right(),
                            actionRect.top()));
                configureSubmenus(childMenu, targetScreen);
            });
        }

        configureSubmenus(submenu, screen);
    }
}

} // namespace WaylandMenu
