#include "DockInternal_p.h"

#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Menu.h>

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QStyleOption>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock::detail {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("fm::dock", text);
}

QString sideName(SideBarLocation side)
{
    switch (side) {
    case SideBarLocation::Left: return tr("왼쪽");
    case SideBarLocation::Top: return tr("위");
    case SideBarLocation::Right: return tr("오른쪽");
    case SideBarLocation::Bottom: return tr("아래");
    case SideBarLocation::None: break;
    }
    return {};
}

constexpr SideBarLocation kSides[] = {SideBarLocation::Left, SideBarLocation::Right, SideBarLocation::Top,
                                      SideBarLocation::Bottom};

} // namespace

// =============================================================================================
// 공통

const fs::ThemeColors &colorsOf(const QWidget *widget)
{
    return fs::themeColorsFor(widget);
}

fs::DockMetrics metricsOf(const QWidget *widget)
{
    return fs::dockMetrics(colorsOf(widget));
}

QFont tabFontOf(const QWidget *widget)
{
    QFont f = widget ? widget->font() : QApplication::font();
    f.setPixelSize(12);
    return f;
}

int tabHeightOf(const QWidget *widget)
{
    QStyleOptionTab opt;
    if (widget)
        opt.initFrom(widget);
    opt.shape = QTabBar::RoundedNorth;
    opt.text = u"Tab"_s;
    const QFontMetrics fm(tabFontOf(widget));
    const QStyle *style = widget ? widget->style() : QApplication::style();
    const int vspace = style->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, widget);
    const QSize contents(fm.horizontalAdvance(opt.text) + style->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, widget),
                         fm.height() + vspace);
    return std::max(18, style->sizeFromContents(QStyle::CT_TabBarTab, &opt, contents, widget).height());
}

int stripHeightOf(const QWidget *widget)
{
    return tabHeightOf(widget) + metricsOf(widget).stripTopGap;
}

bool isHeadlessPlatform()
{
    const QString platform = QGuiApplication::platformName();
    return platform == u"offscreen" || platform == u"minimal";
}

DockWidget *dockOf(const QWidget *widget)
{
    for (const QWidget *w = widget; w; w = w->parentWidget()) {
        if (auto *dock = qobject_cast<DockWidget *>(const_cast<QWidget *>(w)))
            return dock;
    }
    return nullptr;
}

// =============================================================================================
// 메뉴

void execDockMenu(DockWidget *dock, const QPoint &globalPos, QWidget *parent)
{
    if (!dock || !dock->dockManager())
        return;
    DockManager *manager = dock->dockManager();
    const QPointer<DockWidget> guard(dock);
    const QPointer<DockArea> area(dock->dockArea());
    fm::ui::Menu menu(parent);

    const bool floatable = dock->hasFeature(DockWidget::Floatable)
                        && !manager->testConfigFlag(DockManager::NoTopLevelWindows);
    const bool aloneInWindow = dock->isFloating() && dock->dockContainer()
                            && dock->dockContainer()->openedDockWidgets().size() == 1;
    QAction *undock = menu.addAction(tr("떼어 내기"));
    undock->setEnabled(floatable && !aloneInWindow);
    QObject::connect(undock, &QAction::triggered, &menu, [guard] {
        if (guard)
            guard->setFloating();
    });

    const bool pinnable = dock->hasFeature(DockWidget::Pinnable)
                       && manager->testAutoHideFlag(DockManager::AutoHideEnabled);
    if (dock->isAutoHidden()) {
        QAction *unpin = menu.addAction(tr("도크에 고정"));
        QObject::connect(unpin, &QAction::triggered, &menu, [guard, manager] {
            if (guard)
                manager->setAutoHidden(guard, false);
        });
        QMenu *move = menu.addMenu(tr("고정 위치"));
        for (SideBarLocation side : kSides) {
            QAction *a = move->addAction(sideName(side));
            a->setCheckable(true);
            a->setChecked(dock->autoHideSide() == side);
            QObject::connect(a, &QAction::triggered, &menu, [guard, manager, side] {
                if (guard)
                    manager->setAutoHidden(guard, true, side);
            });
        }
    } else {
        QMenu *pin = menu.addMenu(tr("자동 숨김으로 고정"));
        pin->setEnabled(pinnable);
        for (SideBarLocation side : kSides) {
            QAction *a = pin->addAction(sideName(side));
            QObject::connect(a, &QAction::triggered, &menu, [guard, manager, side] {
                if (guard)
                    manager->setAutoHidden(guard, true, side);
            });
        }
        QAction *pinArea = menu.addAction(tr("영역째 자동 숨김"));
        pinArea->setEnabled(pinnable && area);
        QObject::connect(pinArea, &QAction::triggered, &menu, [guard, area, manager] {
            if (guard && area)
                ManagerAccess::pinArea(manager, area, ManagerAccess::sideForDock(manager, guard));
        });
    }

    menu.addSeparator();
    QAction *close = menu.addAction(tr("닫기"));
    close->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F4));
    close->setShortcutVisibleInContextMenu(true);
    close->setEnabled(dock->hasFeature(DockWidget::Closable));
    QObject::connect(close, &QAction::triggered, &menu, [guard] {
        if (guard)
            guard->requestClose();
    });
    QAction *others = menu.addAction(tr("다른 탭 닫기"));
    others->setEnabled(area && area->openDockWidgetsCount() > 1);
    QObject::connect(others, &QAction::triggered, &menu, [guard, area] {
        if (guard && area)
            area->closeOtherTabs(guard);
    });
    QAction *closeArea = menu.addAction(tr("영역 닫기"));
    closeArea->setEnabled(area);
    QObject::connect(closeArea, &QAction::triggered, &menu, [area] {
        if (area)
            area->closeArea();
    });
    menu.exec(globalPos);
}

void execAreaMenu(DockArea *area, const QPoint &globalPos, QWidget *parent)
{
    if (!area || !area->dockManager())
        return;
    DockManager *manager = area->dockManager();
    const QPointer<DockArea> guard(area);
    fm::ui::Menu menu(parent);
    const QList<DockWidget *> open = area->openDockWidgets();
    bool floatable = !open.isEmpty() && !manager->testConfigFlag(DockManager::NoTopLevelWindows);
    bool pinnable = !open.isEmpty() && manager->testAutoHideFlag(DockManager::AutoHideEnabled);
    for (DockWidget *d : open) {
        floatable = floatable && d->hasFeature(DockWidget::Floatable);
        pinnable = pinnable && d->hasFeature(DockWidget::Pinnable);
    }
    const bool aloneInWindow = area->dockContainer() && area->dockContainer()->isFloating()
                            && area->dockContainer()->openedDockAreas().size() == 1;
    QAction *undock = menu.addAction(tr("떼어 내기"));
    undock->setEnabled(floatable && !aloneInWindow);
    QObject::connect(undock, &QAction::triggered, &menu, [guard, manager] {
        if (!guard)
            return;
        const QList<DockWidget *> docks = guard->openDockWidgets();
        ManagerAccess::floatDocks(manager, docks, ManagerAccess::defaultFloatingGeometry(manager, docks));
    });
    QMenu *pin = menu.addMenu(tr("자동 숨김으로 고정"));
    pin->setEnabled(pinnable);
    for (SideBarLocation side : kSides) {
        QAction *a = pin->addAction(sideName(side));
        QObject::connect(a, &QAction::triggered, &menu, [guard, manager, side] {
            if (guard)
                ManagerAccess::pinArea(manager, guard, side);
        });
    }
    menu.addSeparator();
    QAction *close = menu.addAction(tr("영역 닫기"));
    QObject::connect(close, &QAction::triggered, &menu, [guard] {
        if (guard)
            guard->closeArea();
    });
    menu.exec(globalPos);
}

// =============================================================================================
// DockButton

DockButton::DockButton(const QString &kind, bool flat, QWidget *parent)
    : QAbstractButton(parent)
    , m_flat(flat)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    if (flat)
        setProperty(fs::props::kDockButtonFlat, true);
    setKind(kind);
}

void DockButton::setKind(const QString &kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    fs::setDockButton(this, kind);
    QString tip;
    if (kind == u"close")
        tip = tr("닫기");
    else if (kind == u"float")
        tip = tr("떼어 내기");
    else if (kind == u"pin")
        tip = tr("자동 숨김");
    else if (kind == u"unpin")
        tip = tr("도크에 고정");
    else if (kind == u"menu")
        tip = tr("탭 목록");
    else if (kind == u"minimize")
        tip = tr("접기");
    setToolTip(tip);
    setAccessibleName(tip);
    update();
}

void DockButton::setExtent(int px)
{
    if (m_extent == px)
        return;
    m_extent = px;
    updateGeometry();
}

QSize DockButton::sizeHint() const
{
    const int s = m_extent > 0 ? m_extent : metricsOf(this).areaButton;
    return {s, s};
}

void DockButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionToolButton opt;
    opt.initFrom(this);
    opt.state |= QStyle::State_AutoRaise;
    if (isDown() || isChecked())
        opt.state |= QStyle::State_Sunken;
    else if (underMouse() && isEnabled())
        opt.state |= QStyle::State_Raised | QStyle::State_MouseOver;
    style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &p, this);
}

bool DockButton::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
        update();
        break;
    case QEvent::StyleChange:
        updateGeometry();
        break;
    default:
        break;
    }
    return QAbstractButton::event(event);
}

// =============================================================================================
// TabCloseButton

TabCloseButton::TabCloseButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::ArrowCursor);
    setToolTip(tr("닫기"));
    setAccessibleName(tr("닫기"));
}

void TabCloseButton::setSelectedTab(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    update();
}

QSize TabCloseButton::sizeHint() const
{
    const int w = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, nullptr, this);
    const int h = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, nullptr, this);
    return {std::max(12, w), std::max(12, h)};
}

void TabCloseButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    if (isEnabled() && underMouse() && !isDown())
        opt.state |= QStyle::State_Raised | QStyle::State_MouseOver;
    if (isDown())
        opt.state |= QStyle::State_Sunken;
    if (m_selected)
        opt.state |= QStyle::State_Selected;
    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}

bool TabCloseButton::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
        update();
    return QAbstractButton::event(event);
}

} // namespace fm::dock::detail
