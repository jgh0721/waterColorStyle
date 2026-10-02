#pragma once

#include <fmfilelist/FileListModel.h>

#include <QWidget>

#include <array>

class QComboBox;
class QCheckBox;
class QLabel;
class QStandardItemModel;
class QTimer;
class QVBoxLayout;

namespace fm::filelist {
class FileSortProxy;
class LocalFileSource;
class ThumbnailBackend;
}

namespace fm::ui {
class Button;
class Label;
class SegmentedControl;
}

namespace fm::app {

/// 섬네일 비교 창(PLAN §9) — 같은 모델을 Qt 목록(QListView IconMode)과 Qtitan 카드(CardView)에 물려 좌우에 보인다.
/// 원본: 샘플 사진 폴더 · 모의 부하(1만 · 10만 개, 섬네일이 늦게 도착하는 흉내) · 실제 폴더(읽기 전용).
/// 측정: 모델 연결 시간, 메모리 증가(프로세스 전용 바이트), 스크롤(위 → 아래 단계마다 다시 그리는 시간).
class ThumbnailCompare : public QWidget
{
    Q_OBJECT
public:
    enum class Source { Sample, Mock10k, Mock100k, Folder };

    explicit ThumbnailCompare(QWidget *parent = nullptr);
    ~ThumbnailCompare() override;

    /// 원본을 바꾸고 두 구현을 새로 만든다(연결 시간 · 메모리를 잰다). Folder면 path.
    void load(Source source, const QString &path = {});
    /// 두 구현의 스크롤을 잰다 — 결과는 표와 lastReport().
    void measureScroll();
    /// 사람이 읽는 결과 요약(명령줄 --measure 출력용).
    QString lastReport() const;
    /// 모의 부하에서 섬네일이 늦게 도착하는 흉내(도구 줄의 체크 상자).
    void setArrivalSimulation(bool on);

    struct Result
    {
        double attachMs = -1;
        qint64 memoryBytes = -1;
        double scrollAvgMs = -1;
        double scrollMaxMs = -1;
        int scrollSteps = 0;
    };
    const Result &result(int backend) const { return m_results[std::size_t(backend)]; }

Q_SIGNALS:
    /// 원본이 다 읽혔다(실제 폴더는 목록을 다 읽은 뒤).
    void loaded();

private:
    void rebuildViews();
    void attachMeasured();
    void startArrival();
    void arrivalTick();
    void refreshTable();
    void refreshMemory();
    void applySize();

    QComboBox *m_source = nullptr;
    QCheckBox *m_arrival = nullptr;
    fm::ui::SegmentedControl *m_size = nullptr;
    fm::ui::Button *m_measure = nullptr;
    QLabel *m_memory = nullptr;
    QLabel *m_status = nullptr;
    std::array<QVBoxLayout *, 2> m_columns{};
    std::array<fm::filelist::ThumbnailBackend *, 2> m_views{};  // [0] Qt 목록 · [1] Qtitan 카드
    QList<fm::filelist::FileEntry> m_final;                     // 도착 흉내 — 섬네일이 다 온 뒤의 항목
    QStandardItemModel *m_table = nullptr;
    fm::filelist::FileListModel *m_mock = nullptr;
    fm::filelist::LocalFileSource *m_local = nullptr;
    fm::filelist::FileSortProxy *m_proxy = nullptr;
    QTimer *m_arrivalTimer = nullptr;
    QTimer *m_memoryTimer = nullptr;
    int m_arrived = 0;
    Source m_current = Source::Sample;
    std::array<Result, 2> m_results{};
    qint64 m_modelBytes = -1;
};

} // namespace fm::app
