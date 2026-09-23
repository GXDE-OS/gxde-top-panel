#include "tipswidget.h"
#include "../frame/util/CustomSettings.h"

#include <QPainter>

TipsWidget::TipsWidget(QWidget *parent) : QFrame(parent)
{

}

void TipsWidget::setText(const QString &text)
{
    m_text = text;

    setFixedSize(fontMetrics().horizontalAdvance(text) + 6, fontMetrics().height());

    update();
}

void TipsWidget::refreshFont()
{
    setFixedSize(fontMetrics().horizontalAdvance(m_text) + 6, fontMetrics().height());
    update();
}

void TipsWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    refreshFont();

    QPainter painter(this);
    painter.setPen(QPen(CustomSettings::instance()->getActiveFontColor(), 1));

    QTextOption option;
    option.setAlignment(Qt::AlignCenter);
    painter.drawText(rect(), m_text, option);
}
