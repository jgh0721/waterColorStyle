#include "DropResolver_p.h"

#include <algorithm>
#include <limits>

namespace fm::dock::detail {

namespace {

using fm::style::DropSpot;

/// 가장자리에서 이만큼 안이면 사이드바 놓기가 다른 표시보다 먼저다.
constexpr int kSideFirst = 8;

DropSpot outerSpot(DropArea side)
{
    switch (side) {
    case DropArea::Left: return DropSpot::OuterLeft;
    case DropArea::Top: return DropSpot::OuterTop;
    case DropArea::Right: return DropSpot::OuterRight;
    case DropArea::Bottom: return DropSpot::OuterBottom;
    case DropArea::Center: break;
    }
    return DropSpot::Center;
}

DropSpot crossSpot(DropArea side)
{
    switch (side) {
    case DropArea::Left: return DropSpot::Left;
    case DropArea::Top: return DropSpot::Top;
    case DropArea::Right: return DropSpot::Right;
    case DropArea::Bottom: return DropSpot::Bottom;
    case DropArea::Center: break;
    }
    return DropSpot::Center;
}

/// 사각형의 side 쪽 몫(fraction).
QRect sideSlice(const QRect &r, DropArea side, double fraction)
{
    const int w = std::max(1, int(r.width() * fraction));
    const int h = std::max(1, int(r.height() * fraction));
    switch (side) {
    case DropArea::Left: return QRect(r.left(), r.top(), w, r.height());
    case DropArea::Right: return QRect(r.right() - w + 1, r.top(), w, r.height());
    case DropArea::Top: return QRect(r.left(), r.top(), r.width(), h);
    case DropArea::Bottom: return QRect(r.left(), r.bottom() - h + 1, r.width(), h);
    case DropArea::Center: break;
    }
    return r;
}

constexpr DropArea kSides[] = {DropArea::Left, DropArea::Top, DropArea::Right, DropArea::Bottom};

/// 사이드바 놓기 — 사이드바(없으면 가장자리 띠)와 사이드 탭 사이 캐럿.
DropTarget sideBarTarget(const DropContainerGeom &g, const QPoint &cursor, int side, DropVisual &v)
{
    const bool vertical = side == 0 || side == 2;  // 왼쪽 · 오른쪽 사이드바는 세로
    QRect zone = g.sideBars[side];
    if (!zone.isValid()) {
        const int t = g.sideBarThickness;
        const QRect r = g.outer;
        zone = side == 0 ? QRect(r.left(), r.top(), t, r.height())
             : side == 1 ? QRect(r.left(), r.top(), r.width(), t)
             : side == 2 ? QRect(r.right() - t + 1, r.top(), t, r.height())
                         : QRect(r.left(), r.bottom() - t + 1, r.width(), t);
    }
    const QList<QRect> &tabs = g.sideTabs[side];
    int index = 0;
    for (const QRect &tab : tabs) {
        if ((vertical ? tab.center().y() : tab.center().x()) < (vertical ? cursor.y() : cursor.x()))
            ++index;
    }
    QRect caret;
    if (vertical) {
        const int y = tabs.isEmpty() ? zone.top() + 4 : index < tabs.size() ? tabs[index].top() - 2 : tabs.last().bottom() + 1;
        caret = QRect(zone.left() + 3, y, std::max(1, zone.width() - 6), 2);
    } else {
        const int x = tabs.isEmpty() ? zone.left() + 4 : index < tabs.size() ? tabs[index].left() - 2 : tabs.last().right() + 1;
        caret = QRect(x, zone.top() + 3, 2, std::max(1, zone.height() - 6));
    }
    v = DropVisual{};
    v.sideZone = zone;
    v.caret = caret;
    v.preview = zone;
    DropTarget target;
    target.kind = DropTarget::Kind::SideBar;
    target.sideBar = sideAt(side);
    target.index = index;
    return target;
}

} // namespace

QRect edgeIndicatorRect(const QRect &c, DropArea side, const DropOptions &o)
{
    const int s = o.indicator;
    switch (side) {
    case DropArea::Left: return QRect(c.left() + o.edgeInset, c.center().y() - s / 2, s, s);
    case DropArea::Right: return QRect(c.right() - o.edgeInset - s + 1, c.center().y() - s / 2, s, s);
    case DropArea::Top: return QRect(c.center().x() - s / 2, c.top() + o.edgeInset, s, s);
    case DropArea::Bottom: return QRect(c.center().x() - s / 2, c.bottom() - o.edgeInset - s + 1, s, s);
    case DropArea::Center: break;
    }
    return QRect(c.center().x() - s / 2, c.center().y() - s / 2, s, s);
}

QRect crossIndicatorRect(const QRect &content, DropArea side, const DropOptions &o)
{
    const int s = o.indicator;
    const QPoint c = content.center();
    const QRect center(c.x() - s / 2, c.y() - s / 2, s, s);
    switch (side) {
    case DropArea::Left: return center.translated(-(s + o.gap), 0);
    case DropArea::Right: return center.translated(s + o.gap, 0);
    case DropArea::Top: return center.translated(0, -(s + o.gap));
    case DropArea::Bottom: return center.translated(0, s + o.gap);
    case DropArea::Center: break;
    }
    return center;
}

DropTarget resolveDrop(const DropContainerGeom &g, const QPoint &cursor, const DropOptions &o, DropVisual *visual)
{
    DropVisual local;
    DropVisual &v = visual ? *visual : local;
    v = DropVisual{};
    DropTarget target;

    // 사이드바 놓기 후보 — 관리자 바깥 가장자리 sideSense 안
    int side = -1;
    int sideDistance = std::numeric_limits<int>::max();
    if (g.allowSideBars && g.outer.contains(cursor)) {
        const int distances[4] = {cursor.x() - g.outer.left(), cursor.y() - g.outer.top(), g.outer.right() - cursor.x(),
                                  g.outer.bottom() - cursor.y()};
        for (int i = 0; i < 4; ++i) {
            if (distances[i] < o.sideSense && distances[i] < sideDistance) {
                side = i;
                sideDistance = distances[i];
            }
        }
    }
    // 가장자리에 붙어 있으면 무엇보다 먼저
    if (side >= 0 && sideDistance < kSideFirst)
        return sideBarTarget(g, cursor, side, v);

    // 1) 컨테이너 가장자리 표시
    if (g.allowEdges && g.rect.isValid()) {
        for (DropArea edge : kSides) {
            const QRect r = edgeIndicatorRect(g.rect, edge, o);
            const bool hover = target.kind == DropTarget::Kind::None && r.contains(cursor);
            v.indicators.append({r, outerSpot(edge), hover});
            if (hover) {
                target.kind = DropTarget::Kind::ContainerEdge;
                target.side = edge;
                v.preview = sideSlice(g.rect, edge, 1.0 / 3.0);
            }
        }
    }

    // 2) 마우스 아래 영역
    int areaIndex = -1;
    for (int i = 0; i < g.areas.size(); ++i) {
        if (g.areas[i].rect.contains(cursor)) {
            areaIndex = i;
            break;
        }
    }
    if (areaIndex >= 0) {
        const DropAreaGeom &a = g.areas[areaIndex];
        if (a.placeholder) {
            const QRect r = crossIndicatorRect(a.content, DropArea::Center, o);
            v.indicators.append({r, DropSpot::Center, target.kind == DropTarget::Kind::None && r.contains(cursor)});
            if (target.kind == DropTarget::Kind::None && a.content.contains(cursor) && side < 0) {
                target.kind = DropTarget::Kind::CentralEmpty;
                target.area = areaIndex;
                v.preview = a.content;
            }
        } else {
            // 십자 — 위 · 왼쪽 · 가운데 · 오른쪽 · 아래
            static constexpr DropArea kCross[] = {DropArea::Top, DropArea::Left, DropArea::Center, DropArea::Right,
                                                  DropArea::Bottom};
            const QRect content = a.content.isValid() ? a.content : a.rect;
            for (DropArea cross : kCross) {
                const bool allowed = cross == DropArea::Center ? a.allowCenter : a.allowSides;
                if (!allowed)
                    continue;
                const QRect r = crossIndicatorRect(content, cross, o);
                const bool hover = target.kind == DropTarget::Kind::None && r.contains(cursor);
                v.indicators.append({r, crossSpot(cross), hover});
                if (hover) {
                    target.kind = cross == DropArea::Center ? DropTarget::Kind::AreaCenter : DropTarget::Kind::AreaSide;
                    target.area = areaIndex;
                    target.side = cross;
                    v.preview = cross == DropArea::Center ? content : sideSlice(a.rect, cross, 0.5);
                }
            }
            // 탭 줄 위 — 그 자리에 끼워 넣는다
            if (target.kind == DropTarget::Kind::None && a.allowCenter && a.strip.isValid() && a.strip.contains(cursor)) {
                int slot = 0;
                for (const QRect &tab : a.tabs) {
                    if (tab.center().x() < cursor.x())
                        ++slot;
                }
                target.kind = DropTarget::Kind::TabInsert;
                target.area = areaIndex;
                target.index = slot < a.tabIndexes.size() ? a.tabIndexes[slot]
                             : a.tabIndexes.isEmpty()     ? 0
                                                          : a.tabIndexes.last() + 1;
                const int x = a.tabs.isEmpty() ? a.strip.left() + 2
                            : slot < a.tabs.size() ? a.tabs[slot].left() - 1 : a.tabs.last().right() + 1;
                v.caret = QRect(x, a.strip.top() + 3, 2, std::max(1, a.strip.height() - 3));
                v.preview = content;
            }
        }
    }

    // 3) 다른 자리가 없으면 사이드바
    if (target.kind == DropTarget::Kind::None && side >= 0)
        return sideBarTarget(g, cursor, side, v);
    return target;
}

} // namespace fm::dock::detail
