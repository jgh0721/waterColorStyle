#pragma once

#include "fmwidgets/Glyph.h"

#include <QPointer>
#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QMenu;
class QPushButton;

namespace fm::ui {

/// 이어 붙인 버튼 가운데 하나를 고르는 세그먼트 컨트롤 (목업의 1줄 · 2줄 · 자동 · 섬네일).
/// 오른쪽 끝에 고르기와 별개인 켜고 끄는 조각(+ 메뉴 ▾ 조각)을 붙일 수 있다(setToggleItem).
class SegmentedControl : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items WRITE setItems)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Size size READ segmentSize WRITE setSegmentSize)
    Q_PROPERTY(bool expanding READ isExpanding WRITE setExpanding)
    Q_PROPERTY(QStringList itemToolTips READ itemToolTips WRITE setItemToolTips)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    /// Mini: 높이 20 · 11.5 px(그래프 축 전환), Small: 26 · 12 px(레코드 전환),
    /// Normal: 창 밀도를 따른다(대화상자 32 · 설정 30 · 메인 창 시안1 30 / 시안2 24).
    /// Compact: 메인 창 주소 줄의 보기 방식(시안1 28 · 12 px · 좌우 10, 시안2 24).
    enum Size { Mini, Small, Normal, Compact };
    Q_ENUM(Size)

    explicit SegmentedControl(QWidget *parent = nullptr);
    explicit SegmentedControl(const QStringList &items, QWidget *parent = nullptr);

    QStringList items() const { return m_items; }
    void setItems(const QStringList &items);

    int currentIndex() const noexcept { return m_current; }
    void setCurrentIndex(int index);

    Size segmentSize() const noexcept { return m_size; }
    void setSegmentSize(Size size);

    /// 조각을 같은 폭으로 늘려 가로를 채운다(다중 이름 변경의 확장자 대소문자).
    bool isExpanding() const noexcept { return m_expanding; }
    void setExpanding(bool expanding);

    QStringList itemToolTips() const { return m_toolTips; }
    void setItemToolTips(const QStringList &toolTips);

    /// 조각마다 아이콘(목업 글리프, 14 px). None이면 글자만. 색은 글자색을 따른다(켜짐 · 워터컬러 포함).
    QList<glyph::Glyph> itemGlyphs() const { return m_glyphs; }
    void setItemGlyphs(const QList<glyph::Glyph> &glyphs);

    int count() const noexcept { return int(m_items.size()); }
    QAbstractButton *button(int index) const;

    /// 고르기 조각들 오른쪽에 켜고 끄는 조각 하나를 붙인다(고르기와 별개로 켜짐 · 꺼짐 — 목업의 1줄 · … · 섬네일 ‖ 평면).
    /// 고르기 조각과의 경계는 굵은 구분선이다. menu를 주면 그 오른쪽에 ▾ 조각을 붙이고, ▾ 클릭 · 켜고 끄는 조각
    /// 우클릭 · 두 조각에서 Alt+↓로 메뉴를 켜고 끄는 조각 왼쪽 아래에 연다. ▾ 조각은 체크 상태가 없고, 켜고 끄는
    /// 조각이 켜지면 함께 켜진 모양으로, 메뉴가 열린 동안 눌린 모양으로 그린다. text가 비면 두 조각을 없앤다.
    void setToggleItem(const QString &text, const QString &toolTip = QString(), QMenu *menu = nullptr);
    QAbstractButton *toggleButton() const;
    QAbstractButton *toggleMenuButton() const;
    bool isToggleChecked() const;
    /// 상태 동기화용 — toggleClicked를 내지 않는다.
    void setToggleChecked(bool checked);
    /// 켜고 끄는 조각과 ▾ 조각을 함께.
    void setToggleEnabled(bool enabled);

Q_SIGNALS:
    void currentIndexChanged(int index);
    /// 사용자가 켜고 끄는 조각을 눌렀다. checked는 누른 뒤의 상태다.
    void toggleClicked(bool checked);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void placeToggle();
    void showToggleMenu();
    void applyLook();
    void refreshIcons();

    QStringList m_items;
    QStringList m_toolTips;
    QList<glyph::Glyph> m_glyphs;
    int m_current = -1;
    Size m_size = Normal;
    bool m_expanding = false;
    QHBoxLayout *m_layout = nullptr;
    QButtonGroup *m_group = nullptr;
    QPushButton *m_toggle = nullptr;
    QPushButton *m_toggleMenuButton = nullptr;
    QPointer<QMenu> m_toggleMenu;
    QString m_toggleToolTip;
};

} // namespace fm::ui
