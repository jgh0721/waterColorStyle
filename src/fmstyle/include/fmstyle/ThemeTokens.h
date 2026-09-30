#pragma once

// 테마 토큰 51개 — 디자인 캔버스의 "테마 토큰" 보드와 같은 이름, 같은 기본값.
// 시안1(기본)과 시안2(워터컬러)는 같은 51개 토큰을 쓰고 값만 다르다.

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

enum class Variant : std::uint8_t { Light, Dark };

/// 디자인. Standard = 시안1 "파일 관리자 UI", Watercolor = 시안2 "파일 관리자 UI(워터컬러)".
enum class Design : std::uint8_t { Standard, Watercolor };

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
    QRgb dark;
};

const std::array<TokenInfo, kTokenCount> &allTokens() noexcept;
const TokenInfo &tokenInfo(Token t) noexcept;
QString tokenLabel(Token t);
QString tokenCssName(Token t);

/// 디자인 · 변형별 내장 기본값 #AARRGGBB.
QRgb builtinColor(Token t, Variant variant, Design design) noexcept;

/// "시안1 · 기본", "시안2 · 워터컬러"
QString designLabel(Design design);

} // namespace fm::style
