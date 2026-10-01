#pragma once

#include "fmstyle/ThemeTokens.h"

#include <QList>
#include <QPalette>

#include <array>
#include <optional>

namespace fm::style {

/// 한 디자인 · 한 변형(라이트 · 다크 · 남색)의 토큰 51개 값.
class ThemeColors
{
public:
    ThemeColors();
    explicit ThemeColors(Variant variant, Design design = Design::Standard);

    static ThemeColors builtin(Variant variant, Design design = Design::Standard)
    {
        return ThemeColors(variant, design);
    }

    Variant variant() const noexcept { return m_variant; }
    Design design() const noexcept { return m_design; }
    bool isDark() const noexcept { return isDarkVariant(m_variant); }  // 다크 · 남색
    bool isNavy() const noexcept { return m_variant == Variant::Navy; }
    bool isWatercolor() const noexcept { return m_design == Design::Watercolor; }

    QColor color(Token t) const { return m_colors[indexOf(t)]; }
    QColor operator[](Token t) const { return color(t); }
    void setColor(Token t, const QColor &c) { m_colors[indexOf(t)] = c; }

    /// 토큰을 QPalette 역할에 옮긴다. 역할이 없는 토큰은 FmStyle이 직접 읽는다.
    QPalette toPalette() const;

    bool operator==(const ThemeColors &other) const;
    bool operator!=(const ThemeColors &other) const { return !(*this == other); }

private:
    Variant m_variant = Variant::Light;
    Design m_design = Design::Standard;
    std::array<QColor, kTokenCount> m_colors;
};

/// 기준 색 역할 11개(설정 › 테마 색상, docs/specs/04 §2.2.3). 역할마다 첫 토큰이 시드 토큰이다.
enum class SeedRole : std::uint8_t { Accent, Win, Surface, Line, Fg, Sel, InvCur, InvSel, Warn, Danger, Ok, Count };
inline constexpr std::size_t kSeedRoleCount = static_cast<std::size_t>(SeedRole::Count);

/// 디자인 · 변형 하나의 역할 시드. 비어 있는 역할은 손으로 맞춘 내장 값을 쓴다(내장 값 보존 원칙).
struct VariantSeeds
{
    std::array<std::optional<QColor>, kSeedRoleCount> seed{};  // [SeedRole] — Accent 칸은 이 변형만의 강조색
    bool selFollowsAccent = true;      // 선택 · 강조 연동
    bool invSelFollowsAccent = true;   // 역상 선택 · 강조 연동

    bool isEmpty() const noexcept;
    bool operator==(const VariantSeeds &other) const = default;
};

/// 기준 색. 강조색은 두 디자인 · 두 변형 공통(다크 = 흰색 8 % 섞음), 나머지 역할은 디자인 · 변형별.
struct ThemeSeeds
{
    std::optional<QColor> accent;  // 비어 있으면 손으로 맞춘 내장 값을 그대로 쓴다
    bool fixContrast = true;       // 파생 색의 대비가 4.5:1 미만이면 보정
    bool useSystemAccent = false;  // Windows 강조색 사용(설정 › 일반 · 모양)
    std::array<std::array<VariantSeeds, kVariantCount>, 2> roles{};  // [디자인][변형]

    const VariantSeeds &variantSeeds(Design design, Variant variant) const noexcept
    {
        return roles[static_cast<std::size_t>(design)][static_cast<std::size_t>(variant)];
    }
    VariantSeeds &variantSeeds(Design design, Variant variant) noexcept
    {
        return roles[static_cast<std::size_t>(design)][static_cast<std::size_t>(variant)];
    }
    bool operator==(const ThemeSeeds &other) const = default;
};

/// 토큰별 직접 지정 값. 값이 있는 토큰은 기준 색을 바꿔도 따라가지 않는다.
using TokenOverrides = std::array<std::optional<QColor>, kTokenCount>;

/// 테마 색상 화면의 규칙과 같은 식으로 파생 색을 계산한다.
/// 워터컬러는 선택이 강조색 채움(흰 글자)이고 포커스가 글자색 점선이라 규칙이 조금 다르다.
ThemeColors deriveColors(Variant variant, const ThemeSeeds &seeds,
                         const TokenOverrides &overrides = {},
                         Design design = Design::Standard);

struct ContrastResult
{
    QString label;
    Token foreground;
    Token background;
    double ratio;
    bool passes;
};

/// 목업의 '대비 검사'와 같은 9개 조합. 워터컬러의 '선택 레코드'는 흰 글자(강조 위 글자) 기준.
QList<ContrastResult> checkContrast(const ThemeColors &colors, double minimum = 4.5);

double contrastRatio(const QColor &a, const QColor &b);

} // namespace fm::style
