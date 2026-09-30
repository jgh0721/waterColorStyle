#include "fmwidgets/SegmentedControl.h"

#include <fmstyle/StyleProps.h>

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>

#include <algorithm>

namespace fm::ui {

SegmentedControl::SegmentedControl(QWidget *parent)
    : SegmentedControl(QStringList(), parent)
{
}

SegmentedControl::SegmentedControl(const QStringList &items, QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_group(new QButtonGroup(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);  // 조각의 테두리를 겹쳐 1 px 구분선으로
    m_group->setExclusive(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_group, &QButtonGroup::idClicked, this, &SegmentedControl::setCurrentIndex);
    setItems(items);
}

void SegmentedControl::setItems(const QStringList &items)
{
    if (m_items == items && m_layout->count() == items.size())
        return;
    m_items = items;
    rebuild();
}

void SegmentedControl::rebuild()
{
    const auto old = m_group->buttons();
    for (QAbstractButton *b : old) {
        m_group->removeButton(b);
        delete b;
    }
    QList<QAbstractButton *> buttons;
    for (int i = 0; i < m_items.size(); ++i) {
        auto *b = new QPushButton(m_items.at(i), this);
        // 이 위젯에만 따로 스타일이 걸려 있으면(예: Qt Designer 미리보기) 조각에도 건다.
        if (testAttribute(Qt::WA_SetStyle))
            b->setStyle(style());
        m_group->addButton(b, i);
        m_layout->addWidget(b);
        buttons.append(b);
    }
    fm::style::setSegments(buttons);
    if (m_items.isEmpty())
        m_current = -1;
    else
        m_current = std::clamp(m_current < 0 ? 0 : m_current, 0, int(m_items.size()) - 1);
    if (QAbstractButton *b = button(m_current))
        b->setChecked(true);
    updateGeometry();
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (m_items.isEmpty()) {  // .ui에서 항목보다 먼저 올 수 있다 — 항목이 생기면 적용
        m_current = index;
        return;
    }
    if (index < -1 || index >= m_items.size())
        return;
    if (QAbstractButton *b = button(index))
        b->setChecked(true);
    if (m_current == index)
        return;
    m_current = index;
    Q_EMIT currentIndexChanged(index);
}

QAbstractButton *SegmentedControl::button(int index) const
{
    return m_group->button(index);
}

void SegmentedControl::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange && testAttribute(Qt::WA_SetStyle)) {
        const auto buttons = m_group->buttons();
        for (QAbstractButton *b : buttons)
            b->setStyle(style());
    }
    QWidget::changeEvent(event);
}

} // namespace fm::ui
