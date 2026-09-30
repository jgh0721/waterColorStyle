#pragma once

#include <QFrame>

namespace fm::ui {

/// 대화상자 버튼 영역(목업 .dlg-f) — 시안1: --foot 바탕 + 위 1 px --line, 시안2: 선 없음(--foot = --win).
/// Qt Designer 컨테이너. 안에 가로 레이아웃(여백 24 · 16, 간격 8)을 두고
/// [왼쪽 항목] · 늘임 · [기본 단추] · [취소] 순서로 넣는다.
class DialogFooter : public QFrame
{
    Q_OBJECT

public:
    explicit DialogFooter(QWidget *parent = nullptr);
};

} // namespace fm::ui
