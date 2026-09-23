/*
 * (C) 2026 CharOfString <root@charofstring.cc>
 * Licensed under GNU GENERAL PUBLIC LICENSE Version 3.
 */

#ifndef FRAME_UTIL_PANELBACKGROUND_H_
#define FRAME_UTIL_PANELBACKGROUND_H_
#include <QPainter>
#include <QPainterPath>

inline void paintPanelBackground(QPainter &painter, const QRect &rect,
        const QColor &tint, int xRadius, int yRadius) {
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);

    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect), xRadius, yRadius);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillPath(shape, tint);
    painter.restore();
}

#endif  // FRAME_UTIL_PANELBACKGROUND_H_
