#pragma once

#include "fmstyle/FmStyle.h"

namespace fm::style {

/// 시안3 "Paper Fluent" — 시안1(FmStyle)의 Fluent 구조(둥근 모서리 · 밑줄 입력 · 2 px 포커스)에
/// 워터컬러 종이 톤 색과 세로 워시를 얹는다. 크기는 캔버스 "Paper Fluent 토큰" 보드의 밀도 표를 따른다
/// (단추 28 · 입력 28 · 탭 30 · 머리글 24 · 행 22 · 메뉴 막대 26).
///
/// 시안1과 다르게 그리는 것: 단추 · 세그먼트 · 콤보의 세로 워시(--btn-hi → --btn-lo)와 그림자 1단,
/// 기본 · 위험 단추 워시, 밑줄 탭(선택 = 2 px 강조 밑줄 + 목록 바탕), 머리글 워시(--head → --alt),
/// 강조 채움 선택(흰 글자) · 트리의 연한 선택, 2 px 커서 외곽선, 6 → 10 px 스크롤 손잡이,
/// 8 px 둥근 워시 진행 막대, 크림 도구 설명, 파란 워시 MDI 제목 표시줄, --alt 도크 제목.
class PaperFluentStyle final : public FmStyle
{
    Q_OBJECT

public:
    PaperFluentStyle();
    ~PaperFluentStyle() override;
};

} // namespace fm::style
