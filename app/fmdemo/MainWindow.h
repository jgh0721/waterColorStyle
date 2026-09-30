#pragma once

#include <fmfilelist/ListAppearance.h>

#include <QHash>
#include <QMainWindow>

#include <memory>

class QAction;
class QActionGroup;
class QMenu;
class QSplitter;
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

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createCentral();
    void refreshIcons();
    void syncThemeControls();
    void updateWindowTitle();
    void updateActionStates();
    void showPending(const QString &title);
    /// 활성 패널의 대상(표시 · 커서)과 반대 패널 경로로 대화상자 입력을 만든다.
    fm::dialogs::FileOpContext operationContext() const;
    /// 파일 작업 대화상자를 연다(copy · move · rename · delete · deletePermanent · newFolder · newFile · multiRename).
    /// 데모는 읽기 전용 — 복사 · 이동 · 삭제는 확인 후 진행 창만 시뮬레이터로 보인다.
    void openFileOperation(const QString &id);
    QAction *action(const QString &id) const { return m_actions.value(id); }

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
};

} // namespace fm::app
