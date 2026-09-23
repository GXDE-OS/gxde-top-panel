#include "mainsettingwidget.h"
#include "ui_mainsettingwidget.h"
#include "../frame/util/CustomSettings.h"
#include <QIcon>
#include <QSignalBlocker>
#include <QColorDialog>
#include <QFileDialog>
#include <QMovie>
#include <QApplication>
#include <iostream>
#include <QLineEdit>
#include <QTimer>
#include "../frame/util/clockformat.h"

MainSettingWidget::MainSettingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainSettingWidget)
{
    ui->setupUi(this);
    auto *clockPage = new QWidget(ui->tabWidget);
    auto *clockLayout = new QVBoxLayout(clockPage);
    auto *customClock = new QCheckBox(tr("Customize clock format"), clockPage);
    customClock->setObjectName(QStringLiteral("customClockCheckBox"));
    auto *clock12Hour = new QCheckBox(tr("Use 12-hour clock"), clockPage);
    clock12Hour->setObjectName(QStringLiteral("clock12HourCheckBox"));
    auto *clockFormat = new QLineEdit(clockPage);
    clockFormat->setObjectName(QStringLiteral("clockFormatEdit"));
    clockFormat->setMaxLength(256);
    auto *clockHelp = new QLabel(tr("Use Y or YYYY for the full year; use YY for the two-digit year\n"
                                  "M represents the month; D represents the day; ddd represents the weekday\n"
                                  "h represents the hour; m represents the minute; s represents the second\n"
                                  "The clock format can contain custom text. Use \\n for a line break (up to two lines)\n"
                                  "Escape the keywords above with a backslash \\, for example, \\Y displays Y instead of the year; likewise, \\\\ displays a single backslash"), clockPage);
    clockHelp->setWordWrap(true);
    auto *clockPreview = new QLabel(clockPage);
    clockPreview->setObjectName(QStringLiteral("clockPreviewLabel"));
    clockPreview->setTextFormat(Qt::PlainText);
    clockPreview->setAlignment(Qt::AlignCenter);
    clockPreview->setMinimumHeight(64);
    clockLayout->addWidget(customClock);
    clockLayout->addWidget(clock12Hour);
    clockLayout->addWidget(clockFormat);
    clockLayout->addWidget(clockHelp);
    clockLayout->addWidget(new QLabel(tr("Below is a preview of your format:"), clockPage));
    clockLayout->addWidget(clockPreview);
    clockLayout->addStretch();
    ui->tabWidget->insertTab(1, clockPage, tr("Clock"));
    auto syncClock = [customClock, clockFormat, clock12Hour] {
        const auto *settings = CustomSettings::instance();
        const QSignalBlocker enabledBlocker(customClock);
        const QSignalBlocker hourBlocker(clock12Hour);
        clock12Hour->setChecked(settings->isClock12Hour());
        clock12Hour->setEnabled(settings->isCustomClockEnabled());
        const QSignalBlocker formatBlocker(clockFormat);
        customClock->setChecked(settings->isCustomClockEnabled());
        if (ClockFormat::normalize(clockFormat->text()) != settings->getClockFormat()) {
            const int cursor = clockFormat->cursorPosition();
            const int selectionStart = clockFormat->selectionStart();
            const int selectionLength = clockFormat->selectedText().size();
            clockFormat->setText(settings->getClockFormat());
            if (selectionStart >= 0) {
                if (cursor == selectionStart)
                    clockFormat->setSelection(selectionStart + selectionLength, -selectionLength);
                else
                    clockFormat->setSelection(selectionStart, selectionLength);
            } else {
                clockFormat->setCursorPosition(cursor);
            }
        }
        clockFormat->setEnabled(settings->isCustomClockEnabled());
    };
    syncClock();
    connect(CustomSettings::instance(), &CustomSettings::settingsChanged, clockPage, syncClock);
    connect(customClock, &QCheckBox::toggled, CustomSettings::instance(), &CustomSettings::setCustomClockEnabled);
    connect(clock12Hour, &QCheckBox::toggled, CustomSettings::instance(), &CustomSettings::setClock12Hour);
    connect(clockFormat, &QLineEdit::textEdited, CustomSettings::instance(), &CustomSettings::setClockFormat);
    auto updateClockPreview = [clockFormat, clockPreview] {
        clockPreview->setText(ClockFormat::render(clockFormat->text(), QDateTime::currentDateTime(),
            QLocale(), CustomSettings::instance()->isClock12Hour()));
    };
    connect(clockFormat, &QLineEdit::textChanged, clockPage, updateClockPreview);
    connect(CustomSettings::instance(), &CustomSettings::settingsChanged, clockPage, updateClockPreview);
    auto *clockTimer = new QTimer(clockPage);
    clockTimer->setInterval(1000);
    connect(clockTimer, &QTimer::timeout, clockPage, updateClockPreview);
    clockTimer->start();
    updateClockPreview();

    ui->panelColorlabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(CustomSettings::instance()->getPanelBgColor().name()));
    ui->fontColorLabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(CustomSettings::instance()->getActiveFontColor().name()));
    ui->buttonHighlightColorLabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(CustomSettings::instance()->getButtonHighLightColor().name()));

    ui->opacitySpinBox->setValue(CustomSettings::instance()->getPanelOpacity());

    ui->closeButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveCloseIconPath()).pixmap(ui->closeButtonLabel->size()));
    ui->unmaxButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveUnmaximizedIconPath()).pixmap(ui->unmaxButtonLabel->size()));
    ui->minButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveMinimizedIconPath()).pixmap(ui->minButtonLabel->size()));
    ui->defaultIconLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveDefaultAppIconPath()).pixmap(ui->defaultIconLabel->size()));
    ui->menuOnHoverCheckBox->setChecked(CustomSettings::instance()->isShowGlobalMenuOnHover());

    ui->appNameLabel->setText(QApplication::applicationDisplayName());
    ui->appVersionLabel->setText(QApplication::applicationVersion());

    ui->panelColorToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->fontColorToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->defaultIconResetToolButton->setIcon(QIcon(":/icons/reset.svg"));
    ui->defaultIconToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->minToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->minResetToolButton->setIcon(QIcon(":/icons/reset.svg"));
    ui->unmaxButtonToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->unmaxResetToolButton->setIcon(QIcon(":/icons/reset.svg"));
    ui->closeButtonToolButton->setIcon(QIcon(":/icons/config.svg"));
    ui->closeResetToolButton->setIcon(QIcon(":/icons/reset.svg"));
    ui->buttonHighlightColorToolButton->setIcon(QIcon(":/icons/config.svg"));

    movie = new QMovie(":/icons/doge.gif");
    ui->pMovieLabel->setMovie(movie);

    ui->showAppNameCheckBox->setChecked(CustomSettings::instance()->isShowAppNameInsteadIcon());
    ui->alwaysUseDefaultIconCheckBox->setChecked(CustomSettings::instance()->isAlwaysUseDefaultIcon());
    connect(ui->alwaysUseDefaultIconCheckBox, &QCheckBox::toggled,
            CustomSettings::instance(), &CustomSettings::setAlwaysUseDefaultIcon);
    ui->showButtonsCheckBox->setChecked(CustomSettings::instance()->isShowControlButtons());
    ui->showLogoWithAppNameCheckBox->setChecked(CustomSettings::instance()->isShowLogoWithAppName());
    ui->ignoreDockCheckBox->setChecked(CustomSettings::instance()->isIgnoreDock());
    ui->buttonOnRightCheckBox->setChecked(!CustomSettings::instance()->isButtonOnLeft());
    ui->buttonHighlightCheckBox->setChecked(CustomSettings::instance()->isButtonHighlight());
    ui->hideTitlebarCheckBox->setChecked(CustomSettings::instance()->isHideTitleWhenMax());

    ui->followSystemThemeCheckBox->setChecked(CustomSettings::instance()->isFollowSystemTheme());
    ui->newUiCheckBox->setChecked(CustomSettings::instance()->isNewUiEnabled());
    connect(ui->newUiCheckBox, &QCheckBox::toggled,
        CustomSettings::instance(), &CustomSettings::setNewUiEnabled);
    // 深色框显示面板实际深浅；跟随系统时由系统决定，禁止手动修改
    ui->useDarkDtkPanelCheckBox->setChecked(CustomSettings::instance()->isDarkPanel());
    ui->useDarkDtkPanelCheckBox->setEnabled(!CustomSettings::instance()->isFollowSystemTheme());
    // 面板背景色由深浅决定，不再支持自定义颜色
    ui->panelColorToolButton->setEnabled(false);
    connect(ui->useDarkDtkPanelCheckBox, &QCheckBox::toggled,
            CustomSettings::instance(), &CustomSettings::setUseDarkDtkPanel);
    const auto syncIconStyleBoxes = [this] {
        const QString &style = CustomSettings::instance()->getIconStyle();
        const QSignalBlocker b23(ui->useDde23IconsCheckBox);
        const QSignalBlocker b25(ui->useDde25IconsCheckBox);
        ui->useDde23IconsCheckBox->setChecked(style == QLatin1String("dde23"));
        ui->useDde25IconsCheckBox->setChecked(style == QLatin1String("dde25"));
    };
    syncIconStyleBoxes();
    connect(ui->useDde23IconsCheckBox, &QCheckBox::toggled, this, [syncIconStyleBoxes](bool checked) {
        CustomSettings::instance()->setIconStyle(checked ? QStringLiteral("dde23") : QString());
        syncIconStyleBoxes();
    });
    connect(ui->useDde25IconsCheckBox, &QCheckBox::toggled, this, [syncIconStyleBoxes](bool checked) {
        CustomSettings::instance()->setIconStyle(checked ? QStringLiteral("dde25") : QString());
        syncIconStyleBoxes();
    });
    connect(CustomSettings::instance(), &CustomSettings::settingsChanged, this, [this] {
        const auto *settings = CustomSettings::instance();
        const QSignalBlocker newUiBlocker(ui->newUiCheckBox);
        ui->newUiCheckBox->setChecked(settings->isNewUiEnabled());
        const QSignalBlocker blocker(ui->useDarkDtkPanelCheckBox);
        ui->useDarkDtkPanelCheckBox->setChecked(settings->isDarkPanel());
        ui->useDarkDtkPanelCheckBox->setEnabled(!settings->isFollowSystemTheme());
        const QSignalBlocker dde23Blocker(ui->useDde23IconsCheckBox);
        const QSignalBlocker dde25Blocker(ui->useDde25IconsCheckBox);
        ui->useDde23IconsCheckBox->setChecked(settings->getIconStyle() == QLatin1String("dde23"));
        ui->useDde25IconsCheckBox->setChecked(settings->getIconStyle() == QLatin1String("dde25"));
        const QSignalBlocker themeBlocker(ui->followSystemThemeCheckBox);
        ui->followSystemThemeCheckBox->setChecked(settings->isFollowSystemTheme());
        const QSignalBlocker iconBlocker(ui->alwaysUseDefaultIconCheckBox);
        ui->alwaysUseDefaultIconCheckBox->setChecked(settings->isAlwaysUseDefaultIcon());
        ui->panelColorlabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(settings->getPanelBgColor().name()));
        ui->fontColorLabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(settings->getActiveFontColor().name()));
        ui->defaultIconLabel->setPixmap(QIcon(settings->getActiveDefaultAppIconPath()).pixmap(ui->defaultIconLabel->size()));
        ui->closeButtonLabel->setPixmap(QIcon(settings->getActiveCloseIconPath()).pixmap(ui->closeButtonLabel->size()));
        ui->unmaxButtonLabel->setPixmap(QIcon(settings->getActiveUnmaximizedIconPath()).pixmap(ui->unmaxButtonLabel->size()));
        ui->minButtonLabel->setPixmap(QIcon(settings->getActiveMinimizedIconPath()).pixmap(ui->minButtonLabel->size()));
    });

    connect(ui->opacitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &MainSettingWidget::opacityValueChanged);
    connect(ui->panelColorToolButton, &QToolButton::clicked, this, &MainSettingWidget::panelColorButtonClicked);
    connect(ui->fontColorToolButton, &QToolButton::clicked, this, &MainSettingWidget::fontColorButtonClicked);
    connect(ui->closeButtonToolButton, &QToolButton::clicked, this, &MainSettingWidget::closeButtonClicked);
    connect(ui->closeResetToolButton, &QToolButton::clicked, this, &MainSettingWidget::closeResetButtonClicked);
    connect(ui->unmaxButtonToolButton, &QToolButton::clicked, this, &MainSettingWidget::unmaxButtonClicked);
    connect(ui->unmaxResetToolButton, &QToolButton::clicked, this, &MainSettingWidget::unmaxResetButtonClicked);
    connect(ui->buttonHighlightColorToolButton, &QToolButton::clicked, this, &MainSettingWidget::buttonHighlightColorButtonClicked);
    connect(ui->minToolButton, &QToolButton::clicked, this, &MainSettingWidget::minButtonClicked);
    connect(ui->minResetToolButton, &QToolButton::clicked, this, &MainSettingWidget::minResetButtonClicked);
    connect(ui->defaultIconToolButton, &QToolButton::clicked, this, &MainSettingWidget::defaultButtonClicked);
    connect(ui->defaultIconResetToolButton, &QToolButton::clicked, this, &MainSettingWidget::defaultResetButtonClicked);
    connect(ui->menuOnHoverCheckBox, &QCheckBox::stateChanged, this, [this]() {
        CustomSettings::instance()->setShowGlobalMenuOnHover(ui->menuOnHoverCheckBox->isChecked());
    });
    connect(ui->showAppNameCheckBox, &QCheckBox::stateChanged, this, [this]() {
        CustomSettings::instance()->setShowAppNameInsteadIcon(ui->showAppNameCheckBox->isChecked());
    });
    connect(ui->showButtonsCheckBox, &QCheckBox::stateChanged, this, [this]() {
        CustomSettings::instance()->setShowControlButtons(ui->showButtonsCheckBox->isChecked());
    });
    connect(ui->showLogoWithAppNameCheckBox, &QCheckBox::stateChanged, this, [this]() {
        CustomSettings::instance()->setShowLogoWithAppName(ui->showLogoWithAppNameCheckBox->isChecked());
    });
    connect(ui->ignoreDockCheckBox, &QCheckBox::stateChanged, this, [this]() {
        CustomSettings::instance()->setIgnoreDock(ui->ignoreDockCheckBox->isChecked());
    });
    connect(ui->buttonOnRightCheckBox, &QCheckBox::stateChanged, this, [this] {
        CustomSettings::instance()->setButtonOnLeft(!ui->buttonOnRightCheckBox->isChecked());
    });
    connect(ui->buttonHighlightCheckBox, &QCheckBox::stateChanged, this, [this] {
        CustomSettings::instance()->setButtonHighlight(ui->buttonHighlightCheckBox->isChecked());
    });
    connect(ui->hideTitlebarCheckBox, &QCheckBox::stateChanged, this, [this] {
        CustomSettings::instance()->setHideTitleWhenMax(ui->hideTitlebarCheckBox->isChecked());
    });
    connect(ui->followSystemThemeCheckBox, &QCheckBox::stateChanged, this, [this] {
        CustomSettings::instance()->setFollowSystemTheme(ui->followSystemThemeCheckBox->isChecked());
    });
    connect(ui->dragToMoveCheckBox, &QCheckBox::stateChanged, this, [this] {
        CustomSettings::instance()->setAllowDragWindowWhenMax(ui->dragToMoveCheckBox->isChecked());
    });
}

MainSettingWidget::~MainSettingWidget()
{
    delete ui;
}

void MainSettingWidget::opacityValueChanged(int value) {
    CustomSettings::instance()->setPanelOpacity(value);
}

void MainSettingWidget::panelColorButtonClicked() {
    QColor newPanelBgColor = QColorDialog::getColor(CustomSettings::instance()->getPanelBgColor());
    ui->panelColorlabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(newPanelBgColor.name()));
    CustomSettings::instance()->setPanelBgColor(newPanelBgColor);
}

void MainSettingWidget::fontColorButtonClicked() {
    QColor newFontColor = QColorDialog::getColor(CustomSettings::instance()->getActiveFontColor());
    ui->fontColorLabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(newFontColor.name()));
    CustomSettings::instance()->setActiveFontColor(newFontColor);

}

void MainSettingWidget::closeButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select your close button icon"), "~", tr("Images (*.png *.jpg *.svg)"));
    if (!fileName.isNull()) {
        ui->closeButtonLabel->setPixmap(QIcon(fileName).pixmap(ui->closeButtonLabel->size()));
        CustomSettings::instance()->setActiveCloseIconPath(fileName);
    }
}

void MainSettingWidget::closeResetButtonClicked() {
    CustomSettings::instance()->resetCloseIconPath();
    ui->closeButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveCloseIconPath()).pixmap(ui->closeButtonLabel->size()));
}

void MainSettingWidget::unmaxButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select your unmaximized button icon"), "~", tr("Images (*.png *.jpg *.svg)"));
    if (!fileName.isNull()) {
        ui->unmaxButtonLabel->setPixmap(QIcon(fileName).pixmap(ui->unmaxButtonLabel->size()));
        CustomSettings::instance()->setActiveUnmaximizedIconPath(fileName);
    }
}

void MainSettingWidget::unmaxResetButtonClicked() {
    CustomSettings::instance()->resetUnmaxIconPath();
    ui->unmaxButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveUnmaximizedIconPath()).pixmap(ui->unmaxButtonLabel->size()));
}

void MainSettingWidget::minButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select your minimized button icon"), "~", tr("Images (*.png *.jpg *.svg)"));
    if (!fileName.isNull()) {
        ui->minButtonLabel->setPixmap(QIcon(fileName).pixmap(ui->minButtonLabel->size()));
        CustomSettings::instance()->setActiveMinimizedIconPath(fileName);
    }
}

void MainSettingWidget::minResetButtonClicked() {
    CustomSettings::instance()->resetMinIconPath();
    ui->minButtonLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveMinimizedIconPath()).pixmap(ui->minButtonLabel->size()));
}

void MainSettingWidget::defaultButtonClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select your default icon"), "~", tr("Images (*.png *.jpg *.svg)"));
    if (!fileName.isNull()) {
        ui->defaultIconLabel->setPixmap(QIcon(fileName).pixmap(ui->defaultIconLabel->size()));
        CustomSettings::instance()->setActiveDefaultAppIconPath(fileName);
    }
}

void MainSettingWidget::defaultResetButtonClicked() {
    CustomSettings::instance()->resetDefaultIconPath();
    ui->defaultIconLabel->setPixmap(QIcon(CustomSettings::instance()->getActiveDefaultAppIconPath()).pixmap(ui->defaultIconLabel->size()));
}

void MainSettingWidget::showEvent(QShowEvent *event) {
    movie->start();
    QWidget::showEvent(event);
}

void MainSettingWidget::hideEvent(QHideEvent *event) {
    movie->stop();
    QWidget::hideEvent(event);
}

void MainSettingWidget::closeEvent(QCloseEvent *event) {
    movie->stop();
    QWidget::closeEvent(event);
}

void MainSettingWidget::buttonHighlightColorButtonClicked() {
    QColor buttonHighLightColor = QColorDialog::getColor(CustomSettings::instance()->getButtonHighLightColor());
    ui->buttonHighlightColorLabel->setStyleSheet(QString("QLabel {background-color: %1;}").arg(buttonHighLightColor.name()));
    CustomSettings::instance()->setButtonHighLightColor(buttonHighLightColor);
}
