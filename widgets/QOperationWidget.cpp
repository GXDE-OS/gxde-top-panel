//
// Created by septemberhx on 2020/8/14.
//

#include "QOperationWidget.h"
#include "util/CustomSettings.h"

namespace {
constexpr int ControlButtonSize = 20;
}

QOperationWidget::QOperationWidget(bool leftSide, QWidget *parent)
        : QWidget(parent) {
    const int buttonSize = ControlButtonSize;

    this->m_layout = new QHBoxLayout(this);
    this->m_layout->setContentsMargins(2, 1, 2, 1);
    this->m_layout->setSpacing(0);

    this->wrapLabel = new QLabel(this);
    this->wrapLayout = new QHBoxLayout(this->wrapLabel);
    this->wrapLayout->setContentsMargins(6, 0, 6, 0);
    this->wrapLayout->setSpacing(5);


    this->closeButton = new QToolButton(this->wrapLabel);
    this->closeButton->setFixedSize(buttonSize, buttonSize);
    this->closeButton->setIcon(QIcon(CustomSettings::instance()->getActiveCloseIconPath()));
    this->closeButton->setIconSize(QSize(buttonSize - 6, buttonSize - 6));
    connect(this->closeButton, &QToolButton::clicked, this, &QOperationWidget::closeButtonClicked);

    this->maxButton = new QToolButton(this->wrapLabel);
    this->maxButton->setFixedSize(buttonSize, buttonSize);
    this->maxButton->setIcon(QIcon(CustomSettings::instance()->getActiveUnmaximizedIconPath()));
    this->maxButton->setIconSize(QSize(buttonSize - 6, buttonSize - 6));
    connect(this->maxButton, &QToolButton::clicked, this, &QOperationWidget::maxButtonClicked);

    this->minButton = new QToolButton(this->wrapLabel);
    this->minButton->setFixedSize(buttonSize, buttonSize);
    this->minButton->setIcon(QIcon(CustomSettings::instance()->getActiveMinimizedIconPath()));
    this->minButton->setIconSize(QSize(buttonSize - 6, buttonSize - 6));
    connect(this->minButton, &QToolButton::clicked, this, &QOperationWidget::minButtonClicked);

    if (leftSide) {
        this->wrapLayout->addWidget(this->closeButton);
        this->wrapLayout->addWidget(this->maxButton);
        this->wrapLayout->addWidget(this->minButton);
    } else {
        this->wrapLayout->addWidget(this->minButton);
        this->wrapLayout->addWidget(this->maxButton);
        this->wrapLayout->addWidget(this->closeButton);
    }
    this->m_layout->addWidget(this->wrapLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    this->wrapLabel->setFixedHeight(buttonSize + 2);
    this->wrapLabel->setFixedWidth(3 * buttonSize + 22);

    this->m_buttonShowAnimation = new QPropertyAnimation(this, "maximumWidth");
    this->m_buttonShowAnimation->setEndValue(this->wrapLabel->width() + 4);
    this->m_buttonShowAnimation->setDuration(150);
    connect(this->m_buttonShowAnimation, &QAbstractAnimation::finished, this, [this] {
        Q_EMIT animationFinished();
    });

    this->m_buttonHideAnimation = new QPropertyAnimation(this, "maximumWidth");
    this->m_buttonHideAnimation->setEndValue(0);
    this->m_buttonHideAnimation->setDuration(150);
    connect(this->m_buttonHideAnimation, &QPropertyAnimation::finished, this, [this]() {
        this->hide();
        Q_EMIT animationFinished();
    });
}

void QOperationWidget::hideWithAnimation() {
    if (!this->isVisible()) return;

    this->m_buttonShowAnimation->stop();
    this->m_buttonHideAnimation->setStartValue(this->width());
    this->m_buttonHideAnimation->start();
}

void QOperationWidget::showWithAnimation() {
    if (this->isVisible() && m_buttonHideAnimation->state() == QAbstractAnimation::Stopped) return;

    this->m_buttonHideAnimation->stop();
    this->show();
    this->m_buttonShowAnimation->setStartValue(this->width());
    this->m_buttonShowAnimation->start();
}

void QOperationWidget::applyCustomSettings(const CustomSettings &settings) {
    const int buttonSize = ControlButtonSize;
    for (auto *button : {closeButton, maxButton, minButton}) {
        button->setFixedSize(buttonSize, buttonSize);
        button->setIconSize(QSize(buttonSize - 6, buttonSize - 6));
    }
    wrapLabel->setFixedWidth(3 * buttonSize + 22);
    const int targetWidth = wrapLabel->width()
        + m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    m_buttonShowAnimation->setEndValue(targetWidth);
    if (m_buttonShowAnimation->state() == QAbstractAnimation::Stopped
            && m_buttonHideAnimation->state() == QAbstractAnimation::Stopped) {
        setMaximumWidth(targetWidth);
    }

    this->closeButton->setIcon(QIcon(settings.getActiveCloseIconPath()));
    this->maxButton->setIcon(QIcon(settings.getActiveUnmaximizedIconPath()));
    this->minButton->setIcon(QIcon(settings.getActiveMinimizedIconPath()));

    if (settings.isButtonHighlight()) {
        this->wrapLabel->setStyleSheet(
                QString("QLabel { border-radius: 10px; background-color: %1; } QToolButton:hover { background-color: #0081FF; }").arg(settings.getButtonHighLightColor().name()));
    } else {
        this->wrapLabel->setStyleSheet("QLabel { border-radius: 10px; background-color: transparent; } QToolButton:hover { background-color: #0081FF; }");
    }
}
