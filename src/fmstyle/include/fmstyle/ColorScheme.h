#pragma once

// 테마 색상 편집(docs/specs/04 §2.2 · §4.5) — 기준 색 역할 11개, 토큰 출처 · 규칙 문구 · 쓰는 곳 · 표시 문자열,
// 보정 정보까지 돌려주는 파생, 이름 있는 색 구성표(JSON).

#include "fmstyle/ThemeColors.h"

#include <QJsonObject>
#include <QString>

#include <bitset>
#include <optional>

namespace fm::style {

// ------------------------------------------------------------------------------------- 기준 색 역할

struct SeedRoleInfo
{
    SeedRole role;
    const char *id;           // "accent" · "win" … (JSON 키)
    const char *label;        // "강조색"
    const char *description;  // "버튼 · 진행 막대 · 링크 · 포커스"
    std::array<Token, 5> tokens;
    int tokenCount;           // 첫 토큰 = 시드 토큰
    bool followsAccent;       // 선택 · 역상 선택 — "강조 연동"

    Token seedToken() const noexcept { return tokens[0]; }
};

const std::array<SeedRoleInfo, kSeedRoleCount> &seedRoles() noexcept;
const SeedRoleInfo &seedRoleInfo(SeedRole role) noexcept;
/// 토큰이 속한 역할(개별 12개는 없음).
std::optional<SeedRole> roleOfToken(Token token) noexcept;

// ------------------------------------------------------------------------------------- 토큰 메타데이터

/// 토큰 출처 — 역할(없으면 개별), 시드 여부, 규칙 문구(라이트 · 다크).
struct TokenSource
{
    std::optional<SeedRole> role;
    bool isSeed = false;
    QString ruleLight;
    QString ruleDark;

    QString rule(Variant variant) const { return isDarkVariant(variant) ? ruleDark : ruleLight; }
};
/// 워터컬러는 선택(강조 채움 · 흰 글자)과 포커스(글자색 점선) 문구가 다르다.
TokenSource tokenSource(Token token, Design design);
/// "창 · 대화상자 본문, 탭 줄, 상태 표시줄" (§2.2.9 쓰는 곳)
QString tokenUsage(Token token);
/// 표시 문자열 — 반투명 틴트는 "흰색 45 %", 그림자는 "2단 · 20 %", 나머지 "#RRGGBB".
QString tokenDisplayValue(Token token, const QColor &color);
/// "#RRGGBB"(불투명) 또는 "#AARRGGBB"(대문자).
QString colorHex(const QColor &color);
/// "#RGB" · "#RRGGBB" · "#AARRGGBB" · "RRGGBB"를 읽는다. 잘못되면 잘못된 색.
QColor parseColorHex(const QString &text);

// ------------------------------------------------------------------------------------- 파생

struct DerivedTheme
{
    ThemeColors colors;
    std::bitset<kTokenCount> adjusted;  // 대비 보정으로 값이 바뀐 토큰("보정됨")
};

/// 기준 색 11역할 + 직접 지정으로 토큰 51개를 계산한다. 시드가 없는 역할은 내장 값을 그대로 두고,
/// 규칙은 그 역할의 시드가 있거나 규칙이 읽는 시드 토큰(목록 바탕 · 글자 · 창 바탕 · 강조 · 역상 커서)이 바뀌었을 때만 쓴다.
DerivedTheme deriveTheme(Variant variant, const ThemeSeeds &seeds, const TokenOverrides &overrides = {},
                         Design design = Design::Standard);

/// 역할의 기준 색(편집기 · 목록 16진수) — 강조색은 기준 색(시드 없으면 내장 강조), 나머지는 첫 토큰 값.
QColor seedColor(SeedRole role, const ThemeColors &derived, const ThemeSeeds &seeds);

// ------------------------------------------------------------------------------------- 색 구성표

/// 이름 있는 색 구성표 — 기준 색 + 디자인 · 변형별 직접 지정. id "builtin" = "기본 (내장)".
struct ColorScheme
{
    QString id = QStringLiteral("builtin");
    QString name;
    ThemeSeeds seeds;
    std::array<std::array<TokenOverrides, kVariantCount>, 2> overrides{};  // [디자인][변형]

    const TokenOverrides &overridesFor(Design design, Variant variant) const noexcept
    {
        return overrides[static_cast<std::size_t>(design)][static_cast<std::size_t>(variant)];
    }
    TokenOverrides &overridesFor(Design design, Variant variant) noexcept
    {
        return overrides[static_cast<std::size_t>(design)][static_cast<std::size_t>(variant)];
    }
    /// 기준 색 · 직접 지정이 하나도 없는가(내장과 같은 색).
    bool isPristine() const;
    /// 색만 비교(id · 이름 무시) — "— 수정됨" 판정.
    bool sameColors(const ColorScheme &other) const;

    QJsonObject toJson() const;
    static std::optional<ColorScheme> fromJson(const QJsonObject &json, QString *error = nullptr);

    bool operator==(const ColorScheme &other) const = default;
};

} // namespace fm::style
