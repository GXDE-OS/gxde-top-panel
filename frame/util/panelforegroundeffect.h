#ifndef PANELFOREGROUNDEFFECT_H
#define PANELFOREGROUNDEFFECT_H

#include <QGraphicsEffect>
#include <QColor>

// Legacy plugins sometimes paint black/white pixels directly instead of using
// the application palette. Adapt their panel content, not their popup applets.
class PanelForegroundEffect : public QGraphicsEffect
{
public:
    explicit PanelForegroundEffect(QObject *parent = nullptr);

protected:
    void draw(QPainter *painter) override;

private:
    void updateForeground();
    QColor m_foreground;
};

#endif
