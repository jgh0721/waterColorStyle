#include "fmstyle/StylePaint.h"

#include "ColorMath_p.h"
#include "fmstyle/ThemeManager.h"

#include <QFontMetricsF>
#include <QtMath>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

using namespace Qt::StringLiterals;

namespace fm::style {

using T = Token;

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

} // namespace fm::style
