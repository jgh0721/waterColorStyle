#pragma once

// 테마 토큰 51개 — 디자인 캔버스의 "테마 토큰" 보드와 같은 이름, 같은 기본값.
// 시안1(기본) · 시안2(워터컬러) · 시안3(Paper Fluent)은 같은 51개 토큰을 쓰고 값만 다르다.

#include <QColor>
#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>

namespace fm::style {

enum class Token : std::uint8_t {
    // 바탕 · 면
    Win, Foot, Surface, Alt, Head, Field, Btn, BtnLine, Line, Grid,
    // 글자
    Fg, Fg2, Fg3, OnAccent, OnDanger,
    // 강조 · 선택
    Accent, AccentFg, AccentSoft, Sel, SelIn, Focus,
    // 역상 표시
    InvCur, OnInvCur, InvCurSel, InvSel, OnInvSel, InvSelIn,
    // 레코드 틴트
    Tint, TintSel, TintInv,
    // 상태
    Warn, WarnBg, WarnLine, Danger, DangerFill, DangerBg, DangerLine, Ok, OkBg, Paused,
    // 권한
    Shield, Shield2,
    // 파일 아이콘
    Folder, KExe, KPdf, KImg, KZip, KCode, KDoc, KSys,
    // 효과
    Shadow,

    Count
};

inline constexpr std::size_t kTokenCount = static_cast<std::size_t>(Token::Count);
static_assert(kTokenCount == 51, "the design board defines 51 tokens");

constexpr std::size_t indexOf(Token t) noexcept { return static_cast<std::size_t>(t); }

/// 색 구성 변형. Navy(다크 · 남색)는 시안2 · 시안3 전용이다 — 시안1에서 남색을 요청하면 다크 값을 쓴다.
enum class Variant : std::uint8_t { Light, Dark, Navy };
inline constexpr std::size_t kVariantCount = 3;
/// 다크 계열(다크 · 남색) — 어두운 바탕 규칙을 쓸지 정할 때.
constexpr bool isDarkVariant(Variant v) noexcept { return v != Variant::Light; }

/// 디자인. Standard = 시안1 "파일 관리자 UI", Watercolor = 시안2 "파일 관리자 UI(워터컬러)",
/// PaperFluent = 시안3 "Paper Fluent"(워터컬러 종이 색 + 시안1 Fluent 구조).
enum class Design : std::uint8_t { Standard, Watercolor, PaperFluent };
inline constexpr std::size_t kDesignCount = 3;
constexpr std::size_t designIndex(Design d) noexcept { return static_cast<std::size_t>(d); }
/// 남색 변형이 있는 디자인 — 시안1은 남색 대신 다크를 쓴다.
constexpr bool hasNavyVariant(Design d) noexcept { return d != Design::Standard; }

enum class TokenGroup : std::uint8_t {
    Surfaces,
    Text,
    AccentSelection,
    Inverse,
    RecordTint,
    Status,
    Elevation,
    FileIcons,
    Effects,
};

struct TokenInfo {
    Token token;
    TokenGroup group;
    const char *cssName;  // 목업의 CSS 변수 이름, 예: "--win"
    const char *label;    // 한국어 이름 (UTF-8)
    const char *qtRole;   // 대응하는 QPalette 역할, 없으면 nullptr
    QRgb light;           // 시안1 내장 기본값 #AARRGGBB (워터컬러 값은 builtinColor())
    QRgb dark;            // 시안1에는 남색이 없으므로 남색 요청도 이 값
};

const std::array<TokenInfo, kTokenCount> &allTokens() noexcept;
const TokenInfo &tokenInfo(Token t) noexcept;
QString tokenLabel(Token t);
QString tokenCssName(Token t);

/// 디자인 · 변형별 내장 기본값 #AARRGGBB.
QRgb builtinColor(Token t, Variant variant, Design design) noexcept;

/// "시안1 · 기본", "시안2 · 워터컬러", "시안3 · Paper Fluent"
QString designLabel(Design design);

/// 설정 · 명령줄 · 색 구성표 JSON의 디자인 이름: "standard" · "watercolor" · "paperfluent".
QString designKey(Design design);
/// designKey() 값, 번호(1 · 2 · 3), "시안1" ~ "시안3", "paper-fluent"를 읽는다(대소문자 무시). 모르면 fallback.
Design designFromKey(const QString &key, Design fallback = Design::Standard);

/// "라이트", "다크", "다크(남색)"
QString variantLabel(Variant variant);

} // namespace fm::style
