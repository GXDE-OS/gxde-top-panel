//
// Created by septemberhx on 2020/5/26.
//

#include <QDebug>
#include <QWidget>
#include <QWindow>
#include "ActiveWindowControlWidget.h"
#include "util/XUtils.h"
#include "util/utils.h"
#include "util/WaylandMenu.h"
#include "wayland/WaylandWindowManager.h"
#include <QMouseEvent>
#include <NETWM>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QPainter>
#include <KWindowInfo>
#include <QStandardPaths>
#include <iostream>

namespace {
constexpr int IndicatorTwoLineFontSize = 10;
constexpr int IndicatorSingleLineFontSize = 14;

// Keep long window titles from pushing plugins off the panel.
class IndicatorLabel : public QLabel {
public:
    using QLabel::QLabel;
    bool elide = false;

    QSize sizeHint() const override {
        QSize size = QLabel::sizeHint();
        if (elide)
            size.setWidth(qMin(size.width(), 320));
        return size;
    }

    QSize minimumSizeHint() const override {
        return elide ? QSize(0, sizeHint().height()) : QLabel::minimumSizeHint();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        if (!elide) {
            QLabel::paintEvent(event);
            return;
        }
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.setFont(font());
        painter.drawText(contentsRect(), alignment(),
                         fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};
}

ActiveWindowControlWidget::ActiveWindowControlWidget(QWidget *parent)
    : QWidget(parent)
    , currActiveWinId(-1)
    , m_isWayland(Utils::isWayland())
    , mouseClicked(false)
    , m_currentIndex(-1)
    , m_currentMenu(nullptr)
    , m_appInter(new DBusDock(this))
    , m_launcherInter(new LauncherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
    , m_moreMenu(new QMenu())
    , organizeFlag(false)
    , prevAvailableWidth(-1)
{
    m_launcherInter->setSync(true, false);

    QPalette palette1 = this->palette();
    palette1.setColor(QPalette::Window, Qt::transparent);
    this->setPalette(palette1);

    this->m_layout = new QHBoxLayout(this);
    this->m_layout->setSpacing(16);
    this->m_layout->setContentsMargins(20, 0, 0, 0);
    this->setLayout(this->m_layout);

    this->m_iconLabel = new QLabel(this);
    this->m_iconLabel->setFixedSize(18, 18);
    this->m_iconLabel->setAlignment(Qt::AlignCenter);
    this->m_layout->addWidget(this->m_iconLabel);

    this->m_buttonWidget = new QOperationWidget(true, this);
    this->m_layout->addWidget(this->m_buttonWidget);

    this->m_appNameLabel = new IndicatorLabel(this);
    this->m_appNameLabel->setTextFormat(Qt::PlainText);
    this->m_appNameLabel->setFixedHeight(22);
    this->m_layout->addWidget(this->m_appNameLabel);

    this->m_menuWidget = new QWidget(this);
    this->installEventFilter(this);

    this->m_moreLabel = new QClickableLabel(this->m_menuWidget);
    this->m_moreLabel->setText(tr("   ▸   "));
    this->m_moreLabel->setAlignment(Qt::AlignCenter);
    connect(this->m_moreLabel, &QClickableLabel::clicked, this, &ActiveWindowControlWidget::menuLabelClicked);
    this->m_moreLabel->hide();

    this->m_layout->addWidget(this->m_menuWidget);
    this->m_menuLayout = new QHBoxLayout(this->m_menuWidget);
    this->m_menuLayout->setContentsMargins(0, 0, 0, 0);
    this->m_menuLayout->setSpacing(2);
    this->m_appMenuModel = new AppMenuModel(this);
    connect(this->m_appMenuModel, &AppMenuModel::modelNeedsUpdate, this, &ActiveWindowControlWidget::updateMenu);

    this->m_winTitleLabel = new IndicatorLabel(this);
    this->m_winTitleLabel->setTextFormat(Qt::PlainText);
    this->m_winTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    this->m_winTitleLabel->setContentsMargins(0, 4, 0, 4);
    this->m_layout->addWidget(this->m_winTitleLabel);

    m_indicatorWidget = new QWidget(this);
    m_indicatorWidget->setObjectName(QStringLiteral("appIndicator"));
    m_indicatorWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_indicatorLayout = new QVBoxLayout(m_indicatorWidget);
    // Together with the new UI's 8px layout spacing, match the 20px left inset.
    m_indicatorLayout->setContentsMargins(12, 0, 0, 0);
    m_indicatorLayout->setSpacing(0);
    m_indicatorLayout->setAlignment(Qt::AlignVCenter);
    m_layout->insertWidget(2, m_indicatorWidget);
    m_indicatorWidget->hide();

    this->m_layout->addStretch();

    this->setButtonsVisible(false);
    this->setMouseTracking(true);

    connect(this->m_buttonWidget, &QOperationWidget::maxButtonClicked, this, &ActiveWindowControlWidget::maxButtonClicked);
    connect(this->m_buttonWidget, &QOperationWidget::minButtonClicked, this, &ActiveWindowControlWidget::minButtonClicked);
    connect(this->m_buttonWidget, &QOperationWidget::closeButtonClicked, this, &ActiveWindowControlWidget::closeButtonClicked);

    connect(this, &ActiveWindowControlWidget::showOperationButtons, this, [this] {
        if (CustomSettings::instance()->isButtonOnLeft()) {
            this->m_buttonWidget->showWithAnimation();
        }
    });
    connect(this, &ActiveWindowControlWidget::hideOperationButtons, this, [this] {
        if (CustomSettings::instance()->isButtonOnLeft()) {
            this->m_buttonWidget->hideWithAnimation();
        }
    });
    connect(this->m_buttonWidget, &QOperationWidget::animationFinished, this, [this] {
        qDebug() << "organizeMenu() triggered by animationFinished";
        this->organizeMenu();
    });

    if (m_isWayland) {
        connect(WaylandWindowManager::instance(), &WaylandWindowManager::activeWindowChanged,
                this, &ActiveWindowControlWidget::activeWindowInfoChanged);
    } else {
        // detect whether active window maximized signal
        connect(KX11Extras::self(), &KX11Extras::windowChanged,
                this, &ActiveWindowControlWidget::windowChanged);
    }

    this->m_fixTimer = new QTimer(this);
    this->m_fixTimer->setSingleShot(true);
    this->m_fixTimer->setInterval(100);
    connect(this->m_fixTimer, &QTimer::timeout, this, &ActiveWindowControlWidget::activeWindowInfoChanged);
    // some applications like Gtk based and electron based seems still holds the focus after clicking the close button for a little while
    // Thus, we need to check the active window when some windows are closed.
    // However, we will use the dock dbus signal instead of other X operations.
    connect(this->m_appInter, &DBusDock::EntryRemoved, this->m_fixTimer, qOverload<>(&QTimer::start));
    connect(this->m_appInter, &DBusDock::EntryAdded, this->m_fixTimer, qOverload<>(&QTimer::start));

    applyCustomSettings(*CustomSettings::instance());
    this->m_buttonWidget->hide();
}

void ActiveWindowControlWidget::activeWindowInfoChanged() {
    if (m_isWayland) {
        updateWaylandWindowInfo();
        return;
    }

    int activeWinId = XUtils::getFocusWindowId();
    if (activeWinId < 0) {
        qDebug() << "Failed to get active window id !";
        return;
    }

    // fix strange focus losing when pressing alt in some applications like chrome
    if (activeWinId == 0) {
        return;
    }

    // fix focus moving to top panel bug with aurorae
    if (activeWinId == this->winId()) {
        return;
    }

    int currScreenNum = this->currScreenNum();
    int activeWinScreenNum = XUtils::getWindowScreenNum(activeWinId);
    if (activeWinScreenNum >= 0 && activeWinScreenNum != currScreenNum) {
        bool ifFoundPrevActiveWinId = false;
        if (XUtils::checkIfBadWindow(this->currActiveWinId) || this->currActiveWinId == activeWinId || XUtils::checkIfWinMinimun(this->currActiveWinId)) {
            int newCurActiveWinId = -1;
            while (!this->activeIdStack.isEmpty()) {
                int prevActiveId = this->activeIdStack.pop();
                if (XUtils::checkIfBadWindow(prevActiveId) || this->currActiveWinId == prevActiveId || XUtils::checkIfWinMinimun(prevActiveId)) {
                    continue;
                }

                int sNum = XUtils::getWindowScreenNum(prevActiveId);
                if (sNum != currScreenNum) {
                    continue;
                }

                newCurActiveWinId = prevActiveId;
                break;
            }

            if (newCurActiveWinId < 0) {
                this->currActiveWinId = -1;
                if (!CustomSettings::instance()->isShowAppNameInsteadIcon()) {
                    updateWindowIcon();
                    this->m_winTitleLabel->show();
                    this->m_winTitleLabel->setText(tr("Desktop"));
                } else {
                    this->m_winTitleLabel->hide();
                }
                this->m_appNameLabel->setText(tr("Desktop"));
                currActiveWinTitle = tr("Desktop");
                m_applicationNames.clear();
                if (m_newUi)
                    setMenuVisible(false);
            } else {
                activeWinId = newCurActiveWinId;
                ifFoundPrevActiveWinId = true;
            }
        }
        if (!ifFoundPrevActiveWinId) {
            return;
        }
    }

    if (activeWinId != this->currActiveWinId) {
        this->currActiveWinId = activeWinId;
        this->activeIdStack.push(this->currActiveWinId);
    }

    this->setButtonsVisible(XUtils::checkIfWinMaximum(this->currActiveWinId));

    QString activeWinTitle = XUtils::getWindowName(this->currActiveWinId);
    this->currActiveWinTitle = activeWinTitle;
    this->m_winTitleLabel->setText(this->currActiveWinTitle);

    if (!activeWinTitle.isEmpty()) {
        if (!CustomSettings::instance()->isShowAppNameInsteadIcon()) {
            updateWindowIcon();
        }
        KWindowInfo info(this->currActiveWinId, NET::WMName,
                         NET::WM2DesktopFileName | NET::WM2WindowClass);
        QString appId = info.desktopFileName();
        if (appId.isEmpty())
            appId = QString::fromUtf8(info.windowClassClass());
        this->m_appNameLabel->setText(m_newUi
            ? applicationDisplayName(appId, activeWinTitle)
            : XUtils::getWindowAppName(this->currActiveWinId));
    }

    // KWindowSystem will not update menu for desktop when focusing on the desktop
    // It is not a good idea to do the filter here instead of the AppmenuModel.
    // However, it works, and works pretty well.
    if (activeWinTitle == tr("Desktop")) {
        // hide buttons
        this->setButtonsVisible(false);

        // clear menu
        this->m_menuWidget->hide();
        for (auto m_label : this->buttonLabelList) {
            this->m_menuLayout->removeWidget(m_label);
        }
        for (auto m_label : this->buttonLabelListBak) {
            delete m_label;
        }
        this->buttonLabelListBak.clear();
        this->buttonLabelList.clear();
        this->setMenuVisible(false);
    }

    // some applications like KWrite will expose its global menu with an invalid dbus path
    //   thus we need to recheck it again :(
    this->m_appMenuModel->setWinId(this->currActiveWinId);
    if (m_newUi)
        setMenuVisible(!m_menuWidget->isHidden());
}

void ActiveWindowControlWidget::setButtonsVisible(bool visible) {
    if (CustomSettings::instance()->isShowControlButtons()) {
        if (visible) {
            emit showOperationButtons();
        } else {
            emit hideOperationButtons();
        }
    }
}

void ActiveWindowControlWidget::enterEvent(QEnterEvent *event) {
    if (CustomSettings::instance()->isShowGlobalMenuOnHover() && !this->buttonLabelList.isEmpty()
        && isActiveWindowMaximized()) {
        this->setMenuVisible(true);
    }

    QWidget::enterEvent(event);
}

void ActiveWindowControlWidget::leaveEvent(QEvent *event) {
    this->leaveTopPanel();
    QWidget::leaveEvent(event);
}

void ActiveWindowControlWidget::maxButtonClicked() {
    if (m_isWayland) {
        WaylandWindowManager::instance()->toggleMaximized();
        return;
    }

    if (XUtils::checkIfWinMaximum(this->currActiveWinId)) {
        XUtils::unmaximizeWindow(this->currActiveWinId);

        // checkIfWinMaximum is MUST needed here.
        //   I don't know whether XSendEvent is designed like this,
        //   my test shows unmaximizeWindow by XSendEvent will work when others try to fetch its properties.
        //   i.e., checkIfWinMaximum
        XUtils::checkIfWinMaximum(this->currActiveWinId);
    } else {
        // sadly the dbus maximizeWindow cannot unmaximize window :(
        this->m_appInter->MaximizeWindow(this->currActiveWinId);
    }

//    this->activeWindowInfoChanged();
}

void ActiveWindowControlWidget::minButtonClicked() {
    if (m_isWayland) {
        WaylandWindowManager::instance()->minimize();
        return;
    }
    KX11Extras::minimizeWindow(this->currActiveWinId);
}

void ActiveWindowControlWidget::closeButtonClicked() {
    if (m_isWayland) {
        WaylandWindowManager::instance()->close();
        return;
    }
    this->m_appInter->CloseWindow(this->currActiveWinId);
}

void ActiveWindowControlWidget::maximizeWindow() {
    this->maxButtonClicked();
}

void ActiveWindowControlWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (this->childAt(event->pos()) == nullptr) {
        this->maximizeWindow();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ActiveWindowControlWidget::updateMenu() {
    qDebug() << "ActiveWindowControlWidget#updateMenu() starts, current winID reported by menuModel is " << this->m_appMenuModel->winId()
             << ", current winID reported by panel is " << this->currActiveWinId;
    foreach (auto *label, this->buttonLabelListBak) {
        label->deleteLater();
    }
    this->buttonLabelListBak.clear();

    QList<QString> existedMenu;  // tricks for twice menu of libreoffice
    for (int r = 0; r < m_appMenuModel->rowCount(); ++r) {
        QString menuStr = m_appMenuModel->data(m_appMenuModel->index(r, 0), AppMenuModel::MenuRole).toString();

        // tricks to remove useless old menus
        existedMenu.append(menuStr);

        // create new clickable label for the new menu item
        auto *m_label = new QClickableLabel(this->m_menuWidget);
        menuStr = menuStr.remove('&');
        int index = menuStr.lastIndexOf('(');
        if (index > 0) {
            menuStr = menuStr.mid(0, index);
        }
        m_label->setText(QString("   %1   ").arg(menuStr));
        m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        connect(m_label, &QClickableLabel::clicked, this, &ActiveWindowControlWidget::menuLabelClicked);
        this->buttonLabelListBak.append(m_label);
    }

    qDebug() << "organizeMenu() triggered by menuUpdate event";
    this->organizeMenu();
    qDebug() << "ActiveWindowControlWidget#updateMenu() ends";
}

void ActiveWindowControlWidget::menuLabelClicked() {
    auto *label = dynamic_cast<QClickableLabel*>(sender());
    this->trigger(label, this->buttonLabelList.indexOf(label));
}

// for modern gtk applications, some menu has no sub menus, and only action is presented.
// thus, we obtain the action instead of the menu so we can perform the action when no submenu
QAction *ActiveWindowControlWidget::createAction(int idx) const {
    qDebug() << "ActiveWindowControlWidget#createAction() starts";
    const QModelIndex index = m_appMenuModel->index(idx, 0);
    const QVariant data = m_appMenuModel->data(index, AppMenuModel::ActionRole);
    qDebug() << "ActiveWindowControlWidget#createAction() ends";

    if (data.isValid()) {
        return (QAction *) data.value<void *>();
    } else {
        return nullptr;
    }
}

void ActiveWindowControlWidget::trigger(QClickableLabel *ctx, int idx) {
    qDebug() << "ActiveWindowControlWidget#trigger() entered";

    if (ctx == nullptr) return;

    if (m_currentIndex == idx) return;

    qDebug() << "ActiveWindowControlWidget#trigger(): previous triggered is " << m_currentIndex << ", current is " << idx;
    if (m_currentIndex >= this->buttonLabelList.size()) {
        qDebug() << "ActiveWindowControlWidget#trigger(): previous triggered is "
                 << this->buttonLabelListBak[m_currentIndex]->text();
    } else if (m_currentIndex >= 0) {
        qDebug() << "ActiveWindowControlWidget#trigger(): previous triggered is "
                 << this->buttonLabelList[m_currentIndex]->text();
    }

    int oldIndex = m_currentIndex;
    m_currentIndex = idx;

    QMenu *actionMenu = nullptr;
    QAction *action = nullptr;
    if (ctx == this->m_moreLabel) {
        actionMenu = this->m_moreMenu;
    } else {
        action = createAction(idx);
        if (action == nullptr) {
            return;
        }

        actionMenu = action->menu();
    }

    qDebug() << "ActiveWindowControlWidget#trigger() is running..";
    if (actionMenu) {
        actionMenu->installEventFilter(this);
        WaylandMenu::installStyle(actionMenu);
        const QPoint popupPosition =
            m_menuWidget->mapToGlobal(ctx->geometry().bottomLeft()) + QPoint(0, 1);
        if (m_isWayland) {
            QScreen *targetScreen = screen();
            if (!targetScreen) {
                targetScreen = QGuiApplication::screenAt(popupPosition);
            }
            const QPoint layerPosition = targetScreen
                ? popupPosition - targetScreen->geometry().topLeft()
                : popupPosition;
            WaylandMenu::configure(actionMenu, targetScreen, layerPosition);
            WaylandMenu::configureSubmenus(actionMenu, targetScreen);
        } else {
            actionMenu->adjustSize();
            actionMenu->winId(); // create window handle
            actionMenu->windowHandle()->setTransientParent(ctx->windowHandle());
        }
        actionMenu->popup(popupPosition);

        QMenu *oldMenu = m_currentMenu;
        m_currentMenu = actionMenu;
        if (oldMenu && oldMenu != actionMenu) {
            //don't initialize the currentIndex when another menu is already shown
            disconnect(oldMenu, &QMenu::aboutToHide, this, &ActiveWindowControlWidget::onMenuAboutToHide);
            oldMenu->hide();
        }

        // fix: random losing selected color
        ctx->clicked();
        ctx->setClicked(true);
        if (oldIndex >=0 && oldIndex < this->buttonLabelList.size()) {
            this->buttonLabelList[oldIndex]->setClicked(false);
        }

        connect(actionMenu, &QMenu::aboutToHide, this, &ActiveWindowControlWidget::onMenuAboutToHide, Qt::UniqueConnection);
    } else if (action != nullptr) {
        action->trigger();
    }
    qDebug() << "ActiveWindowControlWidget#trigger() ended";
}

void ActiveWindowControlWidget::windowChanged(WId id, NET::Properties properties, NET::Properties2 properties2) {
    if (m_isWayland) {
        return;
    }
    if (properties.testFlag(NET::WMGeometry) || properties.testFlag(NET::WMName)) {
        this->activeWindowInfoChanged();
    }

    // we still don't know why active window is 0 when pressing alt in some applications like chrome.
    if (KX11Extras::activeWindow() != this->currActiveWinId && KX11Extras::activeWindow() != 0) {
        return;
    }

    this->setButtonsVisible(XUtils::checkIfWinMaximum(this->currActiveWinId));
}

void ActiveWindowControlWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QWidget *pressedWidget = childAt(event->pos());
        if (pressedWidget == nullptr || pressedWidget == m_winTitleLabel) {
            this->mouseClicked = !this->mouseClicked;
        } else if (qobject_cast<QLabel*>(pressedWidget) == this->m_iconLabel) {
            if (this->m_launcherInter->visible()) {
                this->m_launcherInter->Hide();
            } else {
                this->m_launcherInter->Show();
            }
        }
        if (m_isWayland) {
            WaylandWindowManager::instance()->activate();
        } else {
            KX11Extras::activateWindow(this->currActiveWinId);
        }
    }
    QWidget::mousePressEvent(event);
}

void ActiveWindowControlWidget::mouseReleaseEvent(QMouseEvent *event) {
    this->mouseClicked = false;
    QWidget::mouseReleaseEvent(event);
}

void ActiveWindowControlWidget::mouseMoveEvent(QMouseEvent *event) {
    if (this->mouseClicked && CustomSettings::instance()->isAllowDragWindowWhenMax()) {
        if (isActiveWindowMaximized()) {
            if (m_isWayland) {
                WaylandWindowManager::instance()->requestMove();
                this->mouseClicked = false;
                QWidget::mouseMoveEvent(event);
                return;
            }
            auto x11App = qApp->nativeInterface<QNativeInterface::QX11Application>();
            if (x11App) {
                NETRootInfo ri(x11App->connection(), NET::WMMoveResize);
                ri.moveResizeRequest(
                        this->currActiveWinId,
                        event->globalPosition().x() * this->devicePixelRatioF(),
                        (this->height() + event->globalPosition().y()) * this->devicePixelRatioF(),
                        NET::Move
                );
            }
            this->mouseClicked = false;
        }
    }
    QWidget::mouseMoveEvent(event);
}

void ActiveWindowControlWidget::applyCustomSettings(const CustomSettings& settings) {
    const bool layoutChanged = m_newUi != settings.isNewUiEnabled();
    m_newUi = settings.isNewUiEnabled();
    m_layout->setSpacing(m_newUi ? 8 : 16);
    if (layoutChanged) {
        if (m_newUi) {
            m_indicatorLayout->addWidget(m_appNameLabel);
            m_indicatorLayout->addWidget(m_winTitleLabel);
            m_layout->removeWidget(m_buttonWidget);
            m_layout->insertWidget(m_layout->indexOf(m_indicatorWidget) + 1, m_buttonWidget);
        } else {
            m_layout->removeWidget(m_buttonWidget);
            m_layout->insertWidget(1, m_buttonWidget);
            m_layout->insertWidget(m_layout->indexOf(m_menuWidget), m_appNameLabel);
            m_layout->insertWidget(m_layout->indexOf(m_menuWidget) + 1, m_winTitleLabel);
        }
    }
    m_indicatorWidget->setVisible(m_newUi);
    static_cast<IndicatorLabel *>(m_appNameLabel)->elide = m_newUi;
    static_cast<IndicatorLabel *>(m_winTitleLabel)->elide = m_newUi;
    m_appNameLabel->setFixedHeight(m_newUi ? 12 : 22);
    m_winTitleLabel->setContentsMargins(0, m_newUi ? 0 : 4, 0, m_newUi ? 0 : 4);
    m_winTitleLabel->setMinimumHeight(m_newUi ? 16 : 0);
    m_winTitleLabel->setMaximumHeight(m_newUi ? 16 : QWIDGETSIZE_MAX);
    m_appNameLabel->setMaximumWidth(m_newUi ? 320 : QWIDGETSIZE_MAX);
    m_winTitleLabel->setMaximumWidth(m_newUi ? 320 : QWIDGETSIZE_MAX);

    // title
    QPalette palette = this->m_winTitleLabel->palette();
    palette.setColor(QPalette::WindowText, settings.getActiveFontColor());
    this->m_winTitleLabel->setPalette(palette);
    this->m_winTitleLabel->setFont(settings.getActiveFont());

    // menu
    for (auto menuItem : this->buttonLabelList) {
        menuItem->setFont(settings.getActiveFont());
        menuItem->setDefaultFontColor(settings.getActiveFontColor());
    }

    // buttons
    this->m_buttonWidget->setVisible(CustomSettings::instance()->isButtonOnLeft() && CustomSettings::instance()->isShowControlButtons());
    this->m_buttonWidget->applyCustomSettings(settings);

    // Icon selection is independent of the app-name and visibility settings.
    palette = this->m_appNameLabel->palette();
    palette.setColor(QPalette::WindowText, settings.getActiveFontColor());
    this->m_appNameLabel->setPalette(palette);
    this->m_appNameLabel->setFont(QFont(settings.getActiveFont().family(), settings.getActiveFont().pointSize(), QFont::DemiBold));
    if (m_newUi) {
        QFont appFont = settings.getActiveFont();
        appFont.setPixelSize(IndicatorTwoLineFontSize);
        appFont.setBold(true);
        m_appNameLabel->setFont(appFont);
        QFont titleFont = settings.getActiveFont();
        titleFont.setPixelSize(12);
        titleFont.setBold(false);
        m_winTitleLabel->setFont(titleFont);
    }
    const bool showAppName = m_newUi || settings.isShowAppNameInsteadIcon();
    m_iconLabel->setVisible(!showAppName || settings.isShowLogoWithAppName());
    m_appNameLabel->setVisible(showAppName);
    updateWindowIcon();
    if (layoutChanged)
        activeWindowInfoChanged();
    organizeMenu();
}

void ActiveWindowControlWidget::updateWindowIcon()
{
    const auto *settings = CustomSettings::instance();
    if (settings->isAlwaysUseDefaultIcon() || settings->isShowAppNameInsteadIcon()) {
        setWindowIcon(QIcon(settings->getActiveDefaultAppIconPath()));
        return;
    }
    if (m_isWayland) {
        const auto info = WaylandWindowManager::instance()->activeWindow();
        QIcon icon;
        if (info.valid && (!screen() || info.geometry.isNull()
            || screen()->geometry().intersects(info.geometry))) {
            icon = QIcon::fromTheme(info.iconName);
            if (icon.isNull())
                icon = QIcon::fromTheme(QFileInfo(info.appId).completeBaseName());
        }
        setWindowIcon(icon.isNull() ? QIcon(settings->getActiveDefaultAppIconPath()) : icon);
    } else {
        setWindowIcon(QIcon(XUtils::getWindowIconNameX11(currActiveWinId)));
    }
}

void ActiveWindowControlWidget::setWindowIcon(const QIcon &icon)
{
    const qreal ratio = m_iconLabel->devicePixelRatioF();
    // At fractional scaling, rounding 18 * 1.25 up to 23 can exceed the
    // label's 22 physical pixels and clip its last row. Render a fitting
    // pixmap directly, preserving aspect ratio and avoiding QLabel scaling.
    const QSize pixels(qMax(1, qFloor(m_iconLabel->width() * ratio)),
                       qMax(1, qFloor(m_iconLabel->height() * ratio)));
    QPixmap pixmap = icon.pixmap(pixels, 1.0);
    pixmap.setDevicePixelRatio(ratio);
    m_iconLabel->setPixmap(pixmap);
}

bool ActiveWindowControlWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == this && event->type() == QEvent::DevicePixelRatioChange)
        updateWindowIcon();

    auto *menu = qobject_cast<QMenu *>(watched);
    if (menu) {
        if (m_isWayland && event->type() == QEvent::Show) {
            WaylandMenu::updateEffects(menu);
        } else if (m_isWayland && event->type() == QEvent::Resize) {
            WaylandMenu::updateBlurRegion(menu);
        }

        if (event->type() == QEvent::MouseMove) {
            auto *e = dynamic_cast<QMouseEvent *>(event);

            if (!this->m_menuLayout || !this->m_menuWidget) {
                return false;
            }

            const QPointF &windowLocalPos = m_menuWidget->mapFromGlobal(e->globalPosition());

            auto *item = dynamic_cast<QClickableLabel *>(m_menuWidget->childAt(windowLocalPos.x(), windowLocalPos.y()));
            if (!item) {
                return false;
            }

            const int buttonIndex = this->buttonLabelList.indexOf(item);
            if (buttonIndex < 0) {
                return false;
            }

            requestActivateIndex(buttonIndex);
        }
    }

    if (event->type() == QEvent::Resize) {
        auto *resizeEvent = dynamic_cast<QResizeEvent*>(event);
        if (!this->organizeFlag) {
            int availableWidth = this->menuAvailableWidth();
            if (availableWidth != this->prevAvailableWidth && resizeEvent->oldSize().width() != resizeEvent->size().width()) {
                qDebug() << "organizeMenu() triggered by resize event: " << resizeEvent->oldSize().width() << " -> " << resizeEvent->size().width();
                this->organizeMenu();
                this->prevAvailableWidth = availableWidth;
            }
        }
    }

    return false;
}

void ActiveWindowControlWidget::leaveTopPanel() {
    if (!this->isMenuShown() && CustomSettings::instance()->isShowGlobalMenuOnHover() && !this->buttonLabelList.isEmpty()
        && isActiveWindowMaximized()) {
        this->setMenuVisible(false);
    }
}

int ActiveWindowControlWidget::currScreenNum() {
    QScreen *screen = this->screen();
    if (screen) {
        return QGuiApplication::screens().indexOf(screen);
    }
    return 0;
}

void ActiveWindowControlWidget::requestActivateIndex(int buttonIndex) {
    qDebug() << "ActiveWindowControlWidget#requestActivateIndex() starts, buttonIndex = " << buttonIndex << ", list size = " << this->buttonLabelList.size();
    if (buttonIndex < this->buttonLabelList.size()) {
        this->trigger(this->buttonLabelList[buttonIndex], buttonIndex);
    }
    qDebug() << "ActiveWindowControlWidget#requestActivateIndex() ends";
}

void ActiveWindowControlWidget::onMenuAboutToHide() {
    qDebug() << "ActiveWindowControlWidget#onMenuAboutToHide(): current index is " << this->m_currentIndex;
    if (this->m_currentIndex >= 0 && this->m_currentIndex < this->buttonLabelList.size()) {
        qDebug() << "==========> Set selected color due to menuAboutToHide";
        this->buttonLabelList[this->m_currentIndex]->setClicked(false);
        this->m_currentIndex = -1;
        this->m_currentMenu = nullptr;
    }
    this->leaveTopPanel();
}

bool ActiveWindowControlWidget::isMenuShown() {
    return this->m_currentMenu != nullptr;
}

void ActiveWindowControlWidget::setMenuVisible(bool visible) {
    if (m_newUi) {
        m_indicatorLayout->setContentsMargins(12, 0, 0, 0);
        QString title = currActiveWinTitle.trimmed();
        const QString displayName = m_appNameLabel->text().trimmed();
        QStringList applicationNames = m_applicationNames;
        applicationNames.prepend(m_appNameLabel->text());
        for (const QString &name : applicationNames) {
            const QString appName = name.trimmed();
            if (appName.isEmpty())
                continue;
            // Only remove a trailing app name, never a dash within the title.
            const QRegularExpression suffix(
                QStringLiteral("\\s+[-\u2013\u2014]\\s+%1\\s*$").arg(QRegularExpression::escape(appName)),
                QRegularExpression::CaseInsensitiveOption);
            const auto match = suffix.match(title);
            if (match.hasMatch() && !title.left(match.capturedStart()).trimmed().isEmpty()) {
                title = title.left(match.capturedStart()).trimmed();
                break;
            }
        }
        m_winTitleLabel->setText(title);
        const bool showMenu = visible && !buttonLabelListBak.isEmpty();
        const bool showName = !displayName.isEmpty();
        const bool showTitle = !showMenu && !title.isEmpty()
            && (!showName || title.compare(displayName, Qt::CaseInsensitive) != 0);
        const bool twoLines = showName && showTitle;
        m_menuWidget->setVisible(showMenu);
        m_winTitleLabel->setVisible(showTitle);
        QFont appFont = m_appNameLabel->font();
        appFont.setPixelSize(twoLines ? IndicatorTwoLineFontSize : IndicatorSingleLineFontSize);
        m_appNameLabel->setFont(appFont);
        m_appNameLabel->setFixedHeight(twoLines ? 12 : 22);
        m_appNameLabel->setVisible(showName);
        QFont titleFont = m_winTitleLabel->font();
        titleFont.setPixelSize(twoLines ? 12 : IndicatorSingleLineFontSize);
        m_winTitleLabel->setFont(titleFont);
        m_winTitleLabel->setFixedHeight(twoLines ? 16 : 22);
        m_indicatorWidget->setVisible(showName || showTitle);
        m_appNameLabel->setToolTip(m_appNameLabel->text());
        m_winTitleLabel->setToolTip(currActiveWinTitle);
        return;
    }
    this->m_winTitleLabel->setVisible(!visible && CustomSettings::instance()->isShowControlButtons());
    this->m_menuWidget->setVisible(visible);

    if (!this->m_winTitleLabel->isVisible()) {
        this->m_menuWidget->setVisible(true);
    }

    if (CustomSettings::instance()->isShowAppNameInsteadIcon()
        && (this->m_appNameLabel->text() == this->m_winTitleLabel->text() || !CustomSettings::instance()->isShowControlButtons() )) {
        this->m_winTitleLabel->setVisible(false);
    }
}

void ActiveWindowControlWidget::organizeMenu() {
    QMutexLocker locker(&this->organizeMenuMutex);

    qDebug() << "ActiveWindowControlWidget#organizeMenu() called";
    this->organizeFlag = true;
    this->m_menuWidget->hide();

    // calculate left space for menus
    int availableWidth = this->menuAvailableWidth();

    // reset menu layout
    int selectedItemIndex = -1;
    for (int i = 0; i < this->buttonLabelList.size(); ++i) {
        this->buttonLabelList[i]->hide();
        this->m_menuLayout->removeWidget(this->buttonLabelList[i]);
        if (this->buttonLabelList[i]->isSelected() && this->buttonLabelList[i] != this->m_moreLabel) {
            selectedItemIndex = i;
        }
    }

    this->buttonLabelList.clear();
    this->m_moreMenu->clear();

    // check where we need to cut the menu
    int totalWidth = this->m_menuLayout->contentsMargins().left() + this->m_menuLayout->contentsMargins().right();
    int breakIndex = -1;
    for (int i = 0; i < this->buttonLabelListBak.size(); ++i) {
        int prevWidth = totalWidth;
        totalWidth += this->buttonLabelListBak[i]->standardWidth();
        if (i != this->buttonLabelListBak.size() - 1) {
            totalWidth += this->m_menuLayout->spacing();
        }

        if (totalWidth > availableWidth) {
            breakIndex = i;
            totalWidth = prevWidth;
            break;
        }
    }

    if (breakIndex > 0) {
        breakIndex -= 1;
    }

    if (breakIndex < 0 || breakIndex >= this->buttonLabelListBak.size()) {
        breakIndex = this->buttonLabelListBak.size();
    }

    qDebug()  << "============> Menu items refreshed";
    // show menus and hide others
    QSet<QString> menuStrSet;
    for (int i = 0; i < breakIndex && i < this->buttonLabelListBak.size(); ++i) {
        if (!this->buttonLabelListBak[i]->text().trimmed().isEmpty() && !menuStrSet.contains(this->buttonLabelListBak[i]->text())) {
            this->buttonLabelListBak[i]->show();
            this->m_menuLayout->addWidget(this->buttonLabelListBak[i]);
        } else {
            this->buttonLabelListBak[i]->hide();
            ++breakIndex;
        }
        this->buttonLabelList.append(this->buttonLabelListBak[i]);
        menuStrSet.insert(this->buttonLabelListBak[i]->text());
    }

    bool actionAdded = false;
    for (int i = breakIndex; i < this->buttonLabelListBak.size(); ++i) {
        this->buttonLabelListBak[i]->hide();
        if (!this->buttonLabelListBak[i]->text().trimmed().isEmpty() && !menuStrSet.contains(this->buttonLabelListBak[i]->text())) {
            actionAdded = true;
            auto *action = this->createAction(i);
            this->m_moreMenu->addAction(action);
            if (action->menu() != nullptr) {
                // it is important to disconnect the connections of the hidden menus
                //    or the hidden menus will send aboutTohide() signal even when they are in m_moreMenu
                disconnect(action->menu(), &QMenu::aboutToHide, this, &ActiveWindowControlWidget::onMenuAboutToHide);
            }
        }
        menuStrSet.insert(this->buttonLabelListBak[i]->text());
    }

    // "More" menu item
    if (breakIndex != this->buttonLabelListBak.size() && actionAdded) {
        this->buttonLabelList.append(this->m_moreLabel);
        this->m_menuLayout->addWidget(this->m_moreLabel);
        this->m_moreLabel->show();
    } else {
        this->m_moreLabel->hide();
    }

    // show the selected menu because the menu may be refreshed when the menu shows
    if (selectedItemIndex >= 0 && selectedItemIndex < breakIndex) {
        this->buttonLabelListBak[selectedItemIndex]->setClicked(true);
    }

    // menu visible
    if (CustomSettings::instance()->isShowGlobalMenuOnHover() && isActiveWindowMaximized() && !this->isMenuShown()) {
        this->setMenuVisible(false);
    } else {
        this->setMenuVisible(!this->buttonLabelListBak.isEmpty());
    }
    this->organizeFlag = false;
    qDebug() << "ActiveWindowControlWidget#organizeMenu() ended";
}

int ActiveWindowControlWidget::menuAvailableWidth() {
    // calculate left space for menus
    int usedWidth = this->m_layout->contentsMargins().left() + this->m_layout->contentsMargins().right();
    usedWidth += this->m_iconLabel->width();

    if (this->m_buttonWidget->isVisible()) {
        usedWidth += this->m_layout->spacing() + this->m_buttonWidget->width();
    }

    if (m_newUi) {
        if (m_appNameLabel->text().trimmed().isEmpty())
            return qMax(0, width() - usedWidth - m_layout->spacing());
        // Reserve the enlarged single-line name even while the title is visible,
        // so showing the menu on hover cannot crowd out the last menu item.
        QFont menuNameFont = m_appNameLabel->font();
        menuNameFont.setPixelSize(IndicatorSingleLineFontSize);
        const int nameWidth = qMin(320, QFontMetrics(menuNameFont).size(
            Qt::TextSingleLine, m_appNameLabel->text()).width());
        usedWidth += m_layout->spacing() + nameWidth
            + m_indicatorLayout->contentsMargins().left()
            + m_indicatorLayout->contentsMargins().right();
        return qMax(0, width() - usedWidth - m_layout->spacing());
    }

    if (this->m_appNameLabel->isVisible()) {
        usedWidth += this->m_layout->spacing() + this->m_appNameLabel->width();
    }

    if (this->m_winTitleLabel->isVisible()) {
        usedWidth += this->m_layout->spacing() + this->m_winTitleLabel->width();
    }
    int availableWidth = this->width() - usedWidth - this->m_layout->spacing();
    return availableWidth;
}

bool ActiveWindowControlWidget::isActiveWindowMaximized() const
{
    if (m_isWayland) {
        return WaylandWindowManager::instance()->activeWindow().maximized;
    }
    return XUtils::checkIfWinMaximum(this->currActiveWinId);
}

void ActiveWindowControlWidget::updateWaylandWindowInfo()
{
    const WaylandWindowManager::WindowInfo info =
        WaylandWindowManager::instance()->activeWindow();

    bool belongsToThisScreen = info.valid;
    if (belongsToThisScreen && screen() && !info.geometry.isNull()) {
        belongsToThisScreen = screen()->geometry().intersects(info.geometry);
    }

    if (!belongsToThisScreen) {
        currActiveWinId = -1;
        currActiveWinTitle = tr("Desktop");
        setButtonsVisible(false);
        m_winTitleLabel->setText(currActiveWinTitle);
        m_appNameLabel->setText(tr("Desktop"));
        m_applicationNames.clear();
        if (!CustomSettings::instance()->isShowAppNameInsteadIcon()) {
            updateWindowIcon();
        }
        m_appMenuModel->clearApplicationMenu();
        setMenuVisible(false);
        return;
    }

    currActiveWinTitle = info.title;
    m_winTitleLabel->setText(currActiveWinTitle);
    m_appNameLabel->setText(applicationDisplayName(info.appId, info.title));
    setButtonsVisible(info.maximized);

    updateWindowIcon();

    if (!info.menuService.isEmpty() && !info.menuObjectPath.isEmpty()) {
        m_appMenuModel->updateApplicationMenu(info.menuService, info.menuObjectPath);
    } else {
        m_appMenuModel->clearApplicationMenu();
    }
    if (m_newUi)
        setMenuVisible(!m_menuWidget->isHidden());
}

QString ActiveWindowControlWidget::applicationDisplayName(const QString &appId,
                                                          const QString &title)
{
    m_applicationNames.clear();
    QString desktopId = QFileInfo(appId).fileName();
    if (!desktopId.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        desktopId += QStringLiteral(".desktop");
    }

    // GXDE Terminal still uses the upstream name in its window titles.
    // Scope legacy names to this app rather than stripping arbitrary suffixes.
    if (desktopId.compare(QStringLiteral("gxde-terminal.desktop"), Qt::CaseInsensitive) == 0
        || desktopId.compare(QStringLiteral("deepin-terminal.desktop"), Qt::CaseInsensitive) == 0) {
        m_applicationNames << QStringLiteral("Deepin Terminal")
                           << QStringLiteral("深度终端")
                           << QStringLiteral("深度終端")
                           << QStringLiteral("Deepin 終端器");
    }

    const QString desktopFile =
        QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId);
    if (!desktopFile.isEmpty()) {
        QSettings desktopEntry(desktopFile, QSettings::IniFormat);
        for (const QString &key : desktopEntry.allKeys()) {
            if (key == QLatin1String("Desktop Entry/Name")
                || key.startsWith(QLatin1String("Desktop Entry/Name[")))
                m_applicationNames.append(desktopEntry.value(key).toString().trimmed());
        }
        m_applicationNames.removeDuplicates();
        const QString localizedKey = QStringLiteral("Desktop Entry/Name[%1]")
                                         .arg(QLocale().name());
        QString name = desktopEntry.value(localizedKey).toString();
        if (name.isEmpty()) {
            name = desktopEntry.value(QStringLiteral("Desktop Entry/Name")).toString();
        }
        if (!name.isEmpty()) {
            return name;
        }
    }

    if (!appId.isEmpty()) {
        QString name = QFileInfo(appId).completeBaseName();
        if (!name.isEmpty()) {
            return name;
        }
    }

    const QStringList titleParts = title.split(QRegularExpression(QStringLiteral("[–—-]")));
    return titleParts.isEmpty() ? title : titleParts.constLast().trimmed();
}
