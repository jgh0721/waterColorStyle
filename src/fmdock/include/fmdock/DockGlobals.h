#pragma once

// fmdock 2 — 공통 선언(07-docking.md). Qt-Advanced-Docking-System의 동작을 참고해 다시 구현했다(코드는 가져오지 않음).

#include <QtGlobal>

#include <cstdint>

namespace fm::dock {

class DockArea;
class DockContainer;
class DockManager;
class DockWidget;
class FloatingWindow;

/// 놓을 자리. 대상 영역이 있으면 그 영역 옆(Left · Top · Right · Bottom)에 나누거나 탭(Center)으로 넣는다.
/// 대상이 없으면 Left ~ Bottom은 컨테이너 가장자리, Center는 중앙 영역이다.
enum class DropArea : std::uint8_t { Left, Top, Right, Bottom, Center };

/// 자동 숨김 사이드바 쪽.
enum class SideBarLocation : std::uint8_t { None, Left, Top, Right, Bottom };

/// 사이드바 · 놓을 자리가 가로 방향(좌우)으로 나누는가.
constexpr bool isHorizontal(DropArea area) noexcept { return area == DropArea::Left || area == DropArea::Right; }
constexpr bool isLeading(DropArea area) noexcept { return area == DropArea::Left || area == DropArea::Top; }

} // namespace fm::dock
