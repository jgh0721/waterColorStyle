#pragma once

// 진행 창 시뮬레이터(02 §7.3) — 목업 스크립트의 속도 모델을 그대로 옮겼다.
// 원시 속도 172 MB/s 사인 파형 + 느려짐 · 빨라짐 구간, 평활 τ = 1.2 s, 0.5 s 틱, 끝나면 처음부터 다시(데모 반복).

#include <QList>
#include <QObject>
#include <QString>

class QTimer;

namespace fm::dialogs {

class ProgressSimulator : public QObject
{
    Q_OBJECT
public:
    struct Sample
    {
        qint64 bytes;
        qint64 ms;
        bool paused;
    };

    explicit ProgressSimulator(QObject *parent = nullptr);

    /// 파일 크기 목록(순서대로 복사). 합계가 전체다.
    void setFiles(const QList<qint64> &sizes);
    QList<qint64> files() const { return m_files; }

    /// 처음 seconds초를 미리 계산한다. 목업 스크립트 그대로 9초면 약 32 %다.
    void preroll(double seconds);
    /// bytes에 이를 때까지(최대 maxSeconds초) 미리 계산한다 — 목업 정적 값 "1.50 GB · 37 %"는 약 10.5초.
    void prerollTo(qint64 bytes, double maxSeconds = 60.0);
    void reset();

    void start();
    void stop();
    bool isRunning() const;

    bool isPaused() const noexcept { return m_paused; }
    void setPaused(bool paused);

    /// 데모 반복(기본 켬): 끝나면 처음부터. 끄면 전체에서 멈추고 finished()를 낸다.
    bool isLooping() const noexcept { return m_loop; }
    void setLoop(bool loop) { m_loop = loop; }
    bool isFinished() const noexcept { return m_finished; }

    qint64 total() const noexcept { return m_total; }
    qint64 bytes() const noexcept { return m_bytes; }
    double speed() const noexcept { return m_ema; }            // B/s(평활)
    double average() const noexcept;                            // 일시 정지 시간을 뺀 평균
    double elapsed() const noexcept { return m_t; }             // 초(일시 정지 포함)
    int percent() const noexcept;
    int fileIndex() const noexcept;                             // 0부터
    qint64 fileDone() const noexcept;
    qint64 fileSize() const noexcept;
    const QList<Sample> &samples() const noexcept { return m_samples; }

    /// "약 15초" · "약 3분" · "—"
    QString remainingText() const;
    /// "mm:ss"
    QString elapsedText() const;

    /// 목업의 원시 속도(MB/s) — 시각 t초.
    static double rawSpeed(double t);

Q_SIGNALS:
    void ticked();
    /// 전체를 다 옮겨 처음부터 다시 시작했다(반복 켬).
    void restarted();
    /// 전체를 다 옮겼다(반복 끔) — 타이머는 멈춰 있다.
    void finished();

private:
    void advance(double dt);

    QTimer *m_timer = nullptr;
    QList<qint64> m_files;
    QList<Sample> m_samples;
    qint64 m_total = 0;
    qint64 m_bytes = 0;
    double m_bytesExact = 0.0;
    double m_ema = 0.0;
    double m_t = 0.0;
    double m_active = 0.0;
    bool m_paused = false;
    bool m_loop = true;
    bool m_finished = false;
};

} // namespace fm::dialogs
