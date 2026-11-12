#include "DockInternal_p.h"
#include "StateCodec_p.h"

#include <fmstyle/StyleProps.h>
#include <fmwidgets/Menu.h>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QGridLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QScreen>
#include <QSet>
#include <QStackedLayout>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

using detail::DockSplitter;
using detail::LayoutNode;

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("fm::dock", text);
}

QStringList featureNames(DockWidget::Features f)
{
    QStringList out;
    if (f.testFlag(DockWidget::Closable))
        out << u"closable"_s;
    if (f.testFlag(DockWidget::Movable))
        out << u"movable"_s;
    if (f.testFlag(DockWidget::Floatable))
        out << u"floatable"_s;
    if (f.testFlag(DockWidget::Pinnable))
        out << u"pinnable"_s;
    return out;
}

DockWidget::Features featuresFromNames(const QStringList &names)
{
    DockWidget::Features f;
    for (const QString &n : names) {
        if (n == u"closable")
            f |= DockWidget::Closable;
        else if (n == u"movable")
            f |= DockWidget::Movable;
        else if (n == u"floatable")
            f |= DockWidget::Floatable;
        else if (n == u"pinnable")
            f |= DockWidget::Pinnable;
    }
    return f;
}

bool isPlaceholder(const QWidget *w)
{
    return qobject_cast<const detail::CentralPlaceholder *>(w) != nullptr;
}

} // namespace

// =============================================================================================
// 생성 · 설정

DockManager::DockManager(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(u"fmDockManager"_s);
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(0);
    m_container = new DockContainer(this, this, nullptr);
    for (int i = 0; i < 4; ++i) {
        m_sideBars[i] = new detail::SideBar(this, detail::sideAt(i));
        m_sideBars[i]->hide();
    }
    m_grid->addWidget(m_sideBars[0], 0, 0, 3, 1);  // 왼쪽 — 위아래로 끝까지
    m_grid->addWidget(m_sideBars[1], 0, 1);        // 위 — 왼쪽 · 오른쪽 사이
    m_grid->addWidget(m_container, 1, 1);
    m_grid->addWidget(m_sideBars[3], 2, 1);        // 아래
    m_grid->addWidget(m_sideBars[2], 0, 2, 3, 1);  // 오른쪽
    m_grid->setRowStretch(1, 1);
    m_grid->setColumnStretch(1, 1);
    if (auto *mainWindow = qobject_cast<QMainWindow *>(parent))
        mainWindow->setCentralWidget(this);
    connect(qApp, &QApplication::focusChanged, this, &DockManager::onFocusChanged);
}

DockManager::~DockManager()
{
    m_dying = true;
    qApp->removeEventFilter(this);
    if (m_drag)
        m_drag->deleteLater();
    // 떠 있는 창은 메인 창이 소유한다 — 관리자와 함께 지운다
    const QList<QPointer<FloatingWindow>> floating = m_floating;
    for (const QPointer<FloatingWindow> &w : floating) {
        if (w)
            delete w.data();
    }
    // 어디에도 놓이지 않은(부모 없는) 도크도 관리자 것이다
    const QList<QPointer<DockWidget>> docks = m_docks;
    for (const QPointer<DockWidget> &d : docks) {
        if (d && !d->parentWidget())
            delete d.data();
    }
}

void DockManager::setConfigFlags(ConfigFlags flags)
{
    if (m_config == flags)
        return;
    m_config = flags;
    refreshAll();
}

void DockManager::setConfigFlag(ConfigFlag flag, bool on)
{
    ConfigFlags f = m_config;
    f.setFlag(flag, on);
    setConfigFlags(f);
}

void DockManager::setAutoHideFlags(AutoHideFlags flags)
{
    if (m_autoHide == flags)
        return;
    m_autoHide = flags;
    refreshAll();
}

void DockManager::setAutoHideFlag(AutoHideFlag flag, bool on)
{
    AutoHideFlags f = m_autoHide;
    f.setFlag(flag, on);
    setAutoHideFlags(f);
}

void DockManager::refreshAll()
{
    for (DockContainer *c : dockContainers()) {
        const auto splitters = c->findChildren<DockSplitter *>();
        for (DockSplitter *s : splitters) {
            s->setOpaqueResize(testConfigFlag(OpaqueSplitterResize));
            s->applyMetrics();
        }
        for (DockArea *a : c->dockAreas()) {
            if (testConfigFlag(FocusHighlighting))
                fs::setPaneActive(a, a->isActive());
            else
                a->setProperty(fs::props::kPaneActive, QVariant());
            a->updateLayoutOrder();
            a->updateTitleBar();
            for (int i = 0; i < a->titleBar()->tabBar()->count(); ++i)
                a->titleBar()->tabBar()->tabAt(i)->updateState();
            a->titleBar()->tabBar()->relayout();
            a->update();
        }
    }
    updateSideBars();
}

QList<FloatingWindow *> DockManager::floatingWindows() const
{
    QList<FloatingWindow *> out;
    for (const QPointer<FloatingWindow> &w : m_floating) {
        if (w)
            out.append(w);
    }
    return out;
}

QList<DockContainer *> DockManager::dockContainers() const
{
    QList<DockContainer *> out{m_container};
    for (FloatingWindow *w : floatingWindows())
        out.append(w->dockContainer());
    return out;
}

// =============================================================================================
// 등록

void DockManager::registerDock(DockWidget *dock)
{
    if (!dock || dock->m_manager == this)
        return;
    if (dock->m_manager)
        dock->m_manager->removeDockWidget(dock);
    if (DockWidget *same = findDockWidget(dock->id()); same && same != dock)
        qWarning("fmdock: dock id '%s' is already used", qPrintable(dock->id()));
    dock->m_manager = this;
    m_docks.append(dock);
    Q_EMIT dockWidgetAdded(dock);
}

void DockManager::unregisterDock(DockWidget *dock)
{
    m_docks.removeAll(dock);
    m_docks.removeAll(nullptr);
    if (dock->isAutoHidden()) {
        if (detail::SideBar *bar = sideBar(dock->m_autoHideSide))
            bar->removeTab(dock);
        dock->m_autoHideSide = SideBarLocation::None;
    }
    if (m_focused == dock)
        m_focused = nullptr;
    if (m_centralWidget == dock)
        m_centralWidget = nullptr;
    dock->m_manager = nullptr;
    Q_EMIT dockWidgetRemoved(dock);
}

void DockManager::removeDockWidget(DockWidget *dock)
{
    if (!dock || dock->m_manager != this)
        return;
    if (expandedAutoHideDockWidget() == dock)
        collapseAutoHide();
    detachDock(dock);
    dock->hide();
    dock->setParent(nullptr);
    unregisterDock(dock);
    for (FloatingWindow *w : floatingWindows())
        w->containerChanged();
}

DockWidget *DockManager::findDockWidget(const QString &id) const
{
    for (const QPointer<DockWidget> &d : m_docks) {
        if (d && d->id() == id)
            return d;
    }
    return nullptr;
}

QList<DockWidget *> DockManager::dockWidgets() const
{
    QList<DockWidget *> out;
    for (const QPointer<DockWidget> &d : m_docks) {
        if (d)
            out.append(d);
    }
    return out;
}

void DockManager::detachDock(DockWidget *dock)
{
    if (dock->isAutoHidden())
        takeFromAutoHide(dock);
    if (DockArea *area = dock->m_area)
        area->removeDockWidget(dock);
}

// =============================================================================================
// 넣기

DockArea *DockManager::setCentralWidget(DockWidget *central)
{
    if (!central)
        return nullptr;
    registerDock(central);
    central->m_central = true;
    central->m_features |= DockWidget::NoTab;
    m_centralWidget = central;
    m_container->ensureCentralRegion();
    placeDocks({central}, DropArea::Center, nullptr, -1, m_container);
    return central->dockArea();
}

void DockManager::setCentralPlaceholderText(const QString &text)
{
    m_placeholderText = text;
    if (QWidget *p = m_container->centralPlaceholder())
        p->update();
}

DockArea *DockManager::addDockWidget(DropArea where, DockWidget *dock, DockArea *target, int index)
{
    if (!dock)
        return nullptr;
    registerDock(dock);
    if (where == DropArea::Center)
        return target ? addDockWidgetTab(dock, target, index) : addDockWidgetToCentral(dock);
    placeDocks({dock}, where, target, index, target ? target->dockContainer() : m_container);
    return dock->dockArea();
}

DockArea *DockManager::addDockWidgetTab(DockWidget *dock, DockArea *target, int index)
{
    if (!dock)
        return nullptr;
    if (!target)
        return addDockWidget(DropArea::Right, dock);
    registerDock(dock);
    placeDocks({dock}, DropArea::Center, target, index, nullptr);
    return dock->dockArea();
}

DockArea *DockManager::addDockWidgetToCentral(DockWidget *dock)
{
    if (!dock)
        return nullptr;
    registerDock(dock);
    if (DockArea *area = lastActiveCentralArea())
        return addDockWidgetTab(dock, area);
    m_container->ensureCentralRegion();
    placeDocks({dock}, DropArea::Center, nullptr, -1, m_container);
    return dock->dockArea();
}

DockWidget *DockManager::addAutoHideDockWidget(SideBarLocation side, DockWidget *dock)
{
    if (!dock)
        return nullptr;
    registerDock(dock);
    pinDock(dock, side == SideBarLocation::None ? SideBarLocation::Left : side, -1);
    Q_EMIT autoHideChanged(dock, true);
    return dock;
}

FloatingWindow *DockManager::addFloatingDockWidget(DockWidget *dock, const QRect &geometry)
{
    if (!dock)
        return nullptr;
    registerDock(dock);
    return floatDocks({dock}, geometry.isValid() ? geometry : defaultFloatingGeometry({dock}));
}

DockArea *DockManager::lastActiveCentralArea() const
{
    if (m_lastCentralArea && m_lastCentralArea->dockContainer() == m_container && m_lastCentralArea->isCentral())
        return m_lastCentralArea;
    for (DockArea *a : m_container->centralAreas()) {
        if (a->dockWidgetsCount() > 0 && !(a->dockWidgetsCount() == 1 && a->dockWidget(0) == m_centralWidget))
            return a;
    }
    return nullptr;
}

bool DockManager::canDropInto(const QList<DockWidget *> &docks, const DockArea *target, bool center) const
{
    if (!target)
        return true;
    const bool anyDocument = std::any_of(docks.cbegin(), docks.cend(), [](DockWidget *d) { return d->isDocument(); });
    const bool anyTool = std::any_of(docks.cbegin(), docks.cend(), [](DockWidget *d) { return !d->isDocument(); });
    if (target->isCentral() && anyTool && testConfigFlag(CentralRejectsTools))
        return false;
    if (!target->isCentral() && anyDocument && testConfigFlag(DocumentsStayCentral))
        return false;
    const QList<DockWidget *> open = target->openDockWidgets();
    const bool wholeTarget = !open.isEmpty() && std::all_of(open.cbegin(), open.cend(), [&docks](DockWidget *d) {
        return docks.contains(d);
    });
    if (wholeTarget)
        return false;
    if (center && std::all_of(docks.cbegin(), docks.cend(), [target](DockWidget *d) { return d->dockArea() == target; }))
        return false;
    // 중앙 고정 위젯 영역에는 탭으로 넣지 않는다
    if (center && open.size() == 1 && open.first() == m_centralWidget)
        return false;
    return true;
}

bool DockManager::moveDockWidget(DockWidget *dock, DropArea where, DockArea *target, int index)
{
    if (!dock)
        return false;
    if (target && target->dockManager() != this)
        return false;
    registerDock(dock);
    if (target && !canDropInto({dock}, target, where == DropArea::Center))
        return false;
    if (where == DropArea::Center && !target) {
        if (DockArea *area = lastActiveCentralArea(); area && area != dock->dockArea()) {
            placeDocks({dock}, DropArea::Center, area, index, nullptr);
        } else {
            m_container->ensureCentralRegion();
            placeDocks({dock}, DropArea::Center, nullptr, -1, m_container);
        }
        return true;
    }
    placeDocks({dock}, where, target, index, target ? target->dockContainer() : m_container);
    return true;
}

void DockManager::setAreaSize(DockArea *area, Qt::Orientation orientation, int size)
{
    if (!area || area->dockManager() != this)
        return;
    if (DockContainer *c = area->dockContainer())
        c->resizeSlot(area, orientation, size);
}

void DockManager::placeDocks(const QList<DockWidget *> &input, DropArea where, DockArea *target, int index,
                             DockContainer *container)
{
    QList<DockWidget *> docks;
    for (DockWidget *d : input) {
        if (d && !docks.contains(d))
            docks.append(d);
    }
    if (docks.isEmpty())
        return;
    if (!container)
        container = target ? target->dockContainer() : m_container;
    if (!container)
        return;
    QHash<DockWidget *, bool> wasFloating;
    for (DockWidget *d : docks) {
        registerDock(d);
        wasFloating.insert(d, d->isFloating());
        Q_EMIT d->aboutToReparent();
    }
    if (DockWidget *expanded = expandedAutoHideDockWidget(); expanded && docks.contains(expanded))
        collapseAutoHide();
    for (DockWidget *d : docks) {
        if (d->isAutoHidden()) {
            takeFromAutoHide(d);
            Q_EMIT autoHideChanged(d, false);
        }
    }

    DockArea *area = nullptr;
    if (where == DropArea::Center && target) {
        area = target;
        int i = index < 0 ? target->dockWidgetsCount() : std::min(index, target->dockWidgetsCount());
        for (DockWidget *d : docks) {
            if (d->m_area == target) {
                const int from = target->indexOf(d);
                const int to = std::clamp(from < i ? i - 1 : i, 0, target->dockWidgetsCount() - 1);
                target->moveTab(from, to);
                i = to + 1;
            } else {
                target->insertDockWidget(i, d, false);
                i = target->indexOf(d) + 1;
            }
        }
    } else {
        area = container->createArea();
        container->insertArea(area, where, target);
        int i = 0;
        for (DockWidget *d : docks)
            area->insertDockWidget(i++, d, false);
    }
    for (DockWidget *d : docks) {
        if (!d->isClosed()) {
            area->setCurrentDockWidget(d);
            break;
        }
    }
    for (DockWidget *d : docks) {
        Q_EMIT d->reparented();
        if (wasFloating.value(d) != d->isFloating())
            Q_EMIT d->topLevelChanged(d->isFloating());
    }
    for (FloatingWindow *w : floatingWindows())
        w->containerChanged();
}

FloatingWindow *DockManager::createFloatingWindow()
{
    auto *window = new FloatingWindow(this);
    m_floating.append(window);
    Q_EMIT floatingWindowCreated(window);
    return window;
}

FloatingWindow *DockManager::floatDocks(const QList<DockWidget *> &docks, const QRect &geometry)
{
    if (docks.isEmpty() || testConfigFlag(NoTopLevelWindows))
        return nullptr;
    FloatingWindow *window = createFloatingWindow();
    const QRect g = geometry.isValid() ? geometry : defaultFloatingGeometry(docks);
    window->setGeometry(g);
    placeDocks(docks, DropArea::Left, nullptr, -1, window->dockContainer());
    window->updateTitle();
    if (!window->dockContainer()->openedDockWidgets().isEmpty()) {
        window->show();
        window->raise();
    }
    return window;
}

void DockManager::floatingWindowEmptied(FloatingWindow *window)
{
    if (!window || m_dying)
        return;
    if (!window->dockContainer()->dockWidgets().isEmpty())
        return;
    m_floating.removeAll(window);
    window->hide();
    window->deleteLater();
}

QRect DockManager::defaultFloatingGeometry(const QList<DockWidget *> &docks) const
{
    QSize size(400, 300);
    if (!docks.isEmpty() && docks.first()->isVisible()) {
        const QSize s = docks.first()->size();
        if (s.width() > 120 && s.height() > 80)
            size = s.boundedTo(QSize(900, 700));
    }
    size.rheight() += detail::metricsOf(this).floatingTitle;
    QPoint pos = QCursor::pos() - QPoint(40, 12);
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
        const QRect avail = screen->availableGeometry();
        pos.setX(std::clamp(pos.x(), avail.left(), std::max(avail.left(), avail.right() - size.width())));
        pos.setY(std::clamp(pos.y(), avail.top(), std::max(avail.top(), avail.bottom() - size.height())));
    }
    return QRect(pos, size);
}

void DockManager::dockClosedStateChanged(DockWidget *dock)
{
    if (dock->isClosed() && m_focused == dock)
        setFocusedDock(nullptr);
}

void DockManager::placeClosedDock(DockWidget *dock)
{
    if (dock->m_lastArea && dock->m_lastArea->dockManager() == this && dock->m_lastArea->dockContainer())
        placeDocks({dock}, DropArea::Center, dock->m_lastArea, dock->m_lastIndex, nullptr);
    else if (dock->m_central || dock->isDocument())
        addDockWidgetToCentral(dock);
    else
        placeDocks({dock}, DropArea::Right, nullptr, -1, m_container);
}

void DockManager::dockBack(DockWidget *dock)
{
    if (!dock)
        return;
    DockArea *area = dock->m_dockedArea;
    if (area && area->dockContainer() && !area->dockContainer()->isFloating() && area != dock->m_area)
        placeDocks({dock}, DropArea::Center, area, dock->m_dockedIndex, nullptr);
    else if (dock->isDocument())
        moveDockWidget(dock, DropArea::Center);
    else
        placeDocks({dock}, DropArea::Right, nullptr, -1, m_container);
}

// =============================================================================================
// 포커스

void DockManager::setFocusedDock(DockWidget *dock)
{
    if (m_focused == dock)
        return;
    DockWidget *old = m_focused;
    m_focused = dock;
    DockArea *oldArea = old ? old->dockArea() : nullptr;
    DockArea *newArea = dock ? dock->dockArea() : nullptr;
    if (oldArea && oldArea != newArea)
        oldArea->setActive(false);
    if (newArea) {
        newArea->setActive(true);
        if (newArea->isCentral() && newArea->dockContainer() == m_container)
            m_lastCentralArea = newArea;
    }
    for (detail::SideBar *bar : m_sideBars) {
        for (detail::SideTab *tab : bar->tabs()) {
            if (auto *flyout = tab->dock() ? qobject_cast<detail::AutoHideFlyout *>(tab->dock()->parentWidget()) : nullptr)
                flyout->setActive(tab->dock() == dock && flyout->isVisible());
        }
    }
    Q_EMIT focusedDockWidgetChanged(old, dock);
}

void DockManager::onFocusChanged(QWidget *, QWidget *now)
{
    if (m_restoring || m_dying || !now)
        return;
    DockWidget *dock = detail::dockOf(now);
    while (dock && dock->m_manager != this)
        dock = detail::dockOf(dock->parentWidget());
    if (DockWidget *expanded = expandedAutoHideDockWidget(); expanded && dock != expanded) {
        // 펼친 창 밖으로 포커스가 가면 접는다(같은 창 안일 때만 — 대화상자는 그대로)
        if (now->window() == window())
            collapseAutoHide();
    }
    if (dock && dock->hasFeature(DockWidget::Focusable))
        setFocusedDock(dock);
}

void DockManager::focusNextArea(bool forward)
{
    QList<DockArea *> areas;
    for (DockContainer *c : dockContainers())
        areas += c->openedDockAreas();
    if (areas.isEmpty())
        return;
    const int at = m_focused && m_focused->dockArea() ? int(areas.indexOf(m_focused->dockArea())) : -1;
    const int next = at < 0 ? 0 : (at + (forward ? 1 : int(areas.size()) - 1)) % int(areas.size());
    DockArea *area = areas[next];
    DockWidget *dock = area->currentDockWidget();
    if (!dock)
        return;
    if (QWidget *content = dock->widget(); content && content->focusPolicy() != Qt::NoFocus)
        content->setFocus(Qt::OtherFocusReason);
    else
        setFocusedDock(dock);
    if (FloatingWindow *w = dock->floatingWindow())
        w->activateWindow();
}

// =============================================================================================
// 상태

LayoutNode DockManager::nodeFor(QWidget *widget) const
{
    LayoutNode node;
    if (auto *area = qobject_cast<DockArea *>(widget)) {
        for (DockWidget *d : area->dockWidgets())
            node.docks << d->id();
        if (DockWidget *current = area->currentDockWidget())
            node.current = current->id();
        return node;
    }
    if (auto *s = qobject_cast<DockSplitter *>(widget)) {
        node.split = true;
        node.orientation = s->orientation();
        node.central = s->isCentral();
        const QList<int> sizes = s->sizes();
        for (int i = 0; i < s->count(); ++i) {
            QWidget *child = s->widget(i);
            if (isPlaceholder(child))
                continue;
            LayoutNode c = nodeFor(child);
            if (c.isEmpty() && !c.central)
                continue;
            node.children << c;
            node.sizes << (i < sizes.size() ? sizes[i] : 0);
        }
    }
    return node;
}

QByteArray DockManager::saveState(int userVersion) const
{
    detail::DockState st;
    st.userVersion = userVersion;
    if (m_container->rootSplitter()) {
        st.main = nodeFor(m_container->rootSplitter());
        st.hasMain = !st.main.isEmpty() || st.main.central;
    }
    for (FloatingWindow *w : floatingWindows()) {
        DockSplitter *root = w->dockContainer()->rootSplitter();
        if (!root)
            continue;
        detail::FloatingState f;
        QScreen *screen = w->screen() ? w->screen() : QGuiApplication::primaryScreen();
        const QRect r = w->isMaximized() ? w->normalGeometry() : w->geometry();
        f.screen = screen ? screen->name() : QString();
        f.rect = screen ? r.translated(-screen->geometry().topLeft()) : r;
        f.maximized = w->isMaximized();
        f.tree = nodeFor(root);
        if (!f.tree.isEmpty())
            st.floating << f;
    }
    for (int i = 0; i < 4; ++i) {
        for (detail::SideTab *tab : m_sideBars[i]->tabs()) {
            if (DockWidget *d = tab->dock())
                st.autoHide[i] << detail::AutoHideState{d->id(), d->m_autoHideSize};
        }
    }
    for (DockWidget *d : dockWidgets()) {
        if (d->isClosed() && (d->m_area || d->isAutoHidden()))
            st.closed << d->id();
    }
    st.locked = featureNames(m_locked);
    if (m_focused)
        st.focus = m_focused->id();
    return detail::encodeState(st);
}

bool DockManager::restoreState(const QByteArray &state, int userVersion)
{
    QString error;
    const std::optional<detail::DockState> st = detail::decodeState(state, userVersion, &error);
    if (!st) {
        if (!state.isEmpty())
            qInfo("fmdock: layout state rejected: %s", qPrintable(error));
        return false;
    }
    return applyState(*st);
}

QWidget *DockManager::buildNode(const LayoutNode &node, DockContainer *container, bool insideCentral,
                                QList<DockWidget *> &placed, QList<QPair<QWidget *, QList<int>>> &sizes)
{
    if (!node.split) {
        QList<DockWidget *> docks;
        for (const QString &id : node.docks) {
            DockWidget *d = findDockWidget(id);
            if (d && !placed.contains(d))
                docks << d;
        }
        if (docks.isEmpty() && !node.central)
            return nullptr;
        DockArea *area = nullptr;
        if (!docks.isEmpty()) {
            area = container->createArea();
            int i = 0;
            for (DockWidget *d : docks) {
                area->insertDockWidget(i++, d, false);
                placed << d;
            }
            if (DockWidget *current = findDockWidget(node.current); current && current->dockArea() == area)
                area->setCurrentDockWidget(current);
        }
        if (node.central && !insideCentral && !container->m_central) {
            auto *central = new DockSplitter(Qt::Horizontal, this);
            central->setCentral(true);
            container->m_placeholder = new detail::CentralPlaceholder(this, central);
            central->addWidget(container->m_placeholder);
            if (area)
                central->addWidget(area);
            container->m_central = central;
            return central;
        }
        return area;
    }
    auto *s = new DockSplitter(node.orientation, this);
    const bool makeCentral = node.central && !insideCentral && !container->m_central;
    QList<int> childSizes;
    if (makeCentral) {
        s->setCentral(true);
        container->m_placeholder = new detail::CentralPlaceholder(this, s);
        s->addWidget(container->m_placeholder);
        container->m_central = s;
        childSizes << 0;
    }
    for (int i = 0; i < node.children.size(); ++i) {
        QWidget *w = buildNode(node.children[i], container, insideCentral || makeCentral, placed, sizes);
        if (!w)
            continue;
        s->addWidget(w);
        childSizes << (i < node.sizes.size() ? std::max(1, node.sizes[i]) : 100);
    }
    if (s->count() == 0) {
        delete s;
        return nullptr;
    }
    sizes << qMakePair(static_cast<QWidget *>(s), childSizes);
    return s;
}

bool DockManager::applyState(const detail::DockState &st)
{
    m_restoring = true;
    Q_EMIT restoringState();
    setUpdatesEnabled(false);
    collapseAutoHide();
    if (m_drag)
        m_drag->cancel();

    // 1) 모든 도크를 떼어 관리자 아래에 숨겨 둔다
    const QList<DockWidget *> all = dockWidgets();
    QSet<DockWidget *> wasPlaced;
    for (DockWidget *d : all) {
        if (d->m_area || d->isAutoHidden())
            wasPlaced.insert(d);
    }
    for (DockWidget *d : all) {
        detachDock(d);
        d->hide();
        d->setParent(this);
    }

    // 2) 컨테이너를 비운다
    m_container->clearLayout();
    for (FloatingWindow *w : floatingWindows()) {
        w->hide();
        w->deleteLater();
    }
    m_floating.clear();
    m_lastCentralArea = nullptr;

    // 3) 메인
    QList<DockWidget *> placed;
    QList<QPair<QWidget *, QList<int>>> sizes;
    if (st.hasMain) {
        QWidget *root = buildNode(st.main, m_container, false, placed, sizes);
        if (auto *split = qobject_cast<DockSplitter *>(root)) {
            m_container->setRoot(split);
        } else if (auto *a = qobject_cast<DockArea *>(root)) {
            auto *wrap = new DockSplitter(Qt::Horizontal, this);
            wrap->addWidget(a);
            m_container->setRoot(wrap);
        }
    }
    if (m_centralWidget && !placed.contains(m_centralWidget)) {
        m_container->ensureCentralRegion();
        placeDocks({m_centralWidget}, DropArea::Center, nullptr, -1, m_container);
        placed << m_centralWidget;
    }

    // 4) 떠 있는 창
    QList<QPair<FloatingWindow *, bool>> windows;
    if (!testConfigFlag(NoTopLevelWindows)) {
        for (const detail::FloatingState &f : st.floating) {
            FloatingWindow *w = createFloatingWindow();
            QWidget *root = buildNode(f.tree, w->dockContainer(), false, placed, sizes);
            if (!root) {
                m_floating.removeAll(w);
                w->deleteLater();
                continue;
            }
            auto *s = qobject_cast<DockSplitter *>(root);
            if (!s) {
                s = new DockSplitter(Qt::Horizontal, this);
                s->addWidget(root);
            }
            w->dockContainer()->setRoot(s);
            QScreen *screen = nullptr;
            for (QScreen *candidate : QGuiApplication::screens()) {
                if (candidate->name() == f.screen)
                    screen = candidate;
            }
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            QRect r = screen ? f.rect.translated(screen->geometry().topLeft()) : f.rect;
            if (screen) {
                // 화면과 30 % 미만으로 겹치면 주 화면 안으로
                const QRect avail = screen->availableGeometry();
                const QRect overlap = r & avail;
                if (qint64(overlap.width()) * overlap.height() < qint64(r.width()) * r.height() * 3 / 10)
                    r.moveTopLeft(avail.topLeft() + QPoint(40, 40));
                r.setSize(r.size().boundedTo(avail.size()));
            }
            w->setGeometry(r);
            windows << qMakePair(w, f.maximized);
        }
    }

    // 5) 자동 숨김
    for (int i = 0; i < 4; ++i) {
        for (const detail::AutoHideState &a : st.autoHide[i]) {
            DockWidget *d = findDockWidget(a.dock);
            if (!d || placed.contains(d))
                continue;
            d->m_autoHideSize = a.size;
            pinDock(d, detail::sideAt(i), -1);
            placed << d;
        }
    }

    // 6) 상태에 없는 도크
    QList<DockWidget *> unplaced;
    for (DockWidget *d : all) {
        if (!placed.contains(d) && wasPlaced.contains(d))
            unplaced << d;
    }
    placeUnplacedDocks(unplaced);

    // 7) 닫힘
    for (DockWidget *d : all) {
        if (!placed.contains(d))
            continue;
        const bool shouldClose = st.closed.contains(d->id());
        if (shouldClose != d->isClosed())
            d->toggleView(!shouldClose);
    }

    // 8) 크기
    for (const auto &[widget, list] : std::as_const(sizes)) {
        if (auto *s = qobject_cast<QSplitter *>(widget); s && list.size() == s->count())
            s->setSizes(list);
    }
    for (DockContainer *c : dockContainers()) {
        c->updateStretch();
        c->updateVisibility();
        for (DockArea *a : c->dockAreas())
            a->updateTitleBar();
    }
    for (const auto &[w, maximized] : std::as_const(windows)) {
        w->updateTitle();
        if (w->dockContainer()->openedDockWidgets().isEmpty())
            continue;
        if (maximized)
            w->showMaximized();
        else
            w->show();
    }

    // 9) 잠금 · 포커스
    lockFeatures(featuresFromNames(st.locked));
    setUpdatesEnabled(true);
    m_restoring = false;
    if (DockWidget *focus = findDockWidget(st.focus); focus && !focus->isClosed())
        setFocusedDock(focus);
    updateSideBars();
    Q_EMIT stateRestored();
    return true;
}

void DockManager::placeUnplacedDocks(const QList<DockWidget *> &unplaced)
{
    std::optional<detail::DockState> defaults;
    if (!m_defaultLayout.isEmpty())
        defaults = detail::decodeState(m_defaultLayout, detail::stateUserVersion(m_defaultLayout));
    for (DockWidget *d : unplaced) {
        bool done = false;
        if (defaults) {
            // 기본 배치에서 같은 영역이던 도크 곁으로
            if (const LayoutNode *node = detail::findAreaNode(*defaults, d->id())) {
                for (const QString &sibling : node->docks) {
                    DockWidget *s = sibling == d->id() ? nullptr : findDockWidget(sibling);
                    if (s && s->m_area) {
                        placeDocks({d}, DropArea::Center, s->m_area, -1, nullptr);
                        done = true;
                        break;
                    }
                }
            }
        }
        if (done)
            continue;
        if (d->m_central || d->isDocument())
            addDockWidgetToCentral(d);
        else
            placeDocks({d}, DropArea::Right, nullptr, -1, m_container);
    }
}

bool DockManager::resetToDefaultLayout()
{
    if (m_defaultLayout.isEmpty())
        return false;
    return restoreState(m_defaultLayout, detail::stateUserVersion(m_defaultLayout));
}

void DockManager::addPerspective(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return;
    m_perspectives.insert(key, saveState());
    Q_EMIT perspectivesChanged();
}

bool DockManager::openPerspective(const QString &name)
{
    const auto it = m_perspectives.constFind(name);
    return it != m_perspectives.cend() && restoreState(*it, detail::stateUserVersion(*it));
}

void DockManager::removePerspective(const QString &name)
{
    if (m_perspectives.remove(name) > 0)
        Q_EMIT perspectivesChanged();
}

void DockManager::setPerspectives(const QMap<QString, QByteArray> &perspectives)
{
    if (m_perspectives == perspectives)
        return;
    m_perspectives = perspectives;
    Q_EMIT perspectivesChanged();
}

void DockManager::lockFeatures(DockWidget::Features locked)
{
    locked &= DockWidget::LockableFeatures;
    if (m_locked == locked)
        return;
    m_locked = locked;
    refreshAll();
    Q_EMIT lockedFeaturesChanged();
}

// =============================================================================================
// 보기 메뉴 · 단축키

void DockManager::populateViewMenu(QMenu *menu)
{
    if (!menu)
        return;
    const QPointer<QMenu> guard(menu);
    const auto fill = [this, guard] {
        if (!guard)
            return;
        QMenu *m = guard;
        const auto generated = m->findChildren<QMenu *>(u"fmDockGeneratedMenu"_s, Qt::FindDirectChildrenOnly);
        m->clear();
        for (QMenu *sub : generated)
            sub->deleteLater();
        for (DockWidget *d : dockWidgets()) {
            if (!d->m_central)
                m->addAction(d->toggleViewAction());
        }
        m->addSeparator();
        auto *layouts = new fm::ui::Menu(tr("배치(&L)"), m);
        layouts->setObjectName(u"fmDockGeneratedMenu"_s);
        m->addMenu(layouts);
        layouts->addAction(tr("지금 배치 저장(&S)…"), this, [this, m] {
            bool ok = false;
            const QString name = QInputDialog::getText(m->window(), tr("배치 저장"), tr("배치 이름"), QLineEdit::Normal,
                                                       QString(), &ok);
            if (ok)
                addPerspective(name);
        });
        const QStringList names = perspectiveNames();
        if (!names.isEmpty()) {
            layouts->addSeparator();
            for (const QString &name : names)
                layouts->addAction(name, this, [this, name] { openPerspective(name); });
            layouts->addSeparator();
            QMenu *remove = layouts->addMenu(tr("배치 삭제(&D)"));
            for (const QString &name : names)
                remove->addAction(name, this, [this, name] { removePerspective(name); });
        }
        QAction *lock = m->addAction(tr("도크 잠금(&K)"));
        lock->setCheckable(true);
        lock->setChecked(m_locked != DockWidget::NoFeatures);
        connect(lock, &QAction::toggled, this, [this](bool on) {
            lockFeatures(on ? DockWidget::Features(DockWidget::Movable | DockWidget::Floatable | DockWidget::Pinnable)
                            : DockWidget::Features(DockWidget::NoFeatures));
        });
        QAction *reset = m->addAction(tr("기본 배치로 되돌리기(&R)"));
        reset->setEnabled(!m_defaultLayout.isEmpty());
        connect(reset, &QAction::triggered, this, [this] { resetToDefaultLayout(); });
    };
    fill();
    connect(menu, &QMenu::aboutToShow, this, fill);
}

void DockManager::setShortcutSource(QWidget *window)
{
    m_shortcutSource = window;
    for (FloatingWindow *w : floatingWindows())
        shareShortcuts(w);
}

void DockManager::shareShortcuts(QWidget *window)
{
    if (!m_shortcutSource || !window)
        return;
    const auto actions = m_shortcutSource->findChildren<QAction *>();
    const QList<QAction *> existing = window->actions();
    for (QAction *a : actions) {
        if (a->shortcut().isEmpty() || a->shortcutContext() != Qt::WindowShortcut || existing.contains(a))
            continue;
        window->addAction(a);
    }
}

// =============================================================================================
// 이벤트

bool DockManager::eventFilter(QObject *watched, QEvent *event)
{
    DockWidget *expanded = expandedAutoHideDockWidget();
    if (!expanded)
        return QWidget::eventFilter(watched, event);
    auto *flyout = qobject_cast<detail::AutoHideFlyout *>(expanded->parentWidget());
    detail::SideBar *bar = sideBar(expanded->m_autoHideSide);
    detail::SideTab *tab = bar ? bar->tabFor(expanded) : nullptr;
    auto *w = qobject_cast<QWidget *>(watched);
    if (event->type() == QEvent::MouseButtonPress && w && testAutoHideFlag(CloseOnOutsideClick)
        && !QApplication::activePopupWidget()) {
        const bool inside = (flyout && (w == flyout || flyout->isAncestorOf(w)))
                         || (tab && (w == tab || tab->isAncestorOf(w)));
        if (!inside && w->window() == window())
            collapseAutoHide();
    } else if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape && w
               && flyout && flyout->isAncestorOf(w)) {
        collapseAutoHide();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void DockManager::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (DockWidget *expanded = expandedAutoHideDockWidget()) {
        if (auto *flyout = qobject_cast<detail::AutoHideFlyout *>(expanded->parentWidget()))
            positionFlyout(flyout);
    }
}

void DockManager::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        if (m_drag)
            m_drag->cancel();
        refreshAll();
    }
}

} // namespace fm::dock
