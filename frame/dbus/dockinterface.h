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
 * This header is partially from the auto-generated class using
 * libdframeworkdbus-qt.
 */

#ifndef FRAME_DBUS_DOCKINTERFACE_H_
#define FRAME_DBUS_DOCKINTERFACE_H_

#include <DBusExtendedAbstractInterface>
#include <QtDBus/QtDBus>

#include "types/dockrect.h"

class DockInterface : public DBusExtendedAbstractInterface {
    Q_OBJECT

public:
    explicit DockInterface(QObject* parent = nullptr);
    ~DockInterface() override;

    static bool usingGXDEDock();

    Q_PROPERTY(int DisplayMode
        READ displayMode
        WRITE setDisplayMode
        NOTIFY DisplayModeChanged)
    int displayMode();
    void setDisplayMode(int value);

    Q_PROPERTY(int HideMode
        READ hideMode
        WRITE setHideMode
        NOTIFY HideModeChanged)
    int hideMode();
    void setHideMode(int value);

    Q_PROPERTY(int Position
        READ position
        WRITE setPosition
        NOTIFY PositionChanged)
    int position();
    void setPosition(int value);

    Q_PROPERTY(DockRect FrontendWindowRect
        READ frontendWindowRect
        NOTIFY FrontendWindowRectChanged)
    DockRect frontendWindowRect();

    Q_PROPERTY(uint WindowSize
        READ windowSize
        NOTIFY WindowSizeChanged)
    uint windowSize();

public Q_SLOTS: // METHODS
    // Following methods are from the auto-generated class using
    // libdframeworkdbus-qt, they're kept as-is.
    inline QDBusPendingReply<> CloseWindow(uint win)
    {
        return asyncCallWithArgumentList(QStringLiteral("CloseWindow"), {QVariant::fromValue(win)});
    }

    inline QDBusPendingReply<> MaximizeWindow(uint win)
    {
        return asyncCallWithArgumentList(QStringLiteral("MaximizeWindow"), {QVariant::fromValue(win)});
    }

    inline QDBusPendingReply<bool> IsOnDock(const QString &desktopFile)
    {
        return asyncCallWithArgumentList(QStringLiteral("IsOnDock"), {QVariant::fromValue(desktopFile)});
    }

    inline QDBusPendingReply<bool> RequestDock(const QString &desktopFile, int index)
    {
        return asyncCallWithArgumentList(QStringLiteral("RequestDock"),
                                         {QVariant::fromValue(desktopFile), QVariant::fromValue(index)});
    }

    inline QDBusPendingReply<> MoveEntry(int index, int newIndex)
    {
        return asyncCallWithArgumentList(QStringLiteral("MoveEntry"),
                                         {QVariant::fromValue(index), QVariant::fromValue(newIndex)});
    }

    inline QDBusPendingReply<> SetFrontendWindowRect(int x, int y, uint width, uint height)
    {
        return asyncCallWithArgumentList(QStringLiteral("SetFrontendWindowRect"),
                                         {QVariant::fromValue(x), QVariant::fromValue(y),
                                          QVariant::fromValue(width), QVariant::fromValue(height)});
    }

    inline QDBusPendingReply<QString> GetPluginSettings()
    {
        return asyncCallWithArgumentList(QStringLiteral("GetPluginSettings"), {});
    }

    inline QDBusPendingReply<> MergePluginSettings(const QString &jsonStr)
    {
        return asyncCallWithArgumentList(QStringLiteral("MergePluginSettings"), {QVariant::fromValue(jsonStr)});
    }

    inline QDBusPendingReply<> RemovePluginSettings(const QString &key1, const QStringList &key2List)
    {
        return asyncCallWithArgumentList(QStringLiteral("RemovePluginSettings"),
                                         {QVariant::fromValue(key1), QVariant::fromValue(key2List)});
    }

Q_SIGNALS: // SIGNALS
    void EntryAdded(const QDBusObjectPath& path, int index);
    void EntryRemoved(const QString& entryId);
    void PluginSettingsSynced();
    void ServiceRestarted();
    void DisplayModeChanged(int value) const;
    void HideModeChanged(int value) const;
    void PositionChanged(int value) const;
    void FrontendWindowRectChanged(DockRect value) const;
    void WindowSizeChanged(uint value) const;

private:
    int m_displayMode = 0;
    int m_hideMode = 0;
    int m_position = 0;
    DockRect m_frontendWindowRect;
    uint m_windowSize = 0;
};

#endif  // FRAME_DBUS_DOCKINTERFACE_H_
