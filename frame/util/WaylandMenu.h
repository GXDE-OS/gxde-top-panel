#ifndef GXDE_TOP_PANEL_WAYLANDMENU_H
#define GXDE_TOP_PANEL_WAYLANDMENU_H

#include <QPoint>

class QMenu;
class QScreen;

namespace WaylandMenu {

// layerPosition is relative to the top-left corner of screen.
void configure(QMenu *menu, QScreen *screen, const QPoint &layerPosition);
void configureSubmenus(QMenu *menu, QScreen *screen);
void updateEffects(QMenu *menu);
void updateBlurRegion(QMenu *menu);

} // namespace WaylandMenu

#endif // GXDE_TOP_PANEL_WAYLANDMENU_H
