#include "fmwidgets/FlowLayout.h"

#include <QStyle>
#include <QWidget>

#include <algorithm>

namespace fm::ui {

namespace {

QSize boundedHint(const QLayoutItem *item)
{
    return item->sizeHint().expandedTo(item->minimumSize()).boundedTo(item->maximumSize());
}

/// 폭이 정해진 항목의 크기 — 폭에 따라 높이가 바뀌는 항목은 그 폭의 높이.
QSize itemSize(const QLayoutItem *item, int maxWidth)
{
    QSize s = boundedHint(item);
    if (maxWidth >= 0)
        s.setWidth(std::max(std::min(s.width(), maxWidth), item->minimumSize().width()));
    if (item->hasHeightForWidth())
        s.setHeight(item->heightForWidth(s.width()));
    return s;
}

} // namespace

FlowLayout::FlowLayout(QWidget *parent)
    : QLayout(parent)
{
}

FlowLayout::FlowLayout(int horizontalSpacing, int verticalSpacing, QWidget *parent)
    : QLayout(parent)
    , m_hSpacing(horizontalSpacing)
    , m_vSpacing(verticalSpacing)
{
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::insertWidget(int index, QWidget *widget)
{
    addChildWidget(widget);
    auto *item = new QWidgetItemV2(widget);
    if (index < 0 || index > m_items.size())
        index = int(m_items.size());
    m_items.insert(index, item);
    invalidate();
}

void FlowLayout::insertItem(int index, QLayoutItem *item)
{
    if (index < 0 || index > m_items.size())
        index = int(m_items.size());
    m_items.insert(index, item);
    invalidate();
}

int FlowLayout::horizontalSpacing() const
{
    return m_hSpacing;
}

void FlowLayout::setHorizontalSpacing(int spacing)
{
    m_hSpacing = spacing;
    invalidate();
}

int FlowLayout::verticalSpacing() const
{
    return m_vSpacing;
}

void FlowLayout::setVerticalSpacing(int spacing)
{
    m_vSpacing = spacing;
    invalidate();
}

void FlowLayout::setSpacing(int spacing)
{
    m_hSpacing = m_vSpacing = spacing;
    invalidate();
}

int FlowLayout::spacing() const
{
    return m_hSpacing == m_vSpacing ? m_hSpacing : -1;
}

void FlowLayout::setLineAlignment(Qt::Alignment alignment)
{
    m_lineAlignment = alignment;
    invalidate();
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
    invalidate();
}

int FlowLayout::count() const
{
    return int(m_items.size());
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return m_items.value(index);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    QLayoutItem *item = m_items.takeAt(index);
    invalidate();
    return item;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    if (width != m_cachedWidth) {
        m_cachedWidth = width;
        m_cachedHeight = layoutItems(QRect(0, 0, width, 0), false);
    }
    return m_cachedHeight;
}

QSize FlowLayout::minimumSize() const
{
    // min-content: 가장 넓은 항목 하나가 들어가는 폭
    QSize size;
    for (const QLayoutItem *item : m_items) {
        if (!item->isEmpty())
            size = size.expandedTo(item->minimumSize());
    }
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
    // max-content: 한 줄에 다 놓은 크기
    int width = 0;
    int height = 0;
    bool first = true;
    for (const QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        const QSize s = itemSize(item, -1);
        width += s.width() + (first ? 0 : spacingFor(item, Qt::Horizontal));
        height = std::max(height, s.height());
        first = false;
    }
    const QMargins m = contentsMargins();
    return QSize(width + m.left() + m.right(), height + m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    layoutItems(rect, true);
}

void FlowLayout::invalidate()
{
    m_cachedWidth = -1;
    QLayout::invalidate();
}

int FlowLayout::spacingFor(const QLayoutItem *item, Qt::Orientation orientation) const
{
    const int fixed = orientation == Qt::Horizontal ? m_hSpacing : m_vSpacing;
    if (fixed >= 0)
        return fixed;
    const QWidget *parent = parentWidget();
    if (parent) {
        const int s = parent->style()->pixelMetric(orientation == Qt::Horizontal ? QStyle::PM_LayoutHorizontalSpacing
                                                                                 : QStyle::PM_LayoutVerticalSpacing,
                                                   nullptr, parent);
        if (s >= 0)
            return s;
    }
    // 스타일이 위젯 종류별 간격을 쓴다(Fusion 등) — 이 항목의 종류로 묻는다
    if (const QWidget *w = item->widget()) {
        const QSizePolicy::ControlType type = w->sizePolicy().controlType();
        return std::max(0, w->style()->layoutSpacing(type, type, orientation));
    }
    return 6;
}

int FlowLayout::layoutItems(const QRect &rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.marginsRemoved(m);
    const int available = std::max(0, area.width());

    struct Entry
    {
        QLayoutItem *item;
        QSize size;
        int gap;  // 앞 항목과의 가로 간격
    };
    QList<Entry> entries;
    entries.reserve(m_items.size());
    for (QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        entries.append({item, itemSize(item, available), entries.isEmpty() ? 0 : spacingFor(item, Qt::Horizontal)});
    }
    if (entries.isEmpty())
        return m.top() + m.bottom();

    const Qt::Alignment horizontal = m_lineAlignment & Qt::AlignHorizontal_Mask;
    const Qt::Alignment vertical = m_lineAlignment & Qt::AlignVertical_Mask;
    const Qt::LayoutDirection direction = parentWidget() ? parentWidget()->layoutDirection() : Qt::LeftToRight;
    int y = area.top();
    int first = 0;
    while (first < entries.size()) {
        // 한 줄에 들어가는 만큼 — 줄의 첫 항목은 넘쳐도 놓는다
        int lineWidth = entries[first].size.width();
        int lineHeight = entries[first].size.height();
        int end = first + 1;
        while (end < entries.size() && lineWidth + entries[end].gap + entries[end].size.width() <= available) {
            lineWidth += entries[end].gap + entries[end].size.width();
            lineHeight = std::max(lineHeight, entries[end].size.height());
            ++end;
        }
        if (apply) {
            const int extra = std::max(0, available - lineWidth);
            const int n = end - first;
            const bool lastLine = end == entries.size();
            int x = area.left();
            int justifyEach = 0;
            int justifyRest = 0;
            if (horizontal & Qt::AlignHCenter)
                x += extra / 2;
            else if (horizontal & Qt::AlignRight)
                x += extra;
            else if ((horizontal & Qt::AlignJustify) && n > 1 && !lastLine) {
                justifyEach = extra / (n - 1);
                justifyRest = extra % (n - 1);
            }
            for (int i = first; i < end; ++i) {
                const Entry &e = entries[i];
                if (i > first) {
                    x += e.gap + justifyEach + (i - first <= justifyRest ? 1 : 0);
                }
                int top = y;
                if (vertical & Qt::AlignVCenter)
                    top += (lineHeight - e.size.height()) / 2;
                else if (vertical & Qt::AlignBottom)
                    top += lineHeight - e.size.height();
                const QRect logical(QPoint(x, top), e.size);
                e.item->setGeometry(QStyle::visualRect(direction, area, logical));
                x += e.size.width();
            }
        }
        y += lineHeight;
        first = end;
        if (first < entries.size())
            y += spacingFor(entries[first].item, Qt::Vertical);
    }
    return y - rect.top() + m.bottom();
}

} // namespace fm::ui
