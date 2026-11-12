#pragma once

// 스타일(FmStyle · WatercolorStyle)과 fm::ui 위젯이 같이 쓰는 그리기 도우미 — 글꼴, 키 칩, 태그,
// 배지 · 배너 바탕. 치수는 캔버스 목업의 CSS 값이고, 시안2(워터컬러)는 모서리가 모두 0이다.

#include "fmstyle/ThemeColors.h"

#include <QFont>
#include <QIcon>
#include <QRectF>
#include <QSize>
#include <QString>

#include <cstdint>

class QPainter;
class QWidget;

namespace fm::style {

// ------------------------------------------------------------------------------------- 글꼴

/// base 글꼴을 소수 px 크기로(12.5 px 등). 배율이 달라도 같은 논리 크기로 보이도록 픽셀 크기가 아닌
/// 포인트 크기(96 dpi 기준 px × 0.75)를 쓴다.
QFont pixelFont(const QFont &base, qreal px, QFont::Weight weight = QFont::Normal);

/// 목업의 mono 글꼴(Cascadia Mono → JetBrains Mono → Consolas).
QFont monoFont(qreal px, QFont::Weight weight = QFont::Normal);

/// 숫자 너비를 고정(OpenType tnum)한다 — 진행 창 통계 · 크기 열.
QFont withTabularNumbers(QFont font);

// ------------------------------------------------------------------------------------- 색조

/// 배지 · 배너 · 태그의 색조.
enum class Tone : std::uint8_t { Info, Ok, Warn, Danger, Mute, Bad };

struct ToneColors
{
    QColor background;  // 투명일 수 있다
    QColor foreground;  // 글자 · 아이콘
    QColor border;      // 투명이면 테두리 없음
};

/// 태그 · 배너 색. Bad는 바탕 없이 위험 글자(다중 이름 변경 표의 '잘못된 이름').
ToneColors toneColors(Tone tone, const ThemeColors &colors);

/// 시안2는 모서리 0 (캔버스 watercolor.css: .fm * { border-radius: 0 }).
inline bool squareCorners(const ThemeColors &colors) { return colors.isWatercolor(); }

// ------------------------------------------------------------------------------------- 키 칩

enum class KeyChipLook : std::uint8_t {
    Normal,   // --surface 바탕 · --line 테두리 · --fg2 글자
    OnFill,   // 강조 · 위험 채움 위(기본 단추 안) — 테두리 강조 위 글자 40 %, 글자 강조 위 글자
};

/// 키 칩 크기 — mono 11 px, 높이 18, 좌우 5.
QSize keyChipSize(const QString &keys);
void paintKeyChip(QPainter *painter, const QRectF &rect, const QString &keys, const ThemeColors &colors,
                  KeyChipLook look = KeyChipLook::Normal);

// ------------------------------------------------------------------------------------- 태그

/// 태그 크기 — 높이 22(compact 20), 좌우 8, 11.5 px 600, 아이콘 12 + 간격 4. Bad는 좌우 0.
QSize tagSize(const QString &text, Tone tone, bool compact, bool hasIcon);
/// icon은 foreground 색으로 이미 칠한 아이콘(없으면 null).
void paintTag(QPainter *painter, const QRectF &rect, const QString &text, Tone tone, const ThemeColors &colors,
              bool compact, const QIcon &icon = QIcon());

// ------------------------------------------------------------------------------------- 배지 · 배너

/// 대화상자 머리 배지 바탕(40 × 40, 모서리 8). 시안2는 바탕 투명 — Warn · Danger만 1 px 테두리.
struct BadgeColors
{
    QColor background;
    QColor border;
    QColor icon;
};
BadgeColors badgeColors(Tone tone, const ThemeColors &colors);
void paintBadge(QPainter *painter, const QRectF &rect, Tone tone, const ThemeColors &colors);

/// 배너 바탕(모서리 6). Info는 테두리 없음, Warn · Danger는 1 px 테두리.
void paintBannerFrame(QPainter *painter, const QRectF &rect, Tone tone, const ThemeColors &colors);

// ------------------------------------------------------------------------------------- 도킹

/// 도크 끌어 놓기 표시(KDDockWidgets의 classic 방식을 옮김) — 놓을 자리 하나의 단추.
/// Outer*는 메인 창 가장자리(그 변의 도크 영역), 나머지는 마우스 아래 도크 기준(Center = 탭으로 묶기).
enum class DropSpot : std::uint8_t { Left, Top, Right, Bottom, Center, OuterLeft, OuterTop, OuterRight, OuterBottom };

/// 표시 단추 한 변(32).
inline constexpr int kDropIndicatorSize = 32;

/// 시안1: 둥근 카드 단추 + 창 모양 안에 놓일 쪽을 강조색으로. 시안2: 입체 단추 + 파란 제목 띠가 있는 창 모양.
void paintDropIndicator(QPainter *painter, const QRectF &rect, DropSpot spot, bool hover, const ThemeColors &colors);

/// 놓일 자리 미리보기 — 강조색 반투명 채움 + 2 px 테두리(시안1 둥근 4, 시안2 네모).
void paintDropPreview(QPainter *painter, const QRectF &rect, const ThemeColors &colors);

/// 자동 숨김 사이드바 바탕 — 창 바탕 + 안쪽(내용과 맞닿는) 변 1 px 선.
void paintDockSideBar(QPainter *painter, const QRect &rect, Qt::Edge innerEdge, const ThemeColors &colors);

// ------------------------------------------------------------------------------------- 도킹 2판 (07)

/// 디자인별 도킹 치수(07 §6.4). 탭 높이는 스타일의 CT_TabBarTab이 정하고, 탭 줄 높이는 그 위에 stripTopGap을 더한다.
struct DockMetrics
{
    int stripTopGap = 4;        // 탭 줄 위 여백 — 시안1 4(32 − 28), 시안2 2(28 − 26), 시안3 2(32 − 30)
    int tabGap = 2;             // 탭 사이 — 시안2는 −1(테두리 겹침)
    int areaButton = 22;        // 영역 단추 한 변
    int splitter = 5;           // 분할 손잡이 두께
    int sideBar = 28;           // 사이드바 두께
    int sideTabPad = 10;        // 사이드 탭 긴 쪽 여백
    int paneTitle = 30;         // 펼친 창 · 미리보기 창 제목 줄
    int floatingTitle = 32;     // 떠 있는 창 제목 줄
    QSize floatingButton{40, 32};
    int floatingButtonGap = 0;
    int floatingFrame = 1;      // 떠 있는 창 바깥 테두리
    int areaRadius = 0;         // 영역 테두리 모서리(시안3 6)
    int flyoutRadius = 0;       // 펼친 창 모서리(시안3 8)
    bool areaBorder = false;    // 영역 전체 테두리(시안3)
    bool contentBorder = false; // 내용 테두리(시안2 tabLine)
};
DockMetrics dockMetrics(const ThemeColors &colors);

/// 탭 줄 바탕 — 시안1 · 2 Win, 시안3 Alt. 내용과 맞닿는 변(lineEdge)에 1 px 선(시안2 tabLine).
/// 시안3은 영역 모서리에 맞춰 바깥쪽 두 모서리를 둥글게 깎는다.
void paintDockStrip(QPainter *painter, const QRect &rect, Qt::Edge lineEdge, const ThemeColors &colors);

/// 영역 테두리 — 시안3 둥근 Line(영역 전체), 시안2 내용 둘레 tabLine(contentRect, 탭 줄 쪽 변 제외).
void paintDockAreaFrame(QPainter *painter, const QRect &areaRect, const QRect &contentRect, Qt::Edge stripEdge,
                        const ThemeColors &colors);

/// 분할 손잡이 — 마우스 올림이면 가운데 2 px 강조색, 시안2는 새긴 점 셋을 늘 그린다.
void paintDockSplitterHandle(QPainter *painter, const QRect &rect, bool hover, const ThemeColors &colors);

enum class DockTitleKind : std::uint8_t { Pane, Floating };

/// 펼친 창 · 미리보기 창 제목 띠 — 시안1 Win + 위 2 px 강조(활성), 시안2 도구 캡션 그라데이션(ttl → cap),
/// 시안3 Alt + 아래 2 px 강조(활성).
void paintDockPaneTitle(QPainter *painter, const QRect &rect, bool active, const ThemeColors &colors);

/// 떠 있는 창 제목 줄 — 시안1 Win, 시안2 도구 캡션(비활성 ttlInactive), 시안3 제목 워시(비활성 Head → Alt).
void paintFloatingTitle(QPainter *painter, const QRect &rect, bool active, const ThemeColors &colors);

/// 제목 글자(12 px) — 디자인별 굵기 · 색 · 그림자. 넘치면 줄임표.
void paintDockTitleText(QPainter *painter, const QRect &rect, const QString &text, DockTitleKind kind, bool active,
                        const ThemeColors &colors);
/// 제목 글자 글꼴(넓이 계산용).
QFont dockTitleFont(const QFont &base, const ThemeColors &colors);

/// 떠 있는 창 바깥 테두리(시안1 Line, 시안2 frm, 시안3 title-2).
void paintFloatingFrame(QPainter *painter, const QRect &rect, bool active, const ThemeColors &colors);

/// 펼친 창 틀 — Win 바탕 + 1 px 테두리(시안2 out, 시안3 모서리 8).
void paintDockFlyoutFrame(QPainter *painter, const QRect &rect, const ThemeColors &colors);

enum class CaptionButton : std::uint8_t { Close, Maximize, Restore };

/// 떠 있는 창 제목 줄 단추 — 시안1 · 3 글자형(닫기 올림 붉게), 시안2 캡션 단추(capFill · capLine).
void paintCaptionButton(QPainter *painter, const QRect &rect, CaptionButton kind, bool hover, bool pressed, bool active,
                        const ThemeColors &colors);

/// 탭 줄 · 사이드바 끼워 넣기 캐럿(강조색 채움).
void paintDropCaret(QPainter *painter, const QRectF &rect, const ThemeColors &colors);

} // namespace fm::style
