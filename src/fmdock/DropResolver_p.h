#pragma once

// 끌어 놓기 판정 — 위젯 없이 기하만으로 놓을 자리와 그릴 표시를 계산한다(07 §4.2-4). 단위 테스트가 직접 부른다.

#include "fmdock/DockGlobals.h"

#include <fmstyle/StylePaint.h>

#include <QList>
#include <QPoint>
#include <QRect>

#include <array>

namespace fm::dock::detail {

/// SideBarLocation ↔ 배열 순번(Left · Top · Right · Bottom).
constexpr int sideIndex(SideBarLocation side) noexcept
{
    switch (side) {
    case SideBarLocation::Left: return 0;
    case SideBarLocation::Top: return 1;
    case SideBarLocation::Right: return 2;
    case SideBarLocation::Bottom: return 3;
    case SideBarLocation::None: break;
    }
    return -1;
}
constexpr SideBarLocation sideAt(int index) noexcept
{
    switch (index) {
    case 0: return SideBarLocation::Left;
    case 1: return SideBarLocation::Top;
    case 2: return SideBarLocation::Right;
    case 3: return SideBarLocation::Bottom;
    default: return SideBarLocation::None;
    }
}
constexpr DropArea dropAreaFor(SideBarLocation side) noexcept
{
    switch (side) {
    case SideBarLocation::Top: return DropArea::Top;
    case SideBarLocation::Right: return DropArea::Right;
    case SideBarLocation::Bottom: return DropArea::Bottom;
    case SideBarLocation::Left:
    case SideBarLocation::None: break;
    }
    return DropArea::Left;
}

struct DropAreaGeom
{
    QRect rect;               // 영역 전체(전역 좌표)
    QRect content;            // 내용
    QRect strip;              // 탭 줄(숨었으면 비어 있음)
    QList<QRect> tabs;        // 보이는 탭(전역), 왼쪽부터
    QList<int> tabIndexes;    // tabs[i]의 탭 순번
    bool allowCenter = true;  // 탭으로 넣기
    bool allowSides = true;   // 옆으로 나누기
    bool placeholder = false; // 빈 중앙 영역 자리
};

struct DropContainerGeom
{
    QRect rect;                             // 컨테이너(전역)
    QRect outer;                            // 사이드바 감지 기준(메인 컨테이너 = 관리자 사각형)
    bool allowEdges = true;
    bool allowSideBars = false;
    std::array<QRect, 4> sideBars;          // 보이는 사이드바(Left · Top · Right · Bottom, 없으면 비어 있음)
    std::array<QList<QRect>, 4> sideTabs;   // 사이드바의 보이는 탭
    int sideBarThickness = 28;
    QList<DropAreaGeom> areas;
};

struct DropOptions
{
    int indicator = fm::style::kDropIndicatorSize;  // 표시 단추 한 변
    int gap = 4;                                    // 십자 단추 사이
    int edgeInset = 10;                             // 가장자리 표시의 안쪽 여백
    int sideSense = 24;                             // 사이드바 놓기 감지 거리
};

struct DropTarget
{
    enum class Kind : std::uint8_t { None, ContainerEdge, AreaSide, AreaCenter, TabInsert, SideBar, CentralEmpty };
    Kind kind = Kind::None;
    int area = -1;                          // DropContainerGeom::areas 순번
    DropArea side = DropArea::Center;
    int index = -1;                         // 끼워 넣을 탭 · 사이드 탭 자리(−1 = 끝)
    SideBarLocation sideBar = SideBarLocation::None;

    bool operator==(const DropTarget &other) const = default;
};

struct DropIndicator
{
    QRect rect;
    fm::style::DropSpot spot = fm::style::DropSpot::Center;
    bool hover = false;

    bool operator==(const DropIndicator &other) const = default;
};

struct DropVisual
{
    QList<DropIndicator> indicators;
    QRect preview;   // 놓일 자리
    QRect caret;     // 탭 줄 · 사이드바 끼워 넣기 자리
    QRect sideZone;  // 사이드바 놓기 영역

    bool operator==(const DropVisual &other) const = default;
    /// 그려지는 모든 것을 덮는 사각형(전역 좌표).
    QRect bounds() const
    {
        QRect r = preview | caret | sideZone;
        for (const DropIndicator &ind : indicators)
            r |= ind.rect;
        return r;
    }
};

QRect edgeIndicatorRect(const QRect &container, DropArea side, const DropOptions &options);
QRect crossIndicatorRect(const QRect &content, DropArea side, const DropOptions &options);

/// 커서 아래 놓을 자리. visual이 있으면 그릴 표시(가장자리 · 십자 · 미리보기 · 캐럿)도 채운다.
DropTarget resolveDrop(const DropContainerGeom &geom, const QPoint &cursor, const DropOptions &options,
                       DropVisual *visual = nullptr);

} // namespace fm::dock::detail
