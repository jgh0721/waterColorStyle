#pragma once

// fmdock 2 내부 — 탭 바 · 영역 제목 줄 · 분할 · 사이드바 · 펼친 창 · 끌기 · 떠 있는 창 제목 줄(07 §3.3).
// 모양은 모두 fm::style(QStyle 요소 · StylePaint 도우미)이 그린다. 색 상수를 두지 않는다.

#include "fmdock/DockManager.h"

#include "DropResolver_p.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>

#include <QAbstractButton>
#include <QPixmap>
#include <QPointer>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>

class QBoxLayout;
class QStyleOptionTab;
class QVBoxLayout;

namespace fm::dock::detail {

// ------------------------------------------------------------------------------------- 공통

const fm::style::ThemeColors &colorsOf(const QWidget *widget);
fm::style::DockMetrics metricsOf(const QWidget *widget);
/// 스타일의 탭 높이(CT_TabBarTab).
int tabHeightOf(const QWidget *widget);
/// 탭 줄 높이 = 탭 높이 + stripTopGap.
int stripHeightOf(const QWidget *widget);
/// 탭 · 사이드 탭 글꼴(12 px).
QFont tabFontOf(const QWidget *widget);
/// 화면 없는 플랫폼(offscreen · minimal) — 네이티브 창 처리를 건너뛴다.
bool isHeadlessPlatform();
/// 위젯을 품은 도크(없으면 nullptr).
DockWidget *dockOf(const QWidget *widget);

/// 탭 · 사이드 탭 메뉴(07 §4.1-6).
void execDockMenu(DockWidget *dock, const QPoint &globalPos, QWidget *parent);
/// 영역 제목 줄 빈 곳 메뉴.
void execAreaMenu(DockArea *area, const QPoint &globalPos, QWidget *parent);

// ------------------------------------------------------------------------------------- 단추

/// 영역 · 펼친 창 단추 — 스타일이 fmDockButton 종류로 바탕과 기호를 그린다(flat = 탭 줄 위).
class DockButton : public QAbstractButton
{
    Q_OBJECT
public:
    DockButton(const QString &kind, bool flat, QWidget *parent);
    void setKind(const QString &kind);
    QString kind() const { return m_kind; }
    void setExtent(int px);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    QString m_kind;
    bool m_flat = true;
    int m_extent = 0;
};

/// 탭 닫기 단추 — PE_IndicatorTabClose.
class TabCloseButton : public QAbstractButton
{
    Q_OBJECT
public:
    explicit TabCloseButton(QWidget *parent);
    void setSelectedTab(bool selected);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    bool m_selected = false;
};

// ------------------------------------------------------------------------------------- 탭

class DockTabBar;

class DockTab : public QWidget
{
    Q_OBJECT
public:
    DockTab(DockWidget *dock, DockTabBar *bar);
    DockWidget *dock() const { return m_dock; }
    void setCurrent(bool current);
    bool isCurrent() const { return m_current; }
    void updateState();
    /// 글자를 줄여 이 폭에 맞춘다(−1 = 줄이지 않음).
    void setTextLimit(int width);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void initStyleOption(QStyleOptionTab *option) const;
    bool showsClose() const;
    int trailingWidth() const;
    QString shownText() const;

    QPointer<DockWidget> m_dock;
    DockTabBar *m_bar = nullptr;
    TabCloseButton *m_close = nullptr;
    bool m_current = false;
    bool m_hover = false;
    int m_textLimit = -1;
    QPoint m_pressGlobal;
    bool m_pressed = false;
    bool m_reordering = false;
};

class DockTabBar : public QWidget
{
    Q_OBJECT
public:
    DockTabBar(DockArea *area, QWidget *parent);
    DockArea *area() const { return m_area; }
    int count() const { return int(m_tabs.size()); }
    DockTab *tabAt(int index) const { return index >= 0 && index < m_tabs.size() ? m_tabs[index] : nullptr; }
    DockTab *tabFor(const DockWidget *dock) const;
    void insertTab(int index, DockWidget *dock);
    void removeTab(DockWidget *dock);
    void moveTab(int from, int to);
    void setCurrentIndex(int index);
    int hiddenCount() const { return m_hidden; }
    bool atBottom() const;
    void relayout();
    /// 탭 끌어 순서 바꾸기 — tab을 global x 자리로.
    void dragTabTo(DockTab *tab, int globalX);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    DockArea *m_area = nullptr;
    QList<DockTab *> m_tabs;
    int m_hidden = 0;
};

class DockAreaTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit DockAreaTitleBar(DockArea *area);
    DockTabBar *tabBar() const { return m_tabBar; }
    void updateButtons();
    /// 넘친 탭 수(+N)만 고친다 — 탭 바 배치가 부른다.
    void updateOverflow();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void showTabsMenu();

    DockArea *m_area = nullptr;
    DockTabBar *m_tabBar = nullptr;
    QWidget *m_overflow = nullptr;
    DockButton *m_menu = nullptr;
    DockButton *m_undock = nullptr;
    DockButton *m_pin = nullptr;
    DockButton *m_close = nullptr;
    QPoint m_pressGlobal;
    bool m_pressed = false;
};

// ------------------------------------------------------------------------------------- 분할

class DockSplitter : public QSplitter
{
    Q_OBJECT
public:
    DockSplitter(Qt::Orientation orientation, DockManager *manager, QWidget *parent = nullptr);
    bool isCentral() const { return m_central; }
    void setCentral(bool central) { m_central = central; }
    void applyMetrics();

protected:
    QSplitterHandle *createHandle() override;
    void changeEvent(QEvent *event) override;

private:
    DockManager *m_manager = nullptr;
    bool m_central = false;
};

class DockSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    DockSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    bool m_hover = false;
};

/// 빈 중앙 영역 — Surface 바탕 + 안내 글자.
class CentralPlaceholder : public QWidget
{
    Q_OBJECT
public:
    CentralPlaceholder(DockManager *manager, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPointer<DockManager> m_manager;
};

// ------------------------------------------------------------------------------------- 자동 숨김

class SideBar;

class SideTab : public QAbstractButton
{
    Q_OBJECT
public:
    SideTab(DockWidget *dock, SideBar *bar);
    DockWidget *dock() const { return m_dock; }
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    void updateState();
    QSize sizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void initStyleOption(QStyleOptionTab *option) const;

    QPointer<DockWidget> m_dock;
    SideBar *m_bar = nullptr;
    bool m_expanded = false;
    QPoint m_pressGlobal;
    bool m_pressed = false;
    bool m_dragged = false;
    QTimer m_hoverTimer;
};

class SideBar : public QWidget
{
    Q_OBJECT
public:
    SideBar(DockManager *manager, SideBarLocation side);
    DockManager *dockManager() const { return m_manager; }
    SideBarLocation side() const { return m_side; }
    bool isVertical() const { return m_side == SideBarLocation::Left || m_side == SideBarLocation::Right; }
    void insertTab(int index, DockWidget *dock);
    void removeTab(DockWidget *dock);
    SideTab *tabFor(const DockWidget *dock) const;
    QList<SideTab *> tabs() const { return m_tabs; }
    QList<DockWidget *> docks() const;
    void updateVisibility();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    DockManager *m_manager = nullptr;
    SideBarLocation m_side = SideBarLocation::Left;
    QBoxLayout *m_layout = nullptr;
    QList<SideTab *> m_tabs;
};

class AutoHideFlyout : public QWidget
{
    Q_OBJECT
public:
    AutoHideFlyout(DockManager *manager, DockWidget *dock, SideBarLocation side);
    ~AutoHideFlyout() override;
    DockWidget *dock() const { return m_dock; }
    SideBarLocation side() const { return m_side; }
    void setSide(SideBarLocation side);
    void setActive(bool active);
    bool isActiveFlyout() const { return m_active; }
    /// 도크를 떼어 돌려준다(부모는 그대로 — 다음 자리가 가져간다).
    DockWidget *releaseDock();
    void updateButtons();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    friend class FlyoutTitle;
    bool onResizeEdge(const QPoint &pos) const;
    void layoutChildren();

    DockManager *m_manager = nullptr;
    QPointer<DockWidget> m_dock;
    SideBarLocation m_side = SideBarLocation::Left;
    QWidget *m_title = nullptr;
    DockButton *m_unpin = nullptr;
    DockButton *m_minimize = nullptr;
    DockButton *m_close = nullptr;
    bool m_active = false;
    bool m_resizing = false;
    QPoint m_pressGlobal;
    int m_pressSize = 0;
};

// ------------------------------------------------------------------------------------- 끌기

class DragPreviewWindow : public QWidget
{
    Q_OBJECT
public:
    DragPreviewWindow(DockManager *manager, const QString &title, const QPixmap &content, const QSize &size,
                      bool topLevel);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QPixmap m_content;
};

class DropOverlay : public QWidget
{
    Q_OBJECT
public:
    DropOverlay(DockManager *manager, bool topLevel);
    /// globalRect를 덮고 표시를 그린다.
    void showVisual(const QRect &globalRect, const DropVisual &visual);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_topLevel = true;
    QRect m_global;
    DropVisual m_visual;
};

class DragSession : public QObject
{
    Q_OBJECT
public:
    enum class Mode : std::uint8_t { Preview, MoveWindow };

    DragSession(DockManager *manager, const QList<DockWidget *> &docks, DockArea *sourceArea,
                FloatingWindow *movingWindow, Mode mode, const QPoint &hotSpot, const QSize &size);
    ~DragSession() override;

    void move(const QPoint &globalPos);
    /// 놓는다. 옮겼으면 true.
    bool drop(const QPoint &globalPos);
    void cancel();
    bool isCancelled() const { return m_cancelled; }
    DropTarget target() const { return m_target; }
    DockContainer *targetContainer() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Candidate
    {
        QPointer<DockContainer> container;
        DropContainerGeom geom;
        QList<QPointer<DockArea>> areas;   // geom.areas와 같은 순서(빈 중앙 자리는 nullptr)
        QRect overlayRect;
    };

    void buildCandidates();
    Candidate *candidateAt(const QPoint &globalPos);
    bool canFloat() const;
    bool allTools() const;
    bool anyDocument() const;
    void finish();

    QPointer<DockManager> m_manager;
    QList<QPointer<DockWidget>> m_docks;
    QPointer<DockArea> m_sourceArea;
    QPointer<FloatingWindow> m_window;
    Mode m_mode = Mode::Preview;
    QPoint m_hotSpot;
    QSize m_size;
    QList<Candidate> m_candidates;
    int m_current = -1;
    DropTarget m_target;
    QPointer<DragPreviewWindow> m_preview;
    QPointer<DropOverlay> m_overlay;
    bool m_cancelled = false;
    bool m_finished = false;
    QTimer m_hoverTimer;
    QPointer<DockWidget> m_hoverDock;
};

// ------------------------------------------------------------------------------------- 떠 있는 창

class CaptionButtonWidget : public QAbstractButton
{
    Q_OBJECT
public:
    CaptionButtonWidget(fm::style::CaptionButton kind, QWidget *parent);
    void setKind(fm::style::CaptionButton kind);
    void setActiveCaption(bool active);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    fm::style::CaptionButton m_kind;
    bool m_active = true;
};

class FloatingTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingTitleBar(FloatingWindow *window);
    void setTitle(const QString &title, const QIcon &icon);
    void setActive(bool active);
    void updateMaximized();
    /// 창 이동에 쓰는 자리인가(단추 밖) — 이 위젯 좌표.
    bool isCaptionArea(const QPoint &pos) const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    FloatingWindow *m_window = nullptr;
    CaptionButtonWidget *m_maximize = nullptr;
    CaptionButtonWidget *m_close = nullptr;
    QString m_title;
    QIcon m_icon;
    bool m_active = true;
    QPoint m_pressGlobal;
    bool m_pressed = false;
};

// ------------------------------------------------------------------------------------- 관리자 내부 동작

/// 메뉴 같은 자유 함수 · 내부 위젯이 관리자 · 도크의 내부 동작을 부를 때.
struct ManagerAccess
{
    static void pinArea(DockManager *m, DockArea *area, SideBarLocation side) { m->pinArea(area, side); }
    static FloatingWindow *floatDocks(DockManager *m, const QList<DockWidget *> &docks, const QRect &geometry)
    {
        return m->floatDocks(docks, geometry);
    }
    static QRect defaultFloatingGeometry(DockManager *m, const QList<DockWidget *> &docks)
    {
        return m->defaultFloatingGeometry(docks);
    }
    static SideBarLocation sideForDock(DockManager *m, const DockWidget *dock) { return m->sideForDock(dock); }
    static void collapseAutoHide(DockManager *m) { m->collapseAutoHide(); }
    static void startDrag(DockManager *m, const QList<DockWidget *> &docks, DockArea *source, QWidget *origin,
                          const QPoint &globalPos, const QPoint &hotSpot)
    {
        m->startDrag(docks, source, origin, globalPos, hotSpot);
    }
    static DragSession *drag(DockManager *m) { return m->m_drag.data(); }
    static void setAutoHideSize(DockWidget *dock, int size) { dock->m_autoHideSize = size; }
    static int autoHideSize(const DockWidget *dock) { return dock->m_autoHideSize; }
    static SideBar *sideBar(const DockManager *m, SideBarLocation side) { return m->sideBar(side); }
    static void positionFlyout(const DockManager *m, AutoHideFlyout *flyout) { m->positionFlyout(flyout); }
    static void restoreFromAutoHide(DockManager *m, DockWidget *dock) { m->restoreFromAutoHide(dock); }
    static void setFocusedDock(DockManager *m, DockWidget *dock) { m->setFocusedDock(dock); }
    static void dockBack(DockManager *m, DockWidget *dock) { m->dockBack(dock); }
};

} // namespace fm::dock::detail
