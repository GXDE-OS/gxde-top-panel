#include "panelforegroundeffect.h"

#include <DGuiApplicationHelper>
#include <QPainter>
#include <QBitArray>
#include <QVector>
#include <algorithm>

using Dtk::Gui::DGuiApplicationHelper;

PanelForegroundEffect::PanelForegroundEffect(QObject *parent)
    : QGraphicsEffect(parent)
{
    updateForeground();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, [this] { updateForeground(); });
}

void PanelForegroundEffect::updateForeground()
{
    m_foreground = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
        ? QColor(Qt::white) : QColor(Qt::black);
    update();
}

void PanelForegroundEffect::draw(QPainter *painter)
{
    QPoint offset;
    const QPixmap source = sourcePixmap(Qt::DeviceCoordinates, &offset, NoPad);
    if (source.isNull())
        return;

    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    QBitArray visited(width * height);
    auto pixelAt = [&image, width](int index) -> QRgb & {
        return reinterpret_cast<QRgb *>(image.scanLine(index / width))[index % width];
    };
    for (int seed = 0; seed < width * height; ++seed) {
        if (visited.testBit(seed) || !qAlpha(pixelAt(seed)))
            continue;
        QVector<int> component{seed};
        visited.setBit(seed);
        int darkest = 255, lightest = 0;
        int edgeLightness = 0, edgeCount = 0;
        for (qsizetype i = 0; i < component.size(); ++i) {
            const int index = component[i];
            const QRgb pixel = pixelAt(index);
            const int high = std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int low = std::min({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int neighbors[] = {index % width ? index - 1 : -1,
                index % width + 1 < width ? index + 1 : -1,
                index >= width ? index - width : -1,
                index + width < width * height ? index + width : -1};
            bool edge = false;
            for (int next : neighbors) {
                if (next < 0 || !qAlpha(pixelAt(next))) {
                    edge = true;
                } else if (!visited.testBit(next)) {
                    visited.setBit(next);
                    component.append(next);
                }
            }
            if (high - low <= 16 && qAlpha(pixel) > 128) {
                darkest = std::min(darkest, low);
                lightest = std::max(lightest, high);
                if (edge) {
                    edgeLightness += qGray(pixel);
                    ++edgeCount;
                }
            }
        }
        // Some icons include an opaque black/white background. Keep their
        // internal contrast instead of turning the whole icon into a square.
        const bool twoTone = darkest < 64 && lightest > 192 && edgeCount;
        const bool invert = twoTone
            && ((edgeLightness / edgeCount >= 128) == (m_foreground == Qt::white));
        for (int index : component) {
            QRgb &pixel = pixelAt(index);
            const int high = std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int low = std::min({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            // Preserve coloured status indicators and antialiased alpha.
            if (high - low <= 16) {
                const int value = twoTone ? (invert ? 255 - qGray(pixel) : qGray(pixel))
                                         : m_foreground.red();
                pixel = qRgba(value, value, value, qAlpha(pixel));
            }
        }
    }
    painter->save();
    painter->setWorldTransform(QTransform());
    painter->drawImage(offset, image);
    painter->restore();
}
