#include "fmstyle/StylePaint.h"

#include "ColorMath_p.h"
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
        // 단추: 카드(Surface · Line, 모서리 6) + 옅은 그림자. 마우스 올림이면 강조 테두리 · 강조색 10 % 바탕.
        const QRectF b = rect.adjusted(0.5, 0.5, -0.5, -0.5);
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(tc[T::Shadow], 0.18));
        p->drawRoundedRect(b.translated(0, 1.5), 6, 6);
        p->setBrush(hover ? mix(tc[T::Surface], tc[T::Accent], 0.10) : tc[T::Surface]);
        p->setPen(QPen(hover ? tc[T::Accent] : tc[T::Line], hover ? 1.5 : 1.0));
        p->drawRoundedRect(b, 6, 6);
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
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(tc[T::Accent], 2.0));
        p->setBrush(withAlpha(tc[T::Accent], 0.16));
        p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4);
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

} // namespace fm::style
