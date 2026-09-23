#ifndef MAINSETTINGWIDGET_H
#define MAINSETTINGWIDGET_H

#include <DSettingsDialog>

namespace Dtk {
namespace Core {
class DSettings;
}
}

// 基于 DTK DSettingsDialog 的设置窗，标题栏带菜单（关于为 DTK 标准关于窗）和关闭按钮。设置项由
// :/settings.json 描述，读写通过自定义后端直接映射到 CustomSettings，
// 配置文件格式保持不变。
class MainSettingWidget : public Dtk::Widget::DSettingsDialog
{
    Q_OBJECT

public:
    explicit MainSettingWidget(QWidget *parent = nullptr);
    ~MainSettingWidget() override;

private:
    void registerCustomWidgets();

    Dtk::Core::DSettings *m_settings = nullptr;
};

#endif // MAINSETTINGWIDGET_H
