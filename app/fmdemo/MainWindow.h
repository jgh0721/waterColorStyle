#pragma once

#include <fmfilelist/ListAppearance.h>
#include <fmsettings/AppSettings.h>

#include <QDateTime>
#include <QHash>
#include <QMainWindow>
#include <QPointer>

#include <memory>

class QAction;
class QDialog;
class QActionGroup;
class QMenu;
class QSplitter;
class QSystemTrayIcon;
class QToolBar;

namespace fm::ui {
class CommandLine;
class FindBox;
class FunctionKeyBar;
class SegmentedControl;
class Switch;
}

namespace fm::dialogs {
class ElevationFlow;
class ProgressDialog;
struct FileOpContext;
class LocalProbe;
class MockProbe;
}

namespace fm::app {

class FilePanel;

/// 메인 창(01 §1 · §7.1, 06 §5.2) — 메뉴 막대 · 도구 모음(찾기 · 디자인 스위치) · 두 파일 패널 · 명령줄 · 기능 키.
/// 두 디자인의 위젯 트리가 같고, 디자인 전환은 스타일 교체와 스타일 치수만으로 이루어진다.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    FilePanel *leftPanel() const noexcept { return m_left; }
    FilePanel *rightPanel() const noexcept { return m_right; }
    FilePanel *activePanel() const noexcept { return m_active; }
    void setActivePanel(FilePanel *panel);

    /// Main 보드 기본 상태(01 §1.10): 왼쪽 샘플 fm-core 1줄 · 커서 src, 오른쪽 샘플 Downloads 자동 · 커서 PDF, 오른쪽 활성.
    void loadBoardState();

    const fm::filelist::ListAppearance &listAppearance() const noexcept { return m_listAppearance; }
    void setListAppearance(const fm::filelist::ListAppearance &appearance);

    /// 권한 흐름 시뮬레이션(03 §0) — 0 = 보호된 폴더로 복사, 1 = 삭제 · 소유권. 흐름은 창의 자식으로 남는다.
    fm::dialogs::ElevationFlow *startElevationFlow(int scenario);

    /// 설정 창(단일 인스턴스, 창 모달). 적용하면 SettingsStore::changed로 applySettings가 불린다.
    void openSettings();
    /// 적용된 설정을 창에 반영한다 — 테마 · 목록 · 섬네일 모양, 숨김 파일 · 폴더 먼저, 파일 그룹, 명령줄 · 기능 키 막대, 단축키.
    void applySettings(fm::settings::Sections sections);
    /// 지금 창 상태(도구 모음의 테마, 보드의 목록 표시)를 설정 보관소에 옮긴다 — 설정 파일이 없을 때 첫 상태.
    void captureSettings();
    /// 도구 › 대화상자 카탈로그 · 섬네일 비교(PLAN §9) — 창 하나씩.
    void openCatalog();
    void openThumbnailCompare();

    /// 파일 작업의 진행 창(모덜리스) — 설정 › 파일 작업의 진행 창 표시 지연 · 자세히 보기 · 완료되면 닫기 · 동시에 실행할
    /// 작업 수와 키보드 › 진행 창 Esc를 따른다. kind = ProgressDialog::Kind, queued(대기열에 추가)면 앞선 작업이 끝난 뒤 시작한다.
    fm::dialogs::ProgressDialog *startJob(int kind, const QString &source, const QString &target,
                                         const QStringList &names, const QList<qint64> &sizes, const QString &policy = {},
                                         bool queued = false);
    /// 시작해서 아직 끝나지 않은 작업 수(대기 중 제외).
    int runningJobs() const;
    QList<fm::dialogs::ProgressDialog *> jobs() const;

    /// 일반 › 시작할 때 — 마지막 탭과 폴더 복원 · 홈 폴더 · 지정한 폴더(실제 폴더). 설정을 읽은 뒤 한 번 부른다.
    void applyStartup();
    /// 지금 두 패널의 탭(끝낼 때 설정 파일에 저장 — 마지막 탭과 폴더 복원).
    fm::settings::SessionState sessionState() const;
    void restoreSession(const fm::settings::SessionState &session);
    /// 다른 인스턴스가 넘긴 요청(창 하나만 실행) — 활성 패널에 새 탭, localPath가 있으면 그 폴더.
    void openInNewTab(const QString &localPath);
    /// 알림 영역 아이콘(일반 › 알림 영역 아이콘 — 표시 안 함 · 작업 중에만 · 항상). 시스템에 없으면 nullptr.
    QSystemTrayIcon *trayIcon() const noexcept { return m_tray; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createCentral();
    void refreshIcons();
    void syncThemeControls();
    void updateWindowTitle();
    void updateActionStates();
    /// 활성 패널의 대상(표시 · 커서)과 반대 패널 경로로 대화상자 입력을 만든다.
    fm::dialogs::FileOpContext operationContext() const;
    /// 파일 작업 대화상자를 연다(copy · move · rename · delete · deletePermanent · newFolder · newFile · multiRename).
    /// 데모는 읽기 전용 — 복사 · 이동 · 삭제는 확인 후 진행 창만 시뮬레이터로 보인다.
    void openFileOperation(const QString &id);
    QAction *action(const QString &id) const { return m_actions.value(id); }
    void applyKeyBindings(const fm::settings::KeyBindingSettings &keys);
    void startWaitingJobs();
    void notifyJobDone(fm::dialogs::ProgressDialog *job);
    /// 권한 상승 도우미가 아직 살아 있는지(설정 › 관리자 권한 › 도우미 유지 · 마지막 승인 시각).
    bool helperAlive() const;
    void updateTray();

    FilePanel *m_left = nullptr;
    FilePanel *m_right = nullptr;
    FilePanel *m_active = nullptr;
    std::unique_ptr<fm::dialogs::LocalProbe> m_localProbe;
    std::unique_ptr<fm::dialogs::MockProbe> m_mockProbe;
    QSplitter *m_splitter = nullptr;
    fm::ui::CommandLine *m_commandLine = nullptr;
    fm::ui::FunctionKeyBar *m_functionKeys = nullptr;
    QToolBar *m_toolBar = nullptr;
    fm::ui::FindBox *m_find = nullptr;
    fm::ui::Switch *m_watercolor = nullptr;
    fm::ui::SegmentedControl *m_scheme = nullptr;
    fm::ui::SegmentedControl *m_tone = nullptr;
    QHash<QString, QAction *> m_actions;
    QActionGroup *m_viewModes = nullptr;
    QActionGroup *m_backends = nullptr;
    QActionGroup *m_sep1 = nullptr;
    QActionGroup *m_sep2 = nullptr;
    fm::filelist::ListAppearance m_listAppearance;
    bool m_syncing = false;
    QPointer<QDialog> m_settingsDialog;
    QPointer<QWidget> m_catalog;
    QPointer<QWidget> m_compare;
    QList<QPointer<fm::dialogs::ProgressDialog>> m_jobs;
    QDateTime m_helperApprovedAt;
    QSystemTrayIcon *m_tray = nullptr;
    bool m_trayHold = false;  // 완료 알림을 보이는 동안 작업 중에만 모드에서도 잠시 남긴다
};

} // namespace fm::app
