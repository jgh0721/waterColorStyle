#include "DockInternal_p.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

namespace detail {

namespace {

constexpr qreal kPreviewOpacity = 0.82;

QRect globalRect(const QWidget *w)
{
    return QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
}

Qt::WindowFlags overlayFlags()
{
    return Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput
         | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint;
}

} // namespace

// =============================================================================================
// DragPreviewWindow

DragPreviewWindow::DragPreviewWindow(DockManager *manager, const QString &title, const QPixmap &content,
                                     const QSize &size, bool topLevel)
    : QWidget(manager->window(), topLevel ? overlayFlags() : Qt::Widget)
    , m_title(title)
    , m_content(content)
{
    setObjectName(u"fmDockDragPreview"_s);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    if (topLevel) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }
    resize(size);
    show();
    raise();
}

void DragPreviewWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setOpacity(kPreviewOpacity);
    const fs::ThemeColors &tc = colorsOf(this);
    const QRect r = rect();
    fs::paintDockFlyoutFrame(&p, r, tc);
    const int titleHeight = metricsOf(this).paneTitle;
    const QRect title(r.left() + 1, r.top() + 1, r.width() - 2, titleHeight);
    fs::paintDockPaneTitle(&p, title, true, tc);
    fs::paintDockTitleText(&p, title.adjusted(8, 0, -8, 0), m_title, fs::DockTitleKind::Pane, true, tc);
    const QRect body(r.left() + 1, title.bottom() + 1, r.width() - 2, std::max(0, r.bottom() - title.bottom() - 1));
    if (!m_content.isNull()) {
        p.save();
        p.setClipRect(body);
        p.drawPixmap(body.topLeft(), m_content);
        p.restore();
    } else {
        // 그릴 수 없는 내용(다른 프로세스 창) — 틀만
        fs::paintDropPreview(&p, body.adjusted(2, 2, -2, -2), tc);
    }
}

// =============================================================================================
// DropOverlay

DropOverlay::DropOverlay(DockManager *manager, bool topLevel)
    : QWidget(manager->window(), topLevel ? overlayFlags() : Qt::Widget)
    , m_topLevel(topLevel)
{
    setObjectName(u"fmDockDropOverlay"_s);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    if (topLevel) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
    } else {
        setAttribute(Qt::WA_NoSystemBackground);
    }
    hide();
}

void DropOverlay::showVisual(const QRect &globalRectangle, const DropVisual &visual)
{
    const bool wasVisible = isVisible();
    const bool sameRect = globalRectangle == m_global;
    // 끄는 동안 대부분의 이동은 표시가 그대로 — 창 전체(배율 1.5면 수백만 화소)를 다시 칠하지 않는다
    if (wasVisible && sameRect && visual == m_visual)
        return;
    const QRect oldBounds = m_visual.bounds();
    m_global = globalRectangle;
    m_visual = visual;
    if (m_topLevel)
        setGeometry(globalRectangle);
    else
        setGeometry(QRect(parentWidget()->mapFromGlobal(globalRectangle.topLeft()), globalRectangle.size()));
    if (!wasVisible) {
        show();
        raise();
    }
    if (!wasVisible || !sameRect) {
        update();
        return;
    }
    // 바뀐 표시의 앞뒤 자리만 — 테두리 · 반투명 가장자리 몫으로 넉넉히
    constexpr int kMargin = 6;
    const QPoint offset = -m_global.topLeft();
    QRegion dirty;
    if (oldBounds.isValid())
        dirty += oldBounds.translated(offset).adjusted(-kMargin, -kMargin, kMargin, kMargin);
    if (const QRect now = m_visual.bounds(); now.isValid())
        dirty += now.translated(offset).adjusted(-kMargin, -kMargin, kMargin, kMargin);
    update(dirty);
}

void DropOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const fs::ThemeColors &tc = colorsOf(this);
    const QPoint offset = -m_global.topLeft();
    if (m_visual.sideZone.isValid())
        fs::paintDropPreview(&p, QRectF(m_visual.sideZone.translated(offset)), tc);
    else if (m_visual.preview.isValid())
        fs::paintDropPreview(&p, QRectF(m_visual.preview.translated(offset)), tc);
    if (m_visual.caret.isValid())
        fs::paintDropCaret(&p, QRectF(m_visual.caret.translated(offset)), tc);
    for (const DropIndicator &ind : std::as_const(m_visual.indicators))
        fs::paintDropIndicator(&p, QRectF(ind.rect.translated(offset)), ind.spot, ind.hover, tc);
}

// =============================================================================================
// DragSession

DragSession::DragSession(DockManager *manager, const QList<DockWidget *> &docks, DockArea *sourceArea,
                         FloatingWindow *movingWindow, Mode mode, const QPoint &hotSpot, const QSize &size)
    : QObject(manager)
    , m_manager(manager)
    , m_sourceArea(sourceArea)
    , m_window(movingWindow)
    , m_mode(mode)
    , m_hotSpot(hotSpot)
    , m_size(size)
{
    for (DockWidget *d : docks) {
        if (d)
            m_docks.append(d);
    }
    const bool topLevel = !manager->testConfigFlag(DockManager::NoTopLevelWindows);
    if (mode == Mode::Preview) {
        DockWidget *first = docks.isEmpty() ? nullptr : docks.first();
        DockWidget *shown = sourceArea && sourceArea->currentDockWidget() && docks.contains(sourceArea->currentDockWidget())
                              ? sourceArea->currentDockWidget()
                              : first;
        const bool nativeContent = std::any_of(docks.cbegin(), docks.cend(), [](DockWidget *d) {
            return d->hasNativeContent();
        });
        QPixmap content;
        if (shown && !nativeContent && manager->testConfigFlag(DockManager::DragPreviewShowsContent) && shown->isVisible())
            content = shown->grab();
        QString title = shown ? shown->title() : QString();
        if (docks.size() > 1)
            title += u" (+"_s + QString::number(docks.size() - 1) + u")"_s;
        m_preview = new DragPreviewWindow(manager, title, content, size, topLevel);
    }
    m_overlay = new DropOverlay(manager, topLevel);
    m_hoverTimer.setSingleShot(true);
    m_hoverTimer.setInterval(500);
    connect(&m_hoverTimer, &QTimer::timeout, this, [this] {
        if (m_manager && m_hoverDock && m_manager->testAutoHideFlag(DockManager::OpenOnDragHover))
            m_manager->expandAutoHide(m_hoverDock);
    });
    buildCandidates();
    qApp->installEventFilter(this);
}

DragSession::~DragSession()
{
    finish();
}

DockContainer *DragSession::targetContainer() const
{
    return m_current >= 0 && m_current < m_candidates.size() ? m_candidates[m_current].container.data() : nullptr;
}

bool DragSession::anyDocument() const
{
    return std::any_of(m_docks.cbegin(), m_docks.cend(), [](const QPointer<DockWidget> &d) {
        return d && d->isDocument();
    });
}

bool DragSession::allTools() const
{
    return !anyDocument();
}

bool DragSession::canFloat() const
{
    if (!m_manager || m_manager->testConfigFlag(DockManager::NoTopLevelWindows))
        return false;
    if (anyDocument() && m_manager->testConfigFlag(DockManager::DocumentsStayCentral))
        return false;
    return std::all_of(m_docks.cbegin(), m_docks.cend(), [](const QPointer<DockWidget> &d) {
        return d && d->hasFeature(DockWidget::Floatable);
    });
}

void DragSession::buildCandidates()
{
    m_candidates.clear();
    DockManager *m = m_manager;
    if (!m)
        return;
    QList<DockWidget *> docks;
    for (const QPointer<DockWidget> &d : m_docks) {
        if (d)
            docks.append(d);
    }
    const bool pinnable = m->testAutoHideFlag(DockManager::AutoHideEnabled)
                       && std::all_of(docks.cbegin(), docks.cend(), [](DockWidget *d) {
                              return d->hasFeature(DockWidget::Pinnable);
                          })
                       && !(anyDocument() && m->testConfigFlag(DockManager::DocumentsStayCentral));
    const bool edges = !(anyDocument() && m->testConfigFlag(DockManager::DocumentsStayCentral));
    const bool rejectTools = !allTools() && m->testConfigFlag(DockManager::CentralRejectsTools);

    // 떠 있는 창이 위에 있으므로 먼저
    QList<DockContainer *> containers;
    for (FloatingWindow *w : m->floatingWindows()) {
        if (w != m_window && w->isVisible())
            containers << w->dockContainer();
    }
    containers << m->dockContainer();

    for (DockContainer *c : containers) {
        Candidate cand;
        cand.container = c;
        const bool main = !c->isFloating();
        DropContainerGeom &g = cand.geom;
        g.rect = globalRect(c);
        g.outer = main ? globalRect(m) : g.rect;
        g.allowEdges = edges;
        g.allowSideBars = main && pinnable;
        g.sideBarThickness = metricsOf(m).sideBar;
        if (main) {
            for (int i = 0; i < 4; ++i) {
                SideBar *bar = m->sideBar(sideAt(i));
                if (!bar || !bar->isVisible())
                    continue;
                g.sideBars[i] = globalRect(bar);
                for (SideTab *tab : bar->tabs()) {
                    if (tab->isVisible())
                        g.sideTabs[i] << globalRect(tab);
                }
            }
        }
        for (DockArea *area : c->openedDockAreas()) {
            if (!area->isVisible())
                continue;
            DropAreaGeom a;
            a.rect = globalRect(area);
            const QRect content = area->contentGeometry();
            a.content = QRect(area->mapToGlobal(content.topLeft()), content.size());
            if (const QRect strip = area->titleBarGeometry(); strip.isValid()) {
                a.strip = QRect(area->mapToGlobal(strip.topLeft()), strip.size());
                DockTabBar *bar = area->titleBar()->tabBar();
                QList<QPair<int, QRect>> tabs;
                for (int i = 0; i < bar->count(); ++i) {
                    DockTab *tab = bar->tabAt(i);
                    if (tab && tab->isVisible())
                        tabs << qMakePair(area->indexOf(tab->dock()), globalRect(tab));
                }
                std::sort(tabs.begin(), tabs.end(), [](const auto &x, const auto &y) { return x.second.left() < y.second.left(); });
                for (const auto &[index, rect] : tabs) {
                    a.tabs << rect;
                    a.tabIndexes << index;
                }
            }
            const bool source = area == m_sourceArea;
            a.allowCenter = !source && m->canDropInto(docks, area, true) && !(area->isCentral() && rejectTools);
            a.allowSides = m->canDropInto(docks, area, false) && !(area->isCentral() && rejectTools);
            if (!a.allowCenter && !a.allowSides)
                continue;
            g.areas << a;
            cand.areas << area;
        }
        if (QWidget *placeholder = c->centralPlaceholder(); placeholder && placeholder->isVisible() && !rejectTools) {
            DropAreaGeom a;
            a.rect = globalRect(placeholder);
            a.content = a.rect;
            a.placeholder = true;
            a.allowSides = false;
            g.areas << a;
            cand.areas << nullptr;
        }
        cand.overlayRect = g.outer;
        m_candidates << cand;
    }
}

DragSession::Candidate *DragSession::candidateAt(const QPoint &globalPos)
{
    for (int i = 0; i < m_candidates.size(); ++i) {
        Candidate &c = m_candidates[i];
        if (!c.container)
            continue;
        QWidget *top = c.container->window();
        if (top && !top->isVisible())
            continue;
        if (c.overlayRect.contains(globalPos)) {
            m_current = i;
            return &c;
        }
    }
    m_current = -1;
    return nullptr;
}

void DragSession::move(const QPoint &globalPos)
{
    if (m_finished)
        return;
    if (m_preview)
        m_preview->move(globalPos - m_hotSpot);
    const bool ctrl = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier);
    Candidate *c = candidateAt(globalPos);
    DropVisual visual;
    m_target = DropTarget{};
    if (c && !ctrl)
        m_target = resolveDrop(c->geom, globalPos, DropOptions{}, &visual);
    if (m_overlay) {
        if (c && !ctrl)
            m_overlay->showVisual(c->overlayRect, visual);
        else
            m_overlay->hide();
    }
    // 끄는 중 사이드 탭 위에 머물면 펼친다
    DockWidget *hover = nullptr;
    if (c && !c->container->isFloating() && m_manager) {
        for (int i = 0; i < 4 && !hover; ++i) {
            SideBar *bar = m_manager->sideBar(sideAt(i));
            if (!bar || !bar->isVisible())
                continue;
            for (SideTab *tab : bar->tabs()) {
                if (tab->isVisible() && globalRect(tab).contains(globalPos))
                    hover = tab->dock();
            }
        }
    }
    if (hover != m_hoverDock) {
        m_hoverDock = hover;
        if (hover)
            m_hoverTimer.start();
        else
            m_hoverTimer.stop();
    }
}

bool DragSession::drop(const QPoint &globalPos)
{
    if (m_finished)
        return false;
    move(globalPos);
    const DropTarget target = m_target;
    Candidate *c = m_current >= 0 && m_current < m_candidates.size() ? &m_candidates[m_current] : nullptr;
    QList<DockWidget *> docks;
    for (const QPointer<DockWidget> &d : m_docks) {
        if (d)
            docks.append(d);
    }
    if (m_mode == Mode::MoveWindow && m_window)
        docks = m_window->dockContainer()->dockWidgets();
    finish();
    const QPointer<DockManager> manager = m_manager;
    bool moved = false;
    if (manager && !docks.isEmpty()) {
        DockArea *area = c && target.area >= 0 && target.area < c->areas.size() ? c->areas[target.area].data() : nullptr;
        switch (target.kind) {
        case DropTarget::Kind::None:
            if (m_mode == Mode::Preview && canFloat()) {
                manager->floatDocks(docks, QRect(globalPos - m_hotSpot, m_size));
                moved = true;
            }
            break;
        case DropTarget::Kind::ContainerEdge:
            if (c && c->container) {
                manager->placeDocks(docks, target.side, nullptr, -1, c->container);
                moved = true;
            }
            break;
        case DropTarget::Kind::AreaSide:
            if (area) {
                manager->placeDocks(docks, target.side, area, -1, nullptr);
                moved = true;
            }
            break;
        case DropTarget::Kind::AreaCenter:
        case DropTarget::Kind::TabInsert:
            if (area) {
                manager->placeDocks(docks, DropArea::Center, area,
                                    target.kind == DropTarget::Kind::TabInsert ? target.index : -1, nullptr);
                moved = true;
            }
            break;
        case DropTarget::Kind::CentralEmpty:
            if (c && c->container) {
                manager->placeDocks(docks, DropArea::Center, nullptr, -1, c->container);
                moved = true;
            }
            break;
        case DropTarget::Kind::SideBar: {
            int index = target.index;
            for (DockWidget *d : docks) {
                manager->moveToSideBar(d, target.sideBar, index);
                ++index;
            }
            moved = true;
            break;
        }
        }
        if (moved) {
            for (DockWidget *d : docks) {
                if (!d->isClosed() && !d->isAutoHidden()) {
                    if (QWidget *content = d->widget(); content && content->focusPolicy() != Qt::NoFocus
                                                        && d->hasFeature(DockWidget::Focusable))
                        content->setFocus(Qt::MouseFocusReason);
                    break;
                }
            }
        }
    }
    if (manager)
        manager->dragFinished();
    deleteLater();
    return moved;
}

void DragSession::cancel()
{
    if (m_finished)
        return;
    m_cancelled = true;
    finish();
    if (m_manager)
        m_manager->dragFinished();
    deleteLater();
}

void DragSession::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_hoverTimer.stop();
    qApp->removeEventFilter(this);
    if (m_overlay) {
        m_overlay->hide();
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
    if (m_preview) {
        m_preview->hide();
        m_preview->deleteLater();
        m_preview = nullptr;
    }
}

bool DragSession::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        cancel();
        return true;
    }
    if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Control) {
        QTimer::singleShot(0, this, [this] {
            if (!m_finished)
                move(QCursor::pos());
        });
    }
    return false;
}

} // namespace detail

// =============================================================================================
// DockManager — 끌기

void DockManager::startDrag(const QList<DockWidget *> &docks, DockArea *sourceArea, QWidget *origin,
                            const QPoint &globalPos, const QPoint &hotSpot)
{
    Q_UNUSED(origin)
    if (m_drag || docks.isEmpty())
        return;
    QList<DockWidget *> movable;
    for (DockWidget *d : docks) {
        if (d && d->hasFeature(DockWidget::Movable))
            movable.append(d);
    }
    if (movable.isEmpty())
        return;
    QSize size = sourceArea ? sourceArea->size() : movable.first()->size();
    if (size.width() < 120 || size.height() < 80)
        size = QSize(300, 200);
    size = size.boundedTo(QSize(400, 300));
    const QPoint hot(std::clamp(hotSpot.x(), 8, size.width() - 8), std::clamp(hotSpot.y(), 6, 24));
    m_drag = new detail::DragSession(this, movable, sourceArea, nullptr, detail::DragSession::Mode::Preview, hot, size);
    m_drag->move(globalPos);
}

void DockManager::dragFinished()
{
    m_drag = nullptr;
}

} // namespace fm::dock
