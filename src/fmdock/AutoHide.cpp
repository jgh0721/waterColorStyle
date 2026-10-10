#include "DockManager_p.h"

#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace fm::dock {

namespace fs = fm::style;

namespace {

constexpr int kResizeGrip = 5;  // 펼친 창 안쪽 가장자리에서 크기를 바꾸는 폭

} // namespace

// =============================================================================================
// SideTab

SideTab::SideTab(QDockWidget *dock, Qt::DockWidgetArea side, QWidget *parent)
    : QAbstractButton(parent)
    , m_dock(dock)
    , m_side(side)
{
    setCheckable(true);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    // 탭 줄과 같은 12 px(스타일이 QTabBar에 주는 글꼴)
    QFont f = font();
    f.setPixelSize(12);
    setFont(f);
    setToolTip(dock->windowTitle());
    connect(dock, &QDockWidget::windowTitleChanged, this, [this](const QString &title) {
        setToolTip(title);
        updateGeometry();
        update();
    });
}

void SideTab::initOption(QStyleOptionTab *opt) const
{
    opt->initFrom(this);
    opt->shape = m_side == Qt::RightDockWidgetArea   ? QTabBar::RoundedEast
               : m_side == Qt::BottomDockWidgetArea  ? QTabBar::RoundedSouth
                                                     : QTabBar::RoundedWest;
    opt->text = m_dock ? m_dock->windowTitle() : QString();
    opt->position = QStyleOptionTab::OnlyOneTab;
    opt->selectedPosition = QStyleOptionTab::NotAdjacent;
    if (isChecked())
        opt->state |= QStyle::State_Selected;
    if (isDown())
        opt->state |= QStyle::State_Sunken;
}

QSize SideTab::sizeHint() const
{
    // QTabBar::tabSizeHint와 같은 규칙 — 스타일의 탭 크기(CT_TabBarTab)
    QStyleOptionTab opt;
    initOption(&opt);
    const int hframe = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this);
    const int vframe = style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this);
    const QFontMetrics fm = fontMetrics();
    const int text = fm.size(Qt::TextShowMnemonic, opt.text).width();
    const bool vertical = m_side != Qt::BottomDockWidgetArea;
    const QSize contents = vertical ? QSize(fm.height() + vframe, text + hframe) : QSize(text + hframe, fm.height() + vframe);
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, contents, this);
}

void SideTab::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionTab opt;
    initOption(&opt);
    style()->drawControl(QStyle::CE_TabBarTabShape, &opt, &p, this);
    style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &p, this);
}

void SideTab::enterEvent(QEnterEvent *event)
{
    update();
    QAbstractButton::enterEvent(event);
}

void SideTab::leaveEvent(QEvent *event)
{
    update();
    QAbstractButton::leaveEvent(event);
}

// =============================================================================================
// SideBar

SideBar::SideBar(Qt::DockWidgetArea side, QWidget *parent)
    : QToolBar(parent)
    , m_side(side)
{
    setObjectName(side == Qt::RightDockWidgetArea    ? u"fmDockSideBarRight"_s
                  : side == Qt::BottomDockWidgetArea ? u"fmDockSideBarBottom"_s
                                                     : u"fmDockSideBarLeft"_s);
    setWindowTitle(tr("자동 숨김"));
    setMovable(false);
    setFloatable(false);
    setAllowedAreas(side == Qt::RightDockWidgetArea    ? Qt::RightToolBarArea
                    : side == Qt::BottomDockWidgetArea ? Qt::BottomToolBarArea
                                                       : Qt::LeftToolBarArea);
    setContextMenuPolicy(Qt::PreventContextMenu);
    toggleViewAction()->setVisible(false);  // 메인 창의 도구 모음 메뉴에 넣지 않는다
}

SideTab *SideBar::addTab(QDockWidget *dock)
{
    auto *tab = new SideTab(dock, m_side, this);
    m_actions.insert(tab, addWidget(tab));
    return tab;
}

void SideBar::removeTab(SideTab *tab)
{
    if (QAction *action = m_actions.take(tab)) {
        removeAction(action);
        delete action;  // addWidget이 만든 QWidgetAction — 탭 위젯도 함께 지운다
    }
}

int SideBar::count() const
{
    return int(m_actions.size());
}

void SideBar::paintEvent(QPaintEvent *)
{
    // 도구 모음 모양(시안2 새긴 선) 대신 창 바탕 + 안쪽 가장자리 선
    QPainter p(this);
    const Qt::Edge inner = m_side == Qt::RightDockWidgetArea    ? Qt::LeftEdge
                         : m_side == Qt::BottomDockWidgetArea   ? Qt::TopEdge
                                                                : Qt::RightEdge;
    fs::paintDockSideBar(&p, rect(), inner, fs::themeColorsFor(this));
}

// =============================================================================================
// AutoHideOverlay

AutoHideOverlay::AutoHideOverlay(DockManagerPrivate *manager, QDockWidget *dock, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_dock(dock)
{
    setObjectName(u"fmDockAutoHideOverlay"_s);
    setMouseTracking(true);
    fs::setPaneActive(this, true);  // 펼친 창의 틀은 늘 활성 모양
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(dock);
}

void AutoHideOverlay::showEvent(QShowEvent *event)
{
    // 틀 두께만큼 띄우고, 크기를 바꾸는 안쪽 가장자리는 손잡이 폭을 더 비운다(내용 위젯이 마우스를 가져가지 않게)
    const int fw = style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, nullptr, this);
    const int grip = fw + kResizeGrip;
    switch (side()) {
    case Qt::RightDockWidgetArea: layout()->setContentsMargins(grip, fw, fw, fw); break;
    case Qt::BottomDockWidgetArea: layout()->setContentsMargins(fw, grip, fw, fw); break;
    default: layout()->setContentsMargins(fw, fw, grip, fw); break;
    }
    QWidget::showEvent(event);
}

Qt::DockWidgetArea AutoHideOverlay::side() const
{
    const auto it = m_manager->autoHidden.constFind(m_dock.data());
    return it == m_manager->autoHidden.cend() ? Qt::LeftDockWidgetArea : it->side;
}

bool AutoHideOverlay::onResizeEdge(const QPoint &pos) const
{
    switch (side()) {
    case Qt::RightDockWidgetArea: return pos.x() < kResizeGrip;
    case Qt::BottomDockWidgetArea: return pos.y() < kResizeGrip;
    default: return pos.x() >= width() - kResizeGrip;
    }
}

void AutoHideOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionFrame opt;
    opt.initFrom(this);
    opt.rect = rect();
    style()->drawPrimitive(QStyle::PE_FrameDockWidget, &opt, &p, this);
}

void AutoHideOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && onResizeEdge(event->position().toPoint())) {
        m_resizing = true;
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressSize = side() == Qt::BottomDockWidgetArea ? height() : width();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AutoHideOverlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const Qt::DockWidgetArea s = side();
    if (!m_resizing) {
        setCursor(onResizeEdge(pos) ? (s == Qt::BottomDockWidgetArea ? Qt::SizeVerCursor : Qt::SizeHorCursor)
                                    : Qt::ArrowCursor);
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
    const int change = s == Qt::RightDockWidgetArea ? -delta.x() : s == Qt::BottomDockWidgetArea ? -delta.y() : delta.x();
    auto it = m_manager->autoHidden.find(m_dock.data());
    if (it != m_manager->autoHidden.end()) {
        it->size = std::max(120, m_pressSize + change);
        m_manager->positionOverlay(*it);
    }
    event->accept();
}

void AutoHideOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace fm::dock
