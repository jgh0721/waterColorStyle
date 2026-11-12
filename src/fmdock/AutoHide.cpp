#include "DockInternal_p.h"

#include <fmstyle/StyleProps.h>

#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionTab>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

namespace detail {

namespace {

constexpr int kResizeGrip = 6;
constexpr int kDefaultFlyout = 300;
constexpr int kMinimumFlyout = 160;

QTabBar::Shape shapeFor(SideBarLocation side)
{
    switch (side) {
    case SideBarLocation::Right: return QTabBar::RoundedEast;
    case SideBarLocation::Top: return QTabBar::RoundedNorth;
    case SideBarLocation::Bottom: return QTabBar::RoundedSouth;
    case SideBarLocation::Left:
    case SideBarLocation::None: break;
    }
    return QTabBar::RoundedWest;
}

AutoHideFlyout *flyoutOf(const DockWidget *dock)
{
    return dock ? qobject_cast<AutoHideFlyout *>(dock->parentWidget()) : nullptr;
}

} // namespace

// =============================================================================================
// SideTab

SideTab::SideTab(DockWidget *dock, SideBar *bar)
    : QAbstractButton(bar)
    , m_dock(dock)
    , m_bar(bar)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setFont(tabFontOf(bar));
    m_hoverTimer.setSingleShot(true);
    m_hoverTimer.setInterval(300);
    connect(&m_hoverTimer, &QTimer::timeout, this, [this] {
        DockManager *m = m_bar->dockManager();
        if (m && m_dock && underMouse() && m->testAutoHideFlag(DockManager::ShowOnMouseOver))
            m->expandAutoHide(m_dock);
    });
    updateState();
}

void SideTab::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    fs::setPaneActive(this, expanded);
    update();
}

void SideTab::updateState()
{
    if (!m_dock)
        return;
    setToolTip(m_dock->title());
    setAccessibleName(m_dock->title());
    setVisible(!m_dock->isClosed());
    updateGeometry();
    update();
}

void SideTab::initStyleOption(QStyleOptionTab *opt) const
{
    opt->initFrom(this);
    opt->shape = shapeFor(m_bar->side());
    const DockManager *m = m_bar->dockManager();
    const bool iconOnly = m && m->testAutoHideFlag(DockManager::SideBarsIconOnly) && m_dock && !m_dock->icon().isNull();
    opt->text = iconOnly || !m_dock ? QString() : m_dock->title();
    if (m_dock && !m_dock->icon().isNull()) {
        opt->icon = m_dock->icon();
        opt->iconSize = QSize(16, 16);
    }
    opt->position = QStyleOptionTab::OnlyOneTab;
    opt->selectedPosition = QStyleOptionTab::NotAdjacent;
    opt->documentMode = true;
    if (m_expanded)
        opt->state |= QStyle::State_Selected;
    else if (underMouse())
        opt->state |= QStyle::State_MouseOver;
    opt->rect = rect();
}

QSize SideTab::sizeHint() const
{
    QStyleOptionTab opt;
    initStyleOption(&opt);
    const fs::DockMetrics metrics = metricsOf(this);
    const QFontMetrics fm = fontMetrics();
    int length = fm.horizontalAdvance(opt.text) + 2 * metrics.sideTabPad;
    if (!opt.icon.isNull())
        length += 16 + (opt.text.isEmpty() ? 0 : 6);
    const int cross = std::max(16, metrics.sideBar - 4);
    return m_bar->isVertical() ? QSize(cross, length) : QSize(length, cross);
}

bool SideTab::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        if (m_bar->dockManager() && m_bar->dockManager()->testAutoHideFlag(DockManager::ShowOnMouseOver))
            m_hoverTimer.start();
        update();
        break;
    case QEvent::HoverLeave:
        m_hoverTimer.stop();
        update();
        break;
    case QEvent::StyleChange:
        setFont(tabFontOf(this));
        updateGeometry();
        break;
    default:
        break;
    }
    return QAbstractButton::event(event);
}

void SideTab::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionTab opt;
    initStyleOption(&opt);
    style()->drawControl(QStyle::CE_TabBarTabShape, &opt, &p, this);
    style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &p, this);
}

void SideTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_pressed = true;
    m_dragged = false;
    m_pressGlobal = event->globalPosition().toPoint();
    event->accept();
}

void SideTab::mouseMoveEvent(QMouseEvent *event)
{
    DockManager *m = m_bar->dockManager();
    const QPoint global = event->globalPosition().toPoint();
    if (m) {
        if (DragSession *drag = ManagerAccess::drag(m)) {
            drag->move(global);
            event->accept();
            return;
        }
    }
    if (!m || !m_pressed || !m_dock || (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance()) {
        event->ignore();
        return;
    }
    m_pressed = false;
    m_dragged = true;
    if (m_dock->hasFeature(DockWidget::Movable))
        ManagerAccess::startDrag(m, {m_dock}, nullptr, this, global, QPoint(20, 10));
    event->accept();
}

void SideTab::mouseReleaseEvent(QMouseEvent *event)
{
    DockManager *m = m_bar->dockManager();
    if (event->button() != Qt::LeftButton || !m) {
        event->ignore();
        return;
    }
    if (DragSession *drag = ManagerAccess::drag(m)) {
        drag->drop(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    const bool click = m_pressed && !m_dragged && rect().contains(event->position().toPoint());
    m_pressed = false;
    if (click && m_dock)
        m->expandAutoHide(m_expanded ? nullptr : m_dock.data());
    event->accept();
}

void SideTab::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_dock)
        execDockMenu(m_dock, event->globalPos(), this);
    event->accept();
}

// =============================================================================================
// SideBar

SideBar::SideBar(DockManager *manager, SideBarLocation side)
    : QWidget(manager)
    , m_manager(manager)
    , m_side(side)
{
    setObjectName(side == SideBarLocation::Left    ? u"fmDockSideBarLeft"_s
                  : side == SideBarLocation::Right ? u"fmDockSideBarRight"_s
                  : side == SideBarLocation::Top   ? u"fmDockSideBarTop"_s
                                                   : u"fmDockSideBarBottom"_s);
    m_layout = new QBoxLayout(isVertical() ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this);
    if (isVertical())
        m_layout->setContentsMargins(0, 6, 0, 6);
    else
        m_layout->setContentsMargins(6, 0, 6, 0);
    m_layout->setSpacing(4);
    m_layout->addStretch(1);
    setSizePolicy(isVertical() ? QSizePolicy::Fixed : QSizePolicy::Preferred,
                  isVertical() ? QSizePolicy::Preferred : QSizePolicy::Fixed);
}

void SideBar::insertTab(int index, DockWidget *dock)
{
    if (!dock || tabFor(dock))
        return;
    auto *tab = new SideTab(dock, this);
    index = std::clamp(index < 0 ? int(m_tabs.size()) : index, 0, int(m_tabs.size()));
    m_tabs.insert(index, tab);
    m_layout->insertWidget(index, tab, 0, isVertical() ? Qt::AlignHCenter : Qt::AlignVCenter);
    updateVisibility();
}

void SideBar::removeTab(DockWidget *dock)
{
    SideTab *tab = tabFor(dock);
    if (!tab)
        return;
    m_tabs.removeAll(tab);
    m_layout->removeWidget(tab);
    tab->hide();
    tab->deleteLater();
    updateVisibility();
}

SideTab *SideBar::tabFor(const DockWidget *dock) const
{
    for (SideTab *tab : m_tabs) {
        if (tab->dock() == dock)
            return tab;
    }
    return nullptr;
}

QList<DockWidget *> SideBar::docks() const
{
    QList<DockWidget *> out;
    for (SideTab *tab : m_tabs) {
        if (tab->dock())
            out.append(tab->dock());
    }
    return out;
}

void SideBar::updateVisibility()
{
    bool any = false;
    for (SideTab *tab : std::as_const(m_tabs)) {
        tab->updateState();
        any = any || (tab->dock() && !tab->dock()->isClosed());
    }
    setVisible(any);
}

QSize SideBar::sizeHint() const
{
    const int t = metricsOf(this).sideBar;
    return isVertical() ? QSize(t, 100) : QSize(100, t);
}

QSize SideBar::minimumSizeHint() const
{
    const int t = metricsOf(this).sideBar;
    return isVertical() ? QSize(t, 20) : QSize(20, t);
}

void SideBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const Qt::Edge inner = m_side == SideBarLocation::Right  ? Qt::LeftEdge
                         : m_side == SideBarLocation::Top    ? Qt::BottomEdge
                         : m_side == SideBarLocation::Bottom ? Qt::TopEdge
                                                             : Qt::RightEdge;
    fs::paintDockSideBar(&p, rect(), inner, colorsOf(this));
}

// =============================================================================================
// 펼친 창

/// 펼친 창 제목 줄 — 끌어서 옮기기, 두 번 눌러 도크에 고정.
class FlyoutTitle : public QWidget
{
public:
    explicit FlyoutTitle(AutoHideFlyout *flyout)
        : QWidget(flyout)
        , m_flyout(flyout)
    {
        setObjectName(u"fmDockFlyoutTitle"_s);
    }

    QSize sizeHint() const override { return {200, metricsOf(this).paneTitle}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const fs::ThemeColors &tc = colorsOf(this);
        const bool active = m_flyout->isActiveFlyout();
        fs::paintDockPaneTitle(&p, rect(), active, tc);
        int right = width() - 4;
        for (QWidget *child : findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
            if (child->isVisible())
                right = std::min(right, child->x() - 6);
        }
        const QString title = m_flyout->dock() ? m_flyout->dock()->title() : QString();
        fs::paintDockTitleText(&p, QRect(9, 0, std::max(0, right - 9), height()), title, fs::DockTitleKind::Pane, active,
                               tc);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            m_pressGlobal = event->globalPosition().toPoint();
        }
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        DockManager *m = m_flyout->m_manager;
        const QPoint global = event->globalPosition().toPoint();
        if (DragSession *drag = ManagerAccess::drag(m)) {
            drag->move(global);
            event->accept();
            return;
        }
        DockWidget *dock = m_flyout->dock();
        if (!m_pressed || !dock || (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance())
            return;
        m_pressed = false;
        if (dock->hasFeature(DockWidget::Movable))
            ManagerAccess::startDrag(m, {dock}, nullptr, this, global, mapFromGlobal(global));
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_pressed = false;
        if (DragSession *drag = ManagerAccess::drag(m_flyout->m_manager))
            drag->drop(event->globalPosition().toPoint());
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_flyout->dock())
            m_flyout->m_manager->setAutoHidden(m_flyout->dock(), false);
        event->accept();
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        if (m_flyout->dock())
            execDockMenu(m_flyout->dock(), event->globalPos(), this);
        event->accept();
    }

private:
    AutoHideFlyout *m_flyout;
    QPoint m_pressGlobal;
    bool m_pressed = false;
};

AutoHideFlyout::AutoHideFlyout(DockManager *manager, DockWidget *dock, SideBarLocation side)
    : QWidget(manager)
    , m_manager(manager)
    , m_dock(dock)
    , m_side(side)
{
    setObjectName(u"fmDockAutoHideFlyout"_s);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    fs::setPaneActive(this, false);
    if (!isHeadlessPlatform()) {
        // 다른 프로세스 창(네이티브 자식) 위에 겹치려면 이 창도 네이티브 자식이어야 한다
        setAttribute(Qt::WA_DontCreateNativeAncestors);
        setAttribute(Qt::WA_NativeWindow);
    }
    auto *title = new FlyoutTitle(this);
    m_title = title;
    m_unpin = new DockButton(u"unpin"_s, false, title);
    m_minimize = new DockButton(u"minimize"_s, false, title);
    m_close = new DockButton(u"close"_s, false, title);
    auto *row = new QHBoxLayout(title);
    row->setContentsMargins(0, 0, 4, 0);
    row->setSpacing(2);
    row->addStretch(1);
    for (DockButton *b : {m_unpin, m_minimize, m_close})
        row->addWidget(b, 0, Qt::AlignVCenter);
    connect(m_unpin, &QAbstractButton::clicked, this, [this] {
        if (m_dock)
            m_manager->setAutoHidden(m_dock, false);
    });
    connect(m_minimize, &QAbstractButton::clicked, this, [this] { m_manager->collapseAutoHide(); });
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        if (m_manager->testAutoHideFlag(DockManager::CloseButtonCollapses)) {
            m_manager->collapseAutoHide();
            return;
        }
        if (m_dock)
            m_dock->requestClose();
    });
    if (dock) {
        dock->setParent(this);
        if (!dock->isClosed())
            dock->show();
    }
    updateButtons();
    hide();
}

AutoHideFlyout::~AutoHideFlyout() = default;

void AutoHideFlyout::setSide(SideBarLocation side)
{
    m_side = side;
    layoutChildren();
}

void AutoHideFlyout::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    fs::setPaneActive(this, active);
    m_title->update();
    for (DockButton *b : {m_unpin, m_minimize, m_close})
        b->update();
}

DockWidget *AutoHideFlyout::releaseDock()
{
    DockWidget *dock = m_dock;
    m_dock = nullptr;
    return dock;
}

void AutoHideFlyout::updateButtons()
{
    const int extent = std::max(14, metricsOf(this).paneTitle - 6);
    for (DockButton *b : {m_unpin, m_minimize, m_close})
        b->setExtent(std::min(extent, metricsOf(this).areaButton));
    m_minimize->setVisible(m_manager->testAutoHideFlag(DockManager::HasMinimizeButton));
    m_close->setVisible(m_dock && m_dock->hasFeature(DockWidget::Closable));
    m_unpin->setVisible(m_dock && m_dock->hasFeature(DockWidget::Pinnable));
}

bool AutoHideFlyout::onResizeEdge(const QPoint &pos) const
{
    switch (m_side) {
    case SideBarLocation::Right: return pos.x() < kResizeGrip;
    case SideBarLocation::Top: return pos.y() >= height() - kResizeGrip;
    case SideBarLocation::Bottom: return pos.y() < kResizeGrip;
    case SideBarLocation::Left:
    case SideBarLocation::None: break;
    }
    return pos.x() >= width() - kResizeGrip;
}

void AutoHideFlyout::layoutChildren()
{
    const int frame = 1;
    QRect inner = rect().adjusted(frame, frame, -frame, -frame);
    switch (m_side) {
    case SideBarLocation::Right: inner.setLeft(inner.left() + kResizeGrip - frame); break;
    case SideBarLocation::Top: inner.setBottom(inner.bottom() - kResizeGrip + frame); break;
    case SideBarLocation::Bottom: inner.setTop(inner.top() + kResizeGrip - frame); break;
    case SideBarLocation::Left:
    case SideBarLocation::None: inner.setRight(inner.right() - kResizeGrip + frame); break;
    }
    const int titleHeight = metricsOf(this).paneTitle;
    m_title->setGeometry(inner.left(), inner.top(), inner.width(), titleHeight);
    if (m_dock)
        m_dock->setGeometry(inner.left(), inner.top() + titleHeight, inner.width(), std::max(0, inner.height() - titleHeight));
}

bool AutoHideFlyout::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::StyleChange:
        updateButtons();
        layoutChildren();
        break;
    case QEvent::Leave:
        if (m_manager->testAutoHideFlag(DockManager::ShowOnMouseOver)) {
            QTimer::singleShot(500, this, [this] {
                if (!isVisible() || underMouse() || !m_dock)
                    return;
                SideBar *bar = ManagerAccess::sideBar(m_manager, m_side);
                SideTab *tab = bar ? bar->tabFor(m_dock) : nullptr;
                if (tab && tab->underMouse())
                    return;
                if (QApplication::focusWidget() && isAncestorOf(QApplication::focusWidget()))
                    return;  // 입력 중이면 그대로
                m_manager->collapseAutoHide();
            });
        }
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

void AutoHideFlyout::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fs::paintDockFlyoutFrame(&p, rect(), colorsOf(this));
}

void AutoHideFlyout::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutChildren();
}

void AutoHideFlyout::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && onResizeEdge(event->position().toPoint())) {
        m_resizing = true;
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressSize = (m_side == SideBarLocation::Left || m_side == SideBarLocation::Right) ? width() : height();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AutoHideFlyout::mouseMoveEvent(QMouseEvent *event)
{
    const bool horizontal = m_side == SideBarLocation::Left || m_side == SideBarLocation::Right;
    if (!m_resizing) {
        setCursor(onResizeEdge(event->position().toPoint()) ? (horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor)
                                                            : Qt::ArrowCursor);
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
    const int change = m_side == SideBarLocation::Right    ? -delta.x()
                     : m_side == SideBarLocation::Bottom   ? -delta.y()
                     : m_side == SideBarLocation::Top      ? delta.y()
                                                           : delta.x();
    if (m_dock) {
        ManagerAccess::setAutoHideSize(m_dock, std::max(kMinimumFlyout, m_pressSize + change));
        ManagerAccess::positionFlyout(m_manager, this);
    }
    event->accept();
}

void AutoHideFlyout::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace detail

// =============================================================================================
// DockManager — 자동 숨김

detail::SideBar *DockManager::sideBar(SideBarLocation side) const
{
    const int i = detail::sideIndex(side);
    return i >= 0 ? m_sideBars[i] : nullptr;
}

void DockManager::updateSideBars()
{
    for (detail::SideBar *bar : m_sideBars) {
        if (bar)
            bar->updateVisibility();
    }
}

QList<DockWidget *> DockManager::autoHideDockWidgets(SideBarLocation side) const
{
    detail::SideBar *bar = sideBar(side);
    return bar ? bar->docks() : QList<DockWidget *>{};
}

SideBarLocation DockManager::sideForDock(const DockWidget *dock) const
{
    if (!dock)
        return SideBarLocation::Left;
    if (dock->preferredSideBar() != SideBarLocation::None)
        return dock->preferredSideBar();
    if (dock->isAutoHidden())
        return dock->autoHideSide();
    DockArea *area = dock->dockArea();
    if (!area || !area->isVisible() || area->dockContainer() != m_container)
        return SideBarLocation::Left;
    // 컨테이너 가장자리 중 가장 가까운 쪽(정규화 거리)
    const QRect c(m_container->mapToGlobal(QPoint(0, 0)), m_container->size());
    const QPoint center = area->mapToGlobal(area->rect().center());
    const double w = std::max(1, c.width());
    const double h = std::max(1, c.height());
    const double distances[4] = {(center.x() - c.left()) / w, (center.y() - c.top()) / h, (c.right() - center.x()) / w,
                                 (c.bottom() - center.y()) / h};
    int best = 0;
    for (int i = 1; i < 4; ++i) {
        if (distances[i] < distances[best])
            best = i;
    }
    return detail::sideAt(best);
}

void DockManager::pinDock(DockWidget *dock, SideBarLocation side, int index)
{
    if (!dock || side == SideBarLocation::None)
        return;
    registerDock(dock);
    Q_EMIT dock->aboutToReparent();
    const bool wasFloating = dock->isFloating();
    if (DockArea *area = dock->m_area) {
        dock->m_lastArea = area;
        dock->m_lastIndex = area->indexOf(dock);
        if (dock->m_autoHideSize <= 0) {
            const int extent = (side == SideBarLocation::Left || side == SideBarLocation::Right) ? area->width()
                                                                                                  : area->height();
            dock->m_autoHideSize = std::clamp(extent, detail::kMinimumFlyout, 900);
        }
        area->removeDockWidget(dock);
    }
    if (dock->m_autoHideSize <= 0)
        dock->m_autoHideSize = detail::kDefaultFlyout;
    dock->m_autoHideSide = side;
    new detail::AutoHideFlyout(this, dock, side);
    sideBar(side)->insertTab(index, dock);
    updateSideBars();
    Q_EMIT dock->reparented();
    if (wasFloating)
        Q_EMIT dock->topLevelChanged(false);
    for (FloatingWindow *w : floatingWindows())
        w->containerChanged();
}

void DockManager::takeFromAutoHide(DockWidget *dock)
{
    if (!dock || !dock->isAutoHidden())
        return;
    if (expandedAutoHideDockWidget() == dock)
        collapseAutoHide();
    if (detail::SideBar *bar = sideBar(dock->m_autoHideSide))
        bar->removeTab(dock);
    dock->m_autoHideSide = SideBarLocation::None;
    if (auto *flyout = detail::flyoutOf(dock)) {
        flyout->releaseDock();
        dock->hide();
        dock->setParent(this);   // 펼친 창은 곧 지운다 — 다음 자리가 가져갈 때까지 관리자 아래에
        flyout->deleteLater();
    }
    updateSideBars();
}

void DockManager::moveToSideBar(DockWidget *dock, SideBarLocation side, int index)
{
    if (!dock || side == SideBarLocation::None)
        return;
    if (dock->isAutoHidden()) {
        detail::SideBar *from = sideBar(dock->m_autoHideSide);
        detail::SideBar *to = sideBar(side);
        if (from == to) {
            const int at = int(from->docks().indexOf(dock));
            from->removeTab(dock);
            from->insertTab(at >= 0 && at < index ? index - 1 : index, dock);
            return;
        }
        from->removeTab(dock);
        dock->m_autoHideSide = side;
        if (auto *flyout = detail::flyoutOf(dock))
            flyout->setSide(side);
        to->insertTab(index, dock);
        updateSideBars();
        return;
    }
    pinDock(dock, side, index);
    Q_EMIT autoHideChanged(dock, true);
}

bool DockManager::setAutoHidden(DockWidget *dock, bool on, SideBarLocation side)
{
    if (!dock)
        return false;
    if (on) {
        if (!testAutoHideFlag(AutoHideEnabled) || !dock->hasFeature(DockWidget::Pinnable))
            return false;
        const SideBarLocation s = side == SideBarLocation::None ? sideForDock(dock) : side;
        if (dock->isAutoHidden()) {
            if (dock->m_autoHideSide != s)
                moveToSideBar(dock, s, -1);
            return true;
        }
        pinDock(dock, s, -1);
        Q_EMIT autoHideChanged(dock, true);
        return true;
    }
    if (!dock->isAutoHidden())
        return true;
    restoreFromAutoHide(dock);
    return true;
}

void DockManager::restoreFromAutoHide(DockWidget *dock)
{
    if (!dock || !dock->isAutoHidden())
        return;
    const SideBarLocation side = dock->m_autoHideSide;
    takeFromAutoHide(dock);
    Q_EMIT autoHideChanged(dock, false);
    DockArea *last = dock->m_lastArea;
    if (last && last->dockManager() == this && last->dockContainer())
        placeDocks({dock}, DropArea::Center, last, dock->m_lastIndex, nullptr);
    else
        placeDocks({dock}, detail::dropAreaFor(side), nullptr, -1, m_container);
}

void DockManager::pinArea(DockArea *area, SideBarLocation side)
{
    if (!area || !testAutoHideFlag(AutoHideEnabled))
        return;
    const QList<DockWidget *> docks = area->openDockWidgets();
    for (DockWidget *d : docks) {
        if (d->hasFeature(DockWidget::Pinnable)) {
            pinDock(d, side, -1);
            Q_EMIT autoHideChanged(d, true);
        }
    }
}

DockWidget *DockManager::expandedAutoHideDockWidget() const
{
    for (detail::SideBar *bar : m_sideBars) {
        if (!bar)
            continue;
        for (detail::SideTab *tab : bar->tabs()) {
            if (tab->isExpanded())
                return tab->dock();
        }
    }
    return nullptr;
}

void DockManager::positionFlyout(detail::AutoHideFlyout *flyout) const
{
    if (!flyout || !flyout->dock())
        return;
    const QRect c = m_container->geometry();
    const SideBarLocation side = flyout->side();
    const bool horizontal = side == SideBarLocation::Left || side == SideBarLocation::Right;
    const int extent = horizontal ? c.width() : c.height();
    int size = flyout->dock()->m_autoHideSize > 0 ? flyout->dock()->m_autoHideSize : detail::kDefaultFlyout;
    size = std::clamp(size, std::min(detail::kMinimumFlyout, extent), std::max(detail::kMinimumFlyout, extent * 8 / 10));
    QRect r;
    switch (side) {
    case SideBarLocation::Right: r = QRect(c.right() - size + 1, c.top(), size, c.height()); break;
    case SideBarLocation::Top: r = QRect(c.left(), c.top(), c.width(), size); break;
    case SideBarLocation::Bottom: r = QRect(c.left(), c.bottom() - size + 1, c.width(), size); break;
    case SideBarLocation::Left:
    case SideBarLocation::None: r = QRect(c.left(), c.top(), size, c.height()); break;
    }
    flyout->setGeometry(r);
}

void DockManager::expandAutoHide(DockWidget *dock)
{
    if (!dock) {
        collapseAutoHide();
        return;
    }
    if (!dock->isAutoHidden() || dock->isClosed())
        return;
    if (expandedAutoHideDockWidget() == dock)
        return;
    collapseAutoHide();
    auto *flyout = detail::flyoutOf(dock);
    if (!flyout)
        return;
    positionFlyout(flyout);
    flyout->updateButtons();
    flyout->show();
    flyout->raise();
    if (detail::SideBar *bar = sideBar(dock->m_autoHideSide)) {
        if (detail::SideTab *tab = bar->tabFor(dock))
            tab->setExpanded(true);
    }
    qApp->installEventFilter(this);
    flyout->setActive(true);
    if (QWidget *content = dock->widget(); content && content->focusPolicy() != Qt::NoFocus
                                            && dock->hasFeature(DockWidget::Focusable))
        content->setFocus(Qt::OtherFocusReason);
    else
        setFocusedDock(dock);
}

void DockManager::collapseAutoHide()
{
    DockWidget *dock = expandedAutoHideDockWidget();
    qApp->removeEventFilter(this);
    if (!dock)
        return;
    if (detail::SideBar *bar = sideBar(dock->m_autoHideSide)) {
        if (detail::SideTab *tab = bar->tabFor(dock))
            tab->setExpanded(false);
    }
    if (auto *flyout = detail::flyoutOf(dock)) {
        flyout->setActive(false);
        flyout->hide();
    }
}

} // namespace fm::dock
