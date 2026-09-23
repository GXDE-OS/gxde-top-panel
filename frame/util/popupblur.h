#ifndef GXDE_TOP_PANEL_POPUPBLUR_H
#define GXDE_TOP_PANEL_POPUPBLUR_H

#include <QPainterPath>
#include <QRegion>

namespace PopupBlur {
inline QRegion interiorRegion(const QPainterPath &shape)
{
    const QRegion region(shape.toFillPolygon().toPolygon());
    // Keep blur inside the antialiased edge, including diagonal arrow edges.
    QRegion interior = region;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            interior &= region.translated(x, y);
    QRegion clipped;
    for (const QRect &rect : interior) {
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            int start = -1;
            for (int x = rect.left(); x <= rect.right() + 1; ++x) {
                const bool inside = x <= rect.right()
                    && shape.contains(QPointF(x, y))
                    && shape.contains(QPointF(x + 1, y))
                    && shape.contains(QPointF(x, y + 1))
                    && shape.contains(QPointF(x + 1, y + 1));
                if (inside && start < 0)
                    start = x;
                if (!inside && start >= 0) {
                    clipped += QRect(start, y, x - start, 1);
                    start = -1;
                }
            }
        }
    }
    return clipped;
}
}
#endif
