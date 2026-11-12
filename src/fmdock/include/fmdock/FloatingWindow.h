#pragma once

// 떠 있는 창 — 메인 창이 소유하는 최상위 창. 제목 줄은 디자인이 그리고, 이동 · 크기 조절 · Aero 스냅은 OS가 한다(07 §4.5).

#include "fmdock/DockGlobals.h"

#include <QPointer>
#include <QWidget>

class QVBoxLayout;

namespace fm::dock {

namespace detail {
class FloatingTitleBar;
class DragSession;
}

class FloatingWindow : public QWidget
{
    Q_OBJECT

public:
    ~FloatingWindow() override;

    DockManager *dockManager() const { return m_manager; }
    DockContainer *dockContainer() const { return m_container; }
    QList<DockWidget *> dockWidgets() const;
    bool isActiveFloating() const { return m_active; }
    /// 제목 줄 높이를 뺀 내용 기준 좌표가 아니라 창 전체(프레임 포함) 사각형을 다룬다.
    void setFrameGeometryHint(const QRect &rect);

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    friend class DockManager;
    friend class DockContainer;
    friend class DockArea;
    friend class DockWidget;
    friend class detail::FloatingTitleBar;
    friend class detail::DragSession;

    explicit FloatingWindow(DockManager *manager);

    void updateTitle();
    void setActive(bool active);
    void updateFrame();
    bool useCustomFrame() const;
    void startOwnMove(const QPoint &globalPos);
    void containerChanged();

    DockManager *m_manager = nullptr;
    DockContainer *m_container = nullptr;
    detail::FloatingTitleBar *m_titleBar = nullptr;
    QVBoxLayout *m_layout = nullptr;
    bool m_active = false;
    bool m_nativeSetupDone = false;
    // OS 이동(WM_MOVING) 중 끌어 놓기
    QPointer<detail::DragSession> m_moveSession;
    QPoint m_moveStart;
    bool m_sizing = false;
};

} // namespace fm::dock
