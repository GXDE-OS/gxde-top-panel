#include "mainsettingwidget.h"
#include "../frame/util/CustomSettings.h"
#include "../frame/util/clockformat.h"

#include <DTitlebar>
#include <DApplication>
#include <DSettingsDialog>
#include <DIconButton>
#include <DGuiApplicationHelper>
#include <DSettings>
#include <dsettingsbackend.h>
#include <DSettingsOption>
#include <DSettingsWidgetFactory>

#include <QCheckBox>
#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

namespace {

// 设置项文字沿用旧设置窗的翻译上下文，已有的中文翻译可以直接复用
constexpr char TranslateContext[] = "MainSettingWidget";

QString tr(const char *text)
{
    return QCoreApplication::translate(TranslateContext, text);
}

CustomSettings *cs()
{
    return CustomSettings::instance();
}

// DSettings 的存储后端：每个选项直接映射到 CustomSettings 的读写方法。
// CustomSettings 负责落盘，面板进程监听配置文件后实时生效。
class PanelSettingsBackend : public DSettingsBackend
{
public:
    struct Accessor {
        std::function<QVariant()> get;
        std::function<void(const QVariant &)> set;
    };

    explicit PanelSettingsBackend(QObject *parent = nullptr)
        : DSettingsBackend(parent)
    {
        const auto boolOpt = [](std::function<bool()> get, std::function<void(bool)> set) {
            return Accessor { [get] { return QVariant(get()); },
                              [set](const QVariant &v) { set(v.toBool()); } };
        };
        const auto strOpt = [](std::function<QString()> get, std::function<void(const QString &)> set) {
            return Accessor { [get] { return QVariant(get()); },
                              [set](const QVariant &v) { set(v.toString()); } };
        };

        m_accessors = {
            { "options.general.panelColor",
              strOpt([] { return cs()->getPanelBgColor().name(); },
                     [](const QString &v) { cs()->setPanelBgColor(QColor(v)); }) },
            { "options.general.fontColor",
              strOpt([] { return cs()->getActiveFontColor().name(); },
                     [](const QString &v) { cs()->setActiveFontColor(QColor(v)); }) },
            { "options.general.opacity",
              { [] { return QVariant(int(cs()->getPanelOpacity())); },
                [](const QVariant &v) { cs()->setPanelOpacity(v.toInt()); } } },
            { "options.general.followSystemTheme",
              boolOpt([] { return cs()->isFollowSystemTheme(); },
                      [](bool v) { cs()->setFollowSystemTheme(v); }) },
            { "options.general.newUi",
              boolOpt([] { return cs()->isNewUiEnabled(); },
                      [](bool v) { cs()->setNewUiEnabled(v); }) },
            // 显示的是面板实际深浅；跟随系统时 setUseDarkDtkPanel 不生效（控件也被禁用）
            { "options.general.darkPanel",
              boolOpt([] { return cs()->isDarkPanel(); },
                      [](bool v) { cs()->setUseDarkDtkPanel(v); }) },
            // V23 / V25 互斥：两个选项共用 iconStyle 一个值
            { "options.general.dde23Icons",
              boolOpt([] { return cs()->getIconStyle() == QLatin1String("dde23"); },
                      [](bool v) {
                          if (v)
                              cs()->setIconStyle(QStringLiteral("dde23"));
                          else if (cs()->getIconStyle() == QLatin1String("dde23"))
                              cs()->setIconStyle(QString());
                      }) },
            { "options.general.dde25Icons",
              boolOpt([] { return cs()->getIconStyle() == QLatin1String("dde25"); },
                      [](bool v) {
                          if (v)
                              cs()->setIconStyle(QStringLiteral("dde25"));
                          else if (cs()->getIconStyle() == QLatin1String("dde25"))
                              cs()->setIconStyle(QString());
                      }) },
            { "options.general.ignoreDock",
              boolOpt([] { return cs()->isIgnoreDock(); },
                      [](bool v) { cs()->setIgnoreDock(v); }) },
            { "options.general.menuOnHover",
              boolOpt([] { return cs()->isShowGlobalMenuOnHover(); },
                      [](bool v) { cs()->setShowGlobalMenuOnHover(v); }) },
            { "options.general.dragToMove",
              boolOpt([] { return cs()->isAllowDragWindowWhenMax(); },
                      [](bool v) { cs()->setAllowDragWindowWhenMax(v); }) },
            { "options.general.hideTitlebar",
              boolOpt([] { return cs()->isHideTitleWhenMax(); },
                      [](bool v) { cs()->setHideTitleWhenMax(v); }) },

            { "options.buttons.showButtons",
              boolOpt([] { return cs()->isShowControlButtons(); },
                      [](bool v) { cs()->setShowControlButtons(v); }) },
            // 图标项的空字符串表示「重置为默认」
            { "options.buttons.closeIcon",
              strOpt([] { return cs()->getActiveCloseIconPath(); },
                     [](const QString &v) {
                         v.isEmpty() ? cs()->resetCloseIconPath() : cs()->setActiveCloseIconPath(v);
                     }) },
            { "options.buttons.unmaxIcon",
              strOpt([] { return cs()->getActiveUnmaximizedIconPath(); },
                     [](const QString &v) {
                         v.isEmpty() ? cs()->resetUnmaxIconPath() : cs()->setActiveUnmaximizedIconPath(v);
                     }) },
            { "options.buttons.minIcon",
              strOpt([] { return cs()->getActiveMinimizedIconPath(); },
                     [](const QString &v) {
                         v.isEmpty() ? cs()->resetMinIconPath() : cs()->setActiveMinimizedIconPath(v);
                     }) },
            { "options.buttons.buttonsOnRight",
              boolOpt([] { return !cs()->isButtonOnLeft(); },
                      [](bool v) { cs()->setButtonOnLeft(!v); }) },
            { "options.buttons.buttonHighlight",
              boolOpt([] { return cs()->isButtonHighlight(); },
                      [](bool v) { cs()->setButtonHighlight(v); }) },
            { "options.buttons.highlightColor",
              strOpt([] { return cs()->getButtonHighLightColor().name(); },
                     [](const QString &v) { cs()->setButtonHighLightColor(QColor(v)); }) },

            { "options.appIcons.defaultIcon",
              strOpt([] { return cs()->getActiveDefaultAppIconPath(); },
                     [](const QString &v) {
                         v.isEmpty() ? cs()->resetDefaultIconPath() : cs()->setActiveDefaultAppIconPath(v);
                     }) },
            { "options.appIcons.alwaysDefaultIcon",
              boolOpt([] { return cs()->isAlwaysUseDefaultIcon(); },
                      [](bool v) { cs()->setAlwaysUseDefaultIcon(v); }) },
            { "options.appIcons.appNameInsteadIcon",
              boolOpt([] { return cs()->isShowAppNameInsteadIcon(); },
                      [](bool v) { cs()->setShowAppNameInsteadIcon(v); }) },
            { "options.appIcons.logoWithAppName",
              boolOpt([] { return cs()->isShowLogoWithAppName(); },
                      [](bool v) { cs()->setShowLogoWithAppName(v); }) },

            { "clock.format.customEnabled",
              boolOpt([] { return cs()->isCustomClockEnabled(); },
                      [](bool v) { cs()->setCustomClockEnabled(v); }) },
            { "clock.format.use12Hour",
              boolOpt([] { return cs()->isClock12Hour(); },
                      [](bool v) { cs()->setClock12Hour(v); }) },
            { "clock.format.format",
              strOpt([] { return cs()->getClockFormat(); },
                     [](const QString &v) { cs()->setClockFormat(v); }) },
        };

        for (auto it = m_accessors.cbegin(); it != m_accessors.cend(); ++it)
            m_cache.insert(it.key(), it.value().get());

        // 设置在别处被改（面板进程、联动的选项、跟随系统主题切换），把变化推回界面
        connect(cs(), &CustomSettings::settingsChanged, this, [this] {
            for (auto it = m_accessors.cbegin(); it != m_accessors.cend(); ++it) {
                const QVariant value = it.value().get();
                if (m_cache.value(it.key()) != value) {
                    m_cache.insert(it.key(), value);
                    Q_EMIT optionChanged(it.key(), value);
                }
            }
        });
    }

    QStringList keys() const override { return m_accessors.keys(); }

    QVariant getOption(const QString &key) const override
    {
        const auto it = m_accessors.constFind(key);
        return it == m_accessors.cend() ? QVariant() : it.value().get();
    }

    void doSync() override {}

protected:
    void doSetOption(const QString &key, const QVariant &value) override
    {
        const auto it = m_accessors.constFind(key);
        if (it == m_accessors.cend() || it.value().get() == value)
            return;
        it.value().set(value);
    }

private:
    QMap<QString, Accessor> m_accessors;
    QMap<QString, QVariant> m_cache;
};

// DTK 原生扁平图标按钮。图标沿用面板自带的 SVG：
// xxx.svg 为浅色图形（深色主题用），xxx-dark.svg 为深色图形（浅色主题用）
DIconButton *makeIconButton(QWidget *parent, const QString &iconName, const QString &toolTip)
{
    auto *button = new DIconButton(parent);
    button->setFlat(true);
    button->setIconSize(QSize(14, 14));
    button->setFixedSize(26, 26);
    button->setToolTip(toolTip);

    const auto applyIcon = [button, iconName] {
        const bool dark = DGuiApplicationHelper::instance()->themeType()
            == DGuiApplicationHelper::DarkType;
        button->setIcon(QIcon(QStringLiteral(":/icons/%1%2.svg")
                                  .arg(iconName, dark ? QString() : QStringLiteral("-dark"))));
    };
    applyIcon();
    QObject::connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
                     button, applyIcon);
    return button;
}

// 窗口按钮/默认应用图标：预览 + 选择文件 + 重置
QPair<QWidget *, QWidget *> createIconPicker(QObject *obj)
{
    auto *option = qobject_cast<DSettingsOption *>(obj);
    auto *box = new QWidget;
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *preview = new QLabel(box);
    preview->setFixedSize(16, 16);
    auto *choose = makeIconButton(box, QStringLiteral("config"), tr("Config"));
    auto *reset = makeIconButton(box, QStringLiteral("reset"), tr("Reset"));
    layout->addStretch();
    layout->addWidget(preview);
    layout->addWidget(choose);
    layout->addWidget(reset);

    const auto refresh = [preview](const QVariant &value) {
        preview->setPixmap(QIcon(value.toString()).pixmap(preview->size()));
    };
    refresh(option->value());
    QObject::connect(option, &DSettingsOption::valueChanged, preview, refresh);

    static const QMap<QString, const char *> titles {
        { "options.buttons.closeIcon", "Select your close button icon" },
        { "options.buttons.unmaxIcon", "Select your unmaximized button icon" },
        { "options.buttons.minIcon", "Select your minimized button icon" },
        { "options.appIcons.defaultIcon", "Select your default icon" },
    };
    const QString title = tr(titles.value(option->key(), "Select your default icon"));
    QObject::connect(choose, &DIconButton::clicked, box, [option, box, title] {
        const QString file = QFileDialog::getOpenFileName(box, title, QStringLiteral("~"),
                                                          tr("Images (*.png *.jpg *.svg)"));
        if (!file.isEmpty())
            option->setValue(file);
    });
    // 后端把空字符串解释为「恢复默认」，写入后读回来的就是默认图标路径
    QObject::connect(reset, &DIconButton::clicked, box, [option] { option->setValue(QString()); });

    return DSettingsWidgetFactory::createStandardItem(TranslateContext, option, box);
}

// 颜色：色块按钮，点开颜色对话框
QPair<QWidget *, QWidget *> createColorPicker(QObject *obj)
{
    auto *option = qobject_cast<DSettingsOption *>(obj);
    auto *button = new QToolButton;
    button->setFixedSize(24, 24);
    button->setToolTip(tr("Config"));
    // 面板背景色由深浅模式决定，这里只显示当前颜色，不可修改
    if (option->key() == QLatin1String("options.general.panelColor"))
        button->setEnabled(false);
    const auto refresh = [button](const QVariant &value) {
        button->setStyleSheet(QStringLiteral("QToolButton { background-color: %1; border-radius: 4px; }")
                                  .arg(value.toString()));
    };
    refresh(option->value());
    QObject::connect(option, &DSettingsOption::valueChanged, button, refresh);
    QObject::connect(button, &QToolButton::clicked, button, [option, button] {
        const QColor color = QColorDialog::getColor(QColor(option->value().toString()), button);
        if (color.isValid())
            option->setValue(color.name());
    });
    return DSettingsWidgetFactory::createStandardItem(TranslateContext, option, button);
}

// 带联动的勾选框：可用状态 / 提示文字随其他设置变化
QPair<QWidget *, QWidget *> createLinkedCheckBox(QObject *obj)
{
    auto *option = qobject_cast<DSettingsOption *>(obj);
    auto *checkBox = new QCheckBox;
    const QString key = option->key();

    if (key == QLatin1String("options.general.ignoreDock")) {
        checkBox->setToolTip(tr("Ignore the dde-dock window. \n"
                                "It is used to solve the desktop icon occlusion problem.\n"
                                "Only works when dde-dock is not running.\n\n"
                                "If anything strange happens, please uncheck this, and restart the dde-dock."));
    }

    const auto syncEnabled = [checkBox, key] {
        if (key == QLatin1String("options.general.darkPanel")) {
            // 跟随系统主题时深浅由系统决定，禁止手动修改
            checkBox->setEnabled(!cs()->isFollowSystemTheme());
        } else if (key == QLatin1String("clock.format.use12Hour")) {
            checkBox->setEnabled(cs()->isCustomClockEnabled());
        }
    };
    const auto syncChecked = [checkBox](const QVariant &value) {
        const QSignalBlocker blocker(checkBox);
        checkBox->setChecked(value.toBool());
    };
    syncChecked(option->value());
    syncEnabled();
    QObject::connect(option, &DSettingsOption::valueChanged, checkBox, syncChecked);
    QObject::connect(cs(), &CustomSettings::settingsChanged, checkBox, syncEnabled);
    QObject::connect(checkBox, &QCheckBox::toggled, checkBox, [option](bool checked) {
        option->setValue(checked);
    });
    // 勾选框统一靠右，和颜色块、图标按钮对齐，不随标签长度错位
    auto *box = new QWidget;
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    layout->addWidget(checkBox);
    return DSettingsWidgetFactory::createStandardItem(TranslateContext, option, box);
}

// 自定义时钟格式：输入框 + 说明 + 实时预览
QWidget *createClockFormat(QObject *obj)
{
    auto *option = qobject_cast<DSettingsOption *>(obj);
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *edit = new QLineEdit(page);
    edit->setMaxLength(256);
    auto *help = new QLabel(tr("Use Y or YYYY for the full year; use YY for the two-digit year\n"
                               "M represents the month; D represents the day; ddd represents the weekday\n"
                               "h represents the hour; m represents the minute; s represents the second\n"
                               "The clock format can contain custom text. Use \\n for a line break (up to two lines)\n"
                               "Escape the keywords above with a backslash \\, for example, \\Y displays Y instead of the year; likewise, \\\\ displays a single backslash"),
                            page);
    help->setWordWrap(true);
    auto *previewTitle = new QLabel(tr("Below is a preview of your format:"), page);
    auto *preview = new QLabel(page);
    preview->setTextFormat(Qt::PlainText);
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumHeight(64);

    layout->addWidget(edit);
    layout->addWidget(help);
    layout->addWidget(previewTitle);
    layout->addWidget(preview);

    const auto syncText = [edit](const QVariant &value) {
        const QString text = value.toString();
        // 输入过程中不要用规范化后的值覆盖用户正在编辑的内容
        if (ClockFormat::normalize(edit->text()) == text)
            return;
        const QSignalBlocker blocker(edit);
        const int cursor = edit->cursorPosition();
        edit->setText(text);
        edit->setCursorPosition(qMin(cursor, text.size()));
    };
    const auto syncEnabled = [edit] { edit->setEnabled(cs()->isCustomClockEnabled()); };
    const auto updatePreview = [edit, preview] {
        preview->setText(ClockFormat::render(edit->text(), QDateTime::currentDateTime(),
                                             QLocale(), cs()->isClock12Hour()));
    };

    syncText(option->value());
    syncEnabled();
    updatePreview();
    QObject::connect(option, &DSettingsOption::valueChanged, edit, syncText);
    QObject::connect(cs(), &CustomSettings::settingsChanged, edit, syncEnabled);
    QObject::connect(cs(), &CustomSettings::settingsChanged, preview, updatePreview);
    QObject::connect(edit, &QLineEdit::textChanged, preview, updatePreview);
    QObject::connect(edit, &QLineEdit::textEdited, edit, [option](const QString &text) {
        option->setValue(text);
    });
    auto *timer = new QTimer(preview);
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, preview, updatePreview);
    timer->start();

    return page;
}

} // namespace

MainSettingWidget::MainSettingWidget(QWidget *parent)
    : DSettingsDialog(parent)
{
    setWindowTitle(tr("GXDE Top Panel Settings"));
    setIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));

    registerCustomWidgets();

    QFile file(QStringLiteral(":/settings.json"));
    file.open(QIODevice::ReadOnly);
    m_settings = DSettings::fromJson(file.readAll());
    m_settings->setParent(this);
    m_settings->setBackend(new PanelSettingsBackend(m_settings));
    updateSettings(TranslateContext, m_settings);

    // 各项都即时写入，「恢复默认」不适用于 CustomSettings 的语义。
    // 必须在 updateSettings 之后调用，否则会被重新显示
    setResetVisible(false);

    // 对话框自带的 DTitlebar：打开菜单按钮，「关于」由 DTK 弹出标准关于窗。
    // 菜单里的主题切换只影响设置窗进程本身，和面板主题无关，隐藏以免混淆
    if (auto *bar = findChild<DTitlebar *>(QString(), Qt::FindDirectChildrenOnly)) {
        bar->setMenuVisible(true);
        bar->setSwitchThemeMenuVisible(false);
    }
    if (auto *app = qobject_cast<DApplication *>(qApp)) {
        app->setProductName(QApplication::applicationDisplayName());
        app->setProductIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));
        // DTK 关于窗的「主页」一栏显示的是发行版官网，项目地址放进描述
        app->setApplicationDescription(QStringLiteral(
            "%1: gfdgd xi, SeptemberHX, CharOfString<br>"
            "%2: <a href=\"https://github.com/GXDE-OS/gxde-top-panel\">github.com/GXDE-OS/gxde-top-panel</a><br>"
            "%3: <a href=\"mailto:september_hx@outlook.com\">september_hx@outlook.com</a>, "
            "<a href=\"mailto:3025613752@qq.com\">3025613752@qq.com</a>, "
            "<a href=\"mailto:root@charofstring.cc\">root@charofstring.cc</a>")
            .arg(tr("Author"), tr("Github"), tr("Email")));
        app->setApplicationHomePage(QStringLiteral("https://github.com/GXDE/gxde-top-panel"));
        app->setApplicationAcknowledgementVisible(false);
    }

    resize(820, 620);
}

MainSettingWidget::~MainSettingWidget() = default;

void MainSettingWidget::registerCustomWidgets()
{
    auto *factory = widgetFactory();
    factory->registerWidget(QStringLiteral("iconpicker"), createIconPicker);
    factory->registerWidget(QStringLiteral("colorpicker"), createColorPicker);
    factory->registerWidget(QStringLiteral("gxdecheckbox"), createLinkedCheckBox);
    factory->registerWidget(QStringLiteral("clockformat"), createClockFormat);
}
