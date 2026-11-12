#pragma once

// 영역 = 탭 묶음. 제목 줄(탭 + 영역 단추) + 내용(선택 탭의 도크)(07 §4.1).

#include "fmdock/DockGlobals.h"

#include <QFrame>
#include <QList>
#include <QPointer>

class QStackedLayout;
class QBoxLayout;

namespace fm::dock {

namespace detail {
class DockAreaTitleBar;
}

class DockArea : public QFrame
{
    Q_OBJECT

public:
    ~DockArea() override;

    DockManager *dockManager() const { return m_manager; }
    DockContainer *dockContainer() const;

    QList<DockWidget *> dockWidgets() const;
    QList<DockWidget *> openDockWidgets() const;
    int dockWidgetsCount() const { return int(m_docks.size()); }
    int openDockWidgetsCount() const;
    DockWidget *dockWidget(int index) const;
    int indexOf(const DockWidget *dock) const;

    DockWidget *currentDockWidget() const;
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    void setCurrentDockWidget(DockWidget *dock);

    /// 탭 순서를 바꾼다(도크 순서와 같다).
    void moveTab(int from, int to);

    /// 중앙 영역(문서 영역) 안에 있는가.
    bool isCentral() const;
    /// 포커스 영역인가(활성 표시).
    bool isActive() const { return m_active; }
    /// 넘쳐서 숨긴 탭 수(탭 목록 단추의 +N).
    int hiddenTabCount() const;

    /// 닫을 수 있는 도크를 모두 닫는다(AreaCloseButtonClosesTab이면 선택 탭만).
    void closeArea();
    void closeOtherTabs(DockWidget *keep);

    detail::DockAreaTitleBar *titleBar() const { return m_titleBar; }
    /// 제목 줄 · 내용 사각형(이 위젯 좌표).
    QRect titleBarGeometry() const;
    QRect contentGeometry() const;

Q_SIGNALS:
    void currentChanged(int index);
    void tabMoved(int from, int to);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class DockContainer;
    friend class DockManager;
    friend class DockWidget;
    friend class FloatingWindow;

    DockArea(DockManager *manager, QWidget *parent);

    void insertDockWidget(int index, DockWidget *dock, bool activate);
    void removeDockWidget(DockWidget *dock);
    void forgetDockWidget(DockWidget *dock);     // 도크 소멸 중 — 레이아웃 손대지 않음
    void dockOpenStateChanged(DockWidget *dock);
    void updateTitleBar();
    void updateVisibility();
    void updateLayoutOrder();
    void setActive(bool active);

    DockManager *m_manager = nullptr;
    detail::DockAreaTitleBar *m_titleBar = nullptr;
    QWidget *m_contentHost = nullptr;
    QStackedLayout *m_stack = nullptr;
    QBoxLayout *m_layout = nullptr;
    QList<DockWidget *> m_docks;
    int m_current = -1;
    bool m_active = false;
    bool m_dying = false;
};

} // namespace fm::dock
