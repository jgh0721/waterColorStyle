#pragma once

// 도킹 관리자 — QMainWindow · QDockWidget 위에 KDDockWidgets의 기능을 옮겨 얹는다(docs/specs/07-docking.md).
// 코드를 가져오지 않고 동작을 다시 구현했다(KDDockWidgets는 GPL). 옮긴 기능:
//   · 사용자 제목 줄 — 떼어 내기 · 자동 숨김(압정) · 닫기 단추, 두 번 눌러 떼기 · 붙이기, 활성 도크 강조
//   · 끌어 놓기 표시(classic) — 메인 창 바깥 가장자리 넷 + 마우스 아래 도크의 십자(왼 · 위 · 오른 · 아래 · 탭), 놓일 자리 미리보기,
//     Esc로 취소, 표시 밖에 놓으면 떠 있는 창
//   · 자동 숨김 사이드바 — 압정으로 도크를 창 가장자리 탭으로 접고, 탭을 누르면 내용 위로 펼친다(바깥을 누르면 접힘)
//   · 도크 탭 닫기 단추, 상태 저장 · 복원과 이름 붙인 배치
// 모양(제목 줄 · 단추 · 탭 · 표시)은 fm::style의 두 디자인이 그린다.

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <cstdint>
#include <memory>

class QDockWidget;
class QMainWindow;
class QMenu;
class QWidget;

namespace fm::dock {

class DockManagerPrivate;

class DockManager : public QObject
{
    Q_OBJECT
public:
    explicit DockManager(QMainWindow *window);
    ~DockManager() override;

    QMainWindow *window() const;

    /// 도크를 만들어 관리한다. id는 objectName(상태 저장의 열쇠)이 된다.
    QDockWidget *addDock(const QString &id, const QString &title, QWidget *content,
                         Qt::DockWidgetArea area = Qt::LeftDockWidgetArea);
    /// 이미 메인 창에 넣은 도크를 관리한다 — 사용자 제목 줄로 바꾼다. objectName이 비어 있으면 무시한다.
    void manage(QDockWidget *dock);
    QList<QDockWidget *> docks() const;
    QDockWidget *dock(const QString &id) const;

    /// 활성 도크 — 키보드 포커스가 들어 있는 관리 도크(없으면 nullptr). 제목 줄 · 도크 탭 · 사이드 탭에 강조색.
    QDockWidget *activeDock() const;

    // ---------------------------------------------------------------- 자동 숨김

    bool isAutoHidden(const QDockWidget *dock) const;
    /// on: 도크를 사이드바 탭으로 접는다. side는 사이드바 쪽(Left · Right · Bottom, NoDockWidgetArea면 지금 영역에서 정함).
    /// off: 사이드바에서 원래 도크 영역 · 크기로 되돌린다.
    void setAutoHidden(QDockWidget *dock, bool on, Qt::DockWidgetArea side = Qt::NoDockWidgetArea);
    /// 자동 숨김 도크를 펼친다(사이드 탭을 누른 것과 같다). nullptr이면 접는다.
    void showAutoHidden(QDockWidget *dock);
    QDockWidget *expandedDock() const;

    // ---------------------------------------------------------------- 끌어 놓기(마우스 없이)

    enum class DropSide : std::uint8_t { Left, Top, Right, Bottom, Center };
    /// target 도크 옆(Left · Top · Right · Bottom) 또는 같은 탭 묶음(Center)으로 — 마우스로 십자 표시에 놓은 것과 같다.
    bool dropOnto(QDockWidget *dock, QDockWidget *target, DropSide side);
    /// 메인 창 가장자리의 도크 영역으로 — 바깥 표시에 놓은 것과 같다.
    bool dropToEdge(QDockWidget *dock, Qt::DockWidgetArea area);

    // ---------------------------------------------------------------- 상태 · 배치

    /// 메인 창 배치(떠 있는 창 포함) + 자동 숨김 도크. 다른 판의 상태는 restoreState가 거절한다.
    QByteArray saveState() const;
    bool restoreState(const QByteArray &state);

    QStringList layoutNames() const;
    /// 지금 상태를 이름으로 저장한다(같은 이름이면 덮어쓴다).
    void saveLayout(const QString &name);
    bool applyLayout(const QString &name);
    void removeLayout(const QString &name);
    /// 앱이 설정 파일에 저장 · 복원한다.
    QMap<QString, QByteArray> layouts() const;
    void setLayouts(const QMap<QString, QByteArray> &layouts);

    /// 보기 메뉴: 도크 켜기 · 끄기 + "배치" 하위 메뉴(배치 저장… · 저장한 배치 · 배치 삭제).
    void populateMenu(QMenu *menu);

Q_SIGNALS:
    void activeDockChanged(QDockWidget *dock);
    void autoHideChanged(QDockWidget *dock, bool autoHidden);
    void layoutsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class DockManagerPrivate;
    std::unique_ptr<DockManagerPrivate> d;
};

} // namespace fm::dock
