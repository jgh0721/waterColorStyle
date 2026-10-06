#pragma once

#include <QList>
#include <QString>
#include <QWidget>

namespace fm::ui {

/// 두 줄 시간 흐름 — 같은 시간 자 위에 두 쪽의 자취를 나란히 그린다.
///
/// 스트림에서 「어디서 멈췄나」는 총량이 아니라 **시각**에서 드러난다. 두 쪽이 주고받은 양이 같아도
/// 한쪽이 모아서 주었으면 받는 쪽에서는 흐름이 멎은 것처럼 보인다. 그래서 두 줄을 같은 자에 올린다.
///
/// - 눈금 하나가 조각 하나다. `First`는 크게, `Cut`은 위험색 X 로 그린다.
/// - `addGap()`으로 준 구간은 띠로 칠한다. 주지 않아도 `gapThresholdMs`를 넘는 빈 자리는 스스로 띠가 된다.
/// - `pairId`가 같은 두 눈금은 가는 선으로 잇는다 — 「이것이 저것이 되었다」를 보이는 자리다.
///   이은 선의 기울기가 곧 지연이다.
/// - 가로 자는 `spanMs`가 정한다. 0이면 넣은 눈금에서 스스로 잡는다.
///
/// 색은 FmStyle 테마 토큰에서 가져오므로 라이트 · 다크를 따로 설정할 필요가 없다.
class PairedTimeline : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 spanMs READ spanMs WRITE setSpanMs)
    Q_PROPERTY(qint64 gapThresholdMs READ gapThresholdMs WRITE setGapThresholdMs)
    Q_PROPERTY(int labelWidth READ labelWidth WRITE setLabelWidth)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Mark { Tick, First, Cut, Note };
    Q_ENUM(Mark)

    static constexpr int kTopRow = 0;
    static constexpr int kBottomRow = 1;
    static constexpr qint64 kDefaultGapMs = 3000;
    static constexpr int kDefaultLabelWidth = 96;

    struct Row
    {
        QString name;
        QString detail;  // 곁글 — 비어도 된다
    };

    struct Point
    {
        qint64 atMs = 0;
        Mark kind = Tick;
        int pairId = -1;
        QString note;  // 마우스를 올렸을 때 보일 글 — 비면 시각만
    };

    struct Gap
    {
        qint64 fromMs = 0;
        qint64 toMs = 0;
    };

    explicit PairedTimeline(QWidget *parent = nullptr);

    void setRows(const Row &top, const Row &bottom);
    Row row(int index) const;

    void addMark(int row, qint64 atMs, Mark kind = Tick, int pairId = -1, const QString &note = QString());
    void addGap(int row, qint64 fromMs, qint64 toMs);
    void clear();

    int markCount(int row) const;
    /// 그 줄의 첫 눈금 시각 — 없으면 -1. 첫 조각 지연이 이것이다.
    qint64 firstMs(int row) const;
    /// 그 줄의 마지막 눈금 시각 — 없으면 -1.
    qint64 lastMs(int row) const;
    /// 띠로 그린 빈 자리 수(손으로 준 것 + 상한을 넘겨 스스로 잡은 것).
    int gapCount(int row) const;

    qint64 spanMs() const;
    void setSpanMs(qint64 ms);
    qint64 gapThresholdMs() const noexcept { return m_gapMs; }
    void setGapThresholdMs(qint64 ms);
    int labelWidth() const noexcept { return m_labelWidth; }
    void setLabelWidth(int px);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// 마우스가 눈금 위에 있다 — 벗어나면 row가 -1이다.
    void markHovered(int row, int index, qint64 atMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QList<Gap> effectiveGaps(int row) const;
    QRectF plotRect() const;
    qreal xFor(qint64 ms) const;
    qreal baselineY(int row) const;
    int markNear(int row, qreal x) const;

    Row m_rows[2];
    QList<Point> m_points[2];
    QList<Gap> m_gaps[2];
    qint64 m_span = 0;
    qint64 m_gapMs = kDefaultGapMs;
    int m_labelWidth = kDefaultLabelWidth;
    int m_hoverRow = -1;
    int m_hoverIndex = -1;
};

} // namespace fm::ui
