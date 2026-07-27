#ifndef GXDE_TOP_PANEL_WAYLANDWINDOWMANAGER_H
#define GXDE_TOP_PANEL_WAYLANDWINDOWMANAGER_H

#include <QObject>
#include <QList>
#include <QRect>
#include <QString>

#include <cstdint>

class QSocketNotifier;

struct wl_array;
struct wl_display;
struct wl_registry;
struct org_kde_plasma_window;
struct org_kde_plasma_window_management;

class WaylandWindowManager : public QObject
{
    Q_OBJECT

public:
    struct WindowInfo {
        bool valid = false;
        bool active = false;
        bool minimized = false;
        bool maximized = false;
        QString title;
        QString appId;
        QString iconName;
        QString menuService;
        QString menuObjectPath;
        QRect geometry;
    };

    static WaylandWindowManager *instance();

    bool isAvailable() const;
    WindowInfo activeWindow() const;

    void activate();
    void minimize();
    void toggleMaximized();
    void close();
    void requestMove();

signals:
    void activeWindowChanged();
    void availabilityChanged(bool available);

private:
    struct Window;

    explicit WaylandWindowManager(QObject *parent = nullptr);
    ~WaylandWindowManager() override;

    void createWindow(uint32_t id, const char *uuid);
    void removeWindow(Window *window);
    void notifyIfActive(Window *window);
    void flush();

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);

    static void showDesktopChanged(void *, org_kde_plasma_window_management *, uint32_t);
    static void windowCreated(void *data, org_kde_plasma_window_management *, uint32_t id);
    static void stackingOrderChanged(void *, org_kde_plasma_window_management *, wl_array *);
    static void stackingOrderUuidChanged(void *, org_kde_plasma_window_management *, const char *);
    static void windowCreatedWithUuid(void *data, org_kde_plasma_window_management *,
                                      uint32_t id, const char *uuid);
    static void stackingOrderChanged2(void *, org_kde_plasma_window_management *);

    static void titleChanged(void *data, org_kde_plasma_window *, const char *title);
    static void appIdChanged(void *data, org_kde_plasma_window *, const char *appId);
    static void stateChanged(void *data, org_kde_plasma_window *, uint32_t flags);
    static void virtualDesktopChanged(void *, org_kde_plasma_window *, int32_t);
    static void themedIconNameChanged(void *data, org_kde_plasma_window *, const char *name);
    static void unmapped(void *data, org_kde_plasma_window *);
    static void initialState(void *data, org_kde_plasma_window *);
    static void parentWindow(void *, org_kde_plasma_window *, org_kde_plasma_window *);
    static void geometryChanged(void *data, org_kde_plasma_window *, int32_t x, int32_t y,
                                uint32_t width, uint32_t height);
    static void iconChanged(void *data, org_kde_plasma_window *);
    static void pidChanged(void *, org_kde_plasma_window *, uint32_t);
    static void virtualDesktopEntered(void *, org_kde_plasma_window *, const char *);
    static void virtualDesktopLeft(void *, org_kde_plasma_window *, const char *);
    static void applicationMenuChanged(void *data, org_kde_plasma_window *,
                                       const char *serviceName, const char *objectPath);
    static void activityEntered(void *, org_kde_plasma_window *, const char *);
    static void activityLeft(void *, org_kde_plasma_window *, const char *);
    static void resourceNameChanged(void *, org_kde_plasma_window *, const char *);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    org_kde_plasma_window_management *m_management = nullptr;
    QSocketNotifier *m_socketNotifier = nullptr;
    QList<Window *> m_windows;
    Window *m_activeWindow = nullptr;
    uint32_t m_managementGlobalName = 0;
    uint32_t m_managementVersion = 0;
};

#endif
