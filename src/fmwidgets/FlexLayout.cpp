#include "fmwidgets/FlexLayout.h"

#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace fm::ui {

namespace {

/// 한 항목의 배치 값 — CSS flexbox 알고리즘의 중간 값들.
struct Flex
{
    QLayoutItem *item = nullptr;
    qreal grow = 0;
    qreal shrink = 1;
    FlexLayout::Align align = FlexLayout::Align::Stretch;
    int minMain = 0;
    int maxMain = 0;
    int minCross = 0;
    int maxCross = 0;
    qreal base = 0;   // flex base size
    qreal hypo = 0;   // 최소 · 최대로 제한한 기준 크기(hypothetical main size)
    qreal size = 0;   // 나눈 뒤의 주 축 크기
    int cross = 0;    // 교차 축 크기(정렬 전)
    bool frozen = false;
    int violation = 0;  // 이번 판에서 최소(+1) · 최대(-1)에 걸렸는지
};

/// CSS 9.7 유연 길이 해석 — 남는(모자란) 길이를 grow(shrink × 기준)로 나누고, 최소 · 최대를 어긴 항목을 고정해 되풀이.
void resolveFlexibleLengths(QList<Flex> &line, qreal available, qreal gaps)
{
    qreal sumHypo = 0;
    for (const Flex &f : line)
        sumHypo += f.hypo;
    for (Flex &f : line)
        f.size = f.hypo;
    const bool growing = sumHypo + gaps < available;
    const bool shrinking = sumHypo + gaps > available;
    if (!growing && !shrinking)
        return;

    for (Flex &f : line) {
        const qreal factor = growing ? f.grow : f.shrink;
        f.frozen = factor <= 0 || (growing && f.base > f.hypo) || (shrinking && f.base < f.hypo);
    }
    qreal initialFree = available - gaps;
    for (const Flex &f : line)
        initialFree -= f.frozen ? f.size : f.base;

    for (int guard = 0; guard <= int(line.size()); ++guard) {
        qreal frozenSum = 0;
        qreal unfrozenBase = 0;
        qreal factorSum = 0;
        qreal scaledSum = 0;
        int unfrozen = 0;
        for (const Flex &f : line) {
            if (f.frozen) {
                frozenSum += f.size;
            } else {
                unfrozenBase += f.base;
                factorSum += growing ? f.grow : f.shrink;
                scaledSum += f.shrink * f.base;
                ++unfrozen;
            }
        }
        if (unfrozen == 0)
            break;
        qreal remaining = available - gaps - frozenSum - unfrozenBase;
        // 인자 합이 1보다 작으면 남는 길이의 그 비율만 나눈다(flex: 0.5 — 반만 늘어남)
        if (factorSum < 1) {
            const qreal limited = initialFree * factorSum;
            if (std::abs(limited) < std::abs(remaining))
                remaining = limited;
        }
        qreal violation = 0;
        for (Flex &f : line) {
            if (f.frozen)
                continue;
            qreal target = f.base;
            if (growing && factorSum > 0)
                target += remaining * f.grow / factorSum;
            else if (shrinking && scaledSum > 0)
                target += remaining * (f.shrink * f.base) / scaledSum;
            const qreal clamped = std::clamp(target, qreal(f.minMain), qreal(f.maxMain));
            violation += clamped - target;
            f.size = clamped;
            f.violation = clamped > target ? 1 : clamped < target ? -1 : 0;
        }
        if (std::abs(violation) < 0.5) {
            for (Flex &f : line)
                f.frozen = true;
            break;
        }
        // 합이 양수면 최소에 걸린 항목, 음수면 최대에 걸린 항목을 고정하고 다시 나눈다
        for (Flex &f : line) {
            if (!f.frozen && ((violation > 0 && f.violation > 0) || (violation < 0 && f.violation < 0)))
                f.frozen = true;
        }
    }
}

/// 남는 길이 free를 n개 사이에 나누는 시작 위치와 간격 더하기(justify-content · align-content 공통).
void distribute(int mode, qreal free, int n, qreal *start, qreal *extraBetween)
{
    using J = FlexLayout::Justify;
    *start = 0;
    *extraBetween = 0;
    switch (J(mode)) {
    case J::Start:
        break;
    case J::End:
        *start = free;
        break;
    case J::Center:
        *start = free / 2;
        break;
    case J::SpaceBetween:
        if (n > 1 && free > 0)
            *extraBetween = free / (n - 1);
        break;
    case J::SpaceAround:
        if (free > 0) {
            *extraBetween = free / n;
            *start = free / (2 * n);
        } else {
            *start = free / 2;
        }
        break;
    case J::SpaceEvenly:
        if (free > 0) {
            *extraBetween = free / (n + 1);
            *start = free / (n + 1);
        } else {
            *start = free / 2;
        }
        break;
    }
}

} // namespace

FlexLayout::FlexLayout(QWidget *parent)
    : QLayout(parent)
{
}

FlexLayout::FlexLayout(Direction direction, QWidget *parent)
    : QLayout(parent)
    , m_direction(direction)
{
}

FlexLayout::~FlexLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlexLayout::setDirection(Direction direction)
{
    m_direction = direction;
    invalidate();
}

void FlexLayout::setWrap(Wrap wrap)
{
    m_wrap = wrap;
    invalidate();
}

void FlexLayout::setJustifyContent(Justify justify)
{
    m_justify = justify;
    invalidate();
}

void FlexLayout::setAlignItems(Align value)
{
    m_alignItems = value == Align::Auto ? Align::Stretch : value;
    invalidate();
}

void FlexLayout::setAlignContent(AlignContent value)
{
    m_alignContent = value;
    invalidate();
}

void FlexLayout::setRowGap(int gap)
{
    m_rowGap = gap;
    invalidate();
}

void FlexLayout::setColumnGap(int gap)
{
    m_columnGap = gap;
    invalidate();
}

void FlexLayout::setGap(int gap)
{
    m_rowGap = m_columnGap = gap;
    invalidate();
}

int FlexLayout::spacing() const
{
    return m_rowGap == m_columnGap ? m_rowGap : -1;
}

void FlexLayout::addWidget(QWidget *widget, const Item &item)
{
    insertWidget(-1, widget, item);
}

void FlexLayout::addWidget(QWidget *widget, qreal grow, qreal shrink, int basis)
{
    Item item;
    item.grow = grow;
    item.shrink = shrink;
    item.basis = basis;
    insertWidget(-1, widget, item);
}

void FlexLayout::insertWidget(int index, QWidget *widget, const Item &item)
{
    addChildWidget(widget);
    if (index < 0 || index > m_entries.size())
        index = int(m_entries.size());
    m_entries.insert(index, Entry{new QWidgetItemV2(widget), item});
    invalidate();
}

FlexLayout::Entry *FlexLayout::entryFor(const QWidget *widget)
{
    for (Entry &e : m_entries) {
        if (e.item->widget() == widget)
            return &e;
    }
    return nullptr;
}

FlexLayout::Item FlexLayout::flexItem(const QWidget *widget) const
{
    for (const Entry &e : m_entries) {
        if (e.item->widget() == widget)
            return e.flex;
    }
    return {};
}

bool FlexLayout::setFlexItem(const QWidget *widget, const Item &item)
{
    Entry *e = entryFor(widget);
    if (!e)
        return false;
    if (!(e->flex == item)) {
        e->flex = item;
        invalidate();
    }
    return true;
}

bool FlexLayout::setGrow(const QWidget *widget, qreal grow)
{
    Item item = flexItem(widget);
    item.grow = grow;
    return setFlexItem(widget, item);
}

bool FlexLayout::setShrink(const QWidget *widget, qreal shrink)
{
    Item item = flexItem(widget);
    item.shrink = shrink;
    return setFlexItem(widget, item);
}

bool FlexLayout::setBasis(const QWidget *widget, int basis)
{
    Item item = flexItem(widget);
    item.basis = basis;
    return setFlexItem(widget, item);
}

bool FlexLayout::setAlignSelf(const QWidget *widget, Align value)
{
    Item item = flexItem(widget);
    item.alignSelf = value;
    return setFlexItem(widget, item);
}

bool FlexLayout::setOrder(const QWidget *widget, int order)
{
    Item item = flexItem(widget);
    item.order = order;
    return setFlexItem(widget, item);
}

void FlexLayout::addItem(QLayoutItem *item)
{
    m_entries.append(Entry{item, Item{}});
    invalidate();
}

int FlexLayout::count() const
{
    return int(m_entries.size());
}

QLayoutItem *FlexLayout::itemAt(int index) const
{
    return index >= 0 && index < m_entries.size() ? m_entries.at(index).item : nullptr;
}

QLayoutItem *FlexLayout::takeAt(int index)
{
    if (index < 0 || index >= m_entries.size())
        return nullptr;
    QLayoutItem *item = m_entries.takeAt(index).item;
    invalidate();
    return item;
}

Qt::Orientations FlexLayout::expandingDirections() const
{
    const Qt::Orientation mainAxis = horizontal() ? Qt::Horizontal : Qt::Vertical;
    const Qt::Orientation crossAxis = horizontal() ? Qt::Vertical : Qt::Horizontal;
    Qt::Orientations result;
    for (const Entry &e : m_entries) {
        if (e.item->isEmpty())
            continue;
        if (e.flex.grow > 0)
            result |= mainAxis;
        const Align self = e.flex.alignSelf == Align::Auto ? m_alignItems : e.flex.alignSelf;
        if (self == Align::Stretch && (e.item->expandingDirections() & crossAxis))
            result |= crossAxis;
    }
    return result;
}

bool FlexLayout::hasHeightForWidth() const
{
    if (horizontal() && m_wrap != Wrap::NoWrap)
        return true;
    return std::any_of(m_entries.cbegin(), m_entries.cend(),
                       [](const Entry &e) { return !e.item->isEmpty() && e.item->hasHeightForWidth(); });
}

int FlexLayout::heightForWidth(int width) const
{
    if (width == m_cachedWidth)
        return m_cachedHeight;
    const QMargins m = contentsMargins();
    const int content = std::max(0, width - m.left() - m.right());
    int height = 0;
    if (horizontal()) {
        height = arrange(QRect(0, 0, content, 0), -1, nullptr);
    } else {
        // 세로 방향: 폭이 정해지면 높이가 바뀌는 항목의 높이를 다시 구한다 — 줄 바꿈은 없다(높이가 정해지지 않음)
        QList<Placement> placements;
        arrange(QRect(0, 0, content, QWIDGETSIZE_MAX), content, &placements);
        for (const Placement &p : std::as_const(placements))
            height = std::max(height, p.rect.bottom() + 1);
    }
    m_cachedWidth = width;
    m_cachedHeight = height + m.top() + m.bottom();
    return m_cachedHeight;
}

QSize FlexLayout::minimumSize() const
{
    const bool row = horizontal();
    const bool wraps = m_wrap != Wrap::NoWrap;
    int main = 0;
    int cross = 0;
    int visible = 0;
    for (const Entry &e : m_entries) {
        if (e.item->isEmpty())
            continue;
        const QSize min = e.item->minimumSize();
        const QSize hint = e.item->sizeHint().expandedTo(min).boundedTo(e.item->maximumSize());
        // 줄일 수 없는 항목(shrink 0)은 기준 크기 아래로 내려가지 않는다
        const int itemMain = e.flex.shrink > 0 ? (row ? min.width() : min.height())
                                                : (e.flex.basis >= 0 ? e.flex.basis : (row ? hint.width() : hint.height()));
        main = wraps ? std::max(main, itemMain) : main + itemMain;
        cross = std::max(cross, row ? min.height() : min.width());
        ++visible;
    }
    if (!wraps && visible > 1)
        main += mainGap() * (visible - 1);
    const QMargins m = contentsMargins();
    const QSize size = row ? QSize(main, cross) : QSize(cross, main);
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlexLayout::sizeHint() const
{
    // 줄 바꿈 없이 한 줄(max-content) — 교차 축은 가장 큰 항목
    const bool row = horizontal();
    int main = 0;
    int cross = 0;
    int visible = 0;
    for (const Entry &e : m_entries) {
        if (e.item->isEmpty())
            continue;
        const QSize hint = e.item->sizeHint().expandedTo(e.item->minimumSize()).boundedTo(e.item->maximumSize());
        int itemMain = e.flex.basis >= 0 ? e.flex.basis : (row ? hint.width() : hint.height());
        itemMain = std::clamp(itemMain, row ? e.item->minimumSize().width() : e.item->minimumSize().height(),
                              std::max(row ? e.item->maximumSize().width() : e.item->maximumSize().height(),
                                       row ? e.item->minimumSize().width() : e.item->minimumSize().height()));
        int itemCross = row ? hint.height() : hint.width();
        if (row && e.item->hasHeightForWidth())
            itemCross = e.item->heightForWidth(itemMain);
        main += itemMain;
        cross = std::max(cross, itemCross);
        ++visible;
    }
    if (visible > 1)
        main += mainGap() * (visible - 1);
    const QMargins m = contentsMargins();
    const QSize size = row ? QSize(main, cross) : QSize(cross, main);
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

void FlexLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    const QRect area = rect.marginsRemoved(contentsMargins());
    QList<Placement> placements;
    arrange(area, horizontal() ? area.height() : area.width(), &placements);
    for (const Placement &p : std::as_const(placements))
        p.item->setGeometry(p.rect);
}

void FlexLayout::invalidate()
{
    m_cachedWidth = -1;
    QLayout::invalidate();
}

int FlexLayout::resolvedGap(int gap, Qt::Orientation orientation) const
{
    if (gap >= 0)
        return gap;
    if (const QWidget *parent = parentWidget()) {
        const int s = parent->style()->pixelMetric(orientation == Qt::Horizontal ? QStyle::PM_LayoutHorizontalSpacing
                                                                                 : QStyle::PM_LayoutVerticalSpacing,
                                                   nullptr, parent);
        if (s >= 0)
            return s;
        return std::max(0, parent->style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, orientation));
    }
    return 6;
}

int FlexLayout::mainGap() const
{
    return horizontal() ? resolvedGap(m_columnGap, Qt::Horizontal) : resolvedGap(m_rowGap, Qt::Vertical);
}

int FlexLayout::crossGap() const
{
    return horizontal() ? resolvedGap(m_rowGap, Qt::Vertical) : resolvedGap(m_columnGap, Qt::Horizontal);
}

int FlexLayout::arrange(const QRect &area, int availableCross, QList<Placement> *out) const
{
    const bool row = horizontal();
    // 세로 방향에서 높이를 구할 때(heightForWidth)는 주 축이 정해지지 않았다 — QWIDGETSIZE_MAX로 표시
    const bool definiteMain = row || area.height() < QWIDGETSIZE_MAX;
    const qreal availableMain = row ? area.width() : (definiteMain ? area.height() : 0);
    const int gapMain = mainGap();
    const int gapCross = crossGap();
    const auto mainOf = [row](const QSize &s) { return row ? s.width() : s.height(); };
    const auto crossOf = [row](const QSize &s) { return row ? s.height() : s.width(); };

    // 순서(order, 같으면 넣은 순서)대로 보이는 항목
    QList<const Entry *> entries;
    for (const Entry &e : m_entries) {
        if (!e.item->isEmpty())
            entries.append(&e);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry *a, const Entry *b) { return a->flex.order < b->flex.order; });
    if (entries.isEmpty())
        return 0;

    QList<Flex> items;
    items.reserve(entries.size());
    for (const Entry *e : std::as_const(entries)) {
        QLayoutItem *it = e->item;
        const QSize min = it->minimumSize();
        const QSize max = it->maximumSize();
        const QSize hint = it->sizeHint();
        Flex f;
        f.item = it;
        f.grow = std::max<qreal>(0, e->flex.grow);
        f.shrink = std::max<qreal>(0, e->flex.shrink);
        f.align = e->flex.alignSelf == Align::Auto ? m_alignItems : e->flex.alignSelf;
        f.minMain = mainOf(min);
        f.maxMain = std::max(mainOf(max), f.minMain);
        f.minCross = crossOf(min);
        f.maxCross = std::max(crossOf(max), f.minCross);
        int base = e->flex.basis >= 0 ? e->flex.basis : mainOf(hint);
        if (!row && e->flex.basis < 0 && it->hasHeightForWidth()) {
            // 세로 방향 · 폭에 따라 높이가 바뀌는 항목: 놓일 폭의 높이가 기준
            const int width = f.align == Align::Stretch && availableCross >= 0
                                  ? std::clamp(availableCross, f.minCross, f.maxCross)
                                  : std::clamp(crossOf(hint), f.minCross, f.maxCross);
            base = it->heightForWidth(width);
        }
        f.base = base;
        f.hypo = std::clamp(f.base, qreal(f.minMain), qreal(f.maxMain));
        items.append(f);
    }

    // 줄 나누기 — 바깥 기준 크기(hypothetical)로. 줄의 첫 항목은 넘쳐도 놓는다.
    const bool wraps = m_wrap != Wrap::NoWrap && definiteMain;
    QList<QList<Flex>> lines;
    {
        QList<Flex> line;
        qreal sum = 0;
        for (const Flex &f : std::as_const(items)) {
            const qreal add = (line.isEmpty() ? 0 : gapMain) + f.hypo;
            if (wraps && !line.isEmpty() && sum + add > availableMain + 0.01) {
                lines.append(line);
                line.clear();
                sum = f.hypo;
            } else {
                sum += add;
            }
            line.append(f);
        }
        lines.append(line);
    }

    // 주 축 크기 · 교차 축 크기
    QList<qreal> lineCross;
    for (QList<Flex> &line : lines) {
        const qreal gaps = qreal(gapMain) * (line.size() - 1);
        if (definiteMain)
            resolveFlexibleLengths(line, availableMain, gaps);
        else
            for (Flex &f : line)
                f.size = f.hypo;
        qreal maxCross = 0;
        for (Flex &f : line) {
            const QSize hint = f.item->sizeHint();
            int cross = crossOf(hint);
            if (row && f.item->hasHeightForWidth())
                cross = f.item->heightForWidth(int(std::lround(f.size)));
            f.cross = std::clamp(cross, f.minCross, f.maxCross);
            maxCross = std::max(maxCross, qreal(f.cross));
        }
        lineCross.append(maxCross);
    }
    // 한 줄 상자는 교차 축 전체가 줄(CSS single-line)
    if (m_wrap == Wrap::NoWrap && availableCross >= 0)
        lineCross[0] = availableCross;

    const int lineCount = int(lines.size());
    qreal natural = qreal(gapCross) * (lineCount - 1);
    for (const qreal c : std::as_const(lineCross))
        natural += c;

    // 줄들의 교차 축 위치(align-content)
    qreal crossStart = 0;
    qreal crossBetween = gapCross;
    if (m_wrap != Wrap::NoWrap && availableCross >= 0) {
        const qreal extra = availableCross - natural;
        if (m_alignContent == AlignContent::Stretch) {
            if (extra > 0) {
                for (qreal &c : lineCross)
                    c += extra / lineCount;
            }
        } else {
            qreal more = 0;
            distribute(int(m_alignContent), extra, lineCount, &crossStart, &more);
            crossBetween += more;
        }
    }
    const qreal crossExtent = availableCross >= 0 ? availableCross : natural;

    if (out) {
        const Qt::LayoutDirection direction = parentWidget() ? parentWidget()->layoutDirection() : Qt::LeftToRight;
        const bool reverseMain = m_direction == Direction::RowReverse || m_direction == Direction::ColumnReverse;
        qreal linePos = crossStart;
        for (int l = 0; l < lineCount; ++l) {
            const QList<Flex> &line = lines.at(l);
            const qreal lc = lineCross.at(l);
            const qreal lineStart = m_wrap == Wrap::WrapReverse ? crossExtent - linePos - lc : linePos;
            qreal used = qreal(gapMain) * (line.size() - 1);
            for (const Flex &f : line)
                used += f.size;
            const qreal mainExtent = definiteMain ? availableMain : used;
            qreal mainStart = 0;
            qreal mainMore = 0;
            distribute(int(m_justify), mainExtent - used, int(line.size()), &mainStart, &mainMore);
            qreal pos = mainStart;
            for (const Flex &f : line) {
                qreal crossSize = f.cross;
                qreal crossPos = lineStart;
                switch (f.align) {
                case Align::Auto:
                case Align::Stretch:
                    crossSize = std::clamp(lc, qreal(f.minCross), qreal(f.maxCross));
                    break;
                case Align::Start:
                    break;
                case Align::End:
                    crossPos = lineStart + lc - crossSize;
                    break;
                case Align::Center:
                    crossPos = lineStart + (lc - crossSize) / 2;
                    break;
                }
                qreal mainPos = reverseMain ? mainExtent - pos - f.size : pos;
                const int m0 = int(std::lround(mainPos));
                const int m1 = int(std::lround(mainPos + f.size));
                const int c0 = int(std::lround(crossPos));
                const int c1 = int(std::lround(crossPos + crossSize));
                QRect r = row ? QRect(area.left() + m0, area.top() + c0, m1 - m0, c1 - c0)
                              : QRect(area.left() + c0, area.top() + m0, c1 - c0, m1 - m0);
                out->append({f.item, QStyle::visualRect(direction, area, r)});
                pos += f.size + gapMain + mainMore;
            }
            linePos += lc + crossBetween;
        }
    }
    return int(std::ceil(natural));
}

} // namespace fm::ui
