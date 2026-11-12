#include "DockInternal_p.h"

#include <fmstyle/StyleProps.h>
#include <fmwidgets/Menu.h>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedLayout>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("fm::dock", text);
}

/// 넘친 탭 수 "+N" — 탭 목록 단추 왼쪽.
class OverflowCount : public QWidget
{
public:
    explicit OverflowCount(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
    }
    void setCount(int count)
    {
        if (m_count == count)
            return;
        m_count = count;
        setVisible(count > 0);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override
    {
        QFont f = font();
        f.setPixelSize(11);
        f.setWeight(QFont::DemiBold);
        return {QFontMetrics(f).horizontalAdvance(text()) + 2, 16};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QFont f = font();
        f.setPixelSize(11);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(detail::colorsOf(this)[fs::Token::Fg2]);
        p.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    QString text() const { return u"+"_s + QString::number(m_count); }
    int m_count = 0;
};

} // namespace

// =============================================================================================
// DockArea

DockArea::DockArea(DockManager *manager, QWidget *parent)
    : QFrame(parent)
    , m_manager(manager)
{
    setObjectName(u"fmDockArea"_s);
    m_layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    m_layout->setSpacing(0);
    m_titleBar = new detail::DockAreaTitleBar(this);
    m_contentHost = new QWidget(this);
    m_contentHost->setObjectName(u"fmDockAreaContent"_s);
    m_stack = new QStackedLayout(m_contentHost);
    m_stack->setContentsMargins(0, 0, 0, 0);
    if (manager && manager->testConfigFlag(DockManager::FocusHighlighting))
        fs::setPaneActive(this, false);
    updateLayoutOrder();

    // 키보드(07 §4.10) — 영역 안에 포커스가 있을 때만: 앞 · 뒤 탭, 문서 탭 닫기
    const auto shortcut = [this](const QKeySequence &keys, auto &&slot) {
        auto *a = new QAction(this);
        a->setShortcut(keys);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(a);
        connect(a, &QAction::triggered, this, slot);
    };
    const auto cycle = [this](int delta) {
        const QList<DockWidget *> open = openDockWidgets();
        if (open.size() < 2)
            return;
        const int at = int(open.indexOf(currentDockWidget()));
        DockWidget *next = open[(std::max(0, at) + delta + int(open.size())) % int(open.size())];
        setCurrentDockWidget(next);
        if (QWidget *content = next->widget(); content && next->hasFeature(DockWidget::Focusable))
            content->setFocus(Qt::ShortcutFocusReason);
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), [cycle] { cycle(-1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), [cycle] { cycle(1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_F4), [this] {
        if (DockWidget *dock = currentDockWidget(); dock && dock->isDocument())
            dock->requestClose();
    });
    hide();
}

DockArea::~DockArea()
{
    m_dying = true;
    for (DockWidget *dock : std::as_const(m_docks)) {
        if (dock && dock->m_area == this)
            dock->m_area = nullptr;
    }
    if (DockContainer *container = dockContainer(); container && !container->m_dying)
        container->m_areas.removeAll(this);
}

DockContainer *DockArea::dockContainer() const
{
    for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
        if (auto *c = qobject_cast<DockContainer *>(p))
            return c;
    }
    return nullptr;
}

QList<DockWidget *> DockArea::dockWidgets() const
{
    return m_docks;
}

QList<DockWidget *> DockArea::openDockWidgets() const
{
    QList<DockWidget *> out;
    for (DockWidget *dock : m_docks) {
        if (dock && !dock->isClosed())
            out.append(dock);
    }
    return out;
}

int DockArea::openDockWidgetsCount() const
{
    return int(std::count_if(m_docks.cbegin(), m_docks.cend(), [](DockWidget *d) { return d && !d->isClosed(); }));
}

DockWidget *DockArea::dockWidget(int index) const
{
    return index >= 0 && index < m_docks.size() ? m_docks[index] : nullptr;
}

int DockArea::indexOf(const DockWidget *dock) const
{
    return int(m_docks.indexOf(const_cast<DockWidget *>(dock)));
}

DockWidget *DockArea::currentDockWidget() const
{
    return dockWidget(m_current);
}

void DockArea::setCurrentIndex(int index)
{
    DockWidget *dock = dockWidget(index);
    if (!dock || dock->isClosed())
        return;
    const bool changed = m_current != index;
    m_current = index;
    m_stack->setCurrentWidget(dock);
    if (dock->isHidden())
        dock->show();   // 닫았다 연 도크가 이미 스택의 현재 위젯이면 QStackedLayout은 다시 보이지 않는다
    m_titleBar->tabBar()->setCurrentIndex(index);
    m_titleBar->updateButtons();
    if (DockContainer *container = dockContainer(); container && container->floatingWindow())
        container->floatingWindow()->updateTitle();
    if (changed)
        Q_EMIT currentChanged(index);
}

void DockArea::setCurrentDockWidget(DockWidget *dock)
{
    setCurrentIndex(indexOf(dock));
}

void DockArea::moveTab(int from, int to)
{
    if (from < 0 || from >= m_docks.size() || to < 0 || to >= m_docks.size() || from == to)
        return;
    DockWidget *current = currentDockWidget();
    DockWidget *moved = m_docks[from];
    m_docks.move(from, to);
    m_stack->removeWidget(moved);
    m_stack->insertWidget(to, moved);
    m_titleBar->tabBar()->moveTab(from, to);
    m_current = indexOf(current);
    if (current)
        m_stack->setCurrentWidget(current);
    m_titleBar->tabBar()->setCurrentIndex(m_current);
    Q_EMIT tabMoved(from, to);
}

bool DockArea::isCentral() const
{
    for (QWidget *p = parentWidget(); p && !qobject_cast<DockContainer *>(p); p = p->parentWidget()) {
        if (auto *s = qobject_cast<detail::DockSplitter *>(p); s && s->isCentral())
            return true;
    }
    return false;
}

int DockArea::hiddenTabCount() const
{
    return m_titleBar->tabBar()->hiddenCount();
}

void DockArea::closeArea()
{
    if (m_manager && m_manager->testConfigFlag(DockManager::AreaCloseButtonClosesTab)) {
        if (DockWidget *dock = currentDockWidget())
            dock->requestClose();
        return;
    }
    const QList<DockWidget *> open = openDockWidgets();
    QList<QPointer<DockWidget>> docks(open.cbegin(), open.cend());
    for (const QPointer<DockWidget> &dock : docks) {
        if (dock && (dock->hasFeature(DockWidget::Closable) || dock->features().testFlag(DockWidget::ForceCloseWithArea)))
            dock->requestClose();
    }
}

void DockArea::closeOtherTabs(DockWidget *keep)
{
    const QList<DockWidget *> open = openDockWidgets();
    QList<QPointer<DockWidget>> docks(open.cbegin(), open.cend());
    for (const QPointer<DockWidget> &dock : docks) {
        if (dock && dock != keep && dock->hasFeature(DockWidget::Closable))
            dock->requestClose();
    }
}

QRect DockArea::titleBarGeometry() const
{
    return m_titleBar->isVisible() ? m_titleBar->geometry() : QRect();
}

QRect DockArea::contentGeometry() const
{
    return m_contentHost->geometry();
}

void DockArea::insertDockWidget(int index, DockWidget *dock, bool activate)
{
    if (!dock)
        return;
    if (dock->m_area == this) {
        const int from = indexOf(dock);
        const int to = std::clamp(index < 0 ? int(m_docks.size()) - 1 : index, 0, int(m_docks.size()) - 1);
        moveTab(from, to);
        if (activate)
            setCurrentDockWidget(dock);
        return;
    }
    if (DockArea *old = dock->m_area)
        old->removeDockWidget(dock);
    index = std::clamp(index < 0 ? int(m_docks.size()) : index, 0, int(m_docks.size()));
    m_docks.insert(index, dock);
    m_stack->insertWidget(index, dock);   // 여기서 한 번만 부모가 바뀐다
    dock->setDockArea(this);
    m_titleBar->tabBar()->insertTab(index, dock);
    if (m_current >= index)
        ++m_current;
    if ((activate || m_current < 0) && !dock->isClosed())
        setCurrentIndex(index);
    else if (DockWidget *current = currentDockWidget())
        m_stack->setCurrentWidget(current);
    m_titleBar->tabBar()->setCurrentIndex(m_current);
    updateTitleBar();
    updateVisibility();
}

void DockArea::removeDockWidget(DockWidget *dock)
{
    const int i = indexOf(dock);
    if (i < 0)
        return;
    DockWidget *current = currentDockWidget();
    m_stack->removeWidget(dock);
    m_docks.removeAt(i);
    m_titleBar->tabBar()->removeTab(dock);
    dock->setDockArea(nullptr);
    if (current == dock) {
        // 다음 열린 탭(없으면 앞 탭)
        m_current = -1;
        for (int k = i; k < m_docks.size() && m_current < 0; ++k) {
            if (!m_docks[k]->isClosed())
                m_current = k;
        }
        for (int k = i - 1; k >= 0 && m_current < 0; --k) {
            if (!m_docks[k]->isClosed())
                m_current = k;
        }
    } else {
        m_current = indexOf(current);
    }
    if (m_current >= 0)
        setCurrentIndex(m_current);
    else
        m_titleBar->tabBar()->setCurrentIndex(-1);
    updateTitleBar();
    updateVisibility();
    if (m_docks.isEmpty() && !m_dying) {
        if (DockContainer *container = dockContainer())
            container->removeAreaIfEmpty(this);
    }
}

void DockArea::forgetDockWidget(DockWidget *dock)
{
    const int i = indexOf(dock);
    if (i < 0)
        return;
    DockWidget *current = currentDockWidget();
    m_docks.removeAt(i);
    m_titleBar->tabBar()->removeTab(dock);
    m_current = current == dock ? -1 : indexOf(current);
    if (m_current < 0) {
        for (int k = 0; k < m_docks.size() && m_current < 0; ++k) {
            if (!m_docks[k]->isClosed())
                m_current = k;
        }
    }
    if (m_dying)
        return;
    if (m_current >= 0)
        setCurrentIndex(m_current);
    updateTitleBar();
    updateVisibility();
    if (m_docks.isEmpty()) {
        if (DockContainer *container = dockContainer())
            container->removeAreaIfEmpty(this);
    }
}

void DockArea::dockOpenStateChanged(DockWidget *dock)
{
    if (dock->isClosed()) {
        if (currentDockWidget() == dock || m_current < 0) {
            m_current = -1;
            const int i = indexOf(dock);
            for (int k = i + 1; k < m_docks.size() && m_current < 0; ++k) {
                if (!m_docks[k]->isClosed())
                    m_current = k;
            }
            for (int k = i - 1; k >= 0 && m_current < 0; --k) {
                if (!m_docks[k]->isClosed())
                    m_current = k;
            }
            if (m_current >= 0)
                setCurrentIndex(m_current);
        }
    } else if (m_current < 0 || !currentDockWidget() || currentDockWidget()->isClosed()) {
        setCurrentDockWidget(dock);
    }
    if (detail::DockTab *tab = m_titleBar->tabBar()->tabFor(dock))
        tab->updateState();
    m_titleBar->tabBar()->setCurrentIndex(m_current);
    updateTitleBar();
    updateVisibility();
}

void DockArea::updateTitleBar()
{
    const QList<DockWidget *> open = openDockWidgets();
    bool hide = !open.isEmpty() && std::all_of(open.cbegin(), open.cend(), [](DockWidget *d) {
        return d->features().testFlag(DockWidget::NoTab);
    });
    if (m_manager && isCentral() && m_manager->testConfigFlag(DockManager::HideSingleCentralTitleBar) && open.size() == 1
        && open.first()->features().testFlag(DockWidget::NoTab))
        hide = true;
    if (DockContainer *container = dockContainer(); container && container->isFloating() && m_manager
        && !m_manager->testConfigFlag(DockManager::AlwaysShowTabs) && open.size() == 1
        && container->openedDockWidgets().size() == 1)
        hide = true;
    m_titleBar->setVisible(!hide);
    m_titleBar->updateButtons();
}

void DockArea::updateVisibility()
{
    const bool any = openDockWidgetsCount() > 0;
    if (any == isHidden())
        setVisible(any);
    if (DockContainer *container = dockContainer())
        container->areaVisibilityChanged();
}

void DockArea::updateLayoutOrder()
{
    const bool bottom = m_manager && m_manager->testConfigFlag(DockManager::TabsAtBottom);
    m_layout->removeWidget(m_titleBar);
    m_layout->removeWidget(m_contentHost);
    if (bottom) {
        m_layout->addWidget(m_contentHost, 1);
        m_layout->addWidget(m_titleBar);
    } else {
        m_layout->addWidget(m_titleBar);
        m_layout->addWidget(m_contentHost, 1);
    }
    const fs::DockMetrics m = detail::metricsOf(this);
    const int border = m.areaBorder ? 1 : 0;
    m_layout->setContentsMargins(border, border, border, border);
    const int c = m.contentBorder ? 1 : 0;
    m_contentHost->setContentsMargins(c, bottom ? c : 0, c, bottom ? 0 : c);
}

void DockArea::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (m_manager && m_manager->testConfigFlag(DockManager::FocusHighlighting)) {
        fs::setPaneActive(this, active);
        m_titleBar->update();
        const auto tabs = m_titleBar->findChildren<QWidget *>();
        for (QWidget *w : tabs)
            w->update();
        const auto views = m_contentHost->findChildren<QAbstractItemView *>();
        for (QAbstractItemView *view : views)
            view->viewport()->update();
    }
    update();
}

void DockArea::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const bool bottom = m_manager && m_manager->testConfigFlag(DockManager::TabsAtBottom);
    fs::paintDockAreaFrame(&p, rect(), m_contentHost->geometry(), bottom ? Qt::BottomEdge : Qt::TopEdge,
                           detail::colorsOf(this));
}

void DockArea::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange)
        updateLayoutOrder();
}

// =============================================================================================
// DockAreaTitleBar

namespace detail {

DockAreaTitleBar::DockAreaTitleBar(DockArea *area)
    : QWidget(area)
    , m_area(area)
{
    setObjectName(u"fmDockAreaTitleBar"_s);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tabBar = new DockTabBar(area, this);
    m_overflow = new OverflowCount(this);
    m_menu = new DockButton(u"menu"_s, true, this);
    m_undock = new DockButton(u"float"_s, true, this);
    m_pin = new DockButton(u"pin"_s, true, this);
    m_close = new DockButton(u"close"_s, true, this);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(4, 0, 3, 0);
    row->setSpacing(1);
    row->addWidget(m_tabBar, 1);
    row->addSpacing(2);
    row->addWidget(m_overflow, 0, Qt::AlignVCenter);
    for (DockButton *b : {m_menu, m_undock, m_pin, m_close})
        row->addWidget(b, 0, Qt::AlignVCenter);

    connect(m_menu, &QAbstractButton::clicked, this, &DockAreaTitleBar::showTabsMenu);
    connect(m_undock, &QAbstractButton::clicked, this, [this] {
        DockManager *m = m_area->dockManager();
        const QList<DockWidget *> docks = m_area->openDockWidgets();
        if (m && !docks.isEmpty())
            ManagerAccess::floatDocks(m, docks, ManagerAccess::defaultFloatingGeometry(m, docks));
    });
    connect(m_pin, &QAbstractButton::clicked, this, [this] {
        DockManager *m = m_area->dockManager();
        DockWidget *current = m_area->currentDockWidget();
        if (!m || !current)
            return;
        const bool ctrl = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        const bool wholeArea = m->testAutoHideFlag(DockManager::PinButtonTogglesArea) != ctrl;
        const SideBarLocation side = ManagerAccess::sideForDock(m, current);
        if (wholeArea)
            ManagerAccess::pinArea(m, m_area, side);
        else
            m->setAutoHidden(current, true, side);
    });
    connect(m_close, &QAbstractButton::clicked, this, [this] { m_area->closeArea(); });
    updateButtons();
}

QSize DockAreaTitleBar::sizeHint() const
{
    return {m_tabBar->sizeHint().width() + 4 * metricsOf(this).areaButton, stripHeightOf(this)};
}

QSize DockAreaTitleBar::minimumSizeHint() const
{
    return {60, stripHeightOf(this)};
}

void DockAreaTitleBar::updateOverflow()
{
    if (m_tabBar && m_overflow)   // 만드는 중(탭 바의 글꼴 변경)에도 불린다
        static_cast<OverflowCount *>(m_overflow)->setCount(m_tabBar->hiddenCount());
}

void DockAreaTitleBar::updateButtons()
{
    DockManager *m = m_area->dockManager();
    if (!m)
        return;
    const QList<DockWidget *> open = m_area->openDockWidgets();
    DockWidget *current = m_area->currentDockWidget();
    DockContainer *container = m_area->dockContainer();
    const bool aloneInWindow = container && container->isFloating() && container->openedDockAreas().size() <= 1;
    const bool hideDisabled = m->testConfigFlag(DockManager::AreaHideDisabledButtons);

    bool floatable = !open.isEmpty() && !m->testConfigFlag(DockManager::NoTopLevelWindows) && !aloneInWindow;
    bool closable = false;
    for (DockWidget *d : open) {
        floatable = floatable && d->hasFeature(DockWidget::Floatable);
        closable = closable || d->hasFeature(DockWidget::Closable);
    }
    if (m->testConfigFlag(DockManager::AreaCloseButtonClosesTab))
        closable = current && current->hasFeature(DockWidget::Closable);
    const bool pinnable = current && current->hasFeature(DockWidget::Pinnable)
                       && m->testAutoHideFlag(DockManager::AutoHideEnabled);

    const auto apply = [hideDisabled](DockButton *b, bool wanted, bool enabled) {
        b->setVisible(wanted && (enabled || !hideDisabled));
        b->setEnabled(enabled);
    };
    apply(m_menu, m->testConfigFlag(DockManager::AreaHasTabsMenuButton), !open.isEmpty());
    apply(m_undock, m->testConfigFlag(DockManager::AreaHasUndockButton), floatable);
    apply(m_pin, m->testAutoHideFlag(DockManager::AreaHasPinButton), pinnable);
    apply(m_close, m->testConfigFlag(DockManager::AreaHasCloseButton), closable);
    for (DockButton *b : {m_menu, m_undock, m_pin, m_close})
        b->setExtent(metricsOf(this).areaButton);
    updateOverflow();
}

void DockAreaTitleBar::showTabsMenu()
{
    fm::ui::Menu menu(this);
    const QPointer<DockArea> area(m_area);
    for (DockWidget *dock : m_area->openDockWidgets()) {
        QAction *a = menu.addAction(dock->icon(), dock->title());
        a->setCheckable(true);
        a->setChecked(dock == m_area->currentDockWidget());
        const QPointer<DockWidget> guard(dock);
        connect(a, &QAction::triggered, &menu, [area, guard] {
            if (area && guard) {
                area->setCurrentDockWidget(guard);
                if (QWidget *content = guard->widget(); content && guard->hasFeature(DockWidget::Focusable))
                    content->setFocus(Qt::OtherFocusReason);
            }
        });
    }
    menu.exec(m_menu->mapToGlobal(QPoint(0, m_menu->height())));
}

void DockAreaTitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fs::paintDockStrip(&p, rect(), m_tabBar->atBottom() ? Qt::TopEdge : Qt::BottomEdge, colorsOf(this));
}

void DockAreaTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_pressGlobal = event->globalPosition().toPoint();
        if (DockWidget *current = m_area->currentDockWidget(); current && current->hasFeature(DockWidget::Focusable)) {
            if (QWidget *content = current->widget(); content && content->focusPolicy() != Qt::NoFocus)
                content->setFocus(Qt::MouseFocusReason);
            else if (DockManager *m = m_area->dockManager())
                ManagerAccess::setFocusedDock(m, current);
        }
        event->accept();
        return;
    }
    event->ignore();
}

void DockAreaTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    DockManager *m = m_area->dockManager();
    const QPoint global = event->globalPosition().toPoint();
    if (m) {
        if (DragSession *drag = ManagerAccess::drag(m)) {
            drag->move(global);
            event->accept();
            return;
        }
    }
    if (!m || !m_pressed || !(event->buttons() & Qt::LeftButton)
        || (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance()) {
        event->ignore();
        return;
    }
    const QList<DockWidget *> docks = m_area->openDockWidgets();
    const bool movable = !docks.isEmpty() && std::all_of(docks.cbegin(), docks.cend(), [](DockWidget *d) {
        return d->hasFeature(DockWidget::Movable);
    });
    m_pressed = false;
    if (movable)
        ManagerAccess::startDrag(m, docks, m_area, this, global, mapFromGlobal(global));
    event->accept();
}

void DockAreaTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    if (DockManager *m = m_area->dockManager()) {
        if (DragSession *drag = ManagerAccess::drag(m)) {
            drag->drop(event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }
    event->ignore();
}

void DockAreaTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    DockManager *m = m_area->dockManager();
    if (event->button() != Qt::LeftButton || !m || !m->testConfigFlag(DockManager::DoubleClickUndocks)) {
        event->ignore();
        return;
    }
    event->accept();
    const QList<DockWidget *> docks = m_area->openDockWidgets();
    if (docks.isEmpty())
        return;
    if (DockContainer *c = m_area->dockContainer(); c && c->isFloating()) {
        ManagerAccess::dockBack(m, docks.first());
        return;
    }
    const bool floatable = !m->testConfigFlag(DockManager::NoTopLevelWindows)
                        && std::all_of(docks.cbegin(), docks.cend(), [](DockWidget *d) {
                               return d->hasFeature(DockWidget::Floatable);
                           });
    if (floatable)
        ManagerAccess::floatDocks(m, docks, ManagerAccess::defaultFloatingGeometry(m, docks));
}

void DockAreaTitleBar::contextMenuEvent(QContextMenuEvent *event)
{
    execAreaMenu(m_area, event->globalPos(), this);
    event->accept();
}

void DockAreaTitleBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange) {
        updateGeometry();
        updateButtons();
    }
}

} // namespace detail

} // namespace fm::dock
