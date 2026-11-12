#pragma once

// 도크 하나 = 내용 위젯 하나. 영역(탭 묶음) · 떠 있는 창 · 자동 숨김 창 어디에 있든 같은 객체다(07 §3.2).

#include "fmdock/DockGlobals.h"

#include <QFrame>
#include <QIcon>
#include <QPointer>

class QAction;
class QScrollArea;
class QVBoxLayout;

namespace fm::dock {

namespace detail {
class AutoHideFlyout;
struct ManagerAccess;
}

class DockWidget : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(bool modified READ isModified WRITE setModified NOTIFY modifiedChanged)

public:
    enum Feature : std::uint32_t {
        NoFeatures = 0x000,
        Closable = 0x001,
        Movable = 0x002,
        Floatable = 0x004,
        DeleteOnClose = 0x008,        // 닫으면 관리자에서 빼고 지운다
        CustomCloseHandling = 0x010,  // 닫기 단추가 닫지 않고 closeRequested만 보낸다
        Focusable = 0x020,            // 포커스 표시 · 탭을 누르면 내용에 포커스
        ForceCloseWithArea = 0x040,   // 영역을 닫으면 함께 닫는다
        NoTab = 0x080,                // 탭을 보이지 않는다(중앙 영역의 고정 위젯)
        Pinnable = 0x200,             // 자동 숨김으로 고정할 수 있다
        DefaultFeatures = Closable | Movable | Floatable | Focusable | Pinnable,
        LockableFeatures = Closable | Movable | Floatable | Pinnable,
    };
    Q_DECLARE_FLAGS(Features, Feature)
    Q_FLAG(Features)

    enum class InsertMode : std::uint8_t {
        AutoScrollArea,     // 스크롤 영역(항목 보기 등)이면 그대로, 아니면 QScrollArea에 넣는다
        ForceScrollArea,
        ForceNoScrollArea,
    };

    enum class MinimumSizeHintMode : std::uint8_t {
        FromDockWidget,     // 작게 보고해 분할을 막지 않는다(기본)
        FromContent,        // 내용의 minimumSizeHint
    };

    /// id는 objectName이자 배치 상태의 열쇠다. 관리자 안에서 겹치면 안 된다.
    explicit DockWidget(const QString &id, const QString &title = {}, QWidget *parent = nullptr);
    ~DockWidget() override;

    QString id() const { return objectName(); }

    void setWidget(QWidget *content, InsertMode mode = InsertMode::AutoScrollArea);
    QWidget *widget() const { return m_content; }
    /// 내용 위젯을 떼어 돌려준다(부모 없음).
    QWidget *takeWidget();

    QString title() const;
    void setTitle(const QString &title);
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon &icon);

    Features features() const { return m_features; }
    void setFeatures(Features features);
    void setFeature(Feature feature, bool on);
    /// 관리자의 전역 잠금(lockFeatures)을 뺀 실제 기능.
    Features effectiveFeatures() const;
    bool hasFeature(Feature feature) const { return effectiveFeatures().testFlag(feature); }

    /// 문서 도크 — 모든 탭에 닫기 단추, 고친 문서 점(07 §4.1). 어느 영역에 있든 같은 규칙.
    bool isDocument() const { return m_document; }
    void setDocument(bool on);
    bool isModified() const { return m_modified; }
    void setModified(bool on);

    /// 다른 프로세스 창 · 웹 뷰처럼 grab()으로 그릴 수 없는 내용 — 끄는 동안 미리보기는 틀만(07 §7.1).
    bool hasNativeContent() const { return m_nativeContent; }
    void setNativeContent(bool on) { m_nativeContent = on; }

    SideBarLocation preferredSideBar() const { return m_preferredSideBar; }
    void setPreferredSideBar(SideBarLocation side) { m_preferredSideBar = side; }

    MinimumSizeHintMode minimumSizeHintMode() const { return m_minimumSizeHintMode; }
    void setMinimumSizeHintMode(MinimumSizeHintMode mode);

    /// 보기 메뉴용 켜기 · 끄기(체크). 끄면 닫히고(toggleView(false)) 켜면 제자리로 연다.
    QAction *toggleViewAction() const { return m_toggleViewAction; }

    DockManager *dockManager() const { return m_manager; }
    DockArea *dockArea() const { return m_area; }
    DockContainer *dockContainer() const;
    FloatingWindow *floatingWindow() const;

    bool isClosed() const { return m_closed; }
    bool isFloating() const;
    bool isAutoHidden() const { return m_autoHideSide != SideBarLocation::None; }
    SideBarLocation autoHideSide() const { return m_autoHideSide; }
    bool isCurrentTab() const;
    bool isInCentralArea() const;

    QSize minimumSizeHint() const override;

public Q_SLOTS:
    /// open = true: 닫힌 도크를 제자리에 연다. false: 닫는다(지우지 않음).
    void toggleView(bool open = true);
    /// 보이게 한다 — 열고, 탭을 앞으로, 자동 숨김이면 펼친다.
    void bringToFront();
    void setAsCurrentTab();
    /// 닫기 단추와 같다 — Closable · CustomCloseHandling · DeleteOnClose를 따른다.
    void requestClose();
    /// 떠 있는 창으로 뗀다.
    void setFloating();

Q_SIGNALS:
    void titleChanged(const QString &title);
    void iconChanged();
    void featuresChanged(fm::dock::DockWidget::Features features);
    void modifiedChanged(bool modified);
    void viewToggled(bool open);
    void closed();
    void closeRequested();
    void topLevelChanged(bool floating);
    void visibilityChanged(bool visible);
    /// 놓기 직전 · 직후 — 부모가 바뀐다. 다른 프로세스 창을 품은 호스트가 대비한다(07 §7.1).
    void aboutToReparent();
    void reparented();

protected:
    bool event(QEvent *event) override;

private:
    friend class DockArea;
    friend class DockContainer;
    friend class DockManager;
    friend class FloatingWindow;
    friend class detail::AutoHideFlyout;
    friend struct detail::ManagerAccess;

    void setDockArea(DockArea *area);
    void setClosedState(bool closed);
    void syncToggleAction();

    QVBoxLayout *m_layout = nullptr;
    QPointer<QWidget> m_content;
    QPointer<QScrollArea> m_scrollArea;
    QIcon m_icon;
    Features m_features = DefaultFeatures;
    bool m_document = false;
    bool m_modified = false;
    bool m_nativeContent = false;
    bool m_closed = false;
    bool m_central = false;               // setCentralWidget로 넣은 고정 위젯
    SideBarLocation m_preferredSideBar = SideBarLocation::None;
    MinimumSizeHintMode m_minimumSizeHintMode = MinimumSizeHintMode::FromDockWidget;
    QAction *m_toggleViewAction = nullptr;
    QPointer<DockManager> m_manager;
    QPointer<DockArea> m_area;
    QPointer<DockArea> m_lastArea;        // 자동 숨김에서 돌아갈 자리
    int m_lastIndex = -1;
    QPointer<DockArea> m_dockedArea;      // 마지막으로 붙어 있던(떠 있지 않은) 자리 — 두 번 눌러 다시 붙이기
    int m_dockedIndex = -1;
    SideBarLocation m_autoHideSide = SideBarLocation::None;
    int m_autoHideSize = 0;               // 펼친 창 폭 · 높이(0 = 기본)
    bool m_visibleNotified = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DockWidget::Features)

} // namespace fm::dock
