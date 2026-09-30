#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QVariantAnimation;

namespace fm::ui {

/// 진행 창 '자세히'의 처리 속도 그래프.
///
/// - 가로축: 진행률(0 ~ 100 %, 남은 부분은 비어 있음) 또는 경과 시간(전체 기록을 폭에 맞춰 압축)
/// - 세로축: 평활한 속도. 눈금은 값에 맞춰 자동으로 바뀌고 부드럽게 움직인다.
/// - 가로선: 현재 속도(실선, 오른쪽 값 표시)가 속도에 따라 위아래로 움직인다.
///   평균 속도(점선)와 속도 제한(경고색 점선)도 함께 그린다.
/// - 일시 정지 구간은 띠로 표시하고, 마우스를 올리면 그 지점의 값을 보여 준다.
///
/// 색은 FmStyle 테마 토큰에서 가져오므로 라이트 · 다크를 따로 설정할 필요가 없다.
class TransferGraph : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(Axis axis READ axis WRITE setAxis)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(bool showAverage READ showAverage WRITE setShowAverage)
    Q_PROPERTY(bool framed READ isFramed WRITE setFramed)
    Q_PROPERTY(double speedLimit READ speedLimit WRITE setSpeedLimit)

public:
    // Qt Designer · uic가 .ui에 'fm::ui::TransferGraph::Time'처럼 쓰므로 범위 없는 enum을 쓴다.
    enum Axis { Progress, Time };
    Q_ENUM(Axis)

    explicit TransferGraph(QWidget *parent = nullptr);
    ~TransferGraph() override;

    /// 새 작업을 시작한다. totalBytes가 0 이하면 진행률 축 대신 시간 축으로 그린다.
    void start(qint64 totalBytes);
    /// 복사 엔진이 주기적으로(200 ~ 500 ms) 부른다. 누적 바이트와 작업 시작 후 경과 시간.
    void addSample(qint64 bytesDone, qint64 elapsedMs);

    void setPaused(bool paused);
    bool isPaused() const noexcept { return m_paused; }

    /// 속도 제한 선. 0이면 그리지 않는다.
    void setSpeedLimit(double bytesPerSecond);
    double speedLimit() const noexcept { return m_limit; }

    void setAxis(Axis axis);
    Axis axis() const noexcept { return m_axis; }

    void setTitle(const QString &title);
    QString title() const { return m_title; }

    void setShowAverage(bool on);
    bool showAverage() const noexcept { return m_showAverage; }

    /// 카드 모양의 바탕과 테두리를 직접 그릴지 여부 (기본 켬).
    void setFramed(bool on);
    bool isFramed() const noexcept { return m_framed; }

    double currentSpeed() const noexcept { return m_current; }  // 평활한 현재 속도 (B/s)
    double averageSpeed() const noexcept;                        // 일시 정지 시간을 뺀 평균
    double peakSpeed() const noexcept { return m_peak; }
    qint64 totalBytes() const noexcept { return m_total; }
    qint64 bytesDone() const noexcept;

    /// "39.4 MB/s" — 1024 단위, 100 미만은 소수 한 자리.
    static QString formatRate(double bytesPerSecond);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void speedChanged(double current, double average);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Sample
    {
        qint64 bytes;
        qint64 ms;
        double speed;  // 평활한 속도
        bool paused;
    };

    Axis effectiveAxis() const noexcept;
    QRectF plotRect() const;
    double xFraction(const Sample &s) const;
    void decimate();
    void retarget();
    double targetScale() const;
    int sampleNear(qreal x) const;

    QList<Sample> m_samples;
    qint64 m_total = 0;
    qint64 m_activeMs = 0;
    double m_current = 0.0;
    double m_peak = 0.0;
    double m_limit = 0.0;
    bool m_paused = false;
    bool m_showAverage = true;
    bool m_framed = true;
    Axis m_axis = Axis::Progress;
    QString m_title;
    QString m_autoDescription;  // addSample()이 마지막으로 넣은 접근성 설명

    // 화면에 그리는 값 (애니메이션)
    double m_lineShown = 0.0;
    double m_scaleShown = 0.0;
    QVariantAnimation *m_lineAnim = nullptr;
    QVariantAnimation *m_scaleAnim = nullptr;

    qreal m_hoverX = -1.0;
};

} // namespace fm::ui
