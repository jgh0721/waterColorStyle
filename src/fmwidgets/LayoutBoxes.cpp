// 배치 상자 — FlowLayout · FlexLayout을 품고 직접 자식을 스스로 배치하는 위젯(Designer에서 쓰려고).

#include "fmwidgets/LayoutBoxes.h"

#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>

#include <QChildEvent>
#include <QDynamicPropertyChangeEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace fm::ui {

namespace {

constexpr char kIgnore[] = "fmLayoutIgnore";  // 이 속성이 참인 자식은 배치에 넣지 않는다

bool adoptable(const QWidget *box, const QObject *child)
{
    if (!child->isWidgetType())
        return false;
    const auto *w = static_cast<const QWidget *>(child);
    return w->parentWidget() == box && !w->isWindow() && !w->property(kIgnore).toBool();
}

} // namespace

// ============================================================================ LayoutBox

LayoutBox::LayoutBox(QWidget *parent)
    : QWidget(parent)
{
}

QStringList LayoutBox::itemOrder() const
{
    QStringList names;
    if (!layout())
        return names;
    for (int i = 0; i < layout()->count(); ++i) {
        if (const QWidget *w = layout()->itemAt(i)->widget(); w && !w->objectName().isEmpty())
            names.append(w->objectName());
    }
    return names;
}

void LayoutBox::setItemOrder(const QStringList &names)
{
    m_order = names;
    reorderTo(names);
}

int LayoutBox::margin() const
{
    return layout() ? layout()->contentsMargins().left() : 0;
}

void LayoutBox::setMargin(int margin)
{
    if (layout())
        layout()->setContentsMargins(margin, margin, margin, margin);
}

void LayoutBox::setDesignMode(bool on)
{
    m_designMode = on;
    update();
}

void LayoutBox::paintEvent(QPaintEvent *)
{
    if (!m_designMode)
        return;
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    QPen pen(tc[fm::style::Token::Fg3], 1, Qt::DashLine);
    p.setPen(pen);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (!layout() || layout()->count() == 0) {
        p.drawText(rect(), Qt::AlignCenter,
                   tr("%1 — 위젯을 끌어 넣으세요").arg(QString::fromLatin1(metaObject()->className()).section(u':', -1)));
    }
}

int LayoutBox::indexOfWidget(const QWidget *widget) const
{
    if (!layout())
        return -1;
    for (int i = 0; i < layout()->count(); ++i) {
        if (layout()->itemAt(i)->widget() == widget)
            return i;
    }
    return -1;
}

void LayoutBox::syncChildren()
{
    const auto children = this->children();
    for (QObject *child : children) {
        if (adoptable(this, child)) {
            auto *w = static_cast<QWidget *>(child);
            if (indexOfWidget(w) < 0)
                adopt(w);
        }
    }
}

void LayoutBox::adopt(QWidget *child)
{
    // itemOrder에 이름이 있으면 그 차례에, 없으면 끝에(넣은 차례)
    const auto rank = [this](const QWidget *w) {
        const qsizetype i = m_order.indexOf(w->objectName());
        return i < 0 ? std::numeric_limits<qsizetype>::max() : i;
    };
    int index = layout()->count();
    const qsizetype own = rank(child);
    if (own != std::numeric_limits<qsizetype>::max()) {
        for (int i = 0; i < layout()->count(); ++i) {
            if (const QWidget *w = layout()->itemAt(i)->widget(); w && rank(w) > own) {
                index = i;
                break;
            }
        }
    }
    insertIntoLayout(index, child);
    child->installEventFilter(this);
    applyChildProperties(child);
}

void LayoutBox::reorderTo(const QStringList &names)
{
    if (!layout() || names.isEmpty())
        return;
    // 이름 순서대로 안정 정렬 — 목록에 없는 항목은 뒤에 지금 차례대로
    const auto rank = [&names](const QWidget *w) {
        const qsizetype i = w ? names.indexOf(w->objectName()) : -1;
        return i < 0 ? std::numeric_limits<qsizetype>::max() : i;
    };
    for (int target = 0; target < layout()->count(); ++target) {
        int best = target;
        for (int i = target + 1; i < layout()->count(); ++i) {
            if (rank(layout()->itemAt(i)->widget()) < rank(layout()->itemAt(best)->widget()))
                best = i;
        }
        if (best != target)
            moveInLayout(best, target);
    }
}

bool LayoutBox::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::ChildAdded:
        // 자식이 아직 다 만들어지지 않았을 수 있다 — 이벤트 고리에서(또는 그 전에 크기를 물으면 그때) 넣는다
        if (adoptable(this, static_cast<QChildEvent *>(event)->child()))
            QTimer::singleShot(0, this, &LayoutBox::syncChildren);
        break;
    case QEvent::ChildPolished:
    case QEvent::Show:
        syncChildren();
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

QSize LayoutBox::sizeHint() const
{
    const_cast<LayoutBox *>(this)->syncChildren();
    return QWidget::sizeHint();
}

QSize LayoutBox::minimumSizeHint() const
{
    const_cast<LayoutBox *>(this)->syncChildren();
    return QWidget::minimumSizeHint();
}

bool LayoutBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched->isWidgetType() && static_cast<QWidget *>(watched)->parentWidget() == this) {
        switch (event->type()) {
        case QEvent::DynamicPropertyChange:
            applyChildProperties(static_cast<QWidget *>(watched));
            break;
        case QEvent::Move:
            // Designer에서 끌어 옮긴 자식 — 다른 자식 위에 놓였으면 그 자리로(배치가 옮긴 것이면 겹치지 않는다)
            if (m_designMode) {
                m_moved = static_cast<QWidget *>(watched);
                scheduleSettle();
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LayoutBox::scheduleSettle()
{
    if (m_settlePending)
        return;
    m_settlePending = true;
    QTimer::singleShot(0, this, &LayoutBox::settle);
}

void LayoutBox::settle()
{
    m_settlePending = false;
    QWidget *moved = m_moved;
    m_moved = nullptr;
    if (!layout() || !moved)
        return;
    // 옮긴 자식의 가운데가 다른 자식 위에 있으면 그 자식 자리로
    const int from = indexOfWidget(moved);
    const QPoint center = moved->geometry().center();
    for (int to = 0; from >= 0 && to < layout()->count(); ++to) {
        QWidget *other = layout()->itemAt(to)->widget();
        if (to == from || !other || other->isHidden() || !other->geometry().contains(center))
            continue;
        moveInLayout(from, to);
        m_order = itemOrder();
        layout()->activate();
        Q_EMIT itemOrderChanged(m_order);
        return;
    }
    // 겹친 곳이 없으면 배치가 정한 자리로 되돌린다
    layout()->invalidate();
    layout()->activate();
}

// ============================================================================ FlowBox

FlowBox::FlowBox(QWidget *parent)
    : LayoutBox(parent)
    , m_layout(new FlowLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
}

void FlowBox::insertIntoLayout(int index, QWidget *widget)
{
    m_layout->insertWidget(index, widget);
}

void FlowBox::moveInLayout(int from, int to)
{
    if (QLayoutItem *item = m_layout->takeAt(from))
        m_layout->insertItem(to, item);
}

// ============================================================================ FlexBox

FlexBox::FlexBox(QWidget *parent)
    : LayoutBox(parent)
    , m_layout(new FlexLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
}

FlexLayout::Item FlexBox::flexItemOf(const QWidget *widget)
{
    FlexLayout::Item item;
    if (const QVariant v = widget->property(kGrow); v.isValid())
        item.grow = v.toDouble();
    if (const QVariant v = widget->property(kShrink); v.isValid())
        item.shrink = v.toDouble();
    if (const QVariant v = widget->property(kBasis); v.isValid())
        item.basis = v.toInt();
    if (const QVariant v = widget->property(kOrder); v.isValid())
        item.order = v.toInt();
    if (const QVariant v = widget->property(kAlignSelf); v.isValid()) {
        const QString s = v.toString().trimmed().toLower();
        item.alignSelf = s == QLatin1String("start")     ? FlexLayout::Align::Start
                       : s == QLatin1String("end")       ? FlexLayout::Align::End
                       : s == QLatin1String("center")    ? FlexLayout::Align::Center
                       : s == QLatin1String("stretch")   ? FlexLayout::Align::Stretch
                                                         : FlexLayout::Align::Auto;
    }
    return item;
}

void FlexBox::insertIntoLayout(int index, QWidget *widget)
{
    m_layout->insertWidget(index, widget, flexItemOf(widget));
}

void FlexBox::moveInLayout(int from, int to)
{
    if (QLayoutItem *item = m_layout->takeAt(from)) {
        const FlexLayout::Item flex = item->widget() ? flexItemOf(item->widget()) : FlexLayout::Item{};
        m_layout->insertItem(to, item, flex);
    }
}

void FlexBox::applyChildProperties(QWidget *widget)
{
    m_layout->setFlexItem(widget, flexItemOf(widget));
}

} // namespace fm::ui
