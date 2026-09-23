/*
 * (C) 2026 CharOfString <root@charofstring.cc>
 * Licensed under GNU GENERAL PUBLIC LICENSE Version 3.
 */


#include "frame/util/paneloutputs.h"
#include <QDebug>
#include <cstdlib>

static void check(const QList<QRect> &outputs, int primary,
        const QList<int> &expected) {
    const auto actual = panelOutputIndices(outputs, primary);
    if (actual != expected) {
        qCritical() << "DT:" << actual << "; GT:" << expected;
        std::exit(1);
    }
}

int main() {
    const QRect main(0, 0, 1920, 1080);
    const QRect right(1920, 0, 1920, 1080);
    const QRect left(-1920, 0, 1920, 1080);
    const QRect below(0, 1080, 1920, 1080);
    check({}, -1, {});
    check({main}, 0, {0});
    check({main, right}, 0, {0, 1});
    check({main, left, below}, 0, {0, 1, 2});
    check({main, main}, 0, {0});
    check({main, main}, 1, {1});
    check({main, QRect(0, 0, 1280, 720)}, 1, {1});
    check({main, main, right, right}, 1, {1, 2});
    check({main, right, right}, 2, {2, 0});
    check({main, right}, -1, {0, 1});
    check({QRect(), main}, 0, {1});
    check({main, main}, 0, {0});
    check({main, right}, 0, {0, 1});
    check({main, main}, 0, {0});
    return 0;
}
