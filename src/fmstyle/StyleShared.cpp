#include "StyleShared_p.h"

#include "StyleCommon_p.h"

#include "fmstyle/FmStyle.h"
#include "fmstyle/Glyphs.h"
#include "fmstyle/StyleProps.h"
#include "fmstyle/ThemeColors.h"
#include "fmstyle/WatercolorChrome.h"
#include "fmstyle/WatercolorStyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCalendarWidget>
#include <QHeaderView>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QSlider>
#include <QStyleOption>
#include <QTextCharFormat>

#include <algorithm>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::style::detail {

namespace {

constexpr qreal kPi = std::numbers::pi;

// 위젯이 실제로 쓰는 스타일(프록시 사슬을 따라)이 시안2인지. 둘 다 아니면 fallback.
bool usesWatercolor(const QWidget *widget, bool fallback)
{
    const QStyle *s = widget ? widget->style() : QApplication::style();
    while (s) {
        if (qobject_cast<const WatercolorStyle *>(s))
            return true;
        if (qobject_cast<const FmStyle *>(s))
            return false;
        const auto *proxy = qobject_cast<const QProxyStyle *>(s);
        s = proxy ? proxy->baseStyle() : nullptr;
    }
    return fallback;
}

// 아이콘 모드별 글자색 — 시안1: 보조 글자(Fg2) · 마우스 올림 Fg · 사용 안 함 Fg3,
// 시안2: 글자(Fg) · 마우스 올림 hoverFg · 사용 안 함 disFg (단추 글자와 같은 규칙).
QColor glyphColor(const QWidget *widget, bool watercolor, QIcon::Mode mode)
{
    if (watercolor) {
        const ThemeColors &tc = WatercolorStyle::colorsFor(widget);
        const WatercolorChrome &x = watercolorChrome(tc.variant());
        switch (mode) {
        case QIcon::Disabled: return x.disFg;
        case QIcon::Active: return x.hoverFg;
        case QIcon::Normal:
        case QIcon::Selected: break;
        }
        return tc[Token::Fg];
    }
    const ThemeColors &tc = FmStyle::colorsFor(widget);
    switch (mode) {
    case QIcon::Disabled: return tc[Token::Fg3];
    case QIcon::Active:
    case QIcon::Selected: return tc[Token::Fg];
    case QIcon::Normal: break;
    }
    return tc[Token::Fg2];
}

// 그릴 때 색을 정하는 아이콘. 위젯을 약하게 붙잡아 범위(ThemeScope)를 따르고, 위젯이 없으면 앱 색을 쓴다.
class ThemedIconEngine final : public QIconEngine
{
public:
    ThemedIconEngine(ChromeGlyph glyph, const QWidget *widget, bool watercolor)
        : m_glyph(glyph)
        , m_widget(const_cast<QWidget *>(widget))
        , m_watercolor(watercolor)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QWidget *w = m_widget.data();
        const bool watercolor = usesWatercolor(w, m_watercolor);
        paintChromeGlyph(painter, m_glyph, QRectF(rect), glyphColor(w, watercolor, mode), watercolor);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pm(QSize(qRound(size.width() * scale), qRound(size.height() * scale)));
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    QIconEngine *clone() const override { return new ThemedIconEngine(*this); }
    QString key() const override { return u"fm.themed"_s; }

private:
    ChromeGlyph m_glyph;
    QPointer<QWidget> m_widget;
    bool m_watercolor;
};

void strokePolyline(QPainter *p, std::initializer_list<QPointF> points, const QPen &pen)
{
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPolyline(points.begin(), int(points.size()));
}

void fillPolygon(QPainter *p, std::initializer_list<QPointF> points, const QColor &color)
{
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPolygon(points.begin(), int(points.size()));
}

} // namespace

// =============================================================================================
// 표준 아이콘

void paintChromeGlyph(QPainter *p, ChromeGlyph glyph, const QRectF &rect, const QColor &color, bool watercolor)
{
    const qreal side = std::min(rect.width(), rect.height());
    if (side <= 0)
        return;
    QRectF square(0, 0, side, side);
    square.moveCenter(rect.center());
    if (glyph == ChromeGlyph::Refresh) {
        paintGlyph(p, Glyph::Refresh, square, color);
        return;
    }
    // 한 변 16 좌표계(모양은 가운데 8 × 8 남짓). 작게 그릴 때(도크 단추는 10 px)는 여백을 줄여 모양을 키운다.
    const qreal s = side < 14 ? side / 11.0 : side / 16.0;
    const QPointF o = square.center() - QPointF(8 * s, 8 * s);
    const auto at = [&](qreal x, qreal y) { return o + QPointF(x * s, y * s); };
    const auto box = [&](qreal x, qreal y, qreal w, qreal h) { return QRectF(at(x, y), QSizeF(w * s, h * s)); };

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (!watercolor) {
        QPen pen(color, std::max(1.0, 1.25 * s));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        switch (glyph) {
        case ChromeGlyph::Close:
            p->drawLine(at(4.5, 4.5), at(11.5, 11.5));
            p->drawLine(at(11.5, 4.5), at(4.5, 11.5));
            break;
        case ChromeGlyph::Minimize:
            p->drawLine(at(4, 8), at(12, 8));
            break;
        case ChromeGlyph::Maximize:
            p->drawRoundedRect(box(4.5, 4.5, 7, 7), 1.2 * s, 1.2 * s);
            break;
        case ChromeGlyph::Restore:
            p->drawRoundedRect(box(4.5, 6.5, 5, 5), 1.0 * s, 1.0 * s);
            strokePolyline(p, {at(6.5, 4.5), at(11.5, 4.5), at(11.5, 9.5)}, pen);
            break;
        case ChromeGlyph::ChevronLeft:
            strokePolyline(p, {at(9.5, 4.5), at(6, 8), at(9.5, 11.5)}, pen);
            break;
        case ChromeGlyph::ChevronRight:
            strokePolyline(p, {at(6.5, 4.5), at(10, 8), at(6.5, 11.5)}, pen);
            break;
        case ChromeGlyph::ChevronUp:
            strokePolyline(p, {at(4.5, 9.5), at(8, 6), at(11.5, 9.5)}, pen);
            break;
        case ChromeGlyph::ChevronDown:
            strokePolyline(p, {at(4.5, 6.5), at(8, 10), at(11.5, 6.5)}, pen);
            break;
        case ChromeGlyph::Extension:
            strokePolyline(p, {at(4, 5), at(7, 8), at(4, 11)}, pen);
            strokePolyline(p, {at(8.5, 5), at(11.5, 8), at(8.5, 11)}, pen);
            break;
        case ChromeGlyph::ExtensionDown:
            strokePolyline(p, {at(5, 4), at(8, 7), at(11, 4)}, pen);
            strokePolyline(p, {at(5, 8.5), at(8, 11.5), at(11, 8.5)}, pen);
            break;
        case ChromeGlyph::Pin:
        case ChromeGlyph::Unpin:
            if (glyph == ChromeGlyph::Unpin) {
                // 눕힌 압정 — 바늘이 왼쪽
                p->translate(square.center());
                p->rotate(90);
                p->translate(-square.center());
            }
            p->drawRoundedRect(box(6, 2.5, 4, 5.5), 0.8 * s, 0.8 * s);
            p->drawLine(at(4, 8.5), at(12, 8.5));
            p->drawLine(at(8, 8.5), at(8, 13.5));
            break;
        case ChromeGlyph::Refresh:
            break;
        }
        p->restore();
        return;
    }

    // 시안2 — 제목 표시줄 단추와 같은 굵은 모양, 화살표는 채운 삼각형
    switch (glyph) {
    case ChromeGlyph::Close: {
        QPen pen(color, 2.0 * s);
        pen.setCapStyle(Qt::FlatCap);
        p->setPen(pen);
        p->drawLine(at(4.5, 4.5), at(11.5, 11.5));
        p->drawLine(at(11.5, 4.5), at(4.5, 11.5));
        break;
    }
    case ChromeGlyph::Minimize:
        p->setRenderHint(QPainter::Antialiasing, false);
        p->fillRect(box(4, 10, 8, 2), color);
        break;
    case ChromeGlyph::Maximize:
        p->setRenderHint(QPainter::Antialiasing, false);
        p->fillRect(box(4, 4, 8, 2), color);
        p->fillRect(box(4, 4, 1, 8), color);
        p->fillRect(box(11, 4, 1, 8), color);
        p->fillRect(box(4, 11, 8, 1), color);
        break;
    case ChromeGlyph::Restore:
        p->setRenderHint(QPainter::Antialiasing, false);
        // 뒤 창: 위 2 줄 + 오른쪽, 앞 창에 가린 아래 · 왼쪽은 보이는 만큼만
        p->fillRect(box(6, 3, 7, 2), color);
        p->fillRect(box(12, 3, 1, 6), color);
        p->fillRect(box(6, 3, 1, 3), color);
        p->fillRect(box(10, 8, 3, 1), color);
        // 앞 창
        p->fillRect(box(3, 6, 7, 2), color);
        p->fillRect(box(3, 6, 1, 7), color);
        p->fillRect(box(9, 6, 1, 7), color);
        p->fillRect(box(3, 12, 7, 1), color);
        break;
    case ChromeGlyph::ChevronLeft:
        fillPolygon(p, {at(10, 4), at(10, 12), at(6, 8)}, color);
        break;
    case ChromeGlyph::ChevronRight:
        fillPolygon(p, {at(6, 4), at(6, 12), at(10, 8)}, color);
        break;
    case ChromeGlyph::ChevronUp:
        fillPolygon(p, {at(4, 10), at(12, 10), at(8, 6)}, color);
        break;
    case ChromeGlyph::ChevronDown:
        fillPolygon(p, {at(4, 6), at(12, 6), at(8, 10)}, color);
        break;
    case ChromeGlyph::Extension:
    case ChromeGlyph::ExtensionDown: {
        QPen pen(color, 1.6 * s);
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
        if (glyph == ChromeGlyph::Extension) {
            strokePolyline(p, {at(4, 4.5), at(7.5, 8), at(4, 11.5)}, pen);
            strokePolyline(p, {at(8.5, 4.5), at(12, 8), at(8.5, 11.5)}, pen);
        } else {
            strokePolyline(p, {at(4.5, 4), at(8, 7.5), at(11.5, 4)}, pen);
            strokePolyline(p, {at(4.5, 8.5), at(8, 12), at(11.5, 8.5)}, pen);
        }
        break;
    }
    case ChromeGlyph::Pin:
    case ChromeGlyph::Unpin: {
        if (glyph == ChromeGlyph::Unpin) {
            p->translate(square.center());
            p->rotate(90);
            p->translate(-square.center());
        }
        p->setRenderHint(QPainter::Antialiasing, false);
        // 머리(속이 빈 네모 + 굵은 오른쪽 줄) · 받침 · 바늘
        p->fillRect(box(6, 2, 4, 1), color);
        p->fillRect(box(6, 2, 1, 6), color);
        p->fillRect(box(9, 2, 2, 6), color);
        p->fillRect(box(4, 8, 8, 1.5), color);
        p->fillRect(box(7.5, 9.5, 1, 4), color);
        break;
    }
    case ChromeGlyph::Refresh:
        break;
    }
    p->restore();
}

void polishDockButton(QWidget *widget)
{
    if (widget->property(props::kDockButton).isValid())
        return;
    const QString name = widget->objectName();
    if (name == u"qt_dockwidget_closebutton")
        widget->setProperty(props::kDockButton, u"close"_s);
    else if (name == u"qt_dockwidget_floatbutton")
        widget->setProperty(props::kDockButton, u"float"_s);
}

ChromeGlyph dockButtonGlyph(const QString &kind)
{
    if (kind == u"float")
        return ChromeGlyph::Restore;
    if (kind == u"pin")
        return ChromeGlyph::Pin;
    if (kind == u"unpin")
        return ChromeGlyph::Unpin;
    if (kind == u"menu")
        return ChromeGlyph::ChevronDown;
    return ChromeGlyph::Close;
}

QSize menuBarItemContents(const QStyleOptionMenuItem *option, const QSize &contents)
{
    if (!option || option->icon.isNull() || option->text.isEmpty())
        return contents;
    return QSize(16 + 6 + option->fontMetrics.horizontalAdvance(plainText(option->text)), contents.height());
}

void drawMenuBarItemContents(QPainter *painter, const QStyleOptionMenuItem *option, const QStyle *style,
                             const QWidget *widget, const QColor &textColor, const QColor &iconTint)
{
    const QRect r = option->rect;
    const bool enabled = option->state & QStyle::State_Enabled;
    int flags = Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
    if (!style->styleHint(QStyle::SH_UnderlineShortcut, option, widget))
        flags |= Qt::TextHideMnemonic;
    const bool hasIcon = !option->icon.isNull();
    const bool hasText = !option->text.isEmpty();
    if (!hasIcon) {
        // 글만 — 예전과 같은 자리(보드 스냅숏 그대로)
        painter->save();
        painter->setPen(textColor);
        painter->drawText(r, flags | Qt::AlignCenter, option->text);
        painter->restore();
        return;
    }
    const int textWidth = hasText ? option->fontMetrics.horizontalAdvance(plainText(option->text)) : 0;
    const int total = (hasIcon ? 16 : 0) + (hasIcon && hasText ? 6 : 0) + textWidth;
    int x = r.left() + (r.width() - total) / 2;
    painter->save();
    if (hasIcon) {
        const QIcon::Mode mode = !enabled ? QIcon::Disabled
                               : (option->state & (QStyle::State_Selected | QStyle::State_Sunken)) ? QIcon::Active
                                                                                                    : QIcon::Normal;
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        QPixmap pm = option->icon.pixmap(QSize(16, 16), dpr, mode);
        if (iconTint.isValid()) {
            QPainter tint(&pm);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(pm.rect(), iconTint);
        }
        painter->drawPixmap(QRect(x, r.top() + (r.height() - 16) / 2, 16, 16), pm);
        x += 16 + 6;
    }
    if (hasText) {
        painter->setPen(textColor);
        painter->drawText(QRect(x, r.top(), textWidth + 2, r.height()), flags | Qt::AlignLeft | Qt::AlignVCenter, option->text);
    }
    painter->restore();
}

ToolButtonParts toolButtonParts(const QStyle *style, const QStyleOptionToolButton *option, const QWidget *widget,
                                int indicatorWidth)
{
    ToolButtonParts parts;
    parts.split = option->features & QStyleOptionToolButton::MenuButtonPopup;
    parts.dropDown = !parts.split && (option->features & QStyleOptionToolButton::HasMenu);
    parts.button = style->subControlRect(QStyle::CC_ToolButton, option, QStyle::SC_ToolButton, widget);
    const bool sunken = option->state & QStyle::State_Sunken;
    const QStyle::State base = option->state & ~QStyle::State_Sunken;
    parts.buttonState = base;
    parts.menuState = base;
    if (parts.split) {
        parts.menu = style->subControlRect(QStyle::CC_ToolButton, option, QStyle::SC_ToolButtonMenu, widget);
        parts.indicator = parts.menu;
        // QCommonStyle은 단추 칸을 눌러도 메뉴 칸까지 눌린 모양으로 그린다 — 눌린 칸만
        if (sunken && (option->activeSubControls & QStyle::SC_ToolButton))
            parts.buttonState |= QStyle::State_Sunken;
        if (sunken && (option->activeSubControls & QStyle::SC_ToolButtonMenu))
            parts.menuState |= QStyle::State_Sunken;
        parts.menuState &= ~(QStyle::State_On | QStyle::State_HasFocus);
        return parts;
    }
    parts.buttonState = option->state;
    if (parts.dropDown) {
        const QRect r = option->rect;
        const int w = std::min(indicatorWidth, r.width() / 2);
        parts.indicator = QStyle::visualRect(option->direction, r, QRect(r.right() - w + 1, r.top(), w, r.height()));
        parts.button = QStyle::visualRect(option->direction, r, QRect(r.left(), r.top(), r.width() - w + 2, r.height()));
    }
    return parts;
}

bool toolButtonHasDropDown(const QStyleOption *option)
{
    const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    return tb && !(tb->features & QStyleOptionToolButton::MenuButtonPopup)
           && (tb->features & QStyleOptionToolButton::HasMenu);
}

QRect groupBoxTitleRect(const QStyleOptionGroupBox *option, QStyle::SubControl sc, int indicator, int gap, int textPad)
{
    const QRect r = option->rect;
    const int lineHeight = option->fontMetrics.height();
    const bool checkable = option->subControls & QStyle::SC_GroupBoxCheckBox;
    const int indicatorSpace = checkable ? indicator + gap : 0;
    const int textWidth = option->text.isEmpty() ? 0 : option->fontMetrics.horizontalAdvance(option->text) + textPad;
    const int width = std::min(r.width(), indicatorSpace + textWidth);
    // 논리(왼쪽에서 오른쪽) 좌표로 두고 visualRect가 뒤집는다 — AlignLeft는 앞쪽, AlignRight는 뒤쪽
    int left = r.left();
    if (option->textAlignment & Qt::AlignHCenter)
        left = r.left() + (r.width() - width) / 2;
    else if (option->textAlignment & Qt::AlignRight)
        left = r.right() - width + 1;
    QRect res;
    if (sc == QStyle::SC_GroupBoxCheckBox)
        res = QRect(left, r.top() + (lineHeight - indicator) / 2, indicator, indicator);
    else
        res = QRect(left + indicatorSpace, r.top(), textWidth, lineHeight);
    return QStyle::visualRect(option->direction, r, res);
}

QRect dockTitleSubRect(QStyle::SubElement element, const QStyleOption *option, const QWidget *widget, int button,
                       int gap, int rightMargin, int textMargin)
{
    const auto *dw = qstyleoption_cast<const QStyleOptionDockWidget *>(option);
    const bool closable = dw ? dw->closable : true;
    const bool floatable = dw ? dw->floatable : false;
    const bool vertical = dw && dw->verticalTitleBar;
    const int extra = widget ? widget->property(props::kDockExtraButtons).toInt() : 0;
    // 세로 제목 줄은 가로로 놓고 계산한 뒤 되돌린다(QCommonStyle과 같은 변환).
    const QRect rect = vertical ? option->rect.transposed() : option->rect;
    int right = rect.right() - rightMargin;
    const auto take = [&] {
        const QRect b(right - button + 1, rect.top() + (rect.height() - button) / 2, button, button);
        right = b.left() - gap - 1;
        return b;
    };
    QRect r;
    const QRect closeRect = closable ? take() : QRect();
    if (element == QStyle::SE_DockWidgetCloseButton) {
        r = closeRect;
    } else {
        const QRect floatRect = floatable ? take() : QRect();
        if (element == QStyle::SE_DockWidgetFloatButton) {
            r = floatRect;
        } else if (element == QStyle::SE_DockWidgetTitleBarText) {
            for (int i = 0; i < extra; ++i)
                take();
            const int left = rect.left() + textMargin;
            r = QRect(left, rect.top(), std::max(0, right - 4 - left + 1), rect.height());
        }
    }
    if (r.isNull())
        return r;
    if (vertical)
        return QRect(rect.left() + r.top() - rect.top(), rect.top() + rect.right() - r.right(), r.height(), r.width());
    return QStyle::visualRect(option->direction, rect, r);
}

QIcon themedStandardIcon(QStyle::StandardPixmap pixmap, const QWidget *widget, bool watercolor)
{
    ChromeGlyph glyph = ChromeGlyph::Close;
    switch (pixmap) {
    case QStyle::SP_ArrowLeft:
    case QStyle::SP_ArrowBack: glyph = ChromeGlyph::ChevronLeft; break;
    case QStyle::SP_ArrowRight:
    case QStyle::SP_ArrowForward: glyph = ChromeGlyph::ChevronRight; break;
    case QStyle::SP_ArrowUp: glyph = ChromeGlyph::ChevronUp; break;
    case QStyle::SP_ArrowDown: glyph = ChromeGlyph::ChevronDown; break;
    case QStyle::SP_TitleBarCloseButton:
    case QStyle::SP_DockWidgetCloseButton:
    case QStyle::SP_TabCloseButton:
    case QStyle::SP_LineEditClearButton: glyph = ChromeGlyph::Close; break;
    case QStyle::SP_TitleBarMinButton: glyph = ChromeGlyph::Minimize; break;
    case QStyle::SP_TitleBarMaxButton: glyph = ChromeGlyph::Maximize; break;
    case QStyle::SP_TitleBarNormalButton: glyph = ChromeGlyph::Restore; break;
    case QStyle::SP_ToolBarHorizontalExtensionButton: glyph = ChromeGlyph::Extension; break;
    case QStyle::SP_ToolBarVerticalExtensionButton: glyph = ChromeGlyph::ExtensionDown; break;
    case QStyle::SP_BrowserReload: glyph = ChromeGlyph::Refresh; break;
    default:
        return QIcon();
    }
    return QIcon(new ThemedIconEngine(glyph, widget, watercolor));
}

// =============================================================================================
// 탭 방향

TabSide tabSide(QTabBar::Shape shape) noexcept
{
    switch (shape) {
    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        return TabSide::South;
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        return TabSide::West;
    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        return TabSide::East;
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        break;
    }
    return TabSide::North;
}

QTransform tabTransform(TabSide side, const QRect &r) noexcept
{
    // QTransform(h11, h12, h21, h22, dx, dy): x' = h11·x + h21·y + dx, y' = h12·x + h22·y + dy
    switch (side) {
    case TabSide::North: return QTransform(1, 0, 0, 1, r.left(), r.top());
    case TabSide::South: return QTransform(1, 0, 0, -1, r.left(), r.bottom() + 1);
    case TabSide::West: return QTransform(0, 1, 1, 0, r.left(), r.top());
    case TabSide::East: return QTransform(0, 1, -1, 0, r.right() + 1, r.top());
    }
    return QTransform();
}

QSize tabLocalSize(TabSide side, const QRect &r) noexcept
{
    return isVerticalTab(side) ? QSize(r.height(), r.width()) : r.size();
}

QRect adjustTabRect(TabSide side, const QRect &r, int outer, int inner) noexcept
{
    switch (side) {
    case TabSide::North: return r.adjusted(0, outer, 0, -inner);
    case TabSide::South: return r.adjusted(0, inner, 0, -outer);
    case TabSide::West: return r.adjusted(outer, 0, -inner, 0);
    case TabSide::East: return r.adjusted(inner, 0, -outer, 0);
    }
    return r;
}

QRect tabBaseLine(TabSide side, const QRect &r) noexcept
{
    switch (side) {
    case TabSide::North: return QRect(r.left(), r.bottom(), r.width(), 1);
    case TabSide::South: return QRect(r.left(), r.top(), r.width(), 1);
    case TabSide::West: return QRect(r.right(), r.top(), 1, r.height());
    case TabSide::East: return QRect(r.left(), r.top(), 1, r.height());
    }
    return QRect();
}

// =============================================================================================
// 슬라이더 · 다이얼

QRect sliderSubControlRect(const QStyleOptionSlider *slider, QStyle::SubControl sc, int length, int thickness)
{
    const QRect r = slider->rect;
    const bool horizontal = slider->orientation == Qt::Horizontal;
    // TicksAbove = TicksLeft, TicksBelow = TicksRight
    const int before = (slider->tickPosition & QSlider::TicksAbove) ? kSliderTickSpace : 0;
    const int after = (slider->tickPosition & QSlider::TicksBelow) ? kSliderTickSpace : 0;
    const int cross = horizontal ? r.height() : r.width();
    const int crossStart = before + (cross - before - after - thickness) / 2;
    const int span = std::max(0, (horizontal ? r.width() : r.height()) - length);
    const int pos = QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, span,
                                                    slider->upsideDown);
    QRect res;
    switch (sc) {
    case QStyle::SC_SliderHandle:
        res = horizontal ? QRect(r.left() + pos, r.top() + crossStart, length, thickness)
                         : QRect(r.left() + crossStart, r.top() + pos, thickness, length);
        break;
    case QStyle::SC_SliderGroove:
        res = horizontal ? QRect(r.left(), r.top() + crossStart, r.width(), thickness)
                         : QRect(r.left() + crossStart, r.top(), thickness, r.height());
        break;
    case QStyle::SC_SliderTickmarks:
        res = r;
        break;
    default:
        return QRect();
    }
    // QCommonStyle과 같이 오른쪽에서 왼쪽 방향이면 뒤집는다(QSlider가 upsideDown도 같이 맞춘다).
    return QStyle::visualRect(slider->direction, r, res);
}

QList<int> sliderTickPositions(const QStyleOptionSlider *slider, int handleLength)
{
    QList<int> out;
    const bool horizontal = slider->orientation == Qt::Horizontal;
    const int span = (horizontal ? slider->rect.width() : slider->rect.height()) - handleLength;
    if (span <= 0 || slider->maximum <= slider->minimum)
        return out;
    // QCommonStyle의 눈금 간격 규칙: tickInterval → singleStep(너무 촘촘하면 pageStep)
    int interval = slider->tickInterval;
    if (interval <= 0) {
        interval = slider->singleStep;
        if (QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, interval, span)
                - QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, 0, span)
            < 3)
            interval = slider->pageStep;
    }
    if (interval <= 0)
        interval = 1;
    if ((qint64(slider->maximum) - slider->minimum) / interval > 200)
        return out;
    for (qint64 v = slider->minimum; v <= slider->maximum; v += interval)
        out.append(QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, int(v), span, slider->upsideDown)
                   + handleLength / 2);
    return out;
}

qreal dialAngle(const QStyleOptionSlider *dial)
{
    const qint64 range = qint64(dial->maximum) - dial->minimum;
    if (range == 0)
        return kPi / 2;
    const qint64 current = dial->upsideDown ? dial->sliderPosition : qint64(dial->maximum) - dial->sliderPosition;
    const qreal fraction = qreal(current - dial->minimum) / qreal(range);
    if (dial->dialWrapping)
        return kPi * 3 / 2 - fraction * 2 * kPi;
    return (kPi * 8 - fraction * 10 * kPi) / 6;
}

QList<DialNotch> dialNotches(const QStyleOptionSlider *dial)
{
    // QStyleHelper::calcLines와 같은 개수 · 큰 눈금 규칙
    QList<DialNotch> out;
    const int ns = dial->tickInterval;
    if (ns <= 0)
        return out;
    qint64 notches = (qint64(dial->maximum) + ns - 1 - dial->minimum) / ns;
    if (dial->maximum < dial->minimum || qint64(dial->maximum) - dial->minimum > 1000)
        notches = (qint64(dial->minimum) + 1000 + ns - 1 - dial->minimum) / ns;
    if (notches <= 0)
        return out;
    const int page = dial->pageStep ? dial->pageStep : 1;
    for (qint64 i = 0; i <= notches; ++i) {
        const qreal angle = dial->dialWrapping ? kPi * 3 / 2 - qreal(i) * 2 * kPi / qreal(notches)
                                               : (kPi * 8 - qreal(i) * 10 * kPi / qreal(notches)) / 6;
        out.append({angle, i == 0 || (ns * i) % page == 0});
    }
    return out;
}

// =============================================================================================
// 항목 보기

namespace {

constexpr char kHoverByStyle[] = "fmViewportHoverByStyle";

QWidget *itemViewViewport(QWidget *widget)
{
    auto *view = qobject_cast<QAbstractItemView *>(widget);
    if (!view || qobject_cast<QHeaderView *>(widget))
        return nullptr;
    return view->viewport();
}

} // namespace

void polishItemView(QWidget *widget)
{
    QWidget *viewport = itemViewViewport(widget);
    if (!viewport || viewport->testAttribute(Qt::WA_Hover))
        return;
    viewport->setAttribute(Qt::WA_Hover, true);
    widget->setProperty(kHoverByStyle, true);
}

void unpolishItemView(QWidget *widget)
{
    QWidget *viewport = itemViewViewport(widget);
    if (!viewport || !widget->property(kHoverByStyle).toBool())
        return;
    viewport->setAttribute(Qt::WA_Hover, false);
    widget->setProperty(kHoverByStyle, QVariant());
}

// =============================================================================================
// 달력

namespace {

constexpr char kNavigationBar[] = "qt_calendar_navigationbar";
constexpr char kWeekendCleared[] = "fmWeekendCleared";

bool isQtDefaultWeekend(const QTextCharFormat &format)
{
    // QCalendarWidget 생성자가 넣는 값: 글자색 Qt::red만
    QTextCharFormat expected;
    expected.setForeground(QBrush(Qt::red));
    return format == expected;
}

} // namespace

void polishCalendarPart(QWidget *widget)
{
    if (widget->objectName() == QLatin1StringView(kNavigationBar)) {
        widget->setBackgroundRole(QPalette::Base);
        return;
    }
    auto *calendar = qobject_cast<QCalendarWidget *>(widget);
    if (!calendar || calendar->property(kWeekendCleared).toBool())
        return;
    if (isQtDefaultWeekend(calendar->weekdayTextFormat(Qt::Saturday))
        && isQtDefaultWeekend(calendar->weekdayTextFormat(Qt::Sunday))) {
        calendar->setWeekdayTextFormat(Qt::Saturday, QTextCharFormat());
        calendar->setWeekdayTextFormat(Qt::Sunday, QTextCharFormat());
        calendar->setProperty(kWeekendCleared, true);
    }
}

void unpolishCalendarPart(QWidget *widget)
{
    if (widget->objectName() == QLatin1StringView(kNavigationBar)) {
        widget->setBackgroundRole(QPalette::Highlight);
        return;
    }
    auto *calendar = qobject_cast<QCalendarWidget *>(widget);
    if (!calendar || !calendar->property(kWeekendCleared).toBool())
        return;
    QTextCharFormat red;
    red.setForeground(QBrush(Qt::red));
    for (const Qt::DayOfWeek day : {Qt::Saturday, Qt::Sunday}) {
        if (calendar->weekdayTextFormat(day) == QTextCharFormat())
            calendar->setWeekdayTextFormat(day, red);
    }
    calendar->setProperty(kWeekendCleared, QVariant());
}

} // namespace fm::style::detail
