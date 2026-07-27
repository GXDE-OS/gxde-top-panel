#include "WaylandWindowManager.h"

#include "plasma-window-management-client-protocol.h"

#include <QApplication>
#include <QDebug>
#include <QSocketNotifier>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>
#include <wayland-client.h>

namespace {
constexpr uint32_t s_supportedVersion = 17;

QString fromWaylandString(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}
}

struct WaylandWindowManager::Window {
    WaylandWindowManager *owner = nullptr;
    org_kde_plasma_window *handle = nullptr;
    uint32_t id = 0;
    QString uuid;
    WindowInfo info;
    bool initialStateReceived = false;
};

WaylandWindowManager *WaylandWindowManager::instance()
{
    static WaylandWindowManager *manager = new WaylandWindowManager(qApp);
    return manager;
}

WaylandWindowManager::WaylandWindowManager(QObject *parent)
    : QObject(parent)
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qWarning() << "Wayland window management: cannot connect to the compositor";
        return;
    }

    static const wl_registry_listener registryListener = {
        &WaylandWindowManager::registryGlobal,
        &WaylandWindowManager::registryGlobalRemove,
    };

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);

    // The first roundtrip discovers globals, the second receives the initial
    // window list, and the third receives the properties requested from the
    // newly-created window objects.
    if (wl_display_roundtrip(m_display) < 0
        || wl_display_roundtrip(m_display) < 0
        || wl_display_roundtrip(m_display) < 0) {
        qWarning() << "Wayland window management: initial roundtrip failed";
        return;
    }
    flush();

    const int fd = wl_display_get_fd(m_display);
    m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, [this] {
        m_socketNotifier->setEnabled(false);
        if (wl_display_dispatch(m_display) < 0) {
            qWarning() << "Wayland window management connection failed:"
                       << strerror(errno);
            m_socketNotifier->setEnabled(false);
            if (m_management) {
                m_management = nullptr;
                emit availabilityChanged(false);
            }
            m_activeWindow = nullptr;
            emit activeWindowChanged();
            return;
        }
        m_socketNotifier->setEnabled(true);
        flush();
    });

    if (!m_management) {
        qWarning() << "Wayland compositor does not advertise"
                      " org_kde_plasma_window_management";
    }
}

WaylandWindowManager::~WaylandWindowManager()
{
    if (m_socketNotifier) {
        m_socketNotifier->setEnabled(false);
    }

    for (Window *window : std::as_const(m_windows)) {
        if (window->handle) {
            org_kde_plasma_window_destroy(window->handle);
        }
        delete window;
    }
    m_windows.clear();

    if (m_management) {
        org_kde_plasma_window_management_destroy(m_management);
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
    if (m_display) {
        wl_display_disconnect(m_display);
    }
}

bool WaylandWindowManager::isAvailable() const
{
    return m_management;
}

WaylandWindowManager::WindowInfo WaylandWindowManager::activeWindow() const
{
    if (!m_activeWindow || !m_activeWindow->initialStateReceived) {
        return {};
    }
    return m_activeWindow->info;
}

void WaylandWindowManager::activate()
{
    if (!m_activeWindow) {
        return;
    }
    org_kde_plasma_window_set_state(
        m_activeWindow->handle,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    flush();
}

void WaylandWindowManager::minimize()
{
    if (!m_activeWindow) {
        return;
    }
    org_kde_plasma_window_set_state(
        m_activeWindow->handle,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED,
        ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    flush();
}

void WaylandWindowManager::toggleMaximized()
{
    if (!m_activeWindow) {
        return;
    }
    const uint32_t maximized = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED;
    org_kde_plasma_window_set_state(m_activeWindow->handle, maximized,
                                    m_activeWindow->info.maximized ? 0 : maximized);
    flush();
}

void WaylandWindowManager::close()
{
    if (!m_activeWindow) {
        return;
    }
    org_kde_plasma_window_close(m_activeWindow->handle);
    flush();
}

void WaylandWindowManager::requestMove()
{
    if (!m_activeWindow || m_managementVersion < 3) {
        return;
    }
    org_kde_plasma_window_request_move(m_activeWindow->handle);
    flush();
}

void WaylandWindowManager::registryGlobal(void *data, wl_registry *registry, uint32_t name,
                                          const char *interface, uint32_t version)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    if (strcmp(interface, org_kde_plasma_window_management_interface.name) != 0
        || manager->m_management) {
        return;
    }

    manager->m_managementVersion = std::min(version, s_supportedVersion);
    manager->m_managementGlobalName = name;
    manager->m_management = static_cast<org_kde_plasma_window_management *>(
        wl_registry_bind(registry, name, &org_kde_plasma_window_management_interface,
                         manager->m_managementVersion));

    static const org_kde_plasma_window_management_listener managementListener = {
        &WaylandWindowManager::showDesktopChanged,
        &WaylandWindowManager::windowCreated,
        &WaylandWindowManager::stackingOrderChanged,
        &WaylandWindowManager::stackingOrderUuidChanged,
        &WaylandWindowManager::windowCreatedWithUuid,
        &WaylandWindowManager::stackingOrderChanged2,
    };
    org_kde_plasma_window_management_add_listener(manager->m_management,
                                                  &managementListener, manager);
    emit manager->availabilityChanged(true);
}

void WaylandWindowManager::registryGlobalRemove(void *data, wl_registry *, uint32_t name)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    if (name != manager->m_managementGlobalName) {
        return;
    }
    manager->m_management = nullptr;
    manager->m_activeWindow = nullptr;
    emit manager->availabilityChanged(false);
    emit manager->activeWindowChanged();
}

void WaylandWindowManager::createWindow(uint32_t id, const char *uuid)
{
    const QString windowUuid = fromWaylandString(uuid);
    for (const Window *window : std::as_const(m_windows)) {
        if (window->id == id || (!windowUuid.isEmpty() && window->uuid == windowUuid)) {
            return;
        }
    }

    auto *window = new Window;
    window->owner = this;
    window->id = id;
    window->uuid = windowUuid;
    if (!windowUuid.isEmpty() && m_managementVersion >= 12) {
        window->handle = org_kde_plasma_window_management_get_window_by_uuid(
            m_management, windowUuid.toUtf8().constData());
    } else {
        window->handle = org_kde_plasma_window_management_get_window(m_management, id);
    }

    static const org_kde_plasma_window_listener windowListener = {
        &WaylandWindowManager::titleChanged,
        &WaylandWindowManager::appIdChanged,
        &WaylandWindowManager::stateChanged,
        &WaylandWindowManager::virtualDesktopChanged,
        &WaylandWindowManager::themedIconNameChanged,
        &WaylandWindowManager::unmapped,
        &WaylandWindowManager::initialState,
        &WaylandWindowManager::parentWindow,
        &WaylandWindowManager::geometryChanged,
        &WaylandWindowManager::iconChanged,
        &WaylandWindowManager::pidChanged,
        &WaylandWindowManager::virtualDesktopEntered,
        &WaylandWindowManager::virtualDesktopLeft,
        &WaylandWindowManager::applicationMenuChanged,
        &WaylandWindowManager::activityEntered,
        &WaylandWindowManager::activityLeft,
        &WaylandWindowManager::resourceNameChanged,
    };
    org_kde_plasma_window_add_listener(window->handle, &windowListener, window);
    m_windows.append(window);
}

void WaylandWindowManager::removeWindow(Window *window)
{
    const bool wasActive = m_activeWindow == window;
    if (wasActive) {
        m_activeWindow = nullptr;
    }
    m_windows.removeOne(window);
    if (window->handle) {
        org_kde_plasma_window_destroy(window->handle);
        window->handle = nullptr;
    }
    delete window;
    if (wasActive) {
        emit activeWindowChanged();
    }
}

void WaylandWindowManager::notifyIfActive(Window *window)
{
    if (window == m_activeWindow && window->initialStateReceived) {
        emit activeWindowChanged();
    }
}

void WaylandWindowManager::flush()
{
    if (m_display && wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        qWarning() << "Wayland window management flush failed:" << strerror(errno);
    }
}

void WaylandWindowManager::showDesktopChanged(void *data,
                                              org_kde_plasma_window_management *,
                                              uint32_t state)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    if (state == ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED
        && manager->m_activeWindow) {
        manager->m_activeWindow = nullptr;
        emit manager->activeWindowChanged();
    }
}

void WaylandWindowManager::windowCreated(void *data, org_kde_plasma_window_management *,
                                         uint32_t id)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    // Version 13 and newer supplies the UUID in window_with_uuid.
    if (manager->m_managementVersion < 13) {
        manager->createWindow(id, nullptr);
    }
}

void WaylandWindowManager::stackingOrderChanged(void *,
                                                org_kde_plasma_window_management *,
                                                wl_array *)
{
}

void WaylandWindowManager::stackingOrderUuidChanged(void *,
                                                    org_kde_plasma_window_management *,
                                                    const char *)
{
}

void WaylandWindowManager::windowCreatedWithUuid(void *data,
                                                 org_kde_plasma_window_management *,
                                                 uint32_t id, const char *uuid)
{
    static_cast<WaylandWindowManager *>(data)->createWindow(id, uuid);
}

void WaylandWindowManager::stackingOrderChanged2(void *,
                                                 org_kde_plasma_window_management *)
{
}

void WaylandWindowManager::titleChanged(void *data, org_kde_plasma_window *,
                                        const char *title)
{
    auto *window = static_cast<Window *>(data);
    window->info.title = fromWaylandString(title);
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::appIdChanged(void *data, org_kde_plasma_window *,
                                        const char *appId)
{
    auto *window = static_cast<Window *>(data);
    window->info.appId = fromWaylandString(appId);
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::stateChanged(void *data, org_kde_plasma_window *,
                                        uint32_t flags)
{
    auto *window = static_cast<Window *>(data);
    WaylandWindowManager *manager = window->owner;
    const bool wasActive = window == manager->m_activeWindow;

    window->info.active = flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE;
    window->info.minimized = flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED;
    window->info.maximized = flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED;

    if (window->info.active) {
        manager->m_activeWindow = window;
    }
    // A pointer press on the panel's layer surface can deactivate the app
    // before QToolButton emits clicked() on release.  Keep the last active
    // application as the control target; otherwise maximize, minimize and
    // close all see a null m_activeWindow and silently do nothing.
    //
    // The pointer is replaced when another app becomes active and is cleared
    // when the window is unmapped or Show Desktop is entered.

    if (window->initialStateReceived && (wasActive || window->info.active)) {
        emit manager->activeWindowChanged();
    }
}

void WaylandWindowManager::virtualDesktopChanged(void *, org_kde_plasma_window *, int32_t)
{
}

void WaylandWindowManager::themedIconNameChanged(void *data, org_kde_plasma_window *,
                                                 const char *name)
{
    auto *window = static_cast<Window *>(data);
    window->info.iconName = fromWaylandString(name);
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::unmapped(void *data, org_kde_plasma_window *)
{
    auto *window = static_cast<Window *>(data);
    window->owner->removeWindow(window);
}

void WaylandWindowManager::initialState(void *data, org_kde_plasma_window *)
{
    auto *window = static_cast<Window *>(data);
    window->initialStateReceived = true;
    window->info.valid = true;
    if (window->info.active) {
        window->owner->m_activeWindow = window;
        emit window->owner->activeWindowChanged();
    }
}

void WaylandWindowManager::parentWindow(void *, org_kde_plasma_window *,
                                        org_kde_plasma_window *)
{
}

void WaylandWindowManager::geometryChanged(void *data, org_kde_plasma_window *,
                                           int32_t x, int32_t y,
                                           uint32_t width, uint32_t height)
{
    auto *window = static_cast<Window *>(data);
    window->info.geometry = QRect(x, y, static_cast<int>(width), static_cast<int>(height));
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::iconChanged(void *data, org_kde_plasma_window *)
{
    auto *window = static_cast<Window *>(data);
    window->info.iconName.clear();
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::pidChanged(void *, org_kde_plasma_window *, uint32_t)
{
}

void WaylandWindowManager::virtualDesktopEntered(void *, org_kde_plasma_window *,
                                                 const char *)
{
}

void WaylandWindowManager::virtualDesktopLeft(void *, org_kde_plasma_window *,
                                              const char *)
{
}

void WaylandWindowManager::applicationMenuChanged(void *data,
                                                  org_kde_plasma_window *,
                                                  const char *serviceName,
                                                  const char *objectPath)
{
    auto *window = static_cast<Window *>(data);
    window->info.menuService = fromWaylandString(serviceName);
    window->info.menuObjectPath = fromWaylandString(objectPath);
    window->owner->notifyIfActive(window);
}

void WaylandWindowManager::activityEntered(void *, org_kde_plasma_window *,
                                           const char *)
{
}

void WaylandWindowManager::activityLeft(void *, org_kde_plasma_window *,
                                        const char *)
{
}

void WaylandWindowManager::resourceNameChanged(void *, org_kde_plasma_window *,
                                               const char *)
{
}
