/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dockpopupwindow.h"
#include "utils.h"
#include "popupblur.h"

#include <LayerShellQt/Window>
#include <QPainter>
#include <KWindowEffects>
#include <QPainterPath>
#include <QScreen>
#include <QApplication>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QTimer>
#include <QWindow>

DWIDGET_USE_NAMESPACE

DockPopupWindow::DockPopupWindow(QWidget *parent)
        : DArrowRectangle(ArrowBottom, parent),
          m_model(false),
          m_regionInter(Utils::isWayland() ? nullptr : new DRegionMonitor(this))
{
    setMargin(Utils::isWayland() ? 10 : 0);
    m_wmHelper = DWindowManagerHelper::instance();

    compositeChanged();

    setWindowFlags(Utils::isWayland()
        ? Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
        : Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &DockPopupWindow::compositeChanged);
    if (m_regionInter)
        connect(m_regionInter, &DRegionMonitor::buttonPress, this, &DockPopupWindow::onGlobMouseRelease);
    if (Utils::isWayland()) {
        connect(this, &DArrowRectangle::windowDeactivate, this, [this] {
            QTimer::singleShot(0, this, [this] {
                if (!model())
                    return;
                // Moving focus into a child popup must not close its applet.
                for (QWindow *focus = QGuiApplication::focusWindow(); focus;
                     focus = focus->transientParent()) {
                    if (focus == windowHandle())
                        return;
                }
                emit accept();
            });
        });
    }
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return isVisible() && m_model;
}

void DockPopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    QAccessibleEvent event(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&event);

    if (!content->objectName().trimmed().isEmpty())
        setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;
    m_lastPoint = pos;

    show(pos.x(), pos.y());

    if (m_regionInter && m_regionInter->registered()) {
        m_regionInter->unregisterRegion();
    }

    if (m_model && m_regionInter) {
        m_regionInter->registerRegion();
    }
}

void DockPopupWindow::show(const int x, const int y)
{
    m_lastPoint = QPoint(x, y);

    if (Utils::isWayland()) {
        // LayerShellQt applies to every top-level window. Configure the popup
        // before mapping it, rather than letting default anchors stretch it.
        ensurePolished();
        resizeWithContent();
        DArrowRectangle::move(x, y);
        QScreen *targetScreen = QGuiApplication::screenAt(m_lastPoint);
        if (!targetScreen)
            targetScreen = screen();
        const QRect output = targetScreen->geometry();
        const QPoint local = pos() - output.topLeft();
        const QPoint bounded(qBound(0, local.x(), qMax(0, output.width() - width())),
                             qBound(0, local.y(), qMax(0, output.height() - height())));
        winId();
        QWindow *window = windowHandle();
        window->setScreen(targetScreen);
        window->resize(size());
        auto *layer = LayerShellQt::Window::get(window);
        layer->setScope(QStringLiteral("panel-popup"));
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
        layer->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
        layer->setExclusiveZone(-1);
        layer->setMargins(QMargins(bounded.x(), bounded.y(), 0, 0));
        layer->setKeyboardInteractivity(m_model
            ? LayerShellQt::Window::KeyboardInteractivityOnDemand
            : LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setCloseOnDismissed(false);
    }

    DArrowRectangle::show(x, y);
    if (Utils::isWayland())
        updateWaylandEffects();
}

void DockPopupWindow::hide()
{
    if (m_regionInter && m_regionInter->registered())
        m_regionInter->unregisterRegion();

    DArrowRectangle::hide();
}

void DockPopupWindow::showEvent(QShowEvent *e)
{
    DArrowRectangle::showEvent(e);

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

void DockPopupWindow::enterEvent(QEnterEvent *e)
{
    DArrowRectangle::enterEvent(e);

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize)
        return false;

    // FIXME: ensure position move after global mouse release event
    if (isVisible())
    {
        QTimer::singleShot(10, this, [=] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint, m_model);
        });
    }

    return false;
}

void DockPopupWindow::onGlobMouseRelease(const QPoint &mousePos, const int flag)
{
    Q_ASSERT(m_model);

    if (!((flag == DRegionMonitor::WatchedFlags::Button_Left) ||
          (flag == DRegionMonitor::WatchedFlags::Button_Right))) {
        return;
    }

    const QRect rect = QRect(pos(), size());
    if (rect.contains(mousePos))
        return;

    emit accept();

    m_regionInter->unregisterRegion();
}

void DockPopupWindow::compositeChanged()
{
    if (m_wmHelper->hasComposite())
        setBorderColor(QColor(255, 255, 255, 255 * 0.05));
    else
        setBorderColor(QColor("#2C3238"));
}

void DockPopupWindow::ensureRaised()
{
    if (isVisible())
        raise();
}

void DockPopupWindow::updateWaylandEffects()
{
    if (!windowHandle() || !getContent())
        return;
    QColor background = palette().color(QPalette::Window);
    background.setAlpha(160);
    setBackgroundColor(background);

    // Match the painted body and arrow, excluding transparent shadow margins.
    const QRectF body = QRectF(getContent()->geometry()).adjusted(-margin(), -margin(), margin(), margin());
    QPainterPath shape;
    shape.addRoundedRect(body, radius(), radius());
    QPainterPath arrow;
    const qreal halfArrow = arrowWidth() / 2.0;
    if (arrowDirection() == ArrowTop || arrowDirection() == ArrowBottom) {
        const qreal center = shadowBlurRadius() + (arrowX() > 0 ? arrowX() : (width() - 2 * shadowBlurRadius()) / 2.0);
        const qreal edge = arrowDirection() == ArrowTop ? body.top() : body.bottom();
        const qreal tip = edge + (arrowDirection() == ArrowTop ? -arrowHeight() : arrowHeight());
        arrow.moveTo(center - halfArrow, edge);
        arrow.lineTo(center, tip);
        arrow.lineTo(center + halfArrow, edge);
    } else {
        const qreal center = shadowBlurRadius() + (arrowY() > 0 ? arrowY() : (height() - 2 * shadowBlurRadius()) / 2.0);
        const qreal edge = arrowDirection() == ArrowLeft ? body.left() : body.right();
        const qreal tip = edge + (arrowDirection() == ArrowLeft ? -arrowHeight() : arrowHeight());
        arrow.moveTo(edge, center - halfArrow);
        arrow.lineTo(tip, center);
        arrow.lineTo(edge, center + halfArrow);
    }
    arrow.closeSubpath();
    shape = shape.united(arrow);
    // Use the same path for painting and regional blur. DTK's whole-window
    // blur also enables compositor-specific surface-wide effects, which can
    // blur transparent margins even when a KDE blur region is provided.
    m_waylandShape = shape;
    const QRegion blurRegion = PopupBlur::interiorRegion(shape);
    // An empty region means full-surface blur to the protocol, not no blur.
    KWindowEffects::enableBlurBehind(windowHandle(), !blurRegion.isEmpty(), blurRegion);
    update();
}

void DockPopupWindow::paintEvent(QPaintEvent *event)
{
    if (!Utils::isWayland()) {
        DArrowRectangle::paintEvent(event);
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(backgroundColor());
    painter.setPen(QPen(borderColor(), borderWidth()));
    painter.drawPath(m_waylandShape);
}
