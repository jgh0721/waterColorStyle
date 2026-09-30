#pragma once

#include <QProgressBar>

class QTimer;

namespace fm::ui {

/// 진행 막대 (시안1 6 px 막대, 시안2 16 px 블록). 상태에 따라 강조색 · 일시 정지색 · 위험색으로 그린다.
/// 최솟값 = 최댓값(진행률 모름)이면 구간이 흐르는 움직임을 그린다.
class ProgressBar : public QProgressBar
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)
    Q_PROPERTY(Thickness thickness READ thickness WRITE setThickness)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum State { Normal, Paused, Error };
    Q_ENUM(State)
    /// 두께 — Thin: 시안1 4 px · 시안2 12 px(진행 창 '현재 파일'), Thick: 시안1 8 px · 시안2 16 px('전체').
    enum Thickness { ThinBar, NormalBar, ThickBar };
    Q_ENUM(Thickness)

    explicit ProgressBar(QWidget *parent = nullptr);

    State state() const noexcept { return m_state; }
    void setState(State state);

    /// 얇은 막대 — 시안2에서 12 px. 시안1은 두께가 같다. (thickness = ThinBar를 권장)
    bool isCompact() const noexcept { return m_compact; }
    void setCompact(bool compact);

    Thickness thickness() const noexcept { return m_thickness; }
    void setThickness(Thickness thickness);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void applySize();
    void tickBusy();

    State m_state = Normal;
    bool m_compact = false;
    Thickness m_thickness = NormalBar;
    QTimer *m_busyTimer = nullptr;
    qreal m_busyPhase = 0.0;
};

} // namespace fm::ui
