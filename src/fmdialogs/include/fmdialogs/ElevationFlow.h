#pragma once

// 권한 요청 흐름 시뮬레이션(docs/specs/03 §0 · PLAN §7.3) — 사전 확인 → 처리 중 거부 → 승인(UAC 흉내) → 관리자 진행 창,
// UAC 거부 · 시간 초과 → 실패 창, 관리자로도 거부 → 소유권 창. 실제 파일 · 권한은 건드리지 않는다.

#include "fmdialogs/ElevationPrompt.h"
#include "fmdialogs/ProgressDialog.h"

#include <QDialog>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>

class QLabel;
class QTimer;

namespace fm::dialogs {

/// UAC 확인 창 흉내 — 흐름을 확인하려는 데모 창이며 제목에 "시뮬레이션"을 밝힌다.
/// 설정 "UAC 응답 기다리기"를 흉내 내 timeoutSeconds가 지나면 시간 초과로 끝난다.
class UacSimulationDialog : public QDialog
{
    Q_OBJECT
public:
    enum Answer { Yes, No, Timeout };
    Q_ENUM(Answer)

    explicit UacSimulationDialog(const QString &program, int timeoutSeconds = 15, QWidget *parent = nullptr);

    void respond(Answer answer);
    int secondsLeft() const noexcept { return m_left; }

Q_SIGNALS:
    void answered(fm::dialogs::UacSimulationDialog::Answer answer);

public Q_SLOTS:
    void reject() override;

private:
    void tick();
    static QString countdownText(int seconds);

    QLabel *m_countdown = nullptr;
    QTimer *m_timer = nullptr;
    int m_left = 0;
    bool m_done = false;
};

class ElevationFlow : public QObject
{
    Q_OBJECT
public:
    enum Scenario {
        CopyToProtected,      // 보호된 폴더로 복사: 항목마다 거부 → 승인 · 건너뛰기 · 실패 · 다시 시도
        DeleteWithOwnership,  // 사전 확인 → 승인 → TrustedInstaller 소유 폴더 → 소유권 창(기본 = 건너뛰기)
    };
    Q_ENUM(Scenario)

    /// 설정 › 관리자 권한 · 파일 작업 · 키보드의 값. 기본값 = 목업 흐름(사전 확인 켬 · 남은 항목에도 적용 켬).
    struct Options
    {
        bool preflight = true;                     // 작업 전 사전 확인 — 끄면 처리 중 거부 창부터
        bool applyToRemaining = true;              // 거부 창 "남은 항목에도 적용" 처음 값
        int uacTimeout = 15;                       // UAC 흉내의 시간 제한(초, 0 = 계속 기다림)
        bool helperRunning = false;                // 권한 상승 도우미가 아직 살아 있다(5분 · 끝날 때까지) — UAC를 다시 묻지 않는다
        bool askOwnership = true;                  // 끄면 소유권 창 없이 조용히 건너뛴다
        elev::Choice ownershipDefault = elev::Choice::Skip;  // 소유권 창의 Enter 단추
        bool backupAcl = true;                     // 소유권을 가져오기 전 원래 권한 저장
        ProgressDialog::Operation progress;        // 진행 창 설정(자세히 · 완료되면 닫기 · 관리자 제목 · Esc)만 쓴다
    };

    ElevationFlow(Scenario scenario, QWidget *window);
    ~ElevationFlow() override;
    const Options &options() const noexcept { return m_options; }
    void setOptions(const Options &options) { m_options = options; }

    void start();
    Scenario scenario() const noexcept { return m_scenario; }
    /// 지금 열린 권한 · UAC 창(자동화 · 테스트). 없으면 nullptr.
    QDialog *currentDialog() const { return m_current; }
    /// 단계 기록 — "권한 요청: vc_redist.x64.exe" · "UAC: 예" …
    QStringList log() const { return m_log; }
    bool isFinished() const noexcept { return m_finished; }
    /// UAC 흉내 창의 시간 제한(초).
    void setUacTimeout(int seconds) { m_options.uacTimeout = seconds; }

Q_SIGNALS:
    void stepChanged(const QString &step);
    void finished(const QString &summary);
    /// UAC를 승인했다(도우미 수명 — 창을 연 쪽이 다음 작업의 helperRunning을 정한다).
    void helperApproved();

private:
    struct Item
    {
        elev::ItemInfo info;
        QString sourceDir;
    };

    void note(const QString &step);
    void finish(const QString &summary);
    void later(std::function<void()> step);
    void askCopy(int index);
    void askCopyFailed(int index);
    void copyElevated(int index);
    void requestUac(std::function<void()> approved, std::function<void()> denied);
    void showProgress(bool elevated, int kind, const QStringList &names, const QList<qint64> &sizes);
    void startDeleteFlow();
    void deleteSkipNeedingAdmin();
    void deleteUacDenied();
    void deleteElevated();

    Scenario m_scenario;
    QPointer<QWidget> m_window;
    QPointer<QDialog> m_current;
    QList<Item> m_items;
    QStringList m_log;
    Options m_options;
    int m_copied = 0;
    int m_skipped = 0;
    bool m_finished = false;
};

} // namespace fm::dialogs
