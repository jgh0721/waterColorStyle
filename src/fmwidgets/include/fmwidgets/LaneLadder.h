#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

namespace fm::ui {

/// 사다리 — 레인 사이를 오간 자취를 차례대로 그린다.
///
/// 한 줄이 「언제 · 어디서 어디로 · 무엇이」 하나다. 가로로 레인이 서고, 줄마다 한 레인에서 다른 레인으로
/// 화살표를 긋는다. 왼쪽에 시각(mono · 오른쪽 맞춤), 가운데에 그림, 오른쪽에 글이 온다.
///
/// 줄의 종류가 선 모양을 정한다 — 이것이 이 위젯의 쓸모다.
/// - `Seen`     본 것. 실선.
/// - `Inferred` 추정 — 이 자리에서 보이지 않는 구간. 점선에 기운 글.
/// - `Warn`     경고. 경고색 실선.
/// - `Failed`   멈춘 자리. 위험색 실선에 굵은 글 — 첫 붉은 화살표가 멈춘 구간이다.
/// - `Stream`   조각 여럿이 흐른 구간. 굵은 점선.
/// - `Self`     한 레인 안에서 일어난 일. 그 레인에 작은 고리를 그린다.
/// - `Divider`  구분 띠. 화살표 없이 글만 — 「두 번째 교환」 같은 자리 표시.
///
/// 레인은 이름과 곁글(mono · 한 줄 말줄임)을 가진다. 자리 수를 넘는 레인 번호를 주면 그 줄은 무시된다.
/// 높이는 줄 수에서 나오므로 스크롤 영역에 그대로 넣으면 된다.
///
/// 색은 FmStyle 테마 토큰에서 가져오므로 라이트 · 다크를 따로 설정할 필요가 없다.
class LaneLadder : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int laneWidth READ laneWidth WRITE setLaneWidth)
    Q_PROPERTY(int timeWidth READ timeWidth WRITE setTimeWidth)
    Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Kind { Seen, Inferred, Warn, Failed, Stream, Self, Divider };
    Q_ENUM(Kind)

    struct Lane
    {
        QString name;
        QString detail;  // mono 곁글 — 비어도 된다
    };

    struct Rung
    {
        int from = 0;
        int to = 0;
        QString time;
        QString text;
        Kind kind = Seen;
    };

    static constexpr int kDefaultLaneWidth = 300;
    static constexpr int kDefaultTimeWidth = 62;
    static constexpr int kRowHeight = 26;

    explicit LaneLadder(QWidget *parent = nullptr);

    void setLanes(const QList<Lane> &lanes);
    QList<Lane> lanes() const { return m_lanes; }

    /// 레인 from에서 to로. 같은 번호면 Self처럼 고리를 그린다.
    void addRung(int from, int to, const QString &time, const QString &text, Kind kind = Seen);
    /// 화살표 없는 구분 띠.
    void addDivider(const QString &time, const QString &text);
    void setRungs(const QList<Rung> &rungs);
    QList<Rung> rungs() const { return m_rungs; }
    int rungCount() const { return static_cast<int>(m_rungs.size()); }
    void clear();

    /// 처음으로 멈춘 줄의 번호 — 없으면 -1. 「어느 구간에서 멈췄나」를 한 번에 묻는 자리다.
    int firstFailure() const;

    int laneWidth() const noexcept { return m_laneWidth; }
    void setLaneWidth(int px);
    int timeWidth() const noexcept { return m_timeWidth; }
    void setTimeWidth(int px);

    /// 고른 줄 — 없으면 -1.
    int currentRow() const noexcept { return m_current; }
    void setCurrentRow(int row);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void currentRowChanged(int row);
    void rowActivated(int row);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    int headerHeight() const;
    int rowAt(const QPoint &pos) const;
    QRect rowRect(int row) const;
    qreal laneCenter(int lane) const;

    QList<Lane> m_lanes;
    QList<Rung> m_rungs;
    int m_laneWidth = kDefaultLaneWidth;
    int m_timeWidth = kDefaultTimeWidth;
    int m_current = -1;
    int m_hover = -1;
};

} // namespace fm::ui
