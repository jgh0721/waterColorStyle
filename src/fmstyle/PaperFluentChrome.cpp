#include "fmstyle/PaperFluentChrome.h"

#include "fmstyle/ThemeColors.h"

#include "ColorMath_p.h"

namespace fm::style {

using T = Token;
using namespace detail;

namespace {

const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kBlack(0x00, 0x00, 0x00);

struct TitleWash { QRgb top; QRgb bottom; QRgb fg; };

// 보드의 --title-1 · --title-2 · --title-fg — 토큰이 아닌 디자인 고정 값.
constexpr TitleWash titleWash(Variant variant)
{
    switch (variant) {
    case Variant::Light: return {0xFF3C77D6, 0xFF2D5FB8, 0xFFFFFFFF};
    case Variant::Dark:  return {0xFF2E4F86, 0xFF223C68, 0xFFE4EAF6};
    case Variant::Navy:  return {0xFF2A4A86, 0xFF1D3462, 0xFFE4EAF6};
    }
    return {0xFF3C77D6, 0xFF2D5FB8, 0xFFFFFFFF};
}

struct TipColors { QRgb bg; QRgb line; };

constexpr TipColors tipColors(Variant variant)
{
    switch (variant) {
    case Variant::Light: return {0xFFFFFBDF, 0xFFD8C878};
    case Variant::Dark:  return {0xFF3A3318, 0xFF6A5A22};
    case Variant::Navy:  return {0xFF2E2A1A, 0xFF5A4E22};
    }
    return {0xFFFFFBDF, 0xFFD8C878};
}

} // namespace

PaperFluentChrome paperFluentChrome(const ThemeColors &tc)
{
    // 라이트 기본값에서 보드 값이 나오도록 맞춘 비율: --btn #F6F5EF → hi #FFFFFF · lo #E6E5DC,
    // 다크 #3A3A35 → #45453F · #2F2F2A.
    const bool dark = tc.isDark();
    const QColor btn = tc[T::Btn];
    PaperFluentChrome c;
    c.buttonHi = dark ? mix(btn, kWhite, 0.06) : mix(btn, kWhite, 0.85);
    c.buttonLo = mix(btn, kBlack, dark ? 0.19 : 0.065);
    c.primaryHi = mix(tc[T::Accent], kWhite, 0.16);
    c.primaryLine = mix(tc[T::Accent], kBlack, 0.22);
    c.dangerHi = mix(tc[T::DangerFill], kWhite, 0.12);
    c.dangerLine = mix(tc[T::DangerFill], kBlack, 0.22);
    c.headTop = tc[T::Head];
    c.headBottom = tc[T::Alt];
    c.progressHi = mix(tc[T::Accent], kWhite, 0.20);
    c.shadow = dark ? QColor(0, 0, 0, 115) : QColor(23, 23, 15, 26);  // rgba(0,0,0,.45) · rgba(23,23,15,.10)
    const TitleWash title = titleWash(tc.variant());
    c.titleTop = QColor::fromRgb(title.top);
    c.titleBottom = QColor::fromRgb(title.bottom);
    c.titleFg = QColor::fromRgb(title.fg);
    const TipColors tip = tipColors(tc.variant());
    c.tipBg = QColor::fromRgb(tip.bg);
    c.tipLine = QColor::fromRgb(tip.line);
    c.tipFg = tc[T::Fg];
    return c;
}

QColor paperFluentTitleColor(Variant variant)
{
    return QColor::fromRgb(titleWash(variant).top);
}

} // namespace fm::style
