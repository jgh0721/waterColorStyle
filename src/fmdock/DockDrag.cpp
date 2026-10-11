#include "DockManager_p.h"

#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QDockWidget>
#include <QKeyEvent>
#include <QMainWindow>
#include <QPainter>

namespace fm::dock {

namespace fs = fm::style;
using fs::DropSpot;

namespace {

constexpr DropSpot kOuterSpots[] = {DropSpot::OuterLeft, DropSpot::OuterTop, DropSpot::OuterRight, DropSpot::OuterBottom};

Qt::DockWidgetArea outerArea(DropSpot spot)
{
    switch (spot) {
    case DropSpot::OuterLeft: return Qt::LeftDockWidgetArea;
    case DropSpot::OuterRight: return Qt::RightDockWidgetArea;
    case DropSpot::OuterTop: return Qt::TopDockWidgetArea;
    case DropSpot::OuterBottom: return Qt::BottomDockWidgetArea;
    default: return Qt::NoDockWidgetArea;
    }
}

bool sameTarget(const std::optional<DropTarget> &a, const std::optional<DropTarget> &b)
{
    if (a.has_value() != b.has_value())
        return false;
    return !a || (a->spot == b->spot && a->dock == b->dock);
}

} // namespace

// =============================================================================================
// DropOverlay

DropOverlay::DropOverlay(DockManagerPrivate *manager, QDockWidget *dragged)
    : QWidget(manager->window)
    , m_manager(manager)
    , m_dragged(dragged)
{
    setObjectName(QStringLiteral("fmDockDropOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(manager->window->rect());
    raise();
    show();
}

QRect DropOverlay::indicatorRect(DropSpot spot, const QRect &content, const QRect &dockRect)
{
    // classic 표시: 바깥 넷은 내용 가장자리 가운데에서 10 안쪽, 십자는 도크 가운데에 단추 사이 4
    const int s = fs::kDropIndicatorSize;
    const int inset = 10;
    const int gap = 4;
    const QPoint c = dockRect.center();
    switch (spot) {
    case DropSpot::OuterLeft: return {content.left() + inset, content.center().y() - s / 2, s, s};
    case DropSpot::OuterRight: return {content.right() - inset - s + 1, content.center().y() - s / 2, s, s};
    case DropSpot::OuterTop: return {content.center().x() - s / 2, content.top() + inset, s, s};
    case DropSpot::OuterBottom: return {content.center().x() - s / 2, content.bottom() - inset - s + 1, s, s};
    case DropSpot::Center: return {c.x() - s / 2, c.y() - s / 2, s, s};
    case DropSpot::Left: return {c.x() - s / 2 - s - gap, c.y() - s / 2, s, s};
    case DropSpot::Right: return {c.x() + s / 2 + gap, c.y() - s / 2, s, s};
    case DropSpot::Top: return {c.x() - s / 2, c.y() - s / 2 - s - gap, s, s};
    case DropSpot::Bottom: return {c.x() - s / 2, c.y() + s / 2 + gap, s, s};
    }
    return {};
}

QList<DropSpot> DropOverlay::spotsFor(const QDockWidget *dock) const
{
    if (!dock || !m_dragged)
        return {};
    QMainWindow *window = m_manager->window;
    auto *target = const_cast<QDockWidget *>(dock);
    if (!m_dragged->isAreaAllowed(window->dockWidgetArea(target)))
        return {};
    // Qt는 탭 묶음을 나눌 수 없다 — 탭 묶음에는 탭 자리만
    if (!window->tabifiedDockWidgets(target).isEmpty())
        return {DropSpot::Center};
    return {DropSpot::Left, DropSpot::Top, DropSpot::Right, DropSpot::Bottom, DropSpot::Center};
}

std::optional<DropTarget> DropOverlay::track(const QPoint &point)
{
    QDockWidget *target = m_manager->dockAt(point, m_dragged);
    const QRect content = m_manager->contentRect();
    std::optional<DropTarget> hit;
    for (const DropSpot spot : kOuterSpots) {
        if (m_dragged && !m_dragged->isAreaAllowed(outerArea(spot)))
            continue;
        if (indicatorRect(spot, content, {}).contains(point))
            hit = DropTarget{spot, nullptr};
    }
    if (!hit && target) {
        for (const DropSpot spot : spotsFor(target)) {
            if (indicatorRect(spot, content, target->geometry()).contains(point))
                hit = DropTarget{spot, target};
        }
    }
    if (target != m_target || !sameTarget(hit, m_hover)) {
        m_target = target;
        m_hover = hit;
        QWidget::update();
    }
    return hit;
}

void DropOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const fs::ThemeColors &tc = fs::themeColorsFor(m_manager->window);
    const QRect content = m_manager->contentRect();
    if (m_hover) {
        const QRect preview = m_manager->previewRect(m_dragged, *m_hover);
        if (!preview.isEmpty())
            fs::paintDropPreview(&p, preview, tc);
    }
    for (const DropSpot spot : kOuterSpots) {
        if (m_dragged && !m_dragged->isAreaAllowed(outerArea(spot)))
            continue;
        const bool hover = m_hover && !m_hover->dock && m_hover->spot == spot;
        fs::paintDropIndicator(&p, indicatorRect(spot, content, {}), spot, hover, tc);
    }
    if (m_target) {
        for (const DropSpot spot : spotsFor(m_target)) {
            const bool hover = m_hover && m_hover->dock == m_target && m_hover->spot == spot;
            fs::paintDropIndicator(&p, indicatorRect(spot, content, m_target->geometry()), spot, hover, tc);
        }
    }
}

// =============================================================================================
// DockDrag

DockDrag::DockDrag(DockManagerPrivate *manager, QDockWidget *dock, const QPoint &offset)
    : m_manager(manager)
    , m_dock(dock)
    , m_offset(offset)
    , m_wasFloating(dock->isFloating())
    , m_startGeometry(dock->geometry())
{
    m_overlay = new DropOverlay(manager, dock);
    // 떠 있는 창은 손으로 옮기는 동안 반투명 — 아래 표시가 보이게
    if (m_wasFloating)
        dock->setWindowOpacity(0.7);
    qApp->installEventFilter(this);
}

DockDrag::~DockDrag()
{
    finish();
}

void DockDrag::finish()
{
    qApp->removeEventFilter(this);
    if (m_overlay) {
        m_overlay->hide();
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
    if (m_dock && m_wasFloating)
        m_dock->setWindowOpacity(1.0);
}

void DockDrag::move(const QPoint &global)
{
    if (m_cancelled || !m_dock || !m_manager->window)
        return;
    if (m_wasFloating)
        m_dock->move(global - m_offset);
    if (m_overlay)
        m_overlay->track(m_manager->window->mapFromGlobal(global));
}

void DockDrag::drop(const QPoint &global)
{
    if (m_cancelled || !m_dock || !m_manager->window) {
        finish();
        return;
    }
    std::optional<DropTarget> target;
    if (m_overlay)
        target = m_overlay->track(m_manager->window->mapFromGlobal(global));
    finish();
    if (target) {
        // 떠 있는 창은 먼저 붙였다가(마지막 자리) 놓을 자리로 옮긴다
        if (m_dock->isFloating())
            m_dock->setFloating(false);
        if (m_manager->applyDrop(m_dock, *target)) {
            if (QWidget *content = m_dock->widget())
                content->setFocus(Qt::MouseFocusReason);
            return;
        }
    }
    // 표시 밖에 놓으면 떠 있는 창(이미 떠 있으면 옮긴 자리 그대로)
    if (!m_wasFloating && (m_dock->features() & QDockWidget::DockWidgetFloatable)) {
        const QSize size = m_dock->size();
        m_dock->setFloating(true);
        m_dock->resize(size);
        m_dock->move(global - m_offset);
    }
}

bool DockDrag::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        // Esc — 끌기 취소(떠 있던 창은 처음 자리로)
        m_cancelled = true;
        if (m_dock && m_wasFloating)
            m_dock->setGeometry(m_startGeometry);
        finish();
        return true;
    }
    return false;
}

} // namespace fm::dock
