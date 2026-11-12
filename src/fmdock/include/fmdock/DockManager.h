#pragma once

// 도킹 관리자(fmdock 2, docs/specs/07-docking.md). QMainWindow · QDockWidget을 쓰지 않고 컨테이너 → 분할 → 영역(탭 묶음)
// → 도크를 직접 관리한다. Qt-Advanced-Docking-System(LGPL)의 동작 · 설계를 참고해 다시 구현했다(코드는 가져오지 않음).
//
//   · 영역 · 탭 · 임의 깊이 분할, 중앙(문서) 영역, 탭 묶음 옆으로 나누기
//   · 끌어 놓기 — 미리보기 창(놓을 때 한 번만 옮김), 컨테이너 가장자리 4 · 영역 십자 5 · 탭 줄 끼워 넣기 · 사이드바 놓기
//   · 떠 있는 창 — 내부 분할, 디자인이 그린 제목 줄 + OS 이동 · 크기 조절 · Aero 스냅
//   · 자동 숨김 — 사이드바 4방향, 펼친 창, 마우스 올림 펼침
//   · 배치 저장 · 복원(JSON) · 이름 붙인 배치 · 전역 잠금 · 포커스 표시
// 모양은 fm::style(시안1 · 2 · 3, 라이트 · 다크 · 남색)이 그린다 — 스타일시트를 쓰지 않는다.

#include "fmdock/DockArea.h"
#include "fmdock/DockContainer.h"
#include "fmdock/DockGlobals.h"
#include "fmdock/DockWidget.h"
#include "fmdock/FloatingWindow.h"

#include <QByteArray>
#include <QMap>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <array>
#include <memory>

class QGridLayout;
class QMenu;

namespace fm::dock {

namespace detail {
class SideBar;
class SideTab;
class AutoHideFlyout;
class FlyoutTitle;
class DockTab;
class DockAreaTitleBar;
class DragSession;
class DropOverlay;
class FloatingTitleBar;
struct ManagerAccess;
struct DockState;
struct LayoutNode;
}

class DockManager : public QWidget
{
    Q_OBJECT

public:
    enum ConfigFlag : std::uint32_t {
        ActiveTabHasCloseButton = 1u << 0,
        HoverTabHasCloseButton = 1u << 1,
        AreaHasCloseButton = 1u << 2,
        AreaHasUndockButton = 1u << 3,
        AreaHasTabsMenuButton = 1u << 4,
        AreaHideDisabledButtons = 1u << 5,
        AreaCloseButtonClosesTab = 1u << 6,
        OpaqueSplitterResize = 1u << 7,
        DragPreviewShowsContent = 1u << 8,
        AlwaysShowTabs = 1u << 9,
        HideSingleCentralTitleBar = 1u << 10,
        FocusHighlighting = 1u << 11,
        EqualSplitOnInsertion = 1u << 12,
        MiddleButtonClosesTab = 1u << 13,
        DoubleClickUndocks = 1u << 14,
        TabsAtBottom = 1u << 15,
        CentralRejectsTools = 1u << 16,   // 도구 도크를 중앙 영역에 넣지 않는다(기본 끔 — 자유 혼합)
        DocumentsStayCentral = 1u << 17,  // 문서 도크는 중앙 영역 안에서만(기본 끔)
        FloatingNativeFrame = 1u << 18,   // 떠 있는 창에 OS 프레임(대체 경로)
        NoTopLevelWindows = 1u << 19,     // 임베드 모드 — 최상위 창을 만들지 않는다(07 §7.2)
        DefaultConfig = ActiveTabHasCloseButton | AreaHasCloseButton | AreaHasUndockButton | AreaHasTabsMenuButton
                      | AreaHideDisabledButtons | DragPreviewShowsContent | HideSingleCentralTitleBar | FocusHighlighting
                      | MiddleButtonClosesTab | DoubleClickUndocks,
    };
    Q_DECLARE_FLAGS(ConfigFlags, ConfigFlag)

    enum AutoHideFlag : std::uint32_t {
        AutoHideEnabled = 1u << 0,
        AreaHasPinButton = 1u << 1,
        PinButtonTogglesArea = 1u << 2,   // 압정이 영역 전체를 고정(Ctrl+압정은 반대)
        SideBarsIconOnly = 1u << 3,
        ShowOnMouseOver = 1u << 4,
        OpenOnDragHover = 1u << 5,
        CloseOnOutsideClick = 1u << 6,
        CloseButtonCollapses = 1u << 7,
        HasMinimizeButton = 1u << 8,
        DefaultAutoHide = AutoHideEnabled | AreaHasPinButton | OpenOnDragHover | CloseOnOutsideClick | HasMinimizeButton,
    };
    Q_DECLARE_FLAGS(AutoHideFlags, AutoHideFlag)

    /// parent가 QMainWindow면 그 central widget이 된다.
    explicit DockManager(QWidget *parent = nullptr);
    ~DockManager() override;

    ConfigFlags configFlags() const { return m_config; }
    void setConfigFlags(ConfigFlags flags);
    void setConfigFlag(ConfigFlag flag, bool on = true);
    bool testConfigFlag(ConfigFlag flag) const { return m_config.testFlag(flag); }
    AutoHideFlags autoHideFlags() const { return m_autoHide; }
    void setAutoHideFlags(AutoHideFlags flags);
    void setAutoHideFlag(AutoHideFlag flag, bool on = true);
    bool testAutoHideFlag(AutoHideFlag flag) const { return m_autoHide.testFlag(flag); }

    DockContainer *dockContainer() const { return m_container; }
    QList<FloatingWindow *> floatingWindows() const;
    QList<DockContainer *> dockContainers() const;

    // ---------------------------------------------------------------- 넣기 · 빼기

    /// 중앙 영역을 만들고 central을 고정 위젯으로 넣는다(닫기 · 이동 · 떼어 내기 · 고정 없음, 탭 숨김).
    DockArea *setCentralWidget(DockWidget *central);
    DockWidget *centralWidget() const { return m_centralWidget; }
    /// 중앙 영역이 비었을 때 보일 안내 글자.
    void setCentralPlaceholderText(const QString &text);
    QString centralPlaceholderText() const { return m_placeholderText; }

    /// where · target 규칙은 DropArea 참고. index는 탭 순서(−1 = 끝).
    DockArea *addDockWidget(DropArea where, DockWidget *dock, DockArea *target = nullptr, int index = -1);
    DockArea *addDockWidgetTab(DockWidget *dock, DockArea *target, int index = -1);
    /// 중앙 영역의 마지막 활성 영역(없으면 첫 영역 · 새 영역)에 탭으로.
    DockArea *addDockWidgetToCentral(DockWidget *dock);
    DockWidget *addAutoHideDockWidget(SideBarLocation side, DockWidget *dock);
    FloatingWindow *addFloatingDockWidget(DockWidget *dock, const QRect &geometry = {});
    /// 관리자에서 뺀다(지우지 않음, 부모 없음).
    void removeDockWidget(DockWidget *dock);

    DockWidget *findDockWidget(const QString &id) const;
    QList<DockWidget *> dockWidgets() const;
    DockWidget *focusedDockWidget() const { return m_focused; }
    DockArea *lastActiveCentralArea() const;

    /// 마우스 없이 — 끌어 놓기와 같은 결과(where · target은 addDockWidget과 같다).
    bool moveDockWidget(DockWidget *dock, DropArea where, DockArea *target = nullptr, int index = -1);

    /// 영역 크기(px) — orientation 방향으로 나뉜 가장 가까운 분할에서 그 영역이 든 칸의 크기를 정한다.
    /// 나머지 칸은 비율대로 줄거나 는다. 보이기 전에 불러도 된다.
    void setAreaSize(DockArea *area, Qt::Orientation orientation, int size);

    // ---------------------------------------------------------------- 자동 숨김

    bool setAutoHidden(DockWidget *dock, bool on, SideBarLocation side = SideBarLocation::None);
    /// 펼친다(nullptr이면 접는다).
    void expandAutoHide(DockWidget *dock);
    DockWidget *expandedAutoHideDockWidget() const;
    QList<DockWidget *> autoHideDockWidgets(SideBarLocation side) const;

    /// 영역 사이 포커스 이동(앱이 F6 등에 묶는다).
    void focusNextArea(bool forward = true);

    // ---------------------------------------------------------------- 상태 · 배치

    /// JSON(07 §4.8). userVersion이 다른 상태는 restoreState가 거절한다.
    QByteArray saveState(int userVersion = 0) const;
    bool restoreState(const QByteArray &state, int userVersion = 0);
    bool isRestoringState() const { return m_restoring; }

    void setDefaultLayout(const QByteArray &state) { m_defaultLayout = state; }
    QByteArray defaultLayout() const { return m_defaultLayout; }
    bool resetToDefaultLayout();

    void addPerspective(const QString &name);
    bool openPerspective(const QString &name);
    void removePerspective(const QString &name);
    QStringList perspectiveNames() const { return m_perspectives.keys(); }
    QMap<QString, QByteArray> perspectives() const { return m_perspectives; }
    void setPerspectives(const QMap<QString, QByteArray> &perspectives);

    /// 전역 잠금 — Closable · Movable · Floatable · Pinnable 중 고른 기능을 모든 도크에서 끈 것처럼(07 §4.9).
    void lockFeatures(DockWidget::Features locked);
    DockWidget::Features lockedFeatures() const { return m_locked; }

    /// 보기 메뉴: 도크 켜기 · 끄기 + 배치(저장 · 적용 · 삭제) + 도크 잠금 + 기본 배치로 되돌리기. 열 때마다 다시 채운다.
    void populateViewMenu(QMenu *menu);

    /// 떠 있는 창에도 window의 창 단축키(Qt::WindowShortcut)가 동작하게 한다(07 §4.5.6).
    void setShortcutSource(QWidget *window);

Q_SIGNALS:
    void focusedDockWidgetChanged(fm::dock::DockWidget *old, fm::dock::DockWidget *now);
    void dockAreaCreated(fm::dock::DockArea *area);
    void dockWidgetAdded(fm::dock::DockWidget *dock);
    void dockWidgetRemoved(fm::dock::DockWidget *dock);
    void floatingWindowCreated(fm::dock::FloatingWindow *window);
    void restoringState();
    void stateRestored();
    void perspectivesChanged();
    void lockedFeaturesChanged();
    void autoHideChanged(fm::dock::DockWidget *dock, bool autoHidden);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class DockArea;
    friend class DockContainer;
    friend class DockWidget;
    friend class FloatingWindow;
    friend class detail::DragSession;
    friend class detail::SideBar;
    friend class detail::SideTab;
    friend class detail::AutoHideFlyout;
    friend class detail::FlyoutTitle;
    friend class detail::DockTab;
    friend class detail::DockAreaTitleBar;
    friend class detail::FloatingTitleBar;
    friend struct detail::ManagerAccess;

    // 등록 · 배치
    void registerDock(DockWidget *dock);
    void unregisterDock(DockWidget *dock);
    bool canDropInto(const QList<DockWidget *> &docks, const DockArea *target, bool center) const;
    void detachDock(DockWidget *dock);
    void placeDocks(const QList<DockWidget *> &docks, DropArea where, DockArea *target, int index,
                    DockContainer *container);
    FloatingWindow *createFloatingWindow();
    FloatingWindow *floatDocks(const QList<DockWidget *> &docks, const QRect &geometry);
    void floatingWindowEmptied(FloatingWindow *window);
    void dockClosedStateChanged(DockWidget *dock);
    void placeClosedDock(DockWidget *dock);
    /// 떠 있는 도크를 마지막으로 붙어 있던 자리로(없으면 오른쪽 가장자리).
    void dockBack(DockWidget *dock);
    QRect defaultFloatingGeometry(const QList<DockWidget *> &docks) const;

    void refreshAll();

    // 자동 숨김
    void pinDock(DockWidget *dock, SideBarLocation side, int index);
    void takeFromAutoHide(DockWidget *dock);
    void moveToSideBar(DockWidget *dock, SideBarLocation side, int index);
    detail::SideBar *sideBar(SideBarLocation side) const;
    void updateSideBars();
    void positionFlyout(detail::AutoHideFlyout *flyout) const;
    void collapseAutoHide();
    SideBarLocation sideForDock(const DockWidget *dock) const;
    void restoreFromAutoHide(DockWidget *dock);
    void pinArea(DockArea *area, SideBarLocation side);

    // 포커스
    void setFocusedDock(DockWidget *dock);
    void onFocusChanged(QWidget *old, QWidget *now);

    // 끌기
    void startDrag(const QList<DockWidget *> &docks, DockArea *sourceArea, QWidget *origin, const QPoint &globalPos,
                   const QPoint &hotSpot);
    void dragFinished();

    // 상태
    detail::LayoutNode nodeFor(QWidget *widget) const;
    QWidget *buildNode(const detail::LayoutNode &node, DockContainer *container, bool insideCentral,
                       QList<DockWidget *> &placed, QList<QPair<QWidget *, QList<int>>> &sizes);
    bool applyState(const detail::DockState &state);
    void placeUnplacedDocks(const QList<DockWidget *> &unplaced);

    // 단축키
    void shareShortcuts(QWidget *window);

    QGridLayout *m_grid = nullptr;
    DockContainer *m_container = nullptr;
    std::array<detail::SideBar *, 4> m_sideBars{};  // Left · Top · Right · Bottom
    QList<QPointer<FloatingWindow>> m_floating;
    QList<QPointer<DockWidget>> m_docks;            // 등록 순서(보기 메뉴 순서)
    QPointer<DockWidget> m_centralWidget;
    QPointer<DockWidget> m_focused;
    QPointer<DockArea> m_lastCentralArea;
    QPointer<detail::DragSession> m_drag;
    QPointer<QWidget> m_shortcutSource;
    QString m_placeholderText;
    QByteArray m_defaultLayout;
    QMap<QString, QByteArray> m_perspectives;
    ConfigFlags m_config = DefaultConfig;
    AutoHideFlags m_autoHide = DefaultAutoHide;
    DockWidget::Features m_locked;
    bool m_restoring = false;
    bool m_dying = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DockManager::ConfigFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(DockManager::AutoHideFlags)

} // namespace fm::dock
