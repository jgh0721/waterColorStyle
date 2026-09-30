#pragma once

#include <QProgressBar>

namespace fm::ui {

/// 진행 막대 (시안1 6 px 막대, 시안2 16 px 블록 · compact면 12 px). 상태에 따라 강조색 · 일시 정지색 · 위험색으로 그린다.
class ProgressBar : public QProgressBar
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum State { Normal, Paused, Error };
    Q_ENUM(State)

    explicit ProgressBar(QWidget *parent = nullptr);

    State state() const noexcept { return m_state; }
    void setState(State state);

    /// 얇은 막대 — 시안2에서 12 px (진행 창의 '현재 파일'). 시안1은 두께가 같다.
    bool isCompact() const noexcept { return m_compact; }
    void setCompact(bool compact);

private:
    State m_state = Normal;
    bool m_compact = false;
};

} // namespace fm::ui
