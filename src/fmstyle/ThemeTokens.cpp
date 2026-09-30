#include "fmstyle/ThemeTokens.h"

namespace fm::style {

namespace {

using G = TokenGroup;
using T = Token;

// 값은 디자인 캔버스의 테마 토큰 보드에서 그대로 옮긴 것. 반투명 값은 알파를 포함한다.
constexpr std::array<TokenInfo, kTokenCount> kTokens{{
    // 바탕 · 면
    {T::Win,        G::Surfaces, "--win",        "창 바탕",           "Window",            0xFFF3F4F6, 0xFF1F2125},
    {T::Foot,       G::Surfaces, "--foot",       "버튼 영역",         nullptr,             0xFFE9EBEF, 0xFF191B1E},
    {T::Surface,    G::Surfaces, "--surface",    "목록 바탕",         "Base",              0xFFFFFFFF, 0xFF16181B},
    {T::Alt,        G::Surfaces, "--alt",        "교차 행",           "AlternateBase",     0xFFF7F8FA, 0xFF1B1D21},
    {T::Head,       G::Surfaces, "--head",       "머리글",            nullptr,             0xFFF1F2F5, 0xFF22252A},
    {T::Field,      G::Surfaces, "--field",      "입력 필드",         nullptr,             0xFFFFFFFF, 0xFF1B1D21},
    {T::Btn,        G::Surfaces, "--btn",        "보조 버튼",         "Button",            0xFFFFFFFF, 0xFF2B2E34},
    {T::BtnLine,    G::Surfaces, "--btn-line",   "버튼 테두리",       "Dark",              0xFFC9CED6, 0xFF3D424A},
    {T::Line,       G::Surfaces, "--line",       "구분선",            "Mid",               0xFFD5D9DF, 0xFF34383F},
    {T::Grid,       G::Surfaces, "--grid",       "그리드 선",         "Midlight",          0xFFECEEF1, 0xFF25282D},
    // 글자
    {T::Fg,         G::Text, "--fg",             "기본 글자",         "WindowText, Text",  0xFF16181C, 0xFFE6E8EB},
    {T::Fg2,        G::Text, "--fg2",            "보조 글자",         nullptr,             0xFF474D57, 0xFFB4B9C1},
    {T::Fg3,        G::Text, "--fg3",            "메타 글자",         "PlaceholderText",   0xFF646A75, 0xFF8D939D},
    {T::OnAccent,   G::Text, "--on-accent",      "강조 위 글자",      "HighlightedText",   0xFFFFFFFF, 0xFFFFFFFF},
    {T::OnDanger,   G::Text, "--on-danger",      "위험 위 글자",      nullptr,             0xFFFFFFFF, 0xFFFFFFFF},
    // 강조 · 선택
    {T::Accent,     G::AccentSelection, "--accent",      "강조 채움",          "Highlight, Accent", 0xFF1F5FD1, 0xFF2F6BD6},
    {T::AccentFg,   G::AccentSelection, "--accent-fg",   "강조 글자",          "Link",              0xFF1A55BD, 0xFF7EA9F4},
    {T::AccentSoft, G::AccentSelection, "--accent-soft", "강조 연한 바탕",     nullptr,             0xFFE3ECFC, 0xFF1E2F4D},
    {T::Sel,        G::AccentSelection, "--sel",         "선택 · 활성 패널",   nullptr,             0xFFD3E2FB, 0xFF203A63},
    {T::SelIn,      G::AccentSelection, "--sel-in",      "선택 · 비활성 패널", "Highlight (Inactive)", 0xFFE2E5EA, 0xFF2C3037},
    {T::Focus,      G::AccentSelection, "--focus",       "포커스 · 커서",      nullptr,             0xFF1F5FD1, 0xFF6A9CF2},
    // 역상 표시
    {T::InvCur,     G::Inverse, "--inv-cur",     "역상 커서",               nullptr, 0xFF2B2F36, 0xFFD5D9DF},
    {T::OnInvCur,   G::Inverse, "--on-inv-cur",  "역상 커서 글자",          nullptr, 0xFFFFFFFF, 0xFF16181B},
    {T::InvCurSel,  G::Inverse, "--inv-cur-sel", "선택 위 역상 커서 글자",  nullptr, 0xFF9DC0FF, 0xFF1A4FB0},
    {T::InvSel,     G::Inverse, "--inv-sel",     "역상 선택 · 활성",        nullptr, 0xFF1F5FD1, 0xFF2F6BD6},
    {T::OnInvSel,   G::Inverse, "--on-inv-sel",  "역상 선택 글자",          nullptr, 0xFFFFFFFF, 0xFFFFFFFF},
    {T::InvSelIn,   G::Inverse, "--inv-sel-in",  "역상 선택 · 비활성",      nullptr, 0xFF5F6773, 0xFF4B525D},
    // 레코드 틴트 (흰색 45 % / 7 %, 흰색 14 % / 검정 8 %)
    {T::Tint,       G::RecordTint, "--tint",     "메타 행 띠",     nullptr, 0xFFEEF1F5, 0xFF202328},
    {T::TintSel,    G::RecordTint, "--tint-sel", "선택 위 띠",     nullptr, 0x73FFFFFF, 0x12FFFFFF},
    {T::TintInv,    G::RecordTint, "--tint-inv", "역상 위 띠",     nullptr, 0x24FFFFFF, 0x14000000},
    // 상태
    {T::Warn,       G::Status, "--warn",        "경고",        nullptr, 0xFF8F4A05, 0xFFF2BA52},
    {T::WarnBg,     G::Status, "--warn-bg",     "경고 바탕",   nullptr, 0xFFFFF5DB, 0xFF2D2412},
    {T::WarnLine,   G::Status, "--warn-line",   "경고 테두리", nullptr, 0xFFEDCB7F, 0xFF5A4619},
    {T::Danger,     G::Status, "--danger",      "위험",        nullptr, 0xFFB42318, 0xFFF2766B},
    {T::DangerFill, G::Status, "--danger-fill", "위험 버튼",   nullptr, 0xFFB42318, 0xFFC4382D},
    {T::DangerBg,   G::Status, "--danger-bg",   "위험 바탕",   nullptr, 0xFFFDEDEB, 0xFF361B19},
    {T::DangerLine, G::Status, "--danger-line", "위험 테두리", nullptr, 0xFFF1C0BA, 0xFF6A2D28},
    {T::Ok,         G::Status, "--ok",          "정상",        nullptr, 0xFF1C7A4A, 0xFF57C792},
    {T::OkBg,       G::Status, "--ok-bg",       "정상 바탕",   nullptr, 0xFFE6F4EC, 0xFF142A1F},
    {T::Paused,     G::Status, "--paused",      "일시 정지",   nullptr, 0xFF8A7A55, 0xFFB8A36E},
    // 권한
    {T::Shield,     G::Elevation, "--shield",   "방패 밝은 쪽",   nullptr, 0xFFE5A11A, 0xFFE5A11A},
    {T::Shield2,    G::Elevation, "--shield-2", "방패 어두운 쪽", nullptr, 0xFFB7780B, 0xFFB7780B},
    // 파일 아이콘
    {T::Folder,     G::FileIcons, "--folder", "폴더",          nullptr, 0xFFD8961A, 0xFFD9A441},
    {T::KExe,       G::FileIcons, "--k-exe",  "실행 파일",     nullptr, 0xFF2F6BD6, 0xFF5B92F0},
    {T::KPdf,       G::FileIcons, "--k-pdf",  "PDF",           nullptr, 0xFFC4382D, 0xFFEE6A5E},
    {T::KImg,       G::FileIcons, "--k-img",  "이미지 · 영상", nullptr, 0xFF138A7E, 0xFF3CC0B0},
    {T::KZip,       G::FileIcons, "--k-zip",  "압축 파일",     nullptr, 0xFF7A4FC4, 0xFFA583E8},
    {T::KCode,      G::FileIcons, "--k-code", "소스 코드",     nullptr, 0xFF2E8540, 0xFF5CBF6E},
    {T::KDoc,       G::FileIcons, "--k-doc",  "문서",          nullptr, 0xFF6B7280, 0xFF9AA1AC},
    {T::KSys,       G::FileIcons, "--k-sys",  "시스템 · 기타", nullptr, 0xFFA0A6AE, 0xFF6F757E},
    // 효과 — 그림자 색 (라이트 rgba(16,20,28,.20), 다크 rgba(0,0,0,.55))
    {T::Shadow,     G::Effects, "--shadow", "그림자", "Shadow", 0x3310141C, 0x8C000000},
}};

constexpr bool tableMatchesEnum() noexcept
{
    for (std::size_t i = 0; i < kTokens.size(); ++i) {
        if (indexOf(kTokens[i].token) != i)
            return false;
    }
    return true;
}
static_assert(tableMatchesEnum(), "token table order must match enum Token");

// 시안2(워터컬러) — 캔버스 "파일 관리자 UI(워터컬러)"의 .fm.light / .fm.dark / .fm.navy 값. 순서는 enum Token과 같다.
struct Triple { QRgb light; QRgb dark; QRgb navy; };
constexpr std::array<Triple, kTokenCount> kWatercolor{{
    {0xFFEBEBE4, 0xFF2E2E2A, 0xFF17213A},  // --win
    {0xFFEBEBE4, 0xFF2E2E2A, 0xFF17213A},  // --foot
    {0xFFFFFFFF, 0xFF1F1F1C, 0xFF0F172C},  // --surface
    {0xFFF4F4EF, 0xFF252522, 0xFF131C33},  // --alt
    {0xFFEBEBE4, 0xFF34342F, 0xFF1D2946},  // --head
    {0xFFFFFFFF, 0xFF1F1F1C, 0xFF0F172C},  // --field
    {0xFFEBEBE4, 0xFF3B3B36, 0xFF22325A},  // --btn
    {0xFFA1A18C, 0xFF6C6C63, 0xFF4A5F8E},  // --btn-line
    {0xFFC2C2B1, 0xFF4A4A43, 0xFF2C3C62},  // --line
    {0xFFE1E1D7, 0xFF33332E, 0xFF1A2540},  // --grid
    {0xFF000000, 0xFFECECE6, 0xFFE4EAF6},  // --fg
    {0xFF3D3D38, 0xFFC2C2B8, 0xFFAEBBD6},  // --fg2
    {0xFF66665F, 0xFF9A9A90, 0xFF8494B5},  // --fg3
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // --on-accent
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // --on-danger
    {0xFF3367BC, 0xFF3F74CC, 0xFF3F74CC},  // --accent
    {0xFF2A559E, 0xFF93B8F2, 0xFF8FB4F0},  // --accent-fg
    {0xFFD6E3F5, 0xFF2A3A55, 0xFF22365E},  // --accent-soft
    {0xFF3367BC, 0xFF3367BC, 0xFF3367BC},  // --sel          선택은 강조색 채움 + 흰 글자
    {0xFFD4D4CD, 0xFF45453F, 0xFF2D3B5C},  // --sel-in
    {0xFF000000, 0xFFECECE6, 0xFFE4EAF6},  // --focus        점선 포커스 · 커서
    {0xFF24428A, 0xFFD8D8D0, 0xFFD5DDF0},  // --inv-cur
    {0xFFFFFFFF, 0xFF1F1F1C, 0xFF0F172C},  // --on-inv-cur
    {0xFFFFD75E, 0xFF1A4FB0, 0xFF1A4FB0},  // --inv-cur-sel
    {0xFF3367BC, 0xFF3367BC, 0xFF3367BC},  // --inv-sel
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // --on-inv-sel
    {0xFF8A8A80, 0xFF55554E, 0xFF3A4A70},  // --inv-sel-in
    {0xFFF0F0EA, 0xFF2A2A26, 0xFF16203A},  // --tint
    {0x29FFFFFF, 0x14FFFFFF, 0x14FFFFFF},  // --tint-sel     흰색 16 % / 8 % / 8 %
    {0x24FFFFFF, 0x1A000000, 0x1A000000},  // --tint-inv     흰색 14 % / 검정 10 % / 10 %
    {0xFF8F4A05, 0xFFF2BA52, 0xFFF2BA52},  // --warn
    {0xFFFFFFE1, 0xFF3A3318, 0xFF2E2A1A},  // --warn-bg
    {0xFFD8C878, 0xFF6A5A22, 0xFF5A4E22},  // --warn-line
    {0xFFB8231A, 0xFFF2766B, 0xFFF2766B},  // --danger
    {0xFFB84A4A, 0xFFB04848, 0xFFB04848},  // --danger-fill
    {0xFFFBE9E7, 0xFF3A1E1B, 0xFF361B22},  // --danger-bg
    {0xFFDFA2A2, 0xFF6E302A, 0xFF6A2D38},  // --danger-line
    {0xFF1C7A32, 0xFF6CCB8E, 0xFF6CCB8E},  // --ok
    {0xFFE4F2E4, 0xFF1A2E20, 0xFF142A26},  // --ok-bg
    {0xFF8A7A55, 0xFFB8A36E, 0xFFB8A36E},  // --paused
    {0xFFE5A11A, 0xFFE5A11A, 0xFFE5A11A},  // --shield
    {0xFFB7780B, 0xFFB7780B, 0xFFB7780B},  // --shield-2
    {0xFFE0AA2E, 0xFFD9A441, 0xFFD9A441},  // --folder
    {0xFF2F6BD6, 0xFF5B92F0, 0xFF5B92F0},  // --k-exe
    {0xFFC4382D, 0xFFEE6A5E, 0xFFEE6A5E},  // --k-pdf
    {0xFF138A7E, 0xFF3CC0B0, 0xFF3CC0B0},  // --k-img
    {0xFF7A4FC4, 0xFFA583E8, 0xFFA583E8},  // --k-zip
    {0xFF2E8540, 0xFF5CBF6E, 0xFF5CBF6E},  // --k-code
    {0xFF6B7280, 0xFF9AA1AC, 0xFF9AA1AC},  // --k-doc
    {0xFFA0A6AE, 0xFF6F757E, 0xFF6F757E},  // --k-sys
    {0x4C000000, 0x99000000, 0xA6000000},  // --shadow       x3 y3 · 30 % / 60 % / 65 %
}};

} // namespace

const std::array<TokenInfo, kTokenCount> &allTokens() noexcept
{
    return kTokens;
}

const TokenInfo &tokenInfo(Token t) noexcept
{
    return kTokens[indexOf(t)];
}

QString tokenLabel(Token t)
{
    return QString::fromUtf8(tokenInfo(t).label);
}

QString tokenCssName(Token t)
{
    return QString::fromLatin1(tokenInfo(t).cssName);
}

QRgb builtinColor(Token t, Variant variant, Design design) noexcept
{
    if (design == Design::Watercolor) {
        const Triple &p = kWatercolor[indexOf(t)];
        switch (variant) {
        case Variant::Light: return p.light;
        case Variant::Dark:  return p.dark;
        case Variant::Navy:  return p.navy;
        }
    }
    const TokenInfo &info = kTokens[indexOf(t)];
    return variant == Variant::Light ? info.light : info.dark;  // 시안1에는 남색이 없다 → 다크
}

QString designLabel(Design design)
{
    return design == Design::Watercolor ? QStringLiteral("시안2 · 워터컬러") : QStringLiteral("시안1 · 기본");
}

QString variantLabel(Variant variant)
{
    switch (variant) {
    case Variant::Light: return QStringLiteral("라이트");
    case Variant::Dark:  return QStringLiteral("다크");
    case Variant::Navy:  return QStringLiteral("다크(남색)");
    }
    return {};
}

} // namespace fm::style
