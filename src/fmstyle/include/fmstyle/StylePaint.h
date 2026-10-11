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

} // namespace fm::style
