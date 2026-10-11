#pragma once

#include "fmwidgets/Glyph.h"

#include <QMenu>
#include <QMenuBar>

class QVariantAnimation;

namespace fm::ui {

/// 메뉴 — QMenu에 두 디자인의 떠 있는 판(그림자) · 열림 효과 · 글리프 아이콘을 더했다(ElaMenu · Windows 11 메뉴).
///
/// - 항목 · 구분선 · 체크 · 단축키 · 하위 메뉴 꺾쇠는 스타일(FmStyle · WatercolorStyle)이 그리던 그대로다.
/// - 판: 시안1 Surface · 1 px 선 · 모서리 6 · 아래로 치우친 부드러운 그림자, 시안2 XP 메뉴처럼 네모 · menuLine 테두리 ·
///   오른쪽 아래 4 px 그림자. 그림자는 창 안 여백(shadowMargins)에 그리고, 띄울 때 그만큼 왼쪽 위로 옮겨 판이
///   QMenu가 정한 자리에 온다.
/// - 열림 효과: 시스템 메뉴 효과(Qt::UI_AnimateMenu)가 꺼져 있을 때만 120 ms 흐려짐(켜져 있으면 시스템 효과).
/// - addAction(glyph, …) · addMenu(…)는 테마를 따르는 단색 아이콘(fm::style::themedGlyphIcon) · fm::ui::Menu 하위 메뉴.
class Menu : public QMenu
{
    Q_OBJECT
    Q_PROPERTY(bool animated READ isAnimated WRITE setAnimated)
    Q_PROPERTY(int itemHeight READ itemHeight WRITE setItemHeight)

public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);
    ~Menu() override;

    using QMenu::addAction;
    using QMenu::addMenu;
    /// 글리프 아이콘 · 단축키가 있는 항목.
    QAction *addAction(glyph::Glyph glyph, const QString &text, const QKeySequence &shortcut = {});
    Menu *addMenu(const QString &title);
    Menu *addMenu(glyph::Glyph glyph, const QString &title);
    Menu *addMenu(const QIcon &icon, const QString &title);

    bool isAnimated() const noexcept { return m_animated; }
    void setAnimated(bool animated);
    /// 항목 높이 px — 0이면 디자인 값(시안1 32 · 시안2 24).
    int itemHeight() const noexcept { return m_itemHeight; }
    void setItemHeight(int px);

    /// 그림자 자리(창 안 여백). 판은 이만큼 안쪽이다.
    QMargins shadowMargins() const;
    /// 판(바탕 · 테두리) 영역 — 위젯 좌표.
    QRect panelRect() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyChrome();

    bool m_animated = true;
    int m_itemHeight = 0;
    QVariantAnimation *m_fade = nullptr;
};

/// 메뉴 막대 — 하위 메뉴를 fm::ui::Menu로 만드는 QMenuBar(ElaMenuBar). 막대 항목은 스타일이 그린다.
class MenuBar : public QMenuBar
{
    Q_OBJECT

public:
    explicit MenuBar(QWidget *parent = nullptr);
    ~MenuBar() override;

    using QMenuBar::addAction;
    using QMenuBar::addMenu;
    Menu *addMenu(const QString &title);
    Menu *addMenu(glyph::Glyph glyph, const QString &title);
    Menu *addMenu(const QIcon &icon, const QString &title);
    /// 막대에 바로 놓는 항목(아이콘 + 글).
    QAction *addAction(glyph::Glyph glyph, const QString &text);
};

} // namespace fm::ui
