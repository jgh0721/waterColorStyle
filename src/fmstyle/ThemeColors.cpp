#include "fmstyle/ThemeColors.h"

#include "fmstyle/ColorScheme.h"
#include "fmstyle/WatercolorChrome.h"

#include "ColorMath_p.h"

namespace fm::style {

using T = Token;
using namespace detail;

ThemeColors::ThemeColors()
    : ThemeColors(Variant::Light)
{
}

ThemeColors::ThemeColors(Variant variant, Design design)
    : m_variant(variant)
    , m_design(design)
{
    for (std::size_t i = 0; i < kTokenCount; ++i)
        m_colors[i] = QColor::fromRgba(builtinColor(static_cast<Token>(i), variant, design));
}

bool ThemeColors::operator==(const ThemeColors &other) const
{
    return m_variant == other.m_variant && m_design == other.m_design && m_colors == other.m_colors;
}

QPalette ThemeColors::toPalette() const
{
    QPalette p;
    const auto all = [&p](QPalette::ColorRole role, const QColor &c) {
        p.setColor(QPalette::All, role, c);
    };

    all(QPalette::Window, color(T::Win));
    all(QPalette::WindowText, color(T::Fg));
    all(QPalette::Base, color(T::Surface));
    all(QPalette::AlternateBase, color(T::Alt));
    all(QPalette::ToolTipBase, color(T::Surface));
    all(QPalette::ToolTipText, color(T::Fg));
    all(QPalette::PlaceholderText, color(T::Fg3));
    all(QPalette::Text, color(T::Fg));
    all(QPalette::Button, color(T::Btn));
    all(QPalette::ButtonText, color(T::Fg));
    all(QPalette::BrightText, QColor(0xFF, 0xFF, 0xFF));
    // Fusion이 직접 그리는 나머지 컨트롤(슬라이더, 다이얼 등)이 어울리도록 입체 역할도 채운다.
    all(QPalette::Light, isDark() ? color(T::BtnLine) : color(T::Surface));
    all(QPalette::Midlight, color(T::Grid));
    all(QPalette::Mid, color(T::Line));
    all(QPalette::Dark, color(T::BtnLine));
    all(QPalette::Shadow, color(T::Shadow));
    all(QPalette::Highlight, color(T::Accent));
    all(QPalette::HighlightedText, color(T::OnAccent));
    all(QPalette::Link, color(T::AccentFg));
    all(QPalette::LinkVisited, color(T::AccentFg));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    all(QPalette::Accent, color(T::Accent));
#endif

    // 비활성 창의 선택 (편집기 안 글자 선택 등)
    p.setColor(QPalette::Inactive, QPalette::Highlight, color(T::SelIn));
    p.setColor(QPalette::Inactive, QPalette::HighlightedText, color(T::Fg));

    // 사용 안 함
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, color(T::Fg3));
    p.setColor(QPalette::Disabled, QPalette::Highlight, color(T::SelIn));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, color(T::Fg3));

    if (m_design == Design::Watercolor) {
        // 스타일이 직접 그리지 않는 컨트롤(슬라이더 등)도 입체 빗면 색을 쓰도록
        const WatercolorChrome &x = watercolorChrome(m_variant);
        all(QPalette::Light, x.hi);
        all(QPalette::Midlight, x.g1);
        all(QPalette::Mid, x.lo);
        all(QPalette::Dark, x.lo);
        all(QPalette::Shadow, x.out);
        all(QPalette::ToolTipBase, x.tipBg);
        all(QPalette::ToolTipText, x.tipFg);
        for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            p.setColor(QPalette::Disabled, role, x.disFg);
    }
    return p;
}

ThemeColors deriveColors(Variant variant, const ThemeSeeds &seeds, const TokenOverrides &overrides,
                         Design design)
{
    return deriveTheme(variant, seeds, overrides, design).colors;
}

double contrastRatio(const QColor &a, const QColor &b)
{
    return contrast(a, b);
}

QList<ContrastResult> checkContrast(const ThemeColors &c, double minimum)
{
    struct Pair { const char *label; Token fg; Token bg; };
    static constexpr Pair kPairs[] = {
        {"기본 글자", T::Fg, T::Surface},
        {"메타 글자", T::Fg3, T::Surface},
        {"선택 레코드", T::Fg, T::Sel},
        {"강조 글자", T::AccentFg, T::Surface},
        {"기본 버튼", T::OnAccent, T::Accent},
        {"영구 삭제", T::OnDanger, T::DangerFill},
        {"역상 커서", T::OnInvCur, T::InvCur},
        {"역상 선택", T::OnInvSel, T::InvSel},
        {"선택 + 역상 커서", T::InvCurSel, T::InvCur},
    };

    QList<ContrastResult> out;
    out.reserve(static_cast<qsizetype>(std::size(kPairs)));
    for (const Pair &pair : kPairs) {
        // 워터컬러의 선택 행은 흰 글자
        const Token fg = (c.isWatercolor() && pair.bg == T::Sel) ? T::OnAccent : pair.fg;
        const double ratio = contrast(c[fg], c[pair.bg]);
        out.append({QString::fromUtf8(pair.label), fg, pair.bg, ratio, ratio >= minimum});
    }
    return out;
}

} // namespace fm::style
