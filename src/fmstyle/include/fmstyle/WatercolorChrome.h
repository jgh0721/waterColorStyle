#pragma once

// 시안2(워터컬러) 전용 입체 색 — 캔버스의 --x-* 변수와 위젯 비교 보드의 값.
// 51개 테마 토큰과 달리 사용자가 고치는 값이 아니라 디자인에 딸린 고정 값이다.

#include "fmstyle/ThemeTokens.h"

#include <QColor>

namespace fm::style {

struct WatercolorChrome
{
    // 입체 테두리 · 버튼 바탕
    QColor out;           // --x-out    버튼 · 머리글 바깥 테두리
    QColor hi;            // --x-hi     밝은 빗면
    QColor lo;            // --x-lo     어두운 빗면
    QColor g1, g2, g3;    // --x-g1..3  버튼 그라데이션 (위 → 아래)
    // 마우스 올림
    QColor h1, h2, h3;    // --x-h1..3
    QColor hoverHi;       // --x-hhi
    QColor hoverLo;       // --x-hlo
    QColor hoverFg;       // --x-hfg
    // 누름 · 켜짐
    QColor p1, p2, p3;    // --x-p1..3  (아래 → 위로 칠한다)
    // 사용 안 함
    QColor disLine;       // --x-dis
    QColor disFg;         // --x-dfg
    // 들어간 입력 칸
    QColor fieldOuter;    // --x-fo     위 · 왼쪽 테두리
    QColor fieldBright;   // --x-fb     아래 · 오른쪽 테두리
    QColor fieldInner;    // --x-fi     안쪽 위 · 왼쪽 선
    // 체크 상자 · 라디오 빗면
    QColor checkOuter;    // --x-co
    QColor checkInner;    // --x-ci
    QColor checkBright;   // --x-cb2
    QColor checkLight;    // --x-cl
    // 제목 표시줄 · 창 틀
    QColor title;         // --x-ttl
    QColor titleHi;       // --x-ttl-hi
    QColor titleLo;       // --x-ttl-lo
    QColor titleLo2;      // --x-ttl-lo2
    QColor titleEdge;     // --x-ttl-edge
    QColor titleCap;      // --x-ttl-cap   오른쪽 끝의 짙은 부분
    QColor frame;         // --x-frm
    QColor frameOuter;    // --x-frm-out
    QColor capFill;       // --x-cbf   제목 표시줄 단추
    QColor capLine;       // --x-cbl
    QColor capGlyph;      // --x-cbg
    QColor capHoverLine;  // --x-cbh
    QColor mosaic1, mosaic2, mosaic3;  // --x-m1..3
    QColor titleInactive;      // 비활성 창
    QColor titleInactiveHi;
    QColor capInactiveLine;
    QColor capInactiveGlyph;
    // 탭
    QColor tab;           // --x-tab
    QColor tabLine;       // --x-tabl
    QColor tabHover;      // --x-tabh
    QColor tabHi;         // --x-tabhi  선택 탭 안쪽 밝은 선
    // 진행 막대 · 스위치 홈
    QColor trough;        // --x-trough
    // 메뉴
    QColor menuLine;
    QColor menuHover;     // --x-mh
    QColor menuHoverLine; // --x-mhl
    // 위험 버튼
    QColor danger1, danger2, danger3;  // --x-dg1..3
    QColor dangerOuter;   // --x-dgo
    // 스크롤 막대 (위젯 비교 보드)
    QColor sbTrack;
    QColor sbThumb;
    QColor sbThumbLine;
    QColor sbGrip;
    QColor sbArrow;
    // 도구 설명
    QColor tipBg;
    QColor tipLine;
    QColor tipFg;
};

/// 변형별 워터컬러 입체 색.
const WatercolorChrome &watercolorChrome(Variant variant) noexcept;

} // namespace fm::style
