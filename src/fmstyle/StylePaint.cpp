#include "fmstyle/StylePaint.h"

#include "ColorMath_p.h"
#include "StyleShared_p.h"
#include "fmstyle/PaperFluentChrome.h"
#include "fmstyle/ThemeManager.h"
#include "fmstyle/WatercolorChrome.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QtMath>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

using namespace Qt::StringLiterals;

namespace fm::style {

using T = Token;
using detail::mix;
using detail::withAlpha;

namespace {

void fillShape(QPainter *p, const QRectF &r, qreal radius, const QColor &fill, const QColor &border)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, radius > 0);
    const QRectF box = border.alpha() > 0 ? r.adjusted(0.5, 0.5, -0.5, -0.5) : r;
    p->setPen(border.alpha() > 0 ? QPen(border, 1.0) : Qt::NoPen);
    p->setBrush(fill.alpha() > 0 ? QBrush(fill) : Qt::NoBrush);
    if (radius > 0)
        p->drawRoundedRect(box, radius, radius);
    else
        p->drawRect(box);
    p->restore();
}

} // namespace

// ---------------------------------------------------------------------------------------------
// 글꼴

QFont pixelFont(const QFont &base, qreal px, QFont::Weight weight)
{
    QFont f = base;
    f.setPointSizeF(px * 0.75);
    f.setWeight(weight);
    return f;
}

QFont monoFont(qreal px, QFont::Weight weight)
{
    QFont f;
    // 설정 › 일반 · 모양의 고정폭 글꼴을 먼저, 없으면 목업 순서(Cascadia Mono → JetBrains Mono → Consolas)
    QStringList families{ThemeManager::instance().monoFontFamily()};
    for (const QString &fallback : {u"Cascadia Mono"_s, u"JetBrains Mono"_s, u"Consolas"_s, u"Malgun Gothic"_s}) {
        if (!families.contains(fallback))
            families.append(fallback);
    }
    f.setFamilies(families);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return pixelFont(f, px, weight);
}

QFont withTabularNumbers(QFont font)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    font.setFeature(QFont::Tag("tnum"), 1);
#endif
    return font;
}

// ---------------------------------------------------------------------------------------------
// 색조

ToneColors toneColors(Tone tone, const ThemeColors &tc)
{
    const QColor none(Qt::transparent);
    switch (tone) {
    case Tone::Info:   return {tc[T::AccentSoft], tc[T::AccentFg], none};
    case Tone::Ok:     return {tc[T::OkBg], tc[T::Ok], none};
    case Tone::Warn:   return {tc[T::WarnBg], tc[T::Warn], tc[T::WarnLine]};
    case Tone::Danger: return {tc[T::DangerBg], tc[T::Danger], tc[T::DangerLine]};
    case Tone::Mute:   return {tc[T::Grid], tc[T::Fg2], none};
    case Tone::Bad:    return {none, tc[T::Danger], none};
    }
    return {none, tc[T::Fg], none};
}

// ---------------------------------------------------------------------------------------------
// 키 칩

QSize keyChipSize(const QString &keys)
{
    const QFontMetricsF fm(monoFont(11));
    return QSize(qCeil(fm.horizontalAdvance(keys)) + 10, 18);
}

void paintKeyChip(QPainter *p, const QRectF &rect, const QString &keys, const ThemeColors &tc, KeyChipLook look)
{
    QColor fill = tc[T::Surface];
    QColor border = tc[T::Line];
    QColor text = tc[T::Fg2];
    if (look == KeyChipLook::OnFill) {
        fill = Qt::transparent;
        border = detail::withAlpha(tc[T::OnAccent], 0.40);
        text = tc[T::OnAccent];
    }
    fillShape(p, rect, squareCorners(tc) ? 0.0 : 3.0, fill, border);
    p->save();
    p->setFont(monoFont(11));
    p->setPen(text);
    p->drawText(rect, Qt::AlignCenter, keys);
    p->restore();
}

// ---------------------------------------------------------------------------------------------
// 태그

namespace {
QFont tagFont() { return pixelFont(QFont(), 11.5, QFont::DemiBold); }
}

QSize tagSize(const QString &text, Tone tone, bool compact, bool hasIcon)
{
    const QFontMetricsF fm(tagFont());
    const int pad = tone == Tone::Bad ? 0 : 8;
    const int icon = hasIcon ? 12 + 4 : 0;
    return QSize(qCeil(fm.horizontalAdvance(text)) + 2 * pad + icon, compact ? 20 : 22);
}

void paintTag(QPainter *p, const QRectF &rect, const QString &text, Tone tone, const ThemeColors &tc, bool compact,
              const QIcon &icon)
{
    const ToneColors c = toneColors(tone, tc);
    const qreal radius = squareCorners(tc) ? 0.0 : (compact ? 10.0 : 11.0);
    fillShape(p, rect, radius, c.background, QColor(Qt::transparent));
    const int pad = tone == Tone::Bad ? 0 : 8;
    QRectF inner = rect.adjusted(pad, 0, -pad, 0);
    p->save();
    if (!icon.isNull()) {
        const QRectF ir(inner.left(), rect.center().y() - 6, 12, 12);
        icon.paint(p, ir.toRect());
        inner.setLeft(ir.right() + 4);
    }
    p->setFont(tagFont());
    p->setPen(c.foreground);
    p->drawText(inner, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, text);
    p->restore();
}

// ---------------------------------------------------------------------------------------------
// 배지 · 배너

BadgeColors badgeColors(Tone tone, const ThemeColors &tc)
{
    const QColor none(Qt::transparent);
    BadgeColors c;
    switch (tone) {
    case Tone::Info:   c = {tc[T::AccentSoft], none, tc[T::AccentFg]}; break;
    case Tone::Ok:     c = {tc[T::OkBg], none, tc[T::Ok]}; break;
    case Tone::Warn:   c = {tc[T::WarnBg], tc[T::WarnLine], tc[T::Warn]}; break;
    case Tone::Danger: c = {tc[T::DangerBg], tc[T::DangerLine], tc[T::Danger]}; break;
    case Tone::Mute:
    case Tone::Bad:    c = {tc[T::Grid], none, tc[T::Fg2]}; break;
    }
    if (tc.isWatercolor())
        c.background = none;  // 워터컬러: 바탕 투명(Warn · Danger는 테두리만 남은 사각)
    return c;
}

void paintBadge(QPainter *p, const QRectF &rect, Tone tone, const ThemeColors &tc)
{
    const BadgeColors c = badgeColors(tone, tc);
    fillShape(p, rect, squareCorners(tc) ? 0.0 : 8.0, c.background, c.border);
}

void paintBannerFrame(QPainter *p, const QRectF &rect, Tone tone, const ThemeColors &tc)
{
    const ToneColors c = toneColors(tone, tc);
    fillShape(p, rect, squareCorners(tc) ? 0.0 : 6.0, c.background, c.border);
}

// ---------------------------------------------------------------------------------------------
// 도킹

namespace {

// 놓일 쪽 — 창 모양 안에서 강조색으로 칠할 부분(가로 · 세로 0 ~ 1 비율).
QRectF dropRegion(DropSpot spot)
{
    switch (spot) {
    case DropSpot::Left: return {0.0, 0.0, 0.5, 1.0};
    case DropSpot::Right: return {0.5, 0.0, 0.5, 1.0};
    case DropSpot::Top: return {0.0, 0.0, 1.0, 0.5};
    case DropSpot::Bottom: return {0.0, 0.5, 1.0, 0.5};
    case DropSpot::Center: return {0.0, 0.0, 1.0, 1.0};
    case DropSpot::OuterLeft: return {0.0, 0.0, 0.3, 1.0};
    case DropSpot::OuterRight: return {0.7, 0.0, 0.3, 1.0};
    case DropSpot::OuterTop: return {0.0, 0.0, 1.0, 0.3};
    case DropSpot::OuterBottom: return {0.0, 0.7, 1.0, 0.3};
    }
    return {};
}

// 바깥 표시의 화살표 방향(바깥 = 창 가장자리 쪽).
Qt::ArrowType outerArrow(DropSpot spot)
{
    switch (spot) {
    case DropSpot::OuterLeft: return Qt::LeftArrow;
    case DropSpot::OuterRight: return Qt::RightArrow;
    case DropSpot::OuterTop: return Qt::UpArrow;
    case DropSpot::OuterBottom: return Qt::DownArrow;
    default: return Qt::NoArrow;
    }
}

void arrowPolygon(QPainter *p, const QPointF &c, qreal s, Qt::ArrowType dir, const QColor &color)
{
    QPolygonF poly;
    switch (dir) {
    case Qt::LeftArrow: poly << QPointF(c.x() - s, c.y()) << QPointF(c.x() + s / 2, c.y() - s) << QPointF(c.x() + s / 2, c.y() + s); break;
    case Qt::RightArrow: poly << QPointF(c.x() + s, c.y()) << QPointF(c.x() - s / 2, c.y() - s) << QPointF(c.x() - s / 2, c.y() + s); break;
    case Qt::UpArrow: poly << QPointF(c.x(), c.y() - s) << QPointF(c.x() - s, c.y() + s / 2) << QPointF(c.x() + s, c.y() + s / 2); break;
    case Qt::DownArrow: poly << QPointF(c.x(), c.y() + s) << QPointF(c.x() - s, c.y() - s / 2) << QPointF(c.x() + s, c.y() - s / 2); break;
    default: return;
    }
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPolygon(poly);
}

} // namespace

void paintDropIndicator(QPainter *p, const QRectF &rect, DropSpot spot, bool hover, const ThemeColors &tc)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const bool outer = outerArrow(spot) != Qt::NoArrow;
    const QRectF regionUnit = dropRegion(spot);
    if (!tc.isWatercolor()) {
        // 단추: 카드(Surface · Line, 모서리 6 · 시안3 8) + 옅은 그림자. 마우스 올림이면 강조 테두리 · 강조색 10 % 바탕.
        const QRectF b = rect.adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = tc.isPaperFluent() ? 8.0 : 6.0;
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(tc[T::Shadow], 0.18));
        p->drawRoundedRect(b.translated(0, 1.5), radius, radius);
        p->setBrush(hover ? mix(tc[T::Surface], tc[T::Accent], 0.10) : tc[T::Surface]);
        p->setPen(QPen(hover ? tc[T::Accent] : tc[T::Line], hover ? 1.5 : 1.0));
        p->drawRoundedRect(b, radius, radius);
        // 창 모양: 18 × 16 둥근 2, 놓일 쪽을 강조색으로
        QRectF win(0, 0, 18, 16);
        win.moveCenter(rect.center());
        if (outer)
            win.adjust(outerArrow(spot) == Qt::LeftArrow ? 4 : 0, outerArrow(spot) == Qt::UpArrow ? 4 : 0,
                       outerArrow(spot) == Qt::RightArrow ? -4 : 0, outerArrow(spot) == Qt::DownArrow ? -4 : 0);
        const QRectF inner = win.adjusted(1.5, 1.5, -1.5, -1.5);
        const QRectF fill(inner.left() + inner.width() * regionUnit.left(), inner.top() + inner.height() * regionUnit.top(),
                          inner.width() * regionUnit.width(), inner.height() * regionUnit.height());
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(tc[T::Accent], spot == DropSpot::Center ? 0.45 : 0.85));
        p->drawRect(fill);
        p->setPen(QPen(hover ? tc[T::Fg] : tc[T::Fg2], 1.2));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(win, 2, 2);
        if (outer) {
            // 바깥 표시는 창 가장자리 쪽 화살표
            const QPointF c = rect.center();
            const qreal d = 11.0;
            const QPointF at = outerArrow(spot) == Qt::LeftArrow ? QPointF(c.x() - d, c.y())
                             : outerArrow(spot) == Qt::RightArrow ? QPointF(c.x() + d, c.y())
                             : outerArrow(spot) == Qt::UpArrow ? QPointF(c.x(), c.y() - d)
                                                               : QPointF(c.x(), c.y() + d);
            arrowPolygon(p, at, 3.0, outerArrow(spot), tc[T::Accent]);
        }
        p->restore();
        return;
    }

    // 시안2: 입체 단추(위에서 아래로 g1 → g3, 빗면) + 파란 제목 띠가 있는 창 모양
    const WatercolorChrome &x = watercolorChrome(tc.variant());
    const QRect r = rect.toAlignedRect();
    p->setRenderHint(QPainter::Antialiasing, false);
    QLinearGradient g(QPointF(0, r.top()), QPointF(0, r.bottom() + 1));
    g.setColorAt(0.0, hover ? x.h1 : x.g1);
    g.setColorAt(0.5, hover ? x.h2 : x.g2);
    g.setColorAt(1.0, hover ? x.h3 : x.g3);
    p->fillRect(r, g);
    p->setPen(hover ? tc[T::Accent] : x.out);
    p->setBrush(Qt::NoBrush);
    p->drawRect(r.adjusted(0, 0, -1, -1));
    p->setPen(hover ? x.hoverHi : x.hi);
    p->drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
    p->drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 1);
    p->setPen(hover ? x.hoverLo : x.lo);
    p->drawLine(r.left() + 1, r.bottom() - 1, r.right() - 1, r.bottom() - 1);
    p->drawLine(r.right() - 1, r.top() + 1, r.right() - 1, r.bottom() - 1);
    QRect win(0, 0, 18, 16);
    win.moveCenter(r.center());
    if (outer)
        win.adjust(outerArrow(spot) == Qt::LeftArrow ? 4 : 0, outerArrow(spot) == Qt::UpArrow ? 4 : 0,
                   outerArrow(spot) == Qt::RightArrow ? -4 : 0, outerArrow(spot) == Qt::DownArrow ? -4 : 0);
    p->fillRect(win, tc[T::Field]);
    const QRect body = win.adjusted(1, 4, -1, -1);
    const QRect fill(body.left() + qRound(body.width() * regionUnit.left()), body.top() + qRound(body.height() * regionUnit.top()),
                     qRound(body.width() * regionUnit.width()), qRound(body.height() * regionUnit.height()));
    p->fillRect(fill, spot == DropSpot::Center ? withAlpha(tc[T::Accent], 0.5) : tc[T::Accent]);
    p->fillRect(QRect(win.left(), win.top(), win.width(), 3), x.title);
    p->setPen(x.out);
    p->drawRect(win.adjusted(0, 0, -1, -1));
    if (outer) {
        const QPointF c = QRectF(r).center();
        const qreal d = 11.0;
        const QPointF at = outerArrow(spot) == Qt::LeftArrow ? QPointF(c.x() - d, c.y())
                         : outerArrow(spot) == Qt::RightArrow ? QPointF(c.x() + d, c.y())
                         : outerArrow(spot) == Qt::UpArrow ? QPointF(c.x(), c.y() - d)
                                                           : QPointF(c.x(), c.y() + d);
        p->setRenderHint(QPainter::Antialiasing, true);
        arrowPolygon(p, at, 3.5, outerArrow(spot), tc[T::Fg]);
    }
    p->restore();
}

void paintDropPreview(QPainter *p, const QRectF &rect, const ThemeColors &tc)
{
    p->save();
    if (tc.isWatercolor()) {
        const QRect r = rect.toAlignedRect();
        p->fillRect(r, withAlpha(tc[T::Accent], 0.22));
        p->setPen(QPen(tc[T::Accent], 1));
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.adjusted(0, 0, -1, -1));
        p->drawRect(r.adjusted(1, 1, -2, -2));
    } else {
        const qreal radius = tc.isPaperFluent() ? 6.0 : 4.0;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(tc[T::Accent], 2.0));
        p->setBrush(withAlpha(tc[T::Accent], 0.16));
        p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), radius, radius);
    }
    p->restore();
}

void paintDockSideBar(QPainter *p, const QRect &rect, Qt::Edge innerEdge, const ThemeColors &tc)
{
    p->fillRect(rect, tc[T::Win]);
    const QColor line = tc.isWatercolor() ? watercolorChrome(tc.variant()).tabLine : tc[T::Line];
    switch (innerEdge) {
    case Qt::LeftEdge: p->fillRect(QRect(rect.left(), rect.top(), 1, rect.height()), line); break;
    case Qt::RightEdge: p->fillRect(QRect(rect.right(), rect.top(), 1, rect.height()), line); break;
    case Qt::TopEdge: p->fillRect(QRect(rect.left(), rect.top(), rect.width(), 1), line); break;
    case Qt::BottomEdge: p->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), 1), line); break;
    }
}

// ---------------------------------------------------------------------------------------------
// 도킹 2판 (07 §6)

namespace {

const QColor kCloseHover(0xC4, 0x2B, 0x1C);    // 시안1 · 3 제목 줄 닫기 올림(Windows 11)
const QColor kClosePressed(0xA8, 0x26, 0x18);

QColor overlayOn(const ThemeColors &tc, qreal amount)
{
    return tc.isDark() ? QColor(255, 255, 255, qRound(255 * amount)) : QColor(0, 0, 0, qRound(255 * amount));
}

void hline(QPainter *p, const QRect &r, int y, const QColor &c)
{
    p->fillRect(QRect(r.left(), y, r.width(), 1), c);
}

} // namespace

DockMetrics dockMetrics(const ThemeColors &tc)
{
    DockMetrics m;
    if (tc.isWatercolor()) {
        m.stripTopGap = 2;
        m.tabGap = -1;
        m.areaButton = 19;
        m.splitter = 4;
        m.sideBar = 24;
        m.paneTitle = 22;
        m.floatingTitle = 23;
        m.floatingButton = QSize(17, 17);
        m.floatingButtonGap = 2;
        m.contentBorder = true;
    } else if (tc.isPaperFluent()) {
        m.stripTopGap = 2;
        m.splitter = 6;
        m.paneTitle = 32;
        m.floatingTitle = 30;
        m.floatingButton = QSize(36, 30);
        m.areaRadius = 6;
        m.flyoutRadius = 8;
        m.areaBorder = true;
    }
    return m;
}

void paintDockStrip(QPainter *p, const QRect &rect, Qt::Edge lineEdge, const ThemeColors &tc)
{
    const QColor fill = tc.isPaperFluent() ? tc[T::Alt] : tc[T::Win];
    const QColor line = tc.isWatercolor() ? watercolorChrome(tc.variant()).tabLine : tc[T::Line];
    p->save();
    if (tc.isPaperFluent()) {
        // 영역 테두리(모서리 6) 안쪽 — 바깥쪽 두 모서리를 5로 깎는다
        const qreal radius = dockMetrics(tc).areaRadius - 1.0;
        QPainterPath path;
        const QRectF r(rect);
        if (lineEdge == Qt::TopEdge) {
            path.moveTo(r.left(), r.top());
            path.lineTo(r.right() + 1, r.top());
            path.lineTo(r.right() + 1, r.bottom() + 1 - radius);
            path.quadTo(r.right() + 1, r.bottom() + 1, r.right() + 1 - radius, r.bottom() + 1);
            path.lineTo(r.left() + radius, r.bottom() + 1);
            path.quadTo(r.left(), r.bottom() + 1, r.left(), r.bottom() + 1 - radius);
        } else {
            path.moveTo(r.left(), r.bottom() + 1);
            path.lineTo(r.left(), r.top() + radius);
            path.quadTo(r.left(), r.top(), r.left() + radius, r.top());
            path.lineTo(r.right() + 1 - radius, r.top());
            path.quadTo(r.right() + 1, r.top(), r.right() + 1, r.top() + radius);
            path.lineTo(r.right() + 1, r.bottom() + 1);
        }
        path.closeSubpath();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->fillPath(path, fill);
        p->setRenderHint(QPainter::Antialiasing, false);
    } else {
        p->fillRect(rect, fill);
    }
    if (lineEdge == Qt::TopEdge)
        hline(p, rect, rect.top(), line);
    else
        hline(p, rect, rect.bottom(), line);
    p->restore();
}

void paintDockAreaFrame(QPainter *p, const QRect &areaRect, const QRect &contentRect, Qt::Edge stripEdge,
                        const ThemeColors &tc)
{
    p->save();
    if (tc.isPaperFluent()) {
        const qreal radius = dockMetrics(tc).areaRadius;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(tc[T::Line], 1.0));
        p->setBrush(tc[T::Surface]);
        p->drawRoundedRect(QRectF(areaRect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    } else if (tc.isWatercolor() && contentRect.isValid()) {
        // 내용 둘레 tabLine — 탭 줄 쪽 변은 탭 줄 선이 대신한다
        const QColor line = watercolorChrome(tc.variant()).tabLine;
        const QRect r = contentRect;
        p->fillRect(QRect(r.left(), r.top(), 1, r.height()), line);
        p->fillRect(QRect(r.right(), r.top(), 1, r.height()), line);
        if (stripEdge == Qt::TopEdge)
            hline(p, r, r.bottom(), line);
        else
            hline(p, r, r.top(), line);
    }
    p->restore();
}

void paintDockSplitterHandle(QPainter *p, const QRect &r, bool hover, const ThemeColors &tc)
{
    const bool vertical = r.width() < r.height();  // 세로 손잡이(가로 분할)
    if (hover) {
        if (vertical)
            p->fillRect(QRect(r.center().x(), r.top(), 2, r.height()), tc[T::Accent]);
        else
            p->fillRect(QRect(r.left(), r.center().y(), r.width(), 2), tc[T::Accent]);
        return;
    }
    if (!tc.isWatercolor())
        return;
    // 시안2: 새긴 점 셋(2 × 2 lo + 1 × 1 hi), 긴 쪽 가운데에 4 px 간격
    const WatercolorChrome &x = watercolorChrome(tc.variant());
    const QPoint c = r.center();
    for (int i = -1; i <= 1; ++i) {
        const QPoint d = vertical ? QPoint(c.x() - 1, c.y() + 4 * i - 1) : QPoint(c.x() + 4 * i - 1, c.y() - 1);
        p->fillRect(QRect(d, QSize(2, 2)), x.lo);
        p->fillRect(QRect(d + QPoint(1, 1), QSize(1, 1)), x.hi);
    }
}

void paintDockPaneTitle(QPainter *p, const QRect &r, bool active, const ThemeColors &tc)
{
    p->save();
    if (tc.isWatercolor()) {
        const WatercolorChrome &x = watercolorChrome(tc.variant());
        if (active) {
            QLinearGradient g(QPointF(r.left(), 0), QPointF(r.right() + 1, 0));
            g.setColorAt(0.0, x.title);
            g.setColorAt(1.0, x.titleCap);
            p->fillRect(r, g);
            hline(p, r, r.top(), x.titleHi);
            hline(p, r, r.bottom(), x.titleLo);
        } else {
            p->fillRect(r, x.titleInactive);
            hline(p, r, r.top(), x.titleInactiveHi);
        }
    } else if (tc.isPaperFluent()) {
        p->fillRect(r, tc[T::Alt]);
        hline(p, r, r.bottom(), tc[T::Line]);
        if (active)
            p->fillRect(QRect(r.left(), r.bottom() - 1, r.width(), 2), tc[T::Accent]);
    } else {
        p->fillRect(r, tc[T::Win]);
        hline(p, r, r.bottom(), tc[T::Line]);
        if (active)
            p->fillRect(QRect(r.left(), r.top(), r.width(), 2), tc[T::Accent]);
    }
    p->restore();
}

void paintFloatingTitle(QPainter *p, const QRect &r, bool active, const ThemeColors &tc)
{
    p->save();
    if (tc.isWatercolor()) {
        const WatercolorChrome &x = watercolorChrome(tc.variant());
        if (active) {
            QLinearGradient g(QPointF(r.left(), 0), QPointF(r.right() + 1, 0));
            g.setColorAt(0.0, x.title);
            g.setColorAt(1.0, x.titleCap);
            p->fillRect(r, g);
            hline(p, r, r.top(), x.titleHi);
            hline(p, r, r.bottom() - 1, x.titleLo2);
            hline(p, r, r.bottom(), x.titleLo);
        } else {
            p->fillRect(r, x.titleInactive);
            hline(p, r, r.top(), x.titleInactiveHi);
        }
    } else if (tc.isPaperFluent()) {
        const PaperFluentChrome x = paperFluentChrome(tc);
        QLinearGradient g(QPointF(0, r.top()), QPointF(0, r.bottom() + 1));
        g.setColorAt(0.0, active ? x.titleTop : tc[T::Head]);
        g.setColorAt(1.0, active ? x.titleBottom : tc[T::Alt]);
        p->fillRect(r, g);
        if (!active)
            hline(p, r, r.bottom(), tc[T::Line]);
    } else {
        p->fillRect(r, tc[T::Win]);
        hline(p, r, r.bottom(), tc[T::Line]);
    }
    p->restore();
}

QFont dockTitleFont(const QFont &base, const ThemeColors &tc)
{
    QFont f = base;
    f.setPixelSize(12);
    f.setWeight(tc.isWatercolor() ? QFont::Bold : QFont::DemiBold);
    return f;
}

void paintDockTitleText(QPainter *p, const QRect &r, const QString &text, DockTitleKind kind, bool active,
                        const ThemeColors &tc)
{
    if (text.isEmpty() || r.width() <= 0)
        return;
    p->save();
    const QFont f = dockTitleFont(p->font(), tc);
    p->setFont(f);
    const QString shown = QFontMetrics(f).elidedText(text, Qt::ElideRight, r.width());
    const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextHideMnemonic;
    QColor color;
    if (tc.isWatercolor()) {
        // 캡션 글자: 흰 굵은 글자 + 오른쪽 아래 그림자
        p->setPen(QColor(0, 0, 0, active ? 115 : 64));
        p->drawText(r.translated(1, 1), flags, shown);
        color = active ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xF0, 0xF0, 0xF0);
    } else if (tc.isPaperFluent() && kind == DockTitleKind::Floating) {
        color = active ? paperFluentChrome(tc).titleFg : tc[T::Fg2];
    } else {
        color = active ? tc[T::Fg] : (kind == DockTitleKind::Floating ? tc[T::Fg3] : tc[T::Fg2]);
    }
    p->setPen(color);
    p->drawText(r, flags, shown);
    p->restore();
}

void paintFloatingFrame(QPainter *p, const QRect &r, bool active, const ThemeColors &tc)
{
    QColor line = tc[T::Line];
    if (tc.isWatercolor()) {
        const WatercolorChrome &x = watercolorChrome(tc.variant());
        line = active ? x.frame : x.titleInactive;
    } else if (tc.isPaperFluent() && active) {
        line = paperFluentChrome(tc).titleBottom;
    }
    p->save();
    p->setPen(line);
    p->setBrush(Qt::NoBrush);
    p->drawRect(r.adjusted(0, 0, -1, -1));
    p->restore();
}

void paintDockFlyoutFrame(QPainter *p, const QRect &r, const ThemeColors &tc)
{
    p->save();
    const QColor border = tc.isWatercolor() ? watercolorChrome(tc.variant()).out : tc[T::Line];
    if (tc.isPaperFluent()) {
        const qreal radius = dockMetrics(tc).flyoutRadius;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(border, 1.0));
        p->setBrush(tc[T::Win]);
        p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    } else {
        p->fillRect(r, tc[T::Win]);
        p->setPen(border);
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.adjusted(0, 0, -1, -1));
    }
    p->restore();
}

void paintCaptionButton(QPainter *p, const QRect &r, CaptionButton kind, bool hover, bool pressed, bool active,
                        const ThemeColors &tc)
{
    const bool close = kind == CaptionButton::Close;
    const detail::ChromeGlyph glyph = close                           ? detail::ChromeGlyph::Close
                                    : kind == CaptionButton::Maximize ? detail::ChromeGlyph::Maximize
                                                                      : detail::ChromeGlyph::Restore;
    p->save();
    if (tc.isWatercolor()) {
        // 캡션 단추(captionButtonFace와 같은 규칙) — 닫기 올림은 붉게
        const WatercolorChrome &x = watercolorChrome(tc.variant());
        QColor fill = active ? x.capFill : x.titleInactive;
        QColor line = active ? x.capLine : x.capInactiveLine;
        QColor fg = active ? x.capGlyph : x.capInactiveGlyph;
        if (close && (hover || pressed)) {
            fill = pressed ? QColor(0xB0, 0x4A, 0x4A) : QColor(0xC9, 0x62, 0x62);
            line = QColor(0xDF, 0xA2, 0xA2);
            fg = QColor(0xFF, 0xFF, 0xFF);
        } else if (pressed) {
            fill = mix(fill, QColor(0, 0, 0), 0.18);
            line = x.capHoverLine;
        } else if (hover) {
            line = x.capHoverLine;
        }
        p->fillRect(r, fill);
        p->setPen(line);
        p->drawRect(r.adjusted(0, 0, -1, -1));
        detail::paintChromeGlyph(p, glyph, QRectF(r).adjusted(2, 2, -2, -2), fg, true);
        p->restore();
        return;
    }
    const bool onWash = tc.isPaperFluent() && active;
    QColor fg = onWash ? paperFluentChrome(tc).titleFg : (active ? tc[T::Fg2] : tc[T::Fg3]);
    if (close && (hover || pressed)) {
        p->fillRect(r, pressed ? kClosePressed : kCloseHover);
        fg = QColor(0xFF, 0xFF, 0xFF);
    } else if (hover || pressed) {
        const qreal amount = pressed ? 0.12 : 0.07;
        p->fillRect(r, onWash ? QColor(255, 255, 255, qRound(255 * amount * 2)) : overlayOn(tc, amount));
        if (!onWash)
            fg = tc[T::Fg];
    }
    QRectF box(0, 0, 14, 14);
    box.moveCenter(QRectF(r).center());
    detail::paintChromeGlyph(p, glyph, box, fg, false);
    p->restore();
}

void paintDropCaret(QPainter *p, const QRectF &rect, const ThemeColors &tc)
{
    p->fillRect(rect, tc[T::Accent]);
}

} // namespace fm::style
