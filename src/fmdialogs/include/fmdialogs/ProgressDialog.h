#pragma once

// 진행 창(02 §7 자세히 · §8 간단히) — ProgressDialog.ui 하나로 두 모드. 목업 속도 모델의 시뮬레이터로 움직인다.

#include <QDialog>
#include <QStringList>

#include <cstdint>
#include <memory>

namespace Ui {
class ProgressDialog;
}

namespace fm::dialogs {

class ProgressSimulator;

class ProgressDialog : public QDialog
{
    Q_OBJECT
public:
    enum Kind { Copy, Move, Delete, DeletePermanent };
    enum Mode { Detail, Compact };

    struct Operation
    {
        Kind kind = Copy;
        bool elevated = false;          // 관리자 권한(방패 아이콘 · 경고 배너 · 백그라운드 없음)
        QString source;                 // "D:\\Downloads"
        QString target;                 // "E:\\Backup\\Installers"
        QStringList fileNames;
        QList<qint64> fileSizes;
        QString policyText;             // "같은 이름이 있으면 매번 묻기"

        // 설정의 값 — 기본값은 목업 기본값
        enum class EscAction : std::uint8_t { ConfirmCancel, CancelImmediately, HideWindow };
        bool detailed = true;           // 파일 작업 › 자세히 보기로 시작(복사만 두 모드)
        bool closeWhenDone = true;      // 파일 작업 › "완료되면 창 닫기" 처음 값
        bool adminTitle = true;         // 관리자 권한 › 관리자 작업이면 제목에 "(관리자)"
        EscAction esc = EscAction::ConfirmCancel;  // 키보드 › 진행 창에서 Esc
    };

    explicit ProgressDialog(const Operation &operation, QWidget *parent = nullptr);
    ~ProgressDialog() override;

    Mode mode() const noexcept { return m_mode; }
    void setMode(Mode mode);
    bool isPaused() const;
    void setPaused(bool paused);
    /// 데모 반복: 끝나면 처음부터(완료되면 창 닫기를 무시한다). 끄면(기본) 끝에서 멈추고
    /// "완료되면 창 닫기"가 켜져 있으면 닫고, 아니면 완료 상태로 둔다.
    void setDemoLoop(bool on);
    bool isDone() const noexcept { return m_done; }
    /// 대기열 — 앞선 작업이 끝날 때까지 시작하지 않는다(대기열에 추가 · 동시에 실행할 작업 수). 끄면 바로 시작.
    /// "복사 · 3개 항목" — 완료 알림 글자.
    QString summaryText() const;
    bool isWaiting() const noexcept { return m_waiting; }
    void setWaiting(bool waiting);
    ProgressSimulator *simulator() const noexcept { return m_sim; }

    static QStringList variants();
    void applyVariant(const QString &id);
    /// 목업 복사 작업(D:\Downloads → E:\Backup\Installers, 3개 · 3.95 GB).
    static Operation boardCopy();

Q_SIGNALS:
    void backgroundRequested();
    /// 작업이 끝났다(창이 닫히기 전 — 대기열의 다음 작업 · 완료 알림용).
    void completed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void refresh();
    void updateTitle();
    void updateButtons();
    void setStatic(int percentTimes10, const QString &left, const QString &right, const QString &path);
    void fitSize();
    /// 목업 상태(시작 후 9초 · 약 37 %)까지 미리 진행하고 그래프에 기록을 넣는다.
    void prerollDemo();
    void finish();

    std::unique_ptr<Ui::ProgressDialog> ui;
    Operation m_op;
    ProgressSimulator *m_sim = nullptr;
    Mode m_mode = Detail;
    bool m_demoLoop = false;
    bool m_done = false;
    bool m_waiting = false;
    bool m_static = false;    // 간단히 보드 변형: 고정 값(시뮬레이터 없음)
    int m_staticPercent = 0;
};

} // namespace fm::dialogs
