#pragma once

#include <QPushButton>

namespace fm::ui {

/// 주소 줄의 드라이브 단추(01 §1.3 A4b-1 · 06 §4.14) — 드라이브 아이콘 16 + "D:" + 아래 꺾쇠 10, 누르면 메뉴.
/// 시안1: 높이 26 · 좌우 8 · 테두리 단추 · 글자 12.5 px 600. 시안2: 24 입체 단추 · 400.
class DriveButton : public QPushButton
{
    Q_OBJECT
public:
    explicit DriveButton(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFont labelFont() const;
};

} // namespace fm::ui
