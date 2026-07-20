/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * This is a class that "routes" D-Bus names & paths. It prefers the new
 * top.gxde.daemon.dock, and falls back to the old com.deepin.dde.daemon.Dock
 * if unavailable.
 */

#include <QDBusConnectionInterface>
#include <QDebug>

#include "dockinterface.h"

namespace {

const char* const GXDE_DOCK_SERVICE = "top.gxde.daemon.dock";
const char* const GXDE_DOCK_PATH = "/top/gxde/daemon/dock";
const char* const GXDE_DOCK_INTERFACE = "top.gxde.daemon.dock";

const char* const DDE_DOCK_SERVICE = "com.deepin.dde.daemon.Dock";
const char* const DDE_DOCK_PATH = "/com/deepin/dde/daemon/Dock";
const char* const DDE_DOCK_INTERFACE = "com.deepin.dde.daemon.Dock";

/**
 * @brief 暂时关闭新 daemon（top.gxde.daemon.dock）支持，疑似该后端有问题。
 * @note 恢复支持时把它改回 true 即可，下面的探测逻辑原样保留。
 */
constexpr bool ENABLE_GXDE_DOCK_BACKEND = false;

/**
 * @brief Checks if top.gxde.daemon.dock is available on the session bus...
 * @param (none)
 * @note The whole thing is wrapped in a static function so the result is
 *       "cached".
 * @return (bool) Whether top.gxde.daemon.dock is available.
 */
bool isNeoDockServiceReachable() {
    if (!ENABLE_GXDE_DOCK_BACKEND) {
        return false;
    }

    static const bool available = [] {
        QDBusConnectionInterface* bus = QDBusConnection::sessionBus()
            .interface();

        if (!bus) {
            return false;
        } else if (bus->isServiceRegistered(GXDE_DOCK_SERVICE).value()) {
            return true;
        }
        return bus->activatableServiceNames().value()
            .contains(GXDE_DOCK_SERVICE);
    }();
    return available;
}

}  // namespace

bool DockInterface::usingGXDEDock() {
    return isNeoDockServiceReachable();
}

DockInterface::DockInterface(QObject* parent) : DBusExtendedAbstractInterface(
            isNeoDockServiceReachable() ? GXDE_DOCK_SERVICE : DDE_DOCK_SERVICE,
            isNeoDockServiceReachable() ? GXDE_DOCK_PATH : DDE_DOCK_PATH,
            isNeoDockServiceReachable() ? GXDE_DOCK_INTERFACE :
                DDE_DOCK_INTERFACE,
            QDBusConnection::sessionBus(), parent) {
    registerDockRectMetaType();

    qDebug() << "(DBus) Backend Router: routing request to"
        << (isNeoDockServiceReachable() ? GXDE_DOCK_SERVICE : DDE_DOCK_SERVICE);

    connect(this, &DockInterface::propertyChanged, this, [this](
            const QString& name, const QVariant& value) {
        if (name == QLatin1String("DisplayMode")) {
            m_displayMode = value.toInt();
        } else if (name == QLatin1String("HideMode")) {
            m_hideMode = value.toInt();
        } else if (name == QLatin1String("Position")) {
            m_position = value.toInt();
        } else if (name == QLatin1String("FrontendWindowRect")) {
            m_frontendWindowRect = qvariant_cast<DockRect>(value);
        } else if (name == QLatin1String("WindowSize")) {
            m_windowSize = value.toUInt();
        }
    });
}

DockInterface::~DockInterface() = default;

int DockInterface::displayMode() {
    return qvariant_cast<int>(internalPropGet("DisplayMode", &m_displayMode));
}

void DockInterface::setDisplayMode(int value) {
    internalPropSet("DisplayMode", QVariant::fromValue(value), &m_displayMode);
}

int DockInterface::hideMode() {
    return qvariant_cast<int>(internalPropGet("HideMode", &m_hideMode));
}

void DockInterface::setHideMode(int value) {
    internalPropSet("HideMode", QVariant::fromValue(value), &m_hideMode);
}

int DockInterface::position() {
    return qvariant_cast<int>(internalPropGet("Position", &m_position));
}

void DockInterface::setPosition(int value) {
    internalPropSet("Position", QVariant::fromValue(value), &m_position);
}

DockRect DockInterface::frontendWindowRect() {
    return qvariant_cast<DockRect>(internalPropGet("FrontendWindowRect",
        &m_frontendWindowRect));
}

uint DockInterface::windowSize() {
    return qvariant_cast<uint>(internalPropGet("WindowSize", &m_windowSize));
}
