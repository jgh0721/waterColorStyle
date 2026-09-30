#pragma once

#include <fmfilelist/ListAppearance.h>

#include <QHash>
#include <QMainWindow>

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
    QAction *action(const QString &id) const { return m_actions.value(id); }

    FilePanel *m_left = nullptr;
    FilePanel *m_right = nullptr;
    FilePanel *m_active = nullptr;
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
