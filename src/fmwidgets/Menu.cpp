// 메뉴 · 메뉴 막대 — 두 디자인의 떠 있는 판(그림자) · 열림 효과 · 글리프 아이콘.

#include "fmwidgets/Menu.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QVariantAnimation>

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr qreal kRadius = 6.0;  // 시안1 메뉴 판(스타일의 둥근 메뉴와 같다)
constexpr int kFadeMs = 120;

bool animationsSupported()
{
    const QString platform = QGuiApplication::platformName();
    return platform != QLatin1String("offscreen") && platform != QLatin1String("minimal");
}

} // namespace

Menu::Menu(QWidget *parent)
    : QMenu(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setProperty(fs::props::kOwnPanel, true);  // 스타일은 PE_PanelMenu · PE_FrameMenu를 건너뛴다
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &Menu::applyChrome);
    applyChrome();
}

Menu::Menu(const QString &title, QWidget *parent)
    : Menu(parent)
{
    setTitle(title);
}

Menu::~Menu() = default;

QAction *Menu::addAction(glyph::Glyph glyph, const QString &text, const QKeySequence &shortcut)
{
    QAction *action = QMenu::addAction(fs::themedGlyphIcon(glyph::toStyle(glyph), this), text);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    return action;
}

Menu *Menu::addMenu(const QString &title)
{
    auto *menu = new Menu(title, this);
    QMenu::addMenu(menu);
    return menu;
}

Menu *Menu::addMenu(glyph::Glyph glyph, const QString &title)
{
    return addMenu(fs::themedGlyphIcon(glyph::toStyle(glyph), this), title);
}

Menu *Menu::addMenu(const QIcon &icon, const QString &title)
{
    Menu *menu = addMenu(title);
    menu->setIcon(icon);
    return menu;
}

void Menu::setAnimated(bool animated)
{
    m_animated = animated;
}

void Menu::setItemHeight(int px)
{
    m_itemHeight = std::max(0, px);
    setProperty(fs::props::kMenuItemHeight, m_itemHeight > 0 ? QVariant(m_itemHeight) : QVariant());
    // 항목 자리를 다시 재게 한다(QMenu는 글꼴 · 스타일 바뀜에 다시 잰다)
    QEvent change(QEvent::FontChange);
    QCoreApplication::sendEvent(this, &change);
    updateGeometry();
    update();
}

QMargins Menu::shadowMargins() const
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    // 시안1 둘레(아래로 치우침), 시안2 XP처럼 오른쪽 · 아래만
    return tc.isWatercolor() ? QMargins(0, 0, 4, 4) : QMargins(8, 4, 8, 12);
}

QRect Menu::panelRect() const
{
    return rect().marginsRemoved(shadowMargins());
}

void Menu::applyChrome()
{
    // 그림자 자리 — 창 안에 묻어 둔 미리보기(Qt::Widget)에서도 같은 모양
    setContentsMargins(shadowMargins());
    update();
}

void Menu::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::ParentChange)
        applyChrome();
    QMenu::changeEvent(event);
}

void Menu::showEvent(QShowEvent *event)
{
    applyChrome();
    if (isWindow()) {
        // QMenu가 정한 자리에 판이 오도록 그림자만큼 옮긴다(시스템 흐려짐 효과는 같은 창을 쓰므로 튀지 않는다)
        const QMargins m = shadowMargins();
        move(pos() - QPoint(m.left(), m.top()));
        if (m_animated && animationsSupported() && !QApplication::isEffectEnabled(Qt::UI_AnimateMenu)) {
            if (!m_fade) {
                m_fade = new QVariantAnimation(this);
                m_fade->setDuration(kFadeMs);
                m_fade->setStartValue(0.0);
                m_fade->setEndValue(1.0);
                m_fade->setEasingCurve(QEasingCurve::OutCubic);
                connect(m_fade, &QVariantAnimation::valueChanged, this,
                        [this](const QVariant &v) { setWindowOpacity(v.toReal()); });
            }
            setWindowOpacity(0.0);
            m_fade->start();
        }
    }
    QMenu::showEvent(event);
}

void Menu::paintEvent(QPaintEvent *event)
{
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        const QRect panel = panelRect();
        QPainter p(this);
        if (tc.isWatercolor()) {
            const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
            p.fillRect(panel.translated(4, 4), QColor(0, 0, 0, 56));  // XP 메뉴 그림자
            p.fillRect(panel, tc[T::Surface]);
            p.setPen(x.menuLine);
            p.drawRect(panel.adjusted(0, 0, -1, -1));
        } else {
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF r = QRectF(panel).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath shape;
            shape.addRoundedRect(r, kRadius, kRadius);
            QColor shadow = tc[T::Shadow];
            const qreal base = shadow.alphaF();
            const QPainterPath lifted = shape.translated(0, 3);
            p.setPen(Qt::NoPen);
            for (int i = 8; i >= 1; --i) {
                QPainterPathStroker stroker;
                stroker.setWidth(i * 2.0);
                stroker.setJoinStyle(Qt::RoundJoin);
                shadow.setAlphaF(base * 0.10 * (9 - i) / 8.0);
                p.setBrush(shadow);
                p.drawPath(stroker.createStroke(lifted).united(lifted));
            }
            p.setBrush(tc[T::Surface]);
            p.setPen(QPen(tc[T::Line], 1));
            p.drawPath(shape);
        }
    }
    // 항목 · 구분선 · 하위 메뉴 꺾쇠는 스타일이 그린다(바탕 · 틀은 fmOwnPanel로 건너뜀)
    QMenu::paintEvent(event);
}

// ---------------------------------------------------------------------------------------------

MenuBar::MenuBar(QWidget *parent)
    : QMenuBar(parent)
{
}

MenuBar::~MenuBar() = default;

Menu *MenuBar::addMenu(const QString &title)
{
    auto *menu = new Menu(title, this);
    QMenuBar::addMenu(menu);
    return menu;
}

Menu *MenuBar::addMenu(glyph::Glyph glyph, const QString &title)
{
    return addMenu(fs::themedGlyphIcon(glyph::toStyle(glyph), this), title);
}

Menu *MenuBar::addMenu(const QIcon &icon, const QString &title)
{
    Menu *menu = addMenu(title);
    menu->setIcon(icon);
    return menu;
}

QAction *MenuBar::addAction(glyph::Glyph glyph, const QString &text)
{
    QAction *action = QMenuBar::addAction(text);
    action->setIcon(fs::themedGlyphIcon(glyph::toStyle(glyph), this));
    return action;
}

} // namespace fm::ui
