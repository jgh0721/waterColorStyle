#pragma once

#include <QLineEdit>

namespace fm::ui {

/// 찾기 상자(01 §1.3 A3 · 06 §4.13) — 돋보기(14 px, --fg3) + 자리표시 "현재 폴더에서 찾기   Ctrl+F", 폭 300.
/// 높이 · 틀은 스타일의 입력 칸(시안1 30, 시안2 25 들어간 칸)을 따른다.
class FindBox : public QLineEdit
{
    Q_OBJECT
public:
    explicit FindBox(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshIcon();

    QAction *m_icon = nullptr;
};

} // namespace fm::ui
