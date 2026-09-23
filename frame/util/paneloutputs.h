/*
 * (C) 2026 CharOfString <root@charofstring.cc>
 * Licensed under GNU GENERAL PUBLIC LICENSE Version 3.
 */

#ifndef FRAME_UTIL_PANELOUTPUTS_H_
#define FRAME_UTIL_PANELOUTPUTS_H_

#include <QList>
#include <QRect>

inline QList<int> panelOutputIndices(const QList<QRect> &geometries,
        int primaryIndex) {
    QList<int> order;
    if (primaryIndex >= 0 && primaryIndex < geometries.size()) {
        order.append(primaryIndex);
    }

    for (int i = 0; i < geometries.size(); ++i) {
        if (i != primaryIndex) {
            order.append(i);
        }
    }

    QList<int> selected;
    for (int index : order) {
        if (geometries[index].isEmpty()) {
            continue;
        }

        bool mirrored = false;
        for (int existing : selected) {
            if (geometries[existing].topLeft() == geometries[index].topLeft()) {
                mirrored = true;
                break;
            }
        }
        if (!mirrored) {
            selected.append(index);
        }
    }
    return selected;
}

#endif  // FRAME_UTIL_PANELOUTPUTS_H_
