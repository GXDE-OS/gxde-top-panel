/*
 * (C) 2026 CharOfString <root@charofstring.cc>
 * Licensed under GNU GENERAL PUBLIC LICENSE Version 3.
 */

#include "frame/util/panelbackground.h"
#include <QImage>
#include <QDebug>

int main() {
    for (int scale : {1, 2}) {
        for (int radius : {0, 6}) {
            QImage expected(320 * scale, 32 * scale, QImage::Format_ARGB32_Premultiplied);
            expected.setDevicePixelRatio(scale);
            expected.fill(Qt::transparent);
            const QRect rect(0, 0, 320, 32);
            const QColor tint(30, 40, 50, 80);

            {
                QPainter painter(&expected);
                paintPanelBackground(painter, rect, tint, radius, radius);
            }

            QImage partial = expected.copy();
            for (int frame = 0; frame < 20; ++frame) {
                QPainter painter(&partial);
                painter.setClipRect(QRect((frame * 17) % 300, 0, 20, 32));
                // Simulate stale content under a plugin's damaged rectangle.
                painter.fillRect(rect, QColor(255, 255, 255, 90));
                paintPanelBackground(painter, rect, tint, radius, radius);
            }

            if (partial != expected) {
                return 1;
            }
        }
    }
    return 0;
}
