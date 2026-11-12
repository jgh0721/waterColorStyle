#pragma once

// 컨테이너 = 분할 트리의 뿌리. 메인 창과 떠 있는 창마다 하나(07 §3.1).

#include "fmdock/DockGlobals.h"

#include <QFrame>
#include <QList>
#include <QPointer>

class QVBoxLayout;
class QSplitter;

namespace fm::dock {

namespace detail {
class DockSplitter;
class CentralPlaceholder;
}

class DockContainer : public QFrame
{
    Q_OBJECT

public:
    ~DockContainer() override;

    DockManager *dockManager() const { return m_manager; }
    FloatingWindow *floatingWindow() const { return m_floating; }
    bool isFloating() const { return m_floating != nullptr; }

    QList<DockArea *> dockAreas() const;
    QList<DockArea *> openedDockAreas() const;
    QList<DockWidget *> dockWidgets() const;
    QList<DockWidget *> openedDockWidgets() const;
    /// 전역 좌표 아래의 보이는 영역.
    DockArea *dockAreaAt(const QPoint &globalPos) const;

    /// 중앙 영역(문서 영역)이 있는가 · 그 안의 영역들.
    bool hasCentralRegion() const;
    QList<DockArea *> centralAreas() const;
    /// 중앙 영역이 비었을 때 보이는 자리(없으면 nullptr).
    QWidget *centralPlaceholder() const;

    /// 열린 도크가 하나뿐이면 그 도크(떠 있는 창 제목).
    DockWidget *topLevelDockWidget() const;

Q_SIGNALS:
    void dockAreasChanged();

private:
    friend class DockArea;
    friend class DockManager;
    friend class FloatingWindow;

    DockContainer(DockManager *manager, QWidget *parent, FloatingWindow *floating);

    DockArea *createArea();
    /// area를 target 옆(where) 또는 컨테이너 가장자리에 넣는다. where == Center면 중앙 영역.
    void insertArea(DockArea *area, DropArea where, DockArea *target);
    /// 비었으면 빼고 지운다(나중에). 분할도 접는다.
    void removeAreaIfEmpty(DockArea *area);
    void detachArea(DockArea *area);
    void ensureCentralRegion();
    void clearLayout();
    void setRoot(detail::DockSplitter *root);
    detail::DockSplitter *rootSplitter() const { return m_root; }
    detail::DockSplitter *centralSplitter() const;
    void collapse(detail::DockSplitter *splitter);
    void updateStretch();
    void updateVisibility();
    void areaVisibilityChanged();
    int splitSize(QSplitter *splitter, int index, int fallback) const;
    void resizeSlot(QWidget *descendant, Qt::Orientation orientation, int size);

    DockManager *m_manager = nullptr;
    FloatingWindow *m_floating = nullptr;
    QVBoxLayout *m_layout = nullptr;
    detail::DockSplitter *m_root = nullptr;
    QPointer<detail::DockSplitter> m_central;
    QPointer<detail::CentralPlaceholder> m_placeholder;
    QList<DockArea *> m_areas;
    bool m_dying = false;
    bool m_updatingVisibility = false;
};

} // namespace fm::dock
