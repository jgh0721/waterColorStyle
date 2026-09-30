#pragma once

#include <QWidget>

class QLabel;

namespace fm::ui {

/// 패널 상태 줄(01 §1.3 A4d · 06 §4.15) — 왼쪽 선택 요약(숫자 폭 고정) · 오른쪽 보조 문구.
/// 시안1: 26 · 위 1 px --line · --win · 좌우 12 · 12 px --fg2. 시안2: 23 얕게 들어간 칸, 바깥 여백 3 3 0.
class PanelStatusBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString leftText READ leftText WRITE setLeftText)
    Q_PROPERTY(QString rightText READ rightText WRITE setRightText)

public:
    explicit PanelStatusBar(QWidget *parent = nullptr);

    QString leftText() const { return m_left; }
    void setLeftText(const QString &text);
    QString rightText() const { return m_right; }
    void setRightText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_left;
    QString m_right;
};

} // namespace fm::ui
