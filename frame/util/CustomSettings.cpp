//
// Created by septemberhx on 2020/6/5.
//

#include "CustomSettings.h"
#include "clockformat.h"
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QScopedValueRollback>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>
#include <DPlatformTheme>
#include <DGuiApplicationHelper>
#include <DSysInfo>

DGUI_USE_NAMESPACE

namespace {
constexpr auto SettingsObjectPath = "/com/gxde/TopPanel/Settings";
constexpr auto SettingsInterface = "com.gxde.TopPanel.Settings";
}

CustomSettings::CustomSettings() {
    this->defaultIconPathLight = ":/icons/linux.svg";
    this->defaultIconPathDark = ":/icons/linux-light.svg";

    this->setDefaultPanelOpacity();
    this->setDefaultPanelBgColor();

    this->setDefaultActiveFont();
    this->setDefaultActiveFontColor();
    this->setDefaultActiveCloseIconPath();
    this->setDefaultActiveDefaultAppIconPath();
    this->setDefaultActiveMinimizedIconPath();
    this->setDefaultActiveUnmaximizedIconPath();
    this->setDefaultShowGlobalMenuOnHover();

    this->showAppNameInsteadIcon = true;
    this->showControlButtons = true;
    this->showLogoWithAppName = true;
    this->ignoreDock = false;
    this->buttonOnLeft = true;

    this->buttonHighlight = true;
    this->buttonHighLightColor = QColor("#9d2933");
    this->followSystemTheme = true;

    this->defaultDarkColor = Qt::black;
    this->defaultLightColor = Qt::white;

    systemIconTheme = QIcon::themeName();
    this->readSettings();
    connect(this, &CustomSettings::settingsChanged, this, &CustomSettings::saveSettings);
    connect(DGuiApplicationHelper::instance()->applicationTheme(),
            &DPlatformTheme::iconThemeNameChanged, this, [this](const QByteArray &name) {
        systemIconTheme = QString::fromUtf8(name);
        applyPanelTheme();
    });

    // The Wayland settings window is a separate, long-lived process. Notify
    // the panel only after QSettings has committed the new values to disk.
    QDBusConnection::sessionBus().connect(QString(), SettingsObjectPath,
        SettingsInterface, "Changed", this, SLOT(reloadSettings(QString)));

    // Also handle external edits, both in-place writes and atomic replacement.
    const QString fileName = QSettings("dde-top-panel", "top-panel").fileName();
    const QString directory = QFileInfo(fileName).absolutePath();
    QDir().mkpath(directory);
    auto *watcher = new QFileSystemWatcher(this);
    watcher->addPath(directory);
    if (QFileInfo::exists(fileName))
        watcher->addPath(fileName);
    auto *reloadTimer = new QTimer(this);
    reloadTimer->setSingleShot(true);
    reloadTimer->setInterval(25);
    connect(watcher, &QFileSystemWatcher::directoryChanged, reloadTimer,
            [reloadTimer] { reloadTimer->start(); });
    connect(watcher, &QFileSystemWatcher::fileChanged, reloadTimer,
            [reloadTimer] { reloadTimer->start(); });
    connect(reloadTimer, &QTimer::timeout, this, [this, watcher, fileName] {
        if (!watcher->files().contains(fileName) && QFileInfo::exists(fileName))
            watcher->addPath(fileName);
        reloadSettings(fileName);
    });
}

void CustomSettings::reloadSettings(const QString &fileName) {
    if (fileName != QSettings("dde-top-panel", "top-panel").fileName())
        return;
    QScopedValueRollback<bool> reloading(reloadingSettings, true);
    readSettings();
    emit settingsChanged();
}

CustomSettings *CustomSettings::instance() {
    static CustomSettings customSettings;
    return &customSettings;
}

qreal CustomSettings::getPanelOpacity() const {
    if (this->isUseDarkDtkPanel() || this->isFollowSystemTheme()) {
        return 80;
    } else {
        return panelOpacity;
    }
}

void CustomSettings::setPanelOpacity(qreal panelOpacity) {
    CustomSettings::panelOpacity = panelOpacity;
    emit settingsChanged();
}

const QColor &CustomSettings::getPanelBgColor() const {
    if (this->isUseDarkDtkPanel() || this->isFollowSystemTheme()) {
        switch (DGuiApplicationHelper::instance()->themeType()) {
            case Dtk::Gui::DGuiApplicationHelper::DarkType:
                return this->defaultDarkColor;
            case Dtk::Gui::DGuiApplicationHelper::LightType:
                return this->defaultLightColor;
        }
    }
    return panelBgColor;
}

void CustomSettings::setPanelBgColor(const QColor &panelBgColor) {
    CustomSettings::panelBgColor = panelBgColor;
    emit settingsChanged();
}

bool CustomSettings::isPanelEnablePluginsOnAllScreen() const {
    return panelEnablePluginsOnAllScreen;
}

void CustomSettings::setPanelEnablePluginsOnAllScreen(bool panelEnablePluginsOnAllScreen) {
    CustomSettings::panelEnablePluginsOnAllScreen = panelEnablePluginsOnAllScreen;
    emit settingsChanged();
}

const QColor &CustomSettings::getActiveFontColor() const {
    return DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
        ? defaultLightColor : defaultDarkColor;
}

void CustomSettings::setActiveFontColor(const QColor &activeFontColor) {
    CustomSettings::activeFontColor = activeFontColor;
    emit settingsChanged();
}

const QFont &CustomSettings::getActiveFont() const {
    return activeFont;
}

void CustomSettings::setActiveFont(const QFont &activeFont) {
    CustomSettings::activeFont = activeFont;
    emit settingsChanged();
}

const QString &CustomSettings::getActiveCloseIconPath() const {
    static const QString lightIcon = QStringLiteral(":/icons/close.svg");
    static const QString darkIcon = QStringLiteral(":/icons/close-dark.svg");
    if (activeCloseIconPath == lightIcon || activeCloseIconPath == darkIcon)
        return DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
            ? lightIcon : darkIcon;
    return activeCloseIconPath;
}

void CustomSettings::setActiveCloseIconPath(const QString &activeCloseIconPath) {
    CustomSettings::activeCloseIconPath = activeCloseIconPath;
    emit settingsChanged();
}

const QString &CustomSettings::getActiveUnmaximizedIconPath() const {
    static const QString lightIcon = QStringLiteral(":/icons/maximum.svg");
    static const QString darkIcon = QStringLiteral(":/icons/maximum-dark.svg");
    if (activeUnmaximizedIconPath == lightIcon || activeUnmaximizedIconPath == darkIcon)
        return DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
            ? lightIcon : darkIcon;
    return activeUnmaximizedIconPath;
}

void CustomSettings::setActiveUnmaximizedIconPath(const QString &activeUnmaximizedIconPath) {
    CustomSettings::activeUnmaximizedIconPath = activeUnmaximizedIconPath;
    emit settingsChanged();
}

const QString &CustomSettings::getActiveMinimizedIconPath() const {
    static const QString lightIcon = QStringLiteral(":/icons/minimum.svg");
    static const QString darkIcon = QStringLiteral(":/icons/minimum-dark.svg");
    if (activeMinimizedIconPath == lightIcon || activeMinimizedIconPath == darkIcon)
        return DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType
            ? lightIcon : darkIcon;
    return activeMinimizedIconPath;
}

void CustomSettings::setActiveMinimizedIconPath(const QString &activeMinimizedIconPath) {
    CustomSettings::activeMinimizedIconPath = activeMinimizedIconPath;
    emit settingsChanged();
}

const QString &CustomSettings::getActiveDefaultAppIconPath() const {
    if (activeDefaultAppIconPath == defaultIconPathLight || activeDefaultAppIconPath == defaultIconPathDark) {
        switch (DGuiApplicationHelper::instance()->themeType()) {
            case Dtk::Gui::DGuiApplicationHelper::DarkType: 
                return this->defaultIconPathDark;
            case Dtk::Gui::DGuiApplicationHelper::LightType:
                return this->defaultIconPathLight;
        }
    }
    return activeDefaultAppIconPath;
}

void CustomSettings::setActiveDefaultAppIconPath(const QString &activeDefaultAppIconPath) {
    CustomSettings::activeDefaultAppIconPath = activeDefaultAppIconPath;
    emit settingsChanged();
}

void CustomSettings::setDefaultActiveCloseIconPath() {
    this->activeCloseIconPath = ":/icons/close.svg";
}

void CustomSettings::setDefaultActiveUnmaximizedIconPath() {
    this->activeUnmaximizedIconPath = ":/icons/maximum.svg";
}

void CustomSettings::setDefaultActiveMinimizedIconPath() {
    this->activeMinimizedIconPath = ":/icons/minimum.svg";
}

void CustomSettings::setDefaultActiveDefaultAppIconPath() {
    this->activeDefaultAppIconPath = ":/icons/linux.svg";
}

void CustomSettings::setDefaultActiveFont() {
    this->activeFont = QFont();
}

void CustomSettings::setDefaultActiveFontColor() {
    this->activeFontColor = Qt::black;
}

void CustomSettings::setDefaultPanelBgColor() {
    this->panelBgColor = Qt::white;
}

void CustomSettings::setDefaultPanelOpacity() {
    this->panelOpacity = 50;
}

void CustomSettings::resetCloseIconPath() {
    this->setDefaultActiveCloseIconPath();
    emit settingsChanged();
}

void CustomSettings::resetUnmaxIconPath() {
    this->setDefaultActiveUnmaximizedIconPath();
    emit settingsChanged();
}

void CustomSettings::resetMinIconPath() {
    this->setDefaultActiveMinimizedIconPath();
    emit settingsChanged();
}

void CustomSettings::resetDefaultIconPath() {
    this->setDefaultActiveDefaultAppIconPath();
    emit settingsChanged();
}

void CustomSettings::setDefaultShowGlobalMenuOnHover() {
    this->showGlobalMenuOnHover = false;
}

bool CustomSettings::isShowGlobalMenuOnHover() const {
    return showGlobalMenuOnHover;
}

void CustomSettings::setShowGlobalMenuOnHover(bool showGlobalMenuOnHover) {
    CustomSettings::showGlobalMenuOnHover = showGlobalMenuOnHover;
    emit settingsChanged();
}

void CustomSettings::saveSettings() {
    if (reloadingSettings)
        return;

    QSettings settings("dde-top-panel", "top-panel");

    settings.setValue("panel/bgColor", this->panelBgColor);
    settings.setValue("panel/opacity", this->panelOpacity);
    settings.setValue("panel/followSystemTheme", this->isFollowSystemTheme());
    settings.setValue("panel/useDarkDtkPanel", useDarkDtkPanel);
    settings.setValue("panel/newUiEnabled", newUiEnabled);
    settings.setValue("clock/customEnabled", customClockEnabled);
    settings.setValue("clock/use12Hour", clock12Hour);
    settings.setValue("clock/format", clockFormat);
    settings.setValue("windowControl/fontColor", this->activeFontColor);
    settings.setValue("windowControl/closeIcon", this->activeCloseIconPath);
    settings.setValue("windowControl/unmaxIcon", this->activeUnmaximizedIconPath);
    settings.setValue("windowControl/minIcon", this->activeMinimizedIconPath);
    settings.setValue("windowControl/defaultIcon", this->activeDefaultAppIconPath);
    settings.setValue("windowControl/showMenuOnHover", this->isShowGlobalMenuOnHover());
    settings.setValue("windowControl/showControlButtons", this->isShowControlButtons());
    settings.setValue("windowControl/showAppNameInsteadIcon", this->isShowAppNameInsteadIcon());
    settings.setValue("windowControl/alwaysUseDefaultIcon", alwaysUseDefaultIcon);
    settings.setValue("windowControl/showLogoWithAppName", this->isShowLogoWithAppName());
    settings.setValue("windowControl/ignoreDock", this->isIgnoreDock());
    settings.setValue("windowControl/buttonOnRight", !this->isButtonOnLeft());
    settings.setValue("windowControl/enableButtonHighlight", this->isButtonHighlight());
    settings.setValue("windowControl/buttonHighlightColor", this->buttonHighLightColor);
    settings.setValue("windowControl/allowDragWindowWhenMax", this->allowDragWindowWhenMax);

    settings.sync();
    if (settings.status() == QSettings::NoError) {
        QDBusMessage notification = QDBusMessage::createSignal(
            SettingsObjectPath, SettingsInterface, "Changed");
        notification << settings.fileName();
        QDBusConnection::sessionBus().send(notification);
    }

    QSettings kwinrc(getConfigPath(), QSettings::IniFormat);
    kwinrc.setValue("Windows/BorderlessMaximizedWindows", this->hideTitleWhenMax);
}

void CustomSettings::readSettings() {
    QSettings settings("dde-top-panel", "top-panel");
    settings.sync();
    this->panelBgColor = settings.value("panel/bgColor", this->panelBgColor).value<QColor>();
    this->panelOpacity = settings.value("panel/opacity", this->panelOpacity).toUInt();
    this->activeFontColor = settings.value("windowControl/fontColor", this->activeFontColor).value<QColor>();
    this->activeCloseIconPath = settings.value("windowControl/closeIcon", this->activeCloseIconPath).toString();
    this->activeUnmaximizedIconPath = settings.value("windowControl/unmaxIcon", this->activeUnmaximizedIconPath).toString();
    this->activeMinimizedIconPath = settings.value("windowControl/minIcon", this->activeMinimizedIconPath).toString();
    this->activeDefaultAppIconPath = settings.value("windowControl/defaultIcon", this->activeDefaultAppIconPath).toString();
    this->showGlobalMenuOnHover = settings.value("windowControl/showMenuOnHover", this->showGlobalMenuOnHover).toBool();
    this->showControlButtons = settings.value("windowControl/showControlButtons", this->showControlButtons).toBool();
    this->showAppNameInsteadIcon = settings.value("windowControl/showAppNameInsteadIcon", this->showAppNameInsteadIcon).toBool();
    alwaysUseDefaultIcon = settings.value("windowControl/alwaysUseDefaultIcon", false).toBool();
    this->showLogoWithAppName = settings.value("windowControl/showLogoWithAppName", this->showLogoWithAppName).toBool();
    this->ignoreDock = settings.value("windowControl/ignoreDock", this->isIgnoreDock()).toBool();
    this->buttonOnLeft = !settings.value("windowControl/buttonOnRight", !this->isButtonOnLeft()).toBool();

    this->buttonHighlight = settings.value("windowControl/enableButtonHighlight", this->isButtonHighlight()).toBool();
    this->buttonHighLightColor = settings.value("windowControl/buttonHighlightColor", this->buttonHighLightColor).value<QColor>();
    this->followSystemTheme = settings.value("panel/followSystemTheme", this->isFollowSystemTheme()).toBool();
    useDarkDtkPanel = settings.value("panel/useDarkDtkPanel", false).toBool();
    if (useDarkDtkPanel && followSystemTheme) {
        followSystemTheme = false;
        settings.setValue("panel/followSystemTheme", false);
        settings.sync();
    }
    newUiEnabled = settings.value("panel/newUiEnabled", true).toBool();
    clock12Hour = settings.value("clock/use12Hour", false).toBool();
    customClockEnabled = settings.value("clock/customEnabled", false).toBool();
    clockFormat = ClockFormat::normalize(settings.value("clock/format", QStringLiteral("hh:mm\\nMM-DD ddd")).toString());
    this->allowDragWindowWhenMax = settings.value("windowControl/allowDragWindowWhenMax", this->allowDragWindowWhenMax).toBool();

    QSettings kwinrc(getConfigPath(), QSettings::IniFormat);
    this->hideTitleWhenMax = kwinrc.value("Windows/BorderlessMaximizedWindows", false).toBool();
    applyPanelTheme();
}

bool CustomSettings::isShowControlButtons() const {
    return showControlButtons;
}

void CustomSettings::setShowControlButtons(bool showControlButtons) {
    CustomSettings::showControlButtons = showControlButtons;
    emit settingsChanged();
}

bool CustomSettings::isShowAppNameInsteadIcon() const {
    return showAppNameInsteadIcon;
}

void CustomSettings::setShowAppNameInsteadIcon(bool showAppNameInsteadIcon) {
    CustomSettings::showAppNameInsteadIcon = showAppNameInsteadIcon;
    emit settingsChanged();
}

bool CustomSettings::isShowLogoWithAppName() const {
    return showLogoWithAppName;
}

bool CustomSettings::isAlwaysUseDefaultIcon() const {
    return alwaysUseDefaultIcon;
}

void CustomSettings::setAlwaysUseDefaultIcon(bool enabled) {
    if (alwaysUseDefaultIcon == enabled)
        return;
    alwaysUseDefaultIcon = enabled;
    emit settingsChanged();
}

void CustomSettings::setShowLogoWithAppName(bool showLogoWithAppName) {
    CustomSettings::showLogoWithAppName = showLogoWithAppName;
    emit settingsChanged();
}

bool CustomSettings::isIgnoreDock() const {
    return ignoreDock;
}

void CustomSettings::setIgnoreDock(bool ignoreDock) {
    CustomSettings::ignoreDock = ignoreDock;
    emit settingsChanged();
}

int CustomSettings::getPanelHeight() const {
    // Match Husky-Panel's barHeight in logical pixels; Qt handles scaling.
    return newUiEnabled ? 32 : 24;
}

bool CustomSettings::isNewUiEnabled() const {
    return newUiEnabled;
}

void CustomSettings::setNewUiEnabled(bool enabled) {
    if (newUiEnabled == enabled)
        return;
    newUiEnabled = enabled;
    emit settingsChanged();
}

bool CustomSettings::isButtonOnLeft() const {
    return buttonOnLeft;
}

void CustomSettings::setButtonOnLeft(bool buttonOnLeft) {
    CustomSettings::buttonOnLeft = buttonOnLeft;
    emit settingsChanged();
}

bool CustomSettings::isButtonHighlight() const {
    return buttonHighlight;
}

void CustomSettings::setButtonHighlight(bool buttonHighlight) {
    CustomSettings::buttonHighlight = buttonHighlight;
    emit settingsChanged();
}

const QColor &CustomSettings::getButtonHighLightColor() const {
    return buttonHighLightColor;
}

void CustomSettings::setButtonHighLightColor(const QColor &buttonHighLightColor) {
    CustomSettings::buttonHighLightColor = buttonHighLightColor;
    emit settingsChanged();
}

bool CustomSettings::isHideTitleWhenMax() const {
    return hideTitleWhenMax;
}

void CustomSettings::setHideTitleWhenMax(bool hideTitleWhenMax) {
    CustomSettings::hideTitleWhenMax = hideTitleWhenMax;
    emit settingsChanged();
}

bool CustomSettings::isFollowSystemTheme() const {
    return followSystemTheme;
}

void CustomSettings::setFollowSystemTheme(bool followSystemTheme) {
    if (CustomSettings::followSystemTheme == followSystemTheme
        && !(followSystemTheme && useDarkDtkPanel))
        return;
    CustomSettings::followSystemTheme = followSystemTheme;
    if (followSystemTheme)
        useDarkDtkPanel = false;
    applyPanelTheme();
    emit settingsChanged();
}

bool CustomSettings::isAllowDragWindowWhenMax() const {
    return allowDragWindowWhenMax;
}

void CustomSettings::setAllowDragWindowWhenMax(bool allowDragWindowWhenMax) {
    CustomSettings::allowDragWindowWhenMax = allowDragWindowWhenMax;
    emit settingsChanged();
}

QString CustomSettings::getConfigPath() {
    QString version = Dtk::Core::DSysInfo::deepinVersion();
    QString configPath = QDir::homePath();
    if (version == "20") {
        configPath += "/.config/kwinrc";
    } else if(version == "23"){
        configPath += "/.config/deepin-kwinrc";
    }
    return configPath;
}

bool CustomSettings::isUseDarkDtkPanel() const {
    return useDarkDtkPanel;
}

void CustomSettings::setUseDarkDtkPanel(bool enabled) {
    if (useDarkDtkPanel == enabled && !(enabled && followSystemTheme))
        return;
    useDarkDtkPanel = enabled;
    if (enabled)
        followSystemTheme = false;
    applyPanelTheme();
    emit settingsChanged();
}

void CustomSettings::applyPanelTheme() {
    auto *helper = DGuiApplicationHelper::instance();
    helper->setPaletteType(useDarkDtkPanel ? DGuiApplicationHelper::DarkType
                                         : (followSystemTheme ? DGuiApplicationHelper::UnknownType
                                                              : DGuiApplicationHelper::LightType));
    // Set the process-local icon theme so plugins using QIcon::fromTheme also
    // select the dark variants, without changing the desktop's icon theme.
    const QString iconTheme = useDarkDtkPanel ? QStringLiteral("gxde-dark") : systemIconTheme;
    if (QIcon::themeName() != iconTheme) {
        QIcon::setThemeName(iconTheme);
        emit panelThemeChanged();
    }
}

void CustomSettings::setCustomClockEnabled(bool enabled) {
    if (customClockEnabled == enabled)
        return;
    customClockEnabled = enabled;
    emit settingsChanged();
}

void CustomSettings::setClockFormat(const QString &format) {
    const QString normalized = ClockFormat::normalize(format);
    if (clockFormat == normalized)
        return;
    clockFormat = normalized;
    emit settingsChanged();
}

void CustomSettings::setClock12Hour(bool enabled) {
    if (clock12Hour == enabled) {
        return;
    }

    clock12Hour = enabled;
    emit settingsChanged();
}
