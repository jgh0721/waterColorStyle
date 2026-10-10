#include "DockManager_p.h"

#include <fmstyle/StyleProps.h>

#include <QApplication>
#include <QDockWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

// =============================================================================================
// DockTitleButton

DockTitleButton::DockTitleButton(const QString &kind, QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setKind(kind);
}

void DockTitleButton::setKind(const QString &kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    fs::setDockButton(this, kind);
    if (kind == u"close")
        setToolTip(tr("닫기"));
    else if (kind == u"float")
        setToolTip(tr("떼어 내기 · 붙이기"));
    else if (kind == u"pin")
        setToolTip(tr("자동 숨김"));
    else if (kind == u"unpin")
        setToolTip(tr("도크에 고정"));
}

QSize DockTitleButton::sizeHint() const
{
    // QDockWidget 기본 단추와 같은 크기 규칙(2 × 여백 + 줄인 아이콘 10)
    const int size = 2 * style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, nullptr, this) + 10;
    return {size, size};
}

void DockTitleButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionToolButton opt;
    opt.initFrom(this);
    opt.state |= QStyle::State_AutoRaise;
    if (isDown())
        opt.state |= QStyle::State_Sunken;
    else if (underMouse() && isEnabled())
        opt.state |= QStyle::State_Raised;
    style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &p, this);
}

void DockTitleButton::enterEvent(QEnterEvent *event)
{
    update();
    QAbstractButton::enterEvent(event);
}

void DockTitleButton::leaveEvent(QEvent *event)
{
    update();
    QAbstractButton::leaveEvent(event);
}

// =============================================================================================
// DockTitleBar

DockTitleBar::DockTitleBar(QDockWidget *dock, DockManagerPrivate *manager)
    : QWidget(dock)
    , m_dock(dock)
    , m_manager(manager)
    , m_pin(new DockTitleButton(u"pin"_s, this))
    , m_float(new DockTitleButton(u"float"_s, this))
    , m_close(new DockTitleButton(u"close"_s, this))
{
    setAttribute(Qt::WA_Hover);
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        if (m_manager->autoHidden.contains(m_dock))
            m_manager->q->setAutoHidden(m_dock, false);
        m_dock->close();
    });
    connect(m_float, &QAbstractButton::clicked, this, [this] { m_dock->setFloating(!m_dock->isFloating()); });
    connect(m_pin, &QAbstractButton::clicked, this, [this] {
        m_manager->q->setAutoHidden(m_dock, !m_manager->autoHidden.contains(m_dock));
    });
    connect(m_dock, &QDockWidget::windowTitleChanged, this, qOverload<>(&QWidget::update));
    updateButtons();
}

DockTitleBar::~DockTitleBar() = default;

int DockTitleBar::buttonSize() const
{
    return 2 * style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, nullptr, this) + 10;
}

QSize DockTitleBar::sizeHint() const
{
    // QDockWidgetLayout::titleHeight와 같은 규칙 — 기본 제목 줄과 높이가 같다
    const int margin = style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, nullptr, m_dock);
    const int height = std::max(buttonSize() + 2, m_dock->fontMetrics().height() + 2 * margin);
    return {fontMetrics().horizontalAdvance(m_dock->windowTitle()) + 4 * buttonSize() + 24, height};
}

QSize DockTitleBar::minimumSizeHint() const
{
    return {4 * buttonSize() + 16, sizeHint().height()};
}

void DockTitleBar::updateButtons()
{
    const QDockWidget::DockWidgetFeatures features = m_dock->features();
    const bool autoHidden = m_manager->autoHidden.contains(m_dock);
    const bool floating = m_dock->isFloating();
    m_close->setVisible(features & QDockWidget::DockWidgetClosable);
    m_float->setVisible((features & QDockWidget::DockWidgetFloatable) && !autoHidden);
    // 압정: 붙은 도크는 자동 숨김으로, 자동 숨김 도크는 다시 도크로. 떠 있는 창에는 없다.
    m_pin->setKind(autoHidden ? u"unpin"_s : u"pin"_s);
    m_pin->setVisible(!floating);
    // 스타일이 제목 글자 자리를 셀 때 닫기 · 떼어 내기 밖의 단추(압정)도 비우게
    setProperty(fs::props::kDockExtraButtons, m_pin->isHidden() ? 0 : 1);
    layoutButtons();
    update();
}

void DockTitleBar::layoutButtons()
{
    // 단추 자리는 스타일의 제목 줄 배치(SE_DockWidget*)를 따른다 — 오른쪽부터 닫기 · 떼어 내기 · 압정
    QStyleOptionDockWidget opt;
    opt.initFrom(this);
    opt.rect = rect();
    opt.closable = true;
    opt.floatable = true;
    const QRect slot0 = style()->subElementRect(QStyle::SE_DockWidgetCloseButton, &opt, this);
    const QRect slot1 = style()->subElementRect(QStyle::SE_DockWidgetFloatButton, &opt, this);
    const QPoint step = slot0.topLeft() - slot1.topLeft();
    int slot = 0;
    for (DockTitleButton *button : {m_close, m_float, m_pin}) {
        if (button->isHidden())
            continue;
        const QRect r = slot0.translated(-step * slot);
        button->setGeometry(r);
        ++slot;
    }
}

bool DockTitleBar::event(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        updateGeometry();
        layoutButtons();
    }
    return QWidget::event(event);
}

void DockTitleBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutButtons();
}

void DockTitleBar::paintEvent(QPaintEvent *)
{
    // 기본 제목 줄과 같은 스타일 요소로 — 단추 수만큼 글자 자리를 비운다
    QPainter p(this);
    QStyleOptionDockWidget opt;
    opt.initFrom(this);
    opt.rect = rect();
    opt.title = m_dock->windowTitle();
    opt.closable = !m_close->isHidden();
    opt.floatable = !m_float->isHidden();
    opt.movable = m_dock->features() & QDockWidget::DockWidgetMovable;
    style()->drawControl(QStyle::CE_DockWidgetTitle, &opt, &p, this);
}

void DockTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_pressed = true;
    m_pressGlobal = event->globalPosition().toPoint();
    m_offset = m_dock->mapFromGlobal(m_pressGlobal);
    // 누르면 도크 안으로 포커스 — 활성 도크가 된다
    if (QWidget *content = m_dock->widget(); content && !m_dock->isAncestorOf(QApplication::focusWidget()))
        content->setFocus(Qt::MouseFocusReason);
    event->accept();
}

void DockTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed) {
        event->ignore();
        return;
    }
    const QPoint global = event->globalPosition().toPoint();
    if (!m_drag) {
        const bool movable = m_dock->features() & QDockWidget::DockWidgetMovable;
        if (!movable || m_manager->autoHidden.contains(m_dock)
            || (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance()) {
            event->accept();
            return;
        }
        m_drag = std::make_unique<DockDrag>(m_manager, m_dock, m_offset);
    }
    m_drag->move(global);
    event->accept();
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_pressed = false;
    if (m_drag) {
        std::unique_ptr<DockDrag> drag = std::move(m_drag);
        if (!drag->isCancelled())
            drag->drop(event->globalPosition().toPoint());
    }
    event->accept();
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 두 번 누르면 떼기 · 붙이기(떠 있는 창은 마지막에 붙어 있던 자리로)
    if (event->button() == Qt::LeftButton && (m_dock->features() & QDockWidget::DockWidgetFloatable)
        && !m_manager->autoHidden.contains(m_dock)) {
        m_dock->setFloating(!m_dock->isFloating());
        event->accept();
        return;
    }
    event->ignore();
}

} // namespace fm::dock
