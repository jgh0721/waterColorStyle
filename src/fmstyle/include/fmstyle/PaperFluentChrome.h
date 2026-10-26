#pragma once

// 시안3(Paper Fluent) 전용 워시 색 — 캔버스 hybrid.css의 --btn-hi · --btn-lo · --title-1 · --title-2 · --tip-*.
// 워터컬러 입체 색(WatercolorChrome)과 달리 빗면이 없고 세로 워시 두 색 · 제목 워시 · 도구 설명만 있다.
// 단추 · 머리글 · 진행 막대 워시는 토큰에서 계산하므로 기준 색 · 직접 지정을 그대로 따라간다.

#include "fmstyle/ThemeTokens.h"

#include <QColor>

namespace fm::style {

class ThemeColors;

struct PaperFluentChrome
{
    // 단추 · 콤보 · 기능 키 — 위(hi) → 아래(lo) 세로 워시
    QColor buttonHi;      // --btn-hi
    QColor buttonLo;      // --btn-lo
    QColor primaryHi;     // 기본 단추 워시 위쪽(아래쪽은 --accent)
    QColor primaryLine;   // 기본 단추 테두리
    QColor dangerHi;      // 위험 단추 워시 위쪽(아래쪽은 --danger-fill)
    QColor dangerLine;    // 위험 단추 테두리
    // 머리글 워시 --head → --alt, 진행 막대 워시 progressHi → --accent
    QColor headTop;
    QColor headBottom;
    QColor progressHi;
    // 그림자 1단(단추 · 입력) — 아래 1 px
    QColor shadow;
    // 제목 표시줄 파란 워시(MDI · 대화상자 머리 · Windows 11 제목 색)
    QColor titleTop;      // --title-1
    QColor titleBottom;   // --title-2
    QColor titleFg;       // --title-fg
    // 크림 도구 설명
    QColor tipBg;         // --tip-bg
    QColor tipLine;       // --tip-line
    QColor tipFg;
};

/// 토큰 값(기준 색 · 직접 지정 반영)으로 워시 색을 계산한다. 디자인과 상관없이 부를 수 있다.
PaperFluentChrome paperFluentChrome(const ThemeColors &colors);

/// 변형별 제목 워시 위쪽 색 — Windows 11 제목 표시줄(DWM)에 입힌다.
QColor paperFluentTitleColor(Variant variant);

} // namespace fm::style
