#pragma once

#include <QWidget>

class QVariantAnimation;

namespace fm::ui {

/// 진행 고리 — 둥근 진행 표시(ElaProgressRing · WinUI ProgressRing). 값이 있으면 12시부터 시계 방향으로 채우고,
/// 바쁨(busy)이면 길이가 늘었다 줄었다 하는 호가 돈다. 범위가 0–0(최솟값 = 최댓값)이어도 바쁨이다.
///
/// - 시안1: 옅은 홈 고리 위에 둥근 끝의 강조색 호(Windows 11). 두께는 지름의 1/10(최소 2).
/// - 시안2: 들어간 홈 고리 안에 XP 진행 막대처럼 칸(블록)을 채운다. 바쁨이면 밝은 칸 셋이 돈다.
/// - 상태(state)는 진행 막대와 같다 — 일시 정지 · 오류 색. 가운데 글(textVisible)은 백분율 또는 값.
/// - 크기는 정사각형 안에 그린다(sizeHint 48 × 48). 숨으면 움직임을 멈춘다.
class ProgressRing : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool busy READ isBusy WRITE setBusy NOTIFY busyChanged)
    Q_PROPERTY(bool textVisible READ isTextVisible WRITE setTextVisible)
    Q_PROPERTY(ValueDisplay valueDisplay READ valueDisplay WRITE setValueDisplay)
    Q_PROPERTY(State state READ state WRITE setState)
    Q_PROPERTY(bool trackVisible READ isTrackVisible WRITE setTrackVisible)
    Q_PROPERTY(int thickness READ thickness WRITE setThickness)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum ValueDisplay { Percent, Actual };
    Q_ENUM(ValueDisplay)
    enum State { Normal, Paused, Error };
    Q_ENUM(State)

    explicit ProgressRing(QWidget *parent = nullptr);
    ~ProgressRing() override;

    int minimum() const noexcept { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const noexcept { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int value() const noexcept { return m_value; }
    void setValue(int value);
    /// 0 ~ 1 — 범위가 없으면 0.
    qreal fraction() const;

    /// 바쁨(진행률 모름). 범위가 0–0이면 이 값과 상관없이 바쁨이다.
    bool isBusy() const noexcept { return m_busy || m_minimum == m_maximum; }
    void setBusy(bool busy);

    bool isTextVisible() const noexcept { return m_textVisible; }
    void setTextVisible(bool visible);
    ValueDisplay valueDisplay() const noexcept { return m_display; }
    void setValueDisplay(ValueDisplay display);
    /// 가운데 글 — "42%" 또는 "42". 바쁨이면 빈 글.
    QString text() const;

    State state() const noexcept { return m_state; }
    void setState(State state);
    /// 홈 고리(채우지 않은 쪽)를 그릴지 — 끄면 호만 보인다(ElaProgressRing IsTransparent).
    bool isTrackVisible() const noexcept { return m_trackVisible; }
    void setTrackVisible(bool visible);
    /// 고리 두께(px). 0이면 지름에서 정한다.
    int thickness() const noexcept { return m_thickness; }
    void setThickness(int px);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return width; }

Q_SIGNALS:
    void valueChanged(int value);
    void busyChanged(bool busy);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateAnimation();
    int ringThickness(qreal diameter) const;

    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    bool m_busy = false;
    bool m_textVisible = true;
    ValueDisplay m_display = Percent;
    State m_state = Normal;
    bool m_trackVisible = true;
    int m_thickness = 0;
    QVariantAnimation *m_spin = nullptr;  // 0 ~ 1 한 바퀴(바쁨)
    qreal m_phase = 0;
};

} // namespace fm::ui
