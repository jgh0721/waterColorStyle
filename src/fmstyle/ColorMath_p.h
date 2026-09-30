#pragma once

// 색 계산 도우미 — 테마 색상 화면(목업)의 JavaScript 식과 같은 계산.

#include <QColor>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace fm::style::detail {

/// a 에서 b 쪽으로 t 만큼 섞는다 (알파 포함).
inline QColor mix(const QColor &a, const QColor &b, qreal t)
{
    const auto lerp = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(lerp(a.red(), b.red()), lerp(a.green(), b.green()), lerp(a.blue(), b.blue()),
                  lerp(a.alpha(), b.alpha()));
}

inline QColor withAlpha(QColor c, qreal alpha)
{
    c.setAlphaF(alpha);
    return c;
}

inline double linearChannel(int v)
{
    const double s = v / 255.0;
    return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

/// WCAG 2.x 상대 휘도.
inline double relativeLuminance(const QColor &c)
{
    return 0.2126 * linearChannel(c.red()) + 0.7152 * linearChannel(c.green())
         + 0.0722 * linearChannel(c.blue());
}

/// WCAG 대비 비율 (1 ~ 21).
inline double contrast(const QColor &a, const QColor &b)
{
    const double x = relativeLuminance(a);
    const double y = relativeLuminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

/// bg 에 대한 대비가 minimum 이 될 때까지 c 를 toward 쪽으로 조금씩 옮긴다.
inline QColor pushUntil(QColor c, const QColor &toward, const QColor &bg, double minimum)
{
    for (int i = 0; i < 30 && contrast(c, bg) < minimum; ++i)
        c = mix(c, toward, 0.06);
    return c;
}

/// 흰색과 짙은 글자색 중 bg 에서 대비가 높은 쪽.
inline QColor bestTextOn(const QColor &bg)
{
    const QColor white(0xFF, 0xFF, 0xFF);
    const QColor ink(0x16, 0x18, 0x1C);
    return contrast(white, bg) >= contrast(ink, bg) ? white : ink;
}

} // namespace fm::style::detail
