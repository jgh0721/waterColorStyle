#include "DockInternal_p.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionTab>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock::detail {

namespace {

constexpr int kIconSize = 16;
constexpr int kIconGap = 6;
constexpr int kDot = 7;
constexpr int kDotGap = 6;

DockManager *managerOf(const DockWidget *dock)
{
    return dock ? dock->dockManager() : nullptr;
}

bool flag(const DockWidget *dock, DockManager::ConfigFlag f)
{
    const DockManager *m = managerOf(dock);
    return m && m->testConfigFlag(f);
}

} // namespace

// =============================================================================================
// DockTab

DockTab::DockTab(DockWidget *dock, DockTabBar *bar)
    : QWidget(bar)
    , m_dock(dock)
    , m_bar(bar)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setFont(tabFontOf(bar));
    m_close = new TabCloseButton(this);
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        if (m_dock)
            m_dock->requestClose();
    });
    updateState();
}

void DockTab::setCurrent(bool current)
{
    if (m_current == current)
        return;
    m_current = current;
    updateState();
}

void DockTab::setTextLimit(int width)
{
    if (m_textLimit == width)
        return;
    m_textLimit = width;
    update();
}

bool DockTab::showsClose() const
{
    if (!m_dock || !m_dock->hasFeature(DockWidget::Closable))
        return false;
    if (m_dock->isDocument())
        return true;
    return (m_current && flag(m_dock, DockManager::ActiveTabHasCloseButton))
        || (m_hover && flag(m_dock, DockManager::HoverTabHasCloseButton));
}

int DockTab::trailingWidth() const
{
    int w = 0;
    if (m_dock && m_dock->isDocument() && m_dock->isModified())
        w += kDot + (showsClose() ? kDotGap : 0);
    if (showsClose())
        w += m_close->sizeHint().width();
    return w;
}

QString DockTab::shownText() const
{
    const QString title = m_dock ? m_dock->title() : QString();
    if (m_textLimit < 0)
        return title;
    return fontMetrics().elidedText(title, Qt::ElideRight, std::max(0, m_textLimit));
}

void DockTab::initStyleOption(QStyleOptionTab *opt) const
{
    opt->initFrom(this);
    opt->shape = m_bar && m_bar->atBottom() ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
    opt->text = shownText();
    opt->position = QStyleOptionTab::OnlyOneTab;
    opt->selectedPosition = QStyleOptionTab::NotAdjacent;
    opt->documentMode = true;
    if (m_dock && !m_dock->icon().isNull()) {
        opt->icon = m_dock->icon();
        opt->iconSize = QSize(kIconSize, kIconSize);
    }
    if (m_current)
        opt->state |= QStyle::State_Selected;
    else if (m_hover)
        opt->state |= QStyle::State_MouseOver;
    const int trailing = trailingWidth();
    if (trailing > 0)
        opt->rightButtonSize = QSize(trailing, m_close->sizeHint().height());
}

QSize DockTab::sizeHint() const
{
    QStyleOptionTab opt;
    initStyleOption(&opt);
    opt.text = m_dock ? m_dock->title() : QString();
    const QFontMetrics fm = fontMetrics();
    int w = fm.horizontalAdvance(opt.text) + style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this);
    if (!opt.icon.isNull())
        w += kIconSize + kIconGap;
    if (opt.rightButtonSize.isValid())
        w += opt.rightButtonSize.width() + 6;
    const QSize contents(w, fm.height() + style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this));
    QSize s = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, contents, this);
    s.setHeight(tabHeightOf(this));
    return s;
}

QSize DockTab::minimumSizeHint() const
{
    const QSize full = sizeHint();
    const QFontMetrics fm = fontMetrics();
    const int text = fm.horizontalAdvance(m_dock ? m_dock->title() : QString());
    const int minText = std::min(text, fm.horizontalAdvance(u"WWW…"_s));
    return {full.width() - text + minText, full.height()};
}

bool DockTab::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        m_hover = true;
        updateState();
        break;
    case QEvent::HoverLeave:
        m_hover = false;
        updateState();
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

void DockTab::updateState()
{
    if (!m_dock)
        return;
    const bool close = showsClose();
    m_close->setVisible(close);
    m_close->setSelectedTab(m_current);
    setToolTip(m_dock->title());
    setAccessibleName(m_dock->title());
    updateGeometry();
    QResizeEvent fake(size(), size());
    resizeEvent(&fake);
    update();
}

void DockTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_close->isHidden())   // 탭이 숨은 동안에도 자리를 잡아 둔다
        return;
    QStyleOptionTab opt;
    initStyleOption(&opt);
    QRect slot = style()->subElementRect(QStyle::SE_TabBarTabRightButton, &opt, this);
    if (!slot.isValid())
        slot = QRect(width() - trailingWidth() - 8, 0, trailingWidth(), height());
    const QSize cs = m_close->sizeHint();
    const int y = slot.isValid() ? slot.center().y() - cs.height() / 2 + 1 : (height() - cs.height()) / 2;
    m_close->setGeometry(slot.right() - cs.width() + 1, y, cs.width(), cs.height());
}

void DockTab::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOptionTab opt;
    initStyleOption(&opt);
    style()->drawControl(QStyle::CE_TabBarTabShape, &opt, &p, this);
    style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &p, this);
    if (m_dock && m_dock->isDocument() && m_dock->isModified()) {
        // 고친 문서 점 — 닫기 단추 왼쪽(없으면 오른쪽 끝)
        QRect slot = style()->subElementRect(QStyle::SE_TabBarTabRightButton, &opt, this);
        if (!slot.isValid())
            slot = QRect(width() - trailingWidth() - 8, 0, trailingWidth(), height());
        const QRect dot(slot.left(), slot.center().y() - kDot / 2 + 1, kDot, kDot);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(colorsOf(this)[fs::Token::Fg2]);
        p.drawEllipse(dot);
    }
}

void DockTab::mousePressEvent(QMouseEvent *event)
{
    if (!m_dock || !m_bar) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_reordering = false;
        m_pressGlobal = event->globalPosition().toPoint();
        if (DockArea *area = m_bar->area())
            area->setCurrentDockWidget(m_dock);
        if (m_dock->hasFeature(DockWidget::Focusable)) {
            QWidget *content = m_dock->widget();
            if (content && content->focusPolicy() != Qt::NoFocus)
                content->setFocus(Qt::MouseFocusReason);
            else if (content && content->focusProxy())
                content->focusProxy()->setFocus(Qt::MouseFocusReason);
            else if (DockManager *m = managerOf(m_dock))
                ManagerAccess::setFocusedDock(m, m_dock);
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        event->accept();
        return;
    }
    event->ignore();
}

void DockTab::mouseMoveEvent(QMouseEvent *event)
{
    DockManager *m = managerOf(m_dock);
    const QPoint global = event->globalPosition().toPoint();
    if (m) {
        if (DragSession *drag = ManagerAccess::drag(m)) {
            drag->move(global);
            event->accept();
            return;
        }
    }
    if (!m_pressed || !(event->buttons() & Qt::LeftButton) || !m_dock || !m || !m_bar) {
        event->ignore();
        return;
    }
    if (!m_reordering && (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance()) {
        event->accept();
        return;
    }
    if (!m_dock->hasFeature(DockWidget::Movable)) {
        event->accept();
        return;
    }
    // 탭 줄을 벗어나면(위아래로 탭 높이만큼) 끌어 놓기, 안이면 순서 바꾸기
    const QPoint inBar = m_bar->mapFromGlobal(global);
    const int slack = std::max(12, height() / 2);
    const bool outside = inBar.y() < -slack || inBar.y() > m_bar->height() + slack || inBar.x() < -40
                      || inBar.x() > m_bar->width() + 40;
    if (outside) {
        m_pressed = false;
        m_reordering = false;
        ManagerAccess::startDrag(m, {m_dock}, m_bar->area(), this, global, mapFromGlobal(global));
        event->accept();
        return;
    }
    m_reordering = true;
    m_bar->dragTabTo(this, global.x());
    event->accept();
}

void DockTab::mouseReleaseEvent(QMouseEvent *event)
{
    DockManager *m = managerOf(m_dock);
    const QPoint global = event->globalPosition().toPoint();
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        m_reordering = false;
        if (m) {
            if (DragSession *drag = ManagerAccess::drag(m)) {
                drag->drop(global);
                event->accept();
                return;
            }
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::MiddleButton && rect().contains(event->position().toPoint()) && m_dock
        && flag(m_dock, DockManager::MiddleButtonClosesTab)) {
        const QPointer<DockWidget> dock(m_dock);
        event->accept();
        dock->requestClose();
        return;
    }
    event->ignore();
}

void DockTab::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dock || !flag(m_dock, DockManager::DoubleClickUndocks)) {
        event->ignore();
        return;
    }
    event->accept();
    const QPointer<DockWidget> dock(m_dock);
    if (dock->isFloating())
        ManagerAccess::dockBack(dock->dockManager(), dock);
    else if (dock->hasFeature(DockWidget::Floatable) && !flag(dock, DockManager::NoTopLevelWindows))
        dock->setFloating();
}

void DockTab::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_dock)
        execDockMenu(m_dock, event->globalPos(), this);
    event->accept();
}

// =============================================================================================
// DockTabBar

DockTabBar::DockTabBar(DockArea *area, QWidget *parent)
    : QWidget(parent)
    , m_area(area)
{
    setFont(tabFontOf(this));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

bool DockTabBar::atBottom() const
{
    return m_area && m_area->dockManager() && m_area->dockManager()->testConfigFlag(DockManager::TabsAtBottom);
}

DockTab *DockTabBar::tabFor(const DockWidget *dock) const
{
    for (DockTab *tab : m_tabs) {
        if (tab->dock() == dock)
            return tab;
    }
    return nullptr;
}

void DockTabBar::insertTab(int index, DockWidget *dock)
{
    auto *tab = new DockTab(dock, this);
    m_tabs.insert(std::clamp(index, 0, int(m_tabs.size())), tab);
    relayout();
}

void DockTabBar::removeTab(DockWidget *dock)
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->dock() == dock || !m_tabs[i]->dock()) {
            DockTab *tab = m_tabs.takeAt(i);
            tab->hide();
            tab->deleteLater();
            --i;
        }
    }
    relayout();
}

void DockTabBar::moveTab(int from, int to)
{
    if (from < 0 || from >= m_tabs.size() || to < 0 || to >= m_tabs.size() || from == to)
        return;
    m_tabs.move(from, to);
    relayout();
}

void DockTabBar::setCurrentIndex(int index)
{
    for (int i = 0; i < m_tabs.size(); ++i)
        m_tabs[i]->setCurrent(i == index);
    relayout();
}

void DockTabBar::relayout()
{
    QList<DockTab *> visible;
    int current = -1;
    for (DockTab *tab : m_tabs) {
        DockWidget *dock = tab->dock();
        if (!dock || dock->isClosed() || dock->features().testFlag(DockWidget::NoTab)) {
            tab->hide();
            continue;
        }
        if (tab->isCurrent())
            current = int(visible.size());
        tab->setTextLimit(-1);
        visible.append(tab);
    }
    const int h = tabHeightOf(this);
    const int top = atBottom() ? 0 : height() - h;
    const int gap = metricsOf(this).tabGap;
    const int avail = width();
    QList<int> widths;
    int total = 0;
    for (DockTab *tab : visible) {
        widths.append(tab->sizeHint().width());
        total += widths.last();
    }
    total += gap * std::max(0, int(visible.size()) - 1);

    QList<bool> shown(visible.size(), true);
    if (total > avail && !visible.isEmpty()) {
        // 앞에서부터 들어가는 만큼, 선택 탭은 언제나 보인다
        std::fill(shown.begin(), shown.end(), false);
        int used = 0;
        const int need = current >= 0 ? widths[current] + gap : 0;
        for (int i = 0; i < visible.size(); ++i) {
            if (i == current)
                continue;
            const int w = widths[i] + gap;
            if (used + w + need <= avail) {
                shown[i] = true;
                used += w;
            }
        }
        if (current >= 0) {
            shown[current] = true;
            used += need;
            if (used > avail) {
                // 선택 탭만으로도 넘친다 — 글자를 줄인다
                visible[current]->setTextLimit(std::max(0, avail - (widths[current] - visible[current]->fontMetrics()
                                                                       .horizontalAdvance(visible[current]->dock()->title()))));
                widths[current] = std::min(widths[current], avail);
            }
        } else {
            // 선택 탭이 없으면 앞쪽부터
            shown[0] = true;
        }
    }
    m_hidden = 0;
    int x = 0;
    for (int i = 0; i < visible.size(); ++i) {
        DockTab *tab = visible[i];
        if (!shown[i]) {
            tab->hide();
            ++m_hidden;
            continue;
        }
        tab->setGeometry(x, top, widths[i], h);
        tab->show();
        x += widths[i] + gap;
    }
    if (current >= 0)
        visible[current]->raise();
    updateGeometry();
    if (auto *title = qobject_cast<DockAreaTitleBar *>(parentWidget()))
        title->updateOverflow();
}

void DockTabBar::dragTabTo(DockTab *tab, int globalX)
{
    if (!m_area)
        return;
    const int from = int(m_tabs.indexOf(tab));
    if (from < 0)
        return;
    const int x = mapFromGlobal(QPoint(globalX, 0)).x();
    int to = from;
    for (int i = 0; i < m_tabs.size(); ++i) {
        DockTab *other = m_tabs[i];
        if (other == tab || other->isHidden())
            continue;
        const QRect g = other->geometry();
        if (i < from && x < g.center().x()) {
            to = i;
            break;
        }
        if (i > from && x > g.center().x())
            to = i;
    }
    if (to != from)
        m_area->moveTab(from, to);
}

QSize DockTabBar::sizeHint() const
{
    int w = 0;
    for (DockTab *tab : m_tabs) {
        if (tab->dock() && !tab->dock()->isClosed())
            w += tab->sizeHint().width();
    }
    return {w, tabHeightOf(this)};
}

QSize DockTabBar::minimumSizeHint() const
{
    return {40, tabHeightOf(this)};
}

void DockTabBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void DockTabBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        for (DockTab *tab : m_tabs) {
            tab->setFont(tabFontOf(this));
            tab->updateState();
        }
        relayout();
    }
}

} // namespace fm::dock::detail
