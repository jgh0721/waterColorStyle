#pragma once

// FmStyle · WatercolorStyle이 같이 쓰는 Qt 표준 위젯 도우미 — 테마 색 표준 아이콘, 창 단추 모양,
// 탭 방향 변환, 슬라이더 영역, 다이얼 각도, 달력 정리. 모양(선 · 입체)은 각 스타일이 정한다.

#include <QIcon>
#include <QRect>
#include <QStyle>
#include <QTabBar>
#include <QTransform>

#include <cstdint>

class QColor;
class QPainter;
class QStyleOptionSlider;
class QWidget;

namespace fm::style::detail {

// ------------------------------------------------------------------------------------- 표준 아이콘

/// 창 단추 · 화살표 모양. 시안1은 둥근 끝 선, 시안2는 굵은 선과 채운 삼각형(XP 방식).
enum class ChromeGlyph : std::uint8_t {
    Close,
    Minimize,
    Maximize,
    Restore,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    ChevronDown,
    Extension,      // » 넘친 가로 도구 모음 · 메뉴 막대
    ExtensionDown,  // 세로 도구 모음
    Refresh,
};

/// rect 가운데 정사각형(한 변 16을 기준으로 늘이고 줄인다)에 그린다.
void paintChromeGlyph(QPainter *painter, ChromeGlyph glyph, const QRectF &rect, const QColor &color,
                      bool watercolor);

/// 테마 색으로 그리는 표준 아이콘 — 화살표 · 제목 표시줄 · 도크 · 탭 닫기 · 입력 지우기 · 도구 모음 확장 ·
/// 새로 고침. 색과 디자인은 그릴 때마다 위젯의 스타일 · 범위(ThemeScope) · 지금 변형에서 읽으므로 다크와
/// 실행 중 전환을 따른다. 다루지 않는 아이콘이면 null. watercolor는 위젯이 없을 때 쓸 디자인.
QIcon themedStandardIcon(QStyle::StandardPixmap pixmap, const QWidget *widget, bool watercolor);

// ------------------------------------------------------------------------------------- 탭 방향

enum class TabSide : std::uint8_t { North, South, West, East };

TabSide tabSide(QTabBar::Shape shape) noexcept;
inline bool isVerticalTab(TabSide side) noexcept { return side == TabSide::West || side == TabSide::East; }

/// 위쪽(North) 탭 좌표 — 가로가 탭 줄 방향, 세로 0이 바깥(내용과 먼 쪽) — 를 실제 탭 좌표로 옮기는 변환.
/// 위쪽 기준 사각형은 원점에서 시작하고 크기는 tabLocalSize().
QTransform tabTransform(TabSide side, const QRect &rect) noexcept;
QSize tabLocalSize(TabSide side, const QRect &rect) noexcept;
/// 위쪽 기준 여백(바깥 · 안쪽)을 실제 방향 사각형에 적용한다 — 탭 글자 자리 맞춤용.
QRect adjustTabRect(TabSide side, const QRect &rect, int outer, int inner) noexcept;
/// 탭 줄 밑선(탭과 내용 사이) 한 줄.
QRect tabBaseLine(TabSide side, const QRect &rect) noexcept;

// ------------------------------------------------------------------------------------- 슬라이더 · 다이얼

/// 슬라이더 눈금 자리(한쪽). QSlider::sizeHint가 눈금 한쪽마다 5를 더한다.
inline constexpr int kSliderTickSpace = 5;

/// 손잡이 · 홈 · 눈금 영역. 손잡이는 눈금 자리를 뺀 띠의 가운데. length · thickness는 손잡이 크기.
QRect sliderSubControlRect(const QStyleOptionSlider *slider, QStyle::SubControl sc, int length, int thickness);

/// 눈금 값 위치(손잡이 가운데 기준, 슬라이더 rect 안 좌표). 눈금이 너무 많으면 빈 목록.
QList<int> sliderTickPositions(const QStyleOptionSlider *slider, int handleLength);

/// QDial의 값 각도(라디안, 3시 방향에서 시계 반대 방향). Qt 공통 스타일과 같은 계산.
qreal dialAngle(const QStyleOptionSlider *dial);
/// 다이얼 눈금 — 각도와 큰 눈금 여부.
struct DialNotch
{
    qreal angle;
    bool major;
};
QList<DialNotch> dialNotches(const QStyleOptionSlider *dial);

// ------------------------------------------------------------------------------------- 항목 보기

/// 항목 보기의 보기 영역(viewport)에 Hover 이벤트를 켠다 — 그래야 Qt가 항목에 State_MouseOver를 넘긴다.
/// 앱이 이미 켠 경우는 건드리지 않고, unpolish는 스타일이 켠 것만 끈다.
void polishItemView(QWidget *widget);
void unpolishItemView(QWidget *widget);

// ------------------------------------------------------------------------------------- 달력

/// QCalendarWidget — 내비게이션 줄을 강조색 띠 대신 날짜 칸 바탕으로, Qt 기본 주말 빨간 글자를 일반 글자로.
/// 앱이 따로 정한 주말 서식은 건드리지 않는다. unpolish가 되돌린다.
void polishCalendarPart(QWidget *widget);
void unpolishCalendarPart(QWidget *widget);

} // namespace fm::style::detail
