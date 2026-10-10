#pragma once

// fmdock 내부 — 관리자 내부 상태, 사용자 제목 줄, 끌어 놓기, 자동 숨김 사이드바.

#include "fmdock/DockManager.h"

#include <fmstyle/StylePaint.h>

#include <QAbstractButton>
#include <QDockWidget>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QTabBar>
#include <QToolBar>

#include <optional>

class QAction;
class QStyleOptionTab;

namespace fm::dock {

class DockManagerPrivate;
class DockDrag;
class SideBar;
class SideTab;

/// 놓을 자리 — 표시 하나. dock이 있으면 그 도크 기준(옆 · 탭), 없으면 메인 창 가장자리.
struct DropTarget
{
    fm::style::DropSpot spot = fm::style::DropSpot::Center;
    QPointer<QDockWidget> dock;
};

// ---------------------------------------------------------------------------------------------
// 사용자 제목 줄

/// 제목 줄 단추 — 스타일이 바탕과 기호를 함께 그린다(fmDockButton).
class DockTitleButton : public QAbstractButton
{
    Q_OBJECT
public:
    DockTitleButton(const QString &kind, QWidget *parent);
    void setKind(const QString &kind);
    QString kind() const { return m_kind; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_kind;
};

class DockTitleBar : public QWidget
{
    Q_OBJECT
public:
    DockTitleBar(QDockWidget *dock, DockManagerPrivate *manager);
    ~DockTitleBar() override;

    /// 기능 · 떠 있음 · 자동 숨김에 맞춰 단추를 보이고 배치한다.
    void updateButtons();
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

private:
    void layoutButtons();
    int buttonSize() const;

    QDockWidget *m_dock;
    DockManagerPrivate *m_manager;
    DockTitleButton *m_pin;
    DockTitleButton *m_float;
    DockTitleButton *m_close;
    bool m_pressed = false;
    QPoint m_pressGlobal;
    QPoint m_offset;  // 누른 곳 — 도크 왼쪽 위 기준
    std::unique_ptr<DockDrag> m_drag;
};

// ---------------------------------------------------------------------------------------------
// 끌어 놓기

/// 끌기 중 메인 창 위에 겹쳐 그리는 표시 — 바깥 가장자리 넷 + 마우스 아래 도크의 십자, 놓일 자리 미리보기.
class DropOverlay : public QWidget
{
    Q_OBJECT
public:
    DropOverlay(DockManagerPrivate *manager, QDockWidget *dragged);

    /// point(메인 창 좌표)에 맞춰 표시를 고치고 지금 가리키는 자리를 돌려준다.
    std::optional<DropTarget> track(const QPoint &point);

    /// 표시 단추 자리 — 바깥은 contentRect 가장자리, 십자는 도크 가운데(메인 창 좌표).
    static QRect indicatorRect(fm::style::DropSpot spot, const QRect &content, const QRect &dockRect);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<fm::style::DropSpot> spotsFor(const QDockWidget *dock) const;

    DockManagerPrivate *m_manager;
    QPointer<QDockWidget> m_dragged;
    QPointer<QDockWidget> m_target;  // 십자를 보일 도크
    std::optional<DropTarget> m_hover;
};

/// 제목 줄을 눌러 끄는 동안 — 떠 있는 도크는 창을 옮기고(반투명), 붙은 도크는 그대로 두고 표시만 보인다.
class DockDrag : public QObject
{
    Q_OBJECT
public:
    DockDrag(DockManagerPrivate *manager, QDockWidget *dock, const QPoint &offset);
    ~DockDrag() override;

    void move(const QPoint &global);
    void drop(const QPoint &global);
    bool isCancelled() const noexcept { return m_cancelled; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void finish();

    DockManagerPrivate *m_manager;
    QPointer<QDockWidget> m_dock;
    QPoint m_offset;
    QPointer<DropOverlay> m_overlay;
    bool m_wasFloating = false;
    QRect m_startGeometry;
    bool m_cancelled = false;
};

// ---------------------------------------------------------------------------------------------
// 자동 숨김

/// 사이드바의 탭 하나 — 스타일의 탭 요소(왼쪽 · 오른쪽 · 아래 모양)로 그린다.
class SideTab : public QAbstractButton
{
    Q_OBJECT
public:
    SideTab(QDockWidget *dock, Qt::DockWidgetArea side, QWidget *parent = nullptr);
    QDockWidget *dock() const { return m_dock; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void initOption(QStyleOptionTab *option) const;

    QPointer<QDockWidget> m_dock;
    Qt::DockWidgetArea m_side;
};

/// 창 가장자리 사이드바(QToolBar — 메인 창 배치에서 도크 영역보다 바깥).
class SideBar : public QToolBar
{
    Q_OBJECT
public:
    SideBar(Qt::DockWidgetArea side, QWidget *parent = nullptr);
    Qt::DockWidgetArea side() const { return m_side; }
    SideTab *addTab(QDockWidget *dock);
    void removeTab(SideTab *tab);
    int count() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::DockWidgetArea m_side;
    QHash<SideTab *, QAction *> m_actions;
};

/// 펼친 자동 숨김 도크 — 메인 창 내용 위에 겹쳐 놓는다. 안쪽 가장자리를 끌어 크기를 바꾼다.
class AutoHideOverlay : public QWidget
{
    Q_OBJECT
public:
    AutoHideOverlay(DockManagerPrivate *manager, QDockWidget *dock, QWidget *parent);
    QDockWidget *dock() const { return m_dock; }

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool onResizeEdge(const QPoint &pos) const;
    Qt::DockWidgetArea side() const;

    DockManagerPrivate *m_manager;
    QPointer<QDockWidget> m_dock;
    bool m_resizing = false;
    QPoint m_pressGlobal;
    int m_pressSize = 0;
};

// ---------------------------------------------------------------------------------------------
// 관리자 내부

struct AutoHideEntry
{
    QPointer<QDockWidget> dock;
    Qt::DockWidgetArea side = Qt::LeftDockWidgetArea;  // 사이드바 쪽(Left · Right · Bottom)
    Qt::DockWidgetArea area = Qt::LeftDockWidgetArea;  // 되돌릴 도크 영역
    int size = 240;                                     // 펼친 창 폭(좌우) · 높이(아래)
    QPointer<AutoHideOverlay> overlay;
    QPointer<SideTab> tab;
};

class DockManagerPrivate
{
public:
    explicit DockManagerPrivate(DockManager *q, QMainWindow *window);

    DockManager *q;
    QPointer<QMainWindow> window;
    QList<QPointer<QDockWidget>> docks;
    QPointer<QDockWidget> active;
    QHash<const QDockWidget *, AutoHideEntry> autoHidden;
    QHash<int, QPointer<SideBar>> sideBars;  // Qt::DockWidgetArea → 사이드바
    QPointer<QDockWidget> expanded;
    QMap<QString, QByteArray> layouts;
    QList<QPointer<QTabBar>> tabBars;
    bool tabRefreshQueued = false;

    bool isManaged(const QDockWidget *dock) const;
    /// 위젯이 들어 있는 관리 도크(없으면 nullptr).
    QDockWidget *dockOf(QWidget *widget) const;
    void setActive(QDockWidget *dock);
    void refreshDock(QDockWidget *dock);
    void queueTabRefresh();
    void refreshTabBars();

    SideBar *sideBar(Qt::DockWidgetArea side);
    /// 중앙 위젯 + 붙은 도크 + 도크 탭 줄을 합친 사각형(메인 창 좌표) — 바깥 표시 · 펼친 창의 자리.
    QRect contentRect() const;
    void positionOverlay(const AutoHideEntry &entry) const;
    void collapse();

    /// 표시 자리를 계산해 놓는다 — 마우스 끌기와 dropOnto · dropToEdge가 같이 쓴다.
    bool applyDrop(QDockWidget *dock, const DropTarget &target);
    /// 끌어 놓을 수 있는 도크(붙어 있고 보이며 dragged가 아닌 관리 · 비관리 도크) 중 point(메인 창 좌표) 아래.
    QDockWidget *dockAt(const QPoint &point, const QDockWidget *dragged) const;
    /// 놓일 자리 미리보기 사각형(메인 창 좌표).
    QRect previewRect(const QDockWidget *dragged, const DropTarget &target) const;
};

} // namespace fm::dock
