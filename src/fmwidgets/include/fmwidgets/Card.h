#pragma once

#include <QFrame>

namespace fm::ui {

/// 카드 — 목록 바탕색과 구분선 테두리 (시안1 모서리 6 px, 시안2 네모). 설정 화면의 항목 묶음에 쓴다.
/// 제목이 필요하면 QGroupBox를 쓴다 (FmStyle이 제목 + 카드로 그린다).
class Card : public QFrame
{
    Q_OBJECT

public:
    explicit Card(QWidget *parent = nullptr);
};

} // namespace fm::ui
