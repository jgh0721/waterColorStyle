#include "DockManager_p.h"

#include <fmstyle/StyleProps.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QDataStream>
#include <QDockWidget>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QTabBar>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

namespace {

constexpr quint32 kStateMagic = 0x464D444B;  // "FMDK"
constexpr quint16 kStateVersion = 1;
constexpr int kWindowStateVersion = 1;

Qt::Orientation orientationFor(Qt::DockWidgetArea area)
{
    return (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea) ? Qt::Horizontal : Qt::Vertical;
}

/// 사이드바 쪽 — 위쪽 도크 영역은 사이드바를 두지 않는다(메인 창 위쪽은 메뉴 · 도구 모음 자리) → 왼쪽으로.
Qt::DockWidgetArea sideFor(Qt::DockWidgetArea area)
{
    switch (area) {
    case Qt::RightDockWidgetArea:
    case Qt::BottomDockWidgetArea:
        return area;
    default:
        return Qt::LeftDockWidgetArea;
    }
}

QDockWidget *dockForTab(const QMainWindow *window, const QTabBar *bar, int index)
{
    const auto id = qvariant_cast<quintptr>(bar->tabData(index));
    const auto docks = window->findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly);
    for (QDockWidget *dock : docks) {
        if (reinterpret_cast<quintptr>(dock) == id)
            return dock;
    }
    return nullptr;
}

} // namespace

// =============================================================================================
// DockManagerPrivate

DockManagerPrivate::DockManagerPrivate(DockManager *manager, QMainWindow *mainWindow)
    : q(manager)
    , window(mainWindow)
{
}

bool DockManagerPrivate::isManaged(const QDockWidget *dock) const
{
    return dock && std::any_of(docks.cbegin(), docks.cend(), [dock](const QPointer<QDockWidget> &d) { return d == dock; });
}

QDockWidget *DockManagerPrivate::dockOf(QWidget *widget) const
{
    for (QWidget *p = widget; p; p = p->parentWidget()) {
        if (auto *dock = qobject_cast<QDockWidget *>(p); dock && isManaged(dock))
            return dock;
    }
    return nullptr;
}

void DockManagerPrivate::setActive(QDockWidget *dock)
{
    if (active == dock)
        return;
    active = dock;
    for (const QPointer<QDockWidget> &d : std::as_const(docks)) {
        if (!d)
            continue;
        const bool on = d == dock;
        if (d->property(fs::props::kPaneActive).toBool() == on)
            continue;
        fs::setPaneActive(d, on);
        refreshDock(d);
    }
    for (const AutoHideEntry &entry : std::as_const(autoHidden)) {
        if (entry.tab) {
            fs::setPaneActive(entry.tab, entry.dock == dock);
            entry.tab->update();
        }
    }
    queueTabRefresh();
    Q_EMIT q->activeDockChanged(dock);
}

void DockManagerPrivate::refreshDock(QDockWidget *dock)
{
    if (!dock)
        return;
    if (auto *title = qobject_cast<DockTitleBar *>(dock->titleBarWidget())) {
        title->updateButtons();
        title->update();
    }
    dock->update();
    // 도크 안 목록의 선택 색(활성 · 비활성)도 다시 그린다
    const auto views = dock->findChildren<QAbstractItemView *>();
    for (QAbstractItemView *view : views)
        view->viewport()->update();
}

void DockManagerPrivate::queueTabRefresh()
{
    if (tabRefreshQueued)
        return;
    tabRefreshQueued = true;
    QTimer::singleShot(0, q, [this] {
        tabRefreshQueued = false;
        refreshTabBars();
    });
}

void DockManagerPrivate::refreshTabBars()
{
    if (!window)
        return;
    tabBars.removeAll(nullptr);
    const auto bars = window->findChildren<QTabBar *>(Qt::FindDirectChildrenOnly);
    for (QTabBar *bar : bars) {
        if (!bar->inherits("QMainWindowTabBar"))
            continue;
        if (!tabBars.contains(bar)) {
            // KDDockWidgets의 탭 닫기 단추 — Qt 도크 탭 줄에 닫기 단추를 붙이고 누르면 그 도크를 닫는다
            tabBars.append(bar);
            bar->setTabsClosable(true);
            QObject::connect(bar, &QTabBar::tabCloseRequested, q, [this, guard = QPointer<QTabBar>(bar)](int index) {
                if (!guard || !window)
                    return;
                QDockWidget *dock = dockForTab(window, guard, index);
                if (dock && (dock->features() & QDockWidget::DockWidgetClosable))
                    dock->close();
            });
            QObject::connect(bar, &QTabBar::currentChanged, q, [this] { queueTabRefresh(); });
        }
        // 활성 도크가 이 탭 줄의 선택 탭이면 강조(패널 탭 줄과 같은 규칙)
        const int current = bar->currentIndex();
        const bool on = active && current >= 0 && dockForTab(window, bar, current) == active;
        if (bar->property(fs::props::kPaneActive).toBool() != on || !bar->property(fs::props::kPaneActive).isValid())
            fs::setPaneActive(bar, on);
        // 닫을 수 없는 도크의 탭은 닫기 단추를 숨긴다
        const auto position = QTabBar::ButtonPosition(
            bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
        for (int i = 0; i < bar->count(); ++i) {
            QDockWidget *dock = dockForTab(window, bar, i);
            if (QWidget *button = bar->tabButton(i, position))
                button->setVisible(!dock || (dock->features() & QDockWidget::DockWidgetClosable));
        }
    }
}

SideBar *DockManagerPrivate::sideBar(Qt::DockWidgetArea side)
{
    QPointer<SideBar> &bar = sideBars[int(side)];
    if (!bar) {
        bar = new SideBar(side, window);
        const Qt::ToolBarArea area = side == Qt::RightDockWidgetArea  ? Qt::RightToolBarArea
                                   : side == Qt::BottomDockWidgetArea ? Qt::BottomToolBarArea
                                                                      : Qt::LeftToolBarArea;
        window->addToolBar(area, bar);
        bar->hide();
    }
    return bar;
}

QRect DockManagerPrivate::contentRect() const
{
    if (!window)
        return {};
    // 탭으로 묶인 도크 중 현재 탭이 아닌 것은 보이는 채로 창 밖 좌표에 남는다 — 창과 겹치는 자식만 센다.
    const QRect bounds = window->rect();
    QRect r;
    if (QWidget *central = window->centralWidget(); central && central->isVisible())
        r = central->geometry() & bounds;
    const auto children = window->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        if (!child->isVisible() || !child->geometry().intersects(bounds))
            continue;
        const auto *dock = qobject_cast<QDockWidget *>(child);
        if ((dock && !dock->isFloating()) || child->inherits("QMainWindowTabBar"))
            r |= child->geometry() & bounds;
    }
    return r.isEmpty() ? bounds : r;
}

void DockManagerPrivate::positionOverlay(const AutoHideEntry &entry) const
{
    if (!entry.overlay)
        return;
    const QRect content = contentRect();
    const bool across = entry.side != Qt::BottomDockWidgetArea;  // 좌우 사이드바는 폭, 아래는 높이
    // 내용의 최소 크기보다 좁히면 도크가 창 밖으로 넘쳐 제목 줄 단추가 잘린다
    const QSize min = entry.overlay->minimumSizeHint();
    const int floor = std::max(120, across ? min.width() : min.height());
    const int limit = int((across ? content.width() : content.height()) * 0.8);
    const int size = std::clamp(entry.size, floor, std::max(floor, limit));
    QRect r;
    switch (entry.side) {
    case Qt::RightDockWidgetArea:
        r = QRect(content.right() - size + 1, content.top(), size, content.height());
        break;
    case Qt::BottomDockWidgetArea:
        r = QRect(content.left(), content.bottom() - size + 1, content.width(), size);
        break;
    default:
        r = QRect(content.left(), content.top(), size, content.height());
        break;
    }
    entry.overlay->setGeometry(r);
}

void DockManagerPrivate::collapse()
{
    if (!expanded)
        return;
    // 숨기면 포커스가 옮겨 가며 다른 처리가 끼어든다 — 반복자를 붙잡지 않고 필요한 것만 먼저 꺼낸다
    const AutoHideEntry entry = autoHidden.value(expanded.data());
    expanded = nullptr;
    qApp->removeEventFilter(q);
    if (entry.overlay)
        entry.overlay->hide();
    if (entry.tab)
        entry.tab->setChecked(false);
    // 접어도 도크는 열려 있다 — Qt가 숨김 이벤트로 푼 보기 메뉴 체크를 되돌린다
    if (entry.dock)
        entry.dock->toggleViewAction()->setChecked(true);
}

QDockWidget *DockManagerPrivate::dockAt(const QPoint &point, const QDockWidget *dragged) const
{
    if (!window)
        return nullptr;
    const auto children = window->findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly);
    for (QDockWidget *dock : children) {
        if (dock == dragged || !dock->isVisible() || dock->isFloating())
            continue;
        if (dock->geometry().contains(point))
            return dock;
    }
    return nullptr;
}

QRect DockManagerPrivate::previewRect(const QDockWidget *dragged, const DropTarget &target) const
{
    using fs::DropSpot;
    const QRect content = contentRect();
    const int across = std::clamp(dragged ? dragged->width() : 200, 120, std::max(120, content.width() / 3));
    const int down = std::clamp(dragged ? dragged->height() : 160, 100, std::max(100, content.height() / 3));
    switch (target.spot) {
    case DropSpot::OuterLeft: return QRect(content.left(), content.top(), across, content.height());
    case DropSpot::OuterRight: return QRect(content.right() - across + 1, content.top(), across, content.height());
    case DropSpot::OuterTop: return QRect(content.left(), content.top(), content.width(), down);
    case DropSpot::OuterBottom: return QRect(content.left(), content.bottom() - down + 1, content.width(), down);
    default: break;
    }
    if (!target.dock)
        return {};
    const QRect r = target.dock->geometry();
    switch (target.spot) {
    case DropSpot::Left: return QRect(r.left(), r.top(), r.width() / 2, r.height());
    case DropSpot::Right: return QRect(r.left() + r.width() / 2, r.top(), r.width() - r.width() / 2, r.height());
    case DropSpot::Top: return QRect(r.left(), r.top(), r.width(), r.height() / 2);
    case DropSpot::Bottom: return QRect(r.left(), r.top() + r.height() / 2, r.width(), r.height() - r.height() / 2);
    default: return r;
    }
}

bool DockManagerPrivate::applyDrop(QDockWidget *dock, const DropTarget &target)
{
    using fs::DropSpot;
    if (!dock || !window)
        return false;
    if (autoHidden.contains(dock))
        q->setAutoHidden(dock, false);

    Qt::DockWidgetArea edge = Qt::NoDockWidgetArea;
    switch (target.spot) {
    case DropSpot::OuterLeft: edge = Qt::LeftDockWidgetArea; break;
    case DropSpot::OuterRight: edge = Qt::RightDockWidgetArea; break;
    case DropSpot::OuterTop: edge = Qt::TopDockWidgetArea; break;
    case DropSpot::OuterBottom: edge = Qt::BottomDockWidgetArea; break;
    default: break;
    }
    // Qt 도크 배치에 이미 있는 도크를 다시 넣으면 QLayout이 경고를 내며 빼므로, 옮기기 전에 먼저 뺀다.
    if (edge != Qt::NoDockWidgetArea) {
        if (!dock->isAreaAllowed(edge))
            return false;
        window->removeDockWidget(dock);
        window->addDockWidget(edge, dock);
    } else {
        QDockWidget *t = target.dock;
        if (!t || t == dock || t->isFloating() || window->dockWidgetArea(t) == Qt::NoDockWidgetArea)
            return false;
        const Qt::DockWidgetArea area = window->dockWidgetArea(t);
        if (!dock->isAreaAllowed(area))
            return false;
        window->removeDockWidget(dock);
        // Qt는 탭 묶음을 나눌 수 없다 — 탭 묶음 옆에 놓으면 탭으로 넣는다(QMainWindow::splitDockWidget 주의 사항)
        if (target.spot == DropSpot::Center || !window->tabifiedDockWidgets(t).isEmpty()) {
            window->tabifyDockWidget(t, dock);
        } else {
            const Qt::Orientation o = (target.spot == DropSpot::Left || target.spot == DropSpot::Right) ? Qt::Horizontal
                                                                                                         : Qt::Vertical;
            window->splitDockWidget(t, dock, o);  // dock이 t 오른쪽 · 아래
            if (target.spot == DropSpot::Left || target.spot == DropSpot::Top) {
                // 앞쪽: t를 dock 뒤로 옮긴다
                window->removeDockWidget(t);
                window->splitDockWidget(dock, t, o);
                t->show();
            }
        }
    }
    dock->show();
    dock->raise();
    queueTabRefresh();
    return true;
}

// =============================================================================================
// DockManager

DockManager::DockManager(QMainWindow *window)
    : QObject(window)
    , d(std::make_unique<DockManagerPrivate>(this, window))
{
    window->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!d->window || (now && now->window() != d->window->window() && !d->dockOf(now)))
            return;  // 다른 창(대화상자 등)으로 포커스가 가면 활성 도크를 그대로 둔다
        d->setActive(now ? d->dockOf(now) : nullptr);
    });
}

DockManager::~DockManager()
{
    qApp->removeEventFilter(this);
}

QMainWindow *DockManager::window() const
{
    return d->window;
}

QDockWidget *DockManager::addDock(const QString &id, const QString &title, QWidget *content, Qt::DockWidgetArea area)
{
    auto *dock = new QDockWidget(title, d->window);
    dock->setObjectName(id);
    dock->setWidget(content);
    d->window->addDockWidget(area, dock);
    manage(dock);
    return dock;
}

void DockManager::manage(QDockWidget *dock)
{
    if (!dock || dock->objectName().isEmpty() || d->isManaged(dock))
        return;
    d->docks.append(dock);
    fs::setPaneActive(dock, false);  // 관리 도크는 포커스로 짐작하지 않고 관리자가 정한다
    dock->setTitleBarWidget(new DockTitleBar(dock, d.get()));
    connect(dock, &QDockWidget::topLevelChanged, this, [this, dock] {
        d->refreshDock(dock);
        d->queueTabRefresh();
    });
    connect(dock, &QDockWidget::featuresChanged, this, [this, dock] {
        d->refreshDock(dock);
        d->queueTabRefresh();
    });
    connect(dock, &QDockWidget::windowTitleChanged, this, [this, dock] {
        if (const auto it = d->autoHidden.constFind(dock); it != d->autoHidden.cend() && it->tab)
            it->tab->updateGeometry();
    });
    // 보기 메뉴(사용자가 누른 것만 — Qt는 부모가 숨겨져도 체크를 푼다): 자동 숨김 도크를 끄면 원래 도크 영역으로
    // 되돌려 닫고(다시 켜면 그 자리에 붙는다), 켜면 펼친다.
    connect(dock->toggleViewAction(), &QAction::triggered, this, [this, dock](bool on) {
        if (!d->autoHidden.contains(dock))
            return;
        if (on) {
            showAutoHidden(dock);
        } else {
            setAutoHidden(dock, false);
            dock->hide();
        }
    });
    connect(dock, &QObject::destroyed, this, [this, dock] {
        d->docks.removeAll(nullptr);
        d->autoHidden.remove(dock);
    });
    d->queueTabRefresh();
}

QList<QDockWidget *> DockManager::docks() const
{
    QList<QDockWidget *> out;
    for (const QPointer<QDockWidget> &dock : d->docks) {
        if (dock)
            out.append(dock);
    }
    return out;
}

QDockWidget *DockManager::dock(const QString &id) const
{
    for (const QPointer<QDockWidget> &dock : d->docks) {
        if (dock && dock->objectName() == id)
            return dock;
    }
    return nullptr;
}

QDockWidget *DockManager::activeDock() const
{
    return d->active;
}

bool DockManager::isAutoHidden(const QDockWidget *dock) const
{
    return d->autoHidden.contains(dock);
}

void DockManager::setAutoHidden(QDockWidget *dock, bool on, Qt::DockWidgetArea side)
{
    if (!dock || !d->isManaged(dock) || on == d->autoHidden.contains(dock))
        return;
    QMainWindow *window = d->window;
    if (on) {
        const Qt::DockWidgetArea current = window->dockWidgetArea(dock);
        AutoHideEntry entry;
        entry.dock = dock;
        entry.area = current == Qt::NoDockWidgetArea ? Qt::LeftDockWidgetArea : current;
        entry.side = side == Qt::NoDockWidgetArea ? sideFor(entry.area) : sideFor(side);
        entry.size = entry.side == Qt::BottomDockWidgetArea ? std::max(dock->height(), 120) : std::max(dock->width(), 160);
        if (dock->isFloating())
            dock->setFloating(false);
        window->removeDockWidget(dock);
        entry.overlay = new AutoHideOverlay(d.get(), dock, window);
        entry.overlay->hide();
        dock->show();  // 펼친 창(숨김) 안에서 보이는 상태로 둔다
        dock->toggleViewAction()->setChecked(true);  // 사이드바에 있는 동안 보기 메뉴는 '열림'
        SideBar *bar = d->sideBar(entry.side);
        entry.tab = bar->addTab(dock);
        bar->show();
        connect(entry.tab, &QAbstractButton::clicked, this, [this, dock] {
            showAutoHidden(d->expanded == dock ? nullptr : dock);
        });
        d->autoHidden.insert(dock, entry);
    } else {
        if (d->expanded == dock)
            d->collapse();
        AutoHideEntry entry = d->autoHidden.take(dock);
        if (entry.tab) {
            if (SideBar *bar = d->sideBars.value(int(entry.side))) {
                bar->removeTab(entry.tab);
                if (bar->count() == 0)
                    bar->hide();
            }
        }
        dock->hide();
        dock->setParent(window);
        window->addDockWidget(entry.area, dock);
        dock->show();
        window->resizeDocks({dock}, {entry.size}, orientationFor(entry.area));
        if (entry.overlay)
            entry.overlay->deleteLater();
    }
    d->refreshDock(dock);
    Q_EMIT autoHideChanged(dock, on);
}

void DockManager::showAutoHidden(QDockWidget *dock)
{
    d->collapse();
    if (!dock)
        return;
    const AutoHideEntry entry = d->autoHidden.value(dock);
    if (!entry.overlay)
        return;
    d->expanded = dock;
    d->positionOverlay(entry);
    entry.overlay->show();
    entry.overlay->raise();
    if (entry.tab)
        entry.tab->setChecked(true);
    // 바깥을 누르면 접는다(KDDockWidgets의 사이드바 동작)
    qApp->installEventFilter(this);
    if (QWidget *content = dock->widget())
        content->setFocus(Qt::OtherFocusReason);
    else
        dock->setFocus(Qt::OtherFocusReason);
}

QDockWidget *DockManager::expandedDock() const
{
    return d->expanded;
}

bool DockManager::dropOnto(QDockWidget *dock, QDockWidget *target, DropSide side)
{
    using fs::DropSpot;
    const DropSpot spot = side == DropSide::Left    ? DropSpot::Left
                        : side == DropSide::Top     ? DropSpot::Top
                        : side == DropSide::Right   ? DropSpot::Right
                        : side == DropSide::Bottom  ? DropSpot::Bottom
                                                    : DropSpot::Center;
    return d->applyDrop(dock, DropTarget{spot, target});
}

bool DockManager::dropToEdge(QDockWidget *dock, Qt::DockWidgetArea area)
{
    using fs::DropSpot;
    DropSpot spot;
    switch (area) {
    case Qt::LeftDockWidgetArea: spot = DropSpot::OuterLeft; break;
    case Qt::RightDockWidgetArea: spot = DropSpot::OuterRight; break;
    case Qt::TopDockWidgetArea: spot = DropSpot::OuterTop; break;
    case Qt::BottomDockWidgetArea: spot = DropSpot::OuterBottom; break;
    default: return false;
    }
    return d->applyDrop(dock, DropTarget{spot, nullptr});
}

QByteArray DockManager::saveState() const
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setVersion(QDataStream::Qt_6_0);
    s << kStateMagic << kStateVersion;
    // 자동 숨김 도크는 메인 창 배치에서 빠져 있으므로 따로 적는다.
    s << d->window->saveState(kWindowStateVersion);
    s << qint32(d->autoHidden.size());
    for (const AutoHideEntry &entry : std::as_const(d->autoHidden)) {
        s << (entry.dock ? entry.dock->objectName() : QString()) << qint32(entry.side) << qint32(entry.area)
          << qint32(entry.size);
    }
    return out;
}

bool DockManager::restoreState(const QByteArray &state)
{
    QDataStream s(state);
    s.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    QByteArray windowState;
    qint32 count = 0;
    s >> magic >> version >> windowState >> count;
    if (s.status() != QDataStream::Ok || magic != kStateMagic || version != kStateVersion || count < 0)
        return false;
    struct Saved { QString id; Qt::DockWidgetArea side; Qt::DockWidgetArea area; int size; };
    QList<Saved> saved;
    for (qint32 i = 0; i < count; ++i) {
        QString id;
        qint32 side = 0, area = 0, size = 0;
        s >> id >> side >> area >> size;
        saved.append({id, Qt::DockWidgetArea(side), Qt::DockWidgetArea(area), int(size)});
    }
    if (s.status() != QDataStream::Ok)
        return false;

    // 지금 자동 숨김을 모두 풀어 메인 창 배치에 넣어야 restoreState가 자리를 잡는다.
    const QList<const QDockWidget *> hidden = d->autoHidden.keys();
    for (const QDockWidget *dock : hidden)
        setAutoHidden(const_cast<QDockWidget *>(dock), false);
    const bool ok = d->window->restoreState(windowState, kWindowStateVersion);
    for (const Saved &entry : std::as_const(saved)) {
        QDockWidget *target = dock(entry.id);
        if (!target)
            continue;
        if (!target->isVisible())
            target->show();
        setAutoHidden(target, true, entry.side);
        if (auto it = d->autoHidden.find(target); it != d->autoHidden.end()) {
            it->area = entry.area;
            it->size = entry.size;
        }
    }
    // 메인 창 상태가 사이드바(도구 모음)를 보이게 되돌렸어도 탭이 없으면 숨긴다
    for (const QPointer<SideBar> &bar : std::as_const(d->sideBars)) {
        if (bar)
            bar->setVisible(bar->count() > 0);
    }
    for (const QPointer<QDockWidget> &each : std::as_const(d->docks))
        d->refreshDock(each);
    d->queueTabRefresh();
    return ok;
}

QStringList DockManager::layoutNames() const
{
    return d->layouts.keys();
}

void DockManager::saveLayout(const QString &name)
{
    if (name.trimmed().isEmpty())
        return;
    d->layouts.insert(name.trimmed(), saveState());
    Q_EMIT layoutsChanged();
}

bool DockManager::applyLayout(const QString &name)
{
    const auto it = d->layouts.constFind(name);
    return it != d->layouts.cend() && restoreState(*it);
}

void DockManager::removeLayout(const QString &name)
{
    if (d->layouts.remove(name) > 0)
        Q_EMIT layoutsChanged();
}

QMap<QString, QByteArray> DockManager::layouts() const
{
    return d->layouts;
}

void DockManager::setLayouts(const QMap<QString, QByteArray> &layouts)
{
    if (d->layouts == layouts)
        return;
    d->layouts = layouts;
    Q_EMIT layoutsChanged();
}

void DockManager::populateMenu(QMenu *menu)
{
    for (QDockWidget *each : docks())
        menu->addAction(each->toggleViewAction());
    menu->addSeparator();
    QMenu *layoutsMenu = menu->addMenu(tr("배치(&L)"));
    // 열 때마다 저장한 배치로 다시 채운다
    connect(layoutsMenu, &QMenu::aboutToShow, this, [this, layoutsMenu] {
        layoutsMenu->clear();
        layoutsMenu->addAction(tr("지금 배치 저장(&S)…"), this, [this, layoutsMenu] {
            bool ok = false;
            const QString name = QInputDialog::getText(layoutsMenu->window(), tr("배치 저장"), tr("배치 이름"),
                                                       QLineEdit::Normal, QString(), &ok);
            if (ok)
                saveLayout(name);
        });
        const QStringList names = layoutNames();
        if (names.isEmpty())
            return;
        layoutsMenu->addSeparator();
        for (const QString &name : names)
            layoutsMenu->addAction(name, this, [this, name] { applyLayout(name); });
        layoutsMenu->addSeparator();
        QMenu *removeMenu = layoutsMenu->addMenu(tr("배치 삭제(&D)"));
        for (const QString &name : names)
            removeMenu->addAction(name, this, [this, name] { removeLayout(name); });
    });
}

bool DockManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->window) {
        switch (event->type()) {
        case QEvent::ChildAdded:
        case QEvent::LayoutRequest:
            d->queueTabRefresh();
            break;
        case QEvent::Resize:
            if (d->expanded)
                d->positionOverlay(d->autoHidden.value(d->expanded.data()));
            break;
        default:
            break;
        }
        return false;
    }
    // 펼친 자동 숨김 도크 — 바깥을 누르거나 Esc면 접는다(메뉴 · 팝업 안은 제외)
    if (d->expanded && event->type() == QEvent::MouseButtonPress && !QApplication::activePopupWidget()) {
        const auto it = d->autoHidden.constFind(d->expanded.data());
        if (auto *w = qobject_cast<QWidget *>(watched); w && it != d->autoHidden.cend()) {
            const bool inside = (it->overlay && (w == it->overlay || it->overlay->isAncestorOf(w)))
                             || (it->tab && (w == it->tab || it->tab->isAncestorOf(w)));
            if (!inside)
                d->collapse();
        }
    } else if (d->expanded && event->type() == QEvent::KeyPress
               && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        if (auto *w = qobject_cast<QWidget *>(watched); w && d->dockOf(w) == d->expanded) {
            QDockWidget *dock = d->expanded;
            d->collapse();
            if (const auto it = d->autoHidden.constFind(dock); it != d->autoHidden.cend() && it->tab)
                it->tab->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    return false;
}

} // namespace fm::dock
