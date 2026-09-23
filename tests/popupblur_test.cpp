#include "frame/util/popupblur.h"
#include <QDebug>
#include <cstdlib>

int main()
{
    for (int radius : {0, 6, 18}) {
        QPainterPath shape;
        shape.addRoundedRect(QRectF(20, 30, 200, 80), radius, radius);
        QPainterPath arrow;
        arrow.moveTo(110, 30);
        arrow.lineTo(120, 20);
        arrow.lineTo(130, 30);
        arrow.closeSubpath();
        shape = shape.united(arrow);
        const QRegion blur = PopupBlur::interiorRegion(shape);
        if (!blur.contains(QPoint(120, 60)) || blur.contains(QPoint(0, 0)))
            return EXIT_FAILURE;
        for (const QRect &rect : blur) {
            for (int y = rect.top(); y <= rect.bottom(); ++y) {
                for (int x = rect.left(); x <= rect.right(); ++x) {
                    if (!shape.contains(QPointF(x + 0.001, y + 0.001))
                        || !shape.contains(QPointF(x + 0.999, y + 0.001))
                        || !shape.contains(QPointF(x + 0.001, y + 0.999))
                        || !shape.contains(QPointF(x + 0.999, y + 0.999))) {
                        qCritical() << "Blur outside painted shape" << x << y << radius;
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }
    if (!PopupBlur::interiorRegion(QPainterPath()).isEmpty())
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
