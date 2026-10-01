#include "fmstyle/ColorScheme.h"

#include "ColorMath_p.h"

#include <QJsonArray>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace fm::style {

using T = Token;
using namespace detail;

namespace {

constexpr std::array<SeedRoleInfo, kSeedRoleCount> kRoles{{
    {SeedRole::Accent, "accent", "강조색", "버튼 · 진행 막대 · 링크 · 포커스",
     {T::Accent, T::AccentFg, T::AccentSoft, T::Focus, T::OnAccent}, 5, false},
    {SeedRole::Win, "win", "창 바탕", "창 · 대화상자 · 탭 줄 · 상태 표시줄", {T::Win, T::Foot, T::Head}, 3, false},
    {SeedRole::Surface, "surface", "목록 바탕", "파일 목록 · 카드 · 입력 필드",
     {T::Surface, T::Alt, T::Field, T::Btn, T::Tint}, 5, false},
    {SeedRole::Line, "line", "구분선", "패널 · 카드 테두리, 그리드 선", {T::Line, T::BtnLine, T::Grid}, 3, false},
    {SeedRole::Fg, "fg", "글자", "기본 · 보조 · 메타데이터 글자", {T::Fg, T::Fg2, T::Fg3}, 3, false},
    {SeedRole::Sel, "sel", "선택", "선택한 레코드 (글자색은 그대로)", {T::Sel, T::SelIn, T::TintSel}, 3, true},
    {SeedRole::InvCur, "invcur", "역상 커서", "TC 방식 역상 커서 막대", {T::InvCur, T::OnInvCur, T::InvCurSel, T::TintInv}, 4, false},
    {SeedRole::InvSel, "invsel", "역상 선택", "TC 방식 역상 선택 채움", {T::InvSel, T::OnInvSel, T::InvSelIn}, 3, true},
    {SeedRole::Warn, "warn", "경고", "권한 배지 · 경고 배너", {T::Warn, T::WarnBg, T::WarnLine}, 3, false},
    {SeedRole::Danger, "danger", "위험", "충돌 · 오류 · 영구 삭제",
     {T::Danger, T::DangerFill, T::DangerBg, T::DangerLine, T::OnDanger}, 5, false},
    {SeedRole::Ok, "ok", "정상", "정상 · 승인 · 자동 태그", {T::Ok, T::OkBg}, 2, false},
}};

struct Rule
{
    T token;
    const char *light;
    const char *dark;  // nullptr = 라이트와 같음
};

// docs/specs/04 §2.2.4 규칙 문구(시안1). 시드 토큰은 "기준 색".
constexpr Rule kRules[] = {
    {T::Accent, "기준 색 · 대비 낮으면 어둡게", "기준 색에 흰색 8 % · 대비 보정"},
    {T::AccentFg, "10 % 어둡게 · 목록 대비 4.5 이상", "흰색 45 % · 목록 대비 4.5 이상"},
    {T::AccentSoft, "목록 바탕에 강조 13 %", "목록 바탕에 강조 28 %"},
    {T::Focus, "= 강조 채움", "강조 채움에 흰색 30 %"},
    {T::OnAccent, "흰색 · 검정 중 대비 높은 쪽", nullptr},
    {T::Win, "기준 색", nullptr},
    {T::Foot, "창 바탕 3 % 어둡게", nullptr},
    {T::Head, "창과 목록 바탕 사이", "창보다 한 단계 밝게"},
    {T::Surface, "기준 색", nullptr},
    {T::Alt, "글자색 2 % 섞음", nullptr},
    {T::Field, "= 목록 바탕", "목록과 창 바탕 사이"},
    {T::Btn, "= 목록 바탕", "목록보다 두 단계 밝게"},
    {T::Tint, "글자색 5 % 섞음", nullptr},
    {T::Line, "기준 색", nullptr},
    {T::BtnLine, "구분선보다 한 단계 진하게", nullptr},
    {T::Grid, "구분선과 목록 바탕 사이", nullptr},
    {T::Fg, "기준 색", nullptr},
    {T::Fg2, "바탕 쪽으로 25 %", nullptr},
    {T::Fg3, "바탕 쪽으로 · 목록 대비 4.5 유지", nullptr},
    {T::Sel, "목록 바탕에 강조 20 % · 글자 대비 4.5 이상", "목록 바탕에 강조 40 % · 글자 대비 4.5 이상"},
    {T::SelIn, "선택 색에서 채도 제거", nullptr},
    {T::TintSel, "흰색 45 %", "흰색 7 %"},
    {T::InvCur, "기준 색", nullptr},
    {T::OnInvCur, "흰색 · 검정 중 대비 높은 쪽", nullptr},
    {T::InvCurSel, "강조에 흰색 55 % · 대비 4.5 이상", "강조에 검정 25 % · 대비 4.5 이상"},
    {T::TintInv, "흰색 14 %", "검정 8 %"},
    {T::InvSel, "= 강조 채움", nullptr},
    {T::OnInvSel, "흰색 · 검정 중 대비 높은 쪽", nullptr},
    {T::InvSelIn, "역상 선택 색에서 채도 제거", nullptr},
    {T::Warn, "기준 색", nullptr},
    {T::WarnBg, "목록 바탕에 경고 12 %", nullptr},
    {T::WarnLine, "목록 바탕에 경고 45 %", nullptr},
    {T::Danger, "기준 색", nullptr},
    {T::DangerFill, "흰 글자 대비 4.5 이상이 되게", nullptr},
    {T::DangerBg, "목록 바탕에 위험 9 %", nullptr},
    {T::DangerLine, "목록 바탕에 위험 30 %", nullptr},
    {T::OnDanger, "흰색 · 검정 중 대비 높은 쪽", nullptr},
    {T::Ok, "기준 색", nullptr},
    {T::OkBg, "목록 바탕에 정상 11 %", nullptr},
};

// 워터컬러(시안2)는 창 바탕 · 버튼 영역 · 머리글 · 보조 버튼이 같은 색이고, 선택은 강조 채움, 포커스는 글자색 점선이다.
constexpr Rule kWatercolorRules[] = {
    {T::Foot, "= 창 바탕", nullptr},
    {T::Head, "= 창 바탕", nullptr},
    {T::Btn, "= 창 바탕", nullptr},
    {T::Field, "= 목록 바탕", nullptr},
    {T::BtnLine, "구분선보다 세 단계 진하게", nullptr},
    {T::Focus, "글자색 점선(규칙 없음)", nullptr},
    {T::Sel, "= 강조 채움 · 흰 글자", nullptr},
    {T::SelIn, "내장 값(비활성 선택 회색)", nullptr},
    {T::InvSelIn, "내장 값(비활성 역상 회색)", nullptr},
};

struct Usage
{
    T token;
    const char *text;
};

constexpr Usage kUsage[] = {
    {T::Win, "창 · 대화상자 본문, 탭 줄, 상태 표시줄"}, {T::Foot, "대화상자 버튼 영역"},
    {T::Surface, "파일 목록, 카드, 명령줄"}, {T::Alt, "2줄 레코드 교차 배경"}, {T::Head, "열 머리글, 카드 머리"},
    {T::Field, "입력 필드, 스위치 꺼짐 바탕"}, {T::Btn, "보조 버튼, 세그먼트"}, {T::BtnLine, "버튼 · 입력 테두리"},
    {T::Line, "패널 · 창 구분선, 카드 테두리"}, {T::Grid, "그리드 선, 레코드 구분선, 태그 바탕"}, {T::Fg, "기본 글자"},
    {T::Fg2, "레이블, 보조 글자"}, {T::Fg3, "메타데이터 행, 캡션, 확장자"},
    {T::OnAccent, "기본 버튼 글자, 켜진 스위치 손잡이"}, {T::OnDanger, "영구 삭제 버튼 글자"},
    {T::Accent, "기본 버튼, 진행 막대, 활성 탭, 켜진 스위치"}, {T::AccentFg, "강조 글자, 링크, 켜진 세그먼트 글자"},
    {T::AccentSoft, "켜진 세그먼트, 선택한 목록 항목, 정보 배지"}, {T::Sel, "선택 레코드 · 활성 패널 (글자색 유지)"},
    {T::SelIn, "선택 레코드 · 비활성 패널"}, {T::Focus, "커서 레코드 테두리, 포커스 링"}, {T::InvCur, "역상 커서 막대"},
    {T::OnInvCur, "역상 커서 위 글자"}, {T::InvCurSel, "선택 항목 위 역상 커서의 글자 (TC 방식)"},
    {T::InvSel, "역상 선택 채움 · 활성 패널"}, {T::OnInvSel, "역상 선택 위 글자"}, {T::InvSelIn, "역상 선택 채움 · 비활성 패널"},
    {T::Tint, "메타 행 틴트 방식의 띠"}, {T::TintSel, "선택 레코드 위의 띠"}, {T::TintInv, "역상 커서 · 역상 선택 위의 띠"},
    {T::Warn, "경고 글자, 경고 태그"}, {T::WarnBg, "권한 배지, 경고 배너, 경고 태그 바탕"},
    {T::WarnLine, "권한 배지 · 경고 배너 테두리"}, {T::Danger, "충돌 · 오류 글자"}, {T::DangerFill, "영구 삭제 버튼"},
    {T::DangerBg, "영구 삭제 경고, 충돌 행 배경"}, {T::DangerLine, "영구 삭제 경고 · 오류 배지 테두리"},
    {T::Ok, "정상 · 승인 글자"}, {T::OkBg, "정상 · 승인 · 자동 태그 바탕"}, {T::Paused, "일시 정지된 진행 막대"},
    {T::Shield, "방패 · 가능하면 SIID_SHIELD 시스템 아이콘"}, {T::Shield2, "방패 어두운 쪽"}, {T::Folder, "폴더"},
    {T::KExe, "실행 파일 · 설치 패키지"}, {T::KPdf, "PDF"}, {T::KImg, "이미지 · 영상"}, {T::KZip, "압축 파일"},
    {T::KCode, "소스 코드 · 설정 파일"}, {T::KDoc, "문서 · 텍스트"}, {T::KSys, "시스템 · 기타"},
    {T::Shadow, "창 안의 팝업 · 카드 (최상위 창은 Windows가 그림)"},
};

const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kBlack(0x00, 0x00, 0x00);

/// HSL 명도를 points(백분율 포인트)만큼 바꾼다.
QColor shiftLightness(const QColor &c, int points)
{
    QColor hsl = c.toHsl();
    const qreal l = std::clamp(hsl.lightnessF() + points / 100.0, 0.0, 1.0);
    return QColor::fromHslF(hsl.hslHueF(), hsl.hslSaturationF(), l, c.alphaF()).toRgb();
}

/// 바탕과의 대비가 커지는 쪽으로 points만큼("한 단계" = 4).
QColor stepAway(const QColor &c, const QColor &bg, int points)
{
    return shiftLightness(c, relativeLuminance(bg) > 0.4 ? -points : points);
}

/// 휘도 유지 회색.
QColor desaturate(const QColor &c)
{
    const int luma = qRound(0.2126 * c.red() + 0.7152 * c.green() + 0.0722 * c.blue());
    return QColor(luma, luma, luma, c.alpha());
}

/// 바탕 쪽으로 옮기되 대비가 minimum 아래로 떨어지기 직전까지.
QColor towardKeeping(const QColor &c, const QColor &bg, double minimum)
{
    QColor best = c;
    for (int step = 1; step <= 50; ++step) {
        const QColor candidate = mix(c, bg, step * 0.02);
        if (contrast(candidate, bg) < minimum)
            break;
        best = candidate;
    }
    return best;
}

const VariantSeeds &seedsFor(const ThemeSeeds &seeds, Design design, Variant variant)
{
    // 시안1에는 남색이 없다 — 다크 칸을 쓴다
    const Variant v = (design == Design::Standard && variant == Variant::Navy) ? Variant::Dark : variant;
    return seeds.variantSeeds(design, v);
}

std::optional<Token> tokenByCss(const QString &css)
{
    for (const TokenInfo &info : allTokens()) {
        if (css == QLatin1StringView(info.cssName))
            return info.token;
    }
    return std::nullopt;
}

QString designKey(Design d) { return d == Design::Watercolor ? u"watercolor"_s : u"standard"_s; }
QString variantKey(Variant v)
{
    switch (v) {
    case Variant::Light: return u"light"_s;
    case Variant::Dark:  return u"dark"_s;
    case Variant::Navy:  return u"navy"_s;
    }
    return {};
}

} // namespace

bool VariantSeeds::isEmpty() const noexcept
{
    return std::none_of(seed.cbegin(), seed.cend(), [](const std::optional<QColor> &c) { return c.has_value(); })
        && selFollowsAccent && invSelFollowsAccent;
}

const std::array<SeedRoleInfo, kSeedRoleCount> &seedRoles() noexcept
{
    return kRoles;
}

const SeedRoleInfo &seedRoleInfo(SeedRole role) noexcept
{
    return kRoles[static_cast<std::size_t>(role)];
}

std::optional<SeedRole> roleOfToken(Token token) noexcept
{
    for (const SeedRoleInfo &info : kRoles) {
        for (int i = 0; i < info.tokenCount; ++i) {
            if (info.tokens[i] == token)
                return info.role;
        }
    }
    return std::nullopt;
}

TokenSource tokenSource(Token token, Design design)
{
    TokenSource source;
    source.role = roleOfToken(token);
    if (!source.role)
        return source;  // 개별
    source.isSeed = seedRoleInfo(*source.role).seedToken() == token;
    auto find = [token](const auto &table) -> const Rule * {
        for (const Rule &r : table) {
            if (r.token == token)
                return &r;
        }
        return nullptr;
    };
    const Rule *rule = design == Design::Watercolor ? find(kWatercolorRules) : nullptr;
    if (!rule)
        rule = find(kRules);
    if (rule) {
        source.ruleLight = QString::fromUtf8(rule->light);
        source.ruleDark = QString::fromUtf8(rule->dark ? rule->dark : rule->light);
    }
    return source;
}

QString tokenUsage(Token token)
{
    for (const Usage &u : kUsage) {
        if (u.token == token)
            return QString::fromUtf8(u.text);
    }
    return QString();
}

QString colorHex(const QColor &color)
{
    return (color.alpha() == 255 ? color.name(QColor::HexRgb) : color.name(QColor::HexArgb)).toUpper();
}

QColor parseColorHex(const QString &text)
{
    QString t = text.trimmed();
    if (!t.startsWith(u'#'))
        t.prepend(u'#');
    static const QRegularExpression valid(u"^#([0-9A-Fa-f]{3}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$"_s);
    if (!valid.match(t).hasMatch())
        return QColor();
    return QColor::fromString(t);
}

QString tokenDisplayValue(Token token, const QColor &color)
{
    if (color.alpha() < 255) {
        const int pct = qRound(color.alphaF() * 100);
        if (token == T::Shadow)
            return u"2단 · %1 %"_s.arg(pct);
        if (color.rgb() == kWhite.rgb())
            return u"흰색 %1 %"_s.arg(pct);
        if (color.red() == 0 && color.green() == 0 && color.blue() == 0)
            return u"검정 %1 %"_s.arg(pct);
    }
    return colorHex(color);
}

DerivedTheme deriveTheme(Variant variant, const ThemeSeeds &seeds, const TokenOverrides &overrides, Design design)
{
    DerivedTheme out{ThemeColors(variant, design), {}};
    ThemeColors &c = out.colors;
    const ThemeColors builtin(variant, design);
    const bool light = variant == Variant::Light;  // 남색은 다크 규칙
    const bool watercolor = design == Design::Watercolor;
    const bool fix = seeds.fixContrast;
    const VariantSeeds &vs = seedsFor(seeds, design, variant);
    auto seed = [&vs](SeedRole r) -> std::optional<QColor> {
        const auto &s = vs.seed[static_cast<std::size_t>(r)];
        return s && s->isValid() ? s : std::nullopt;
    };
    auto set = [&c](T t, const QColor &v) { c.setColor(t, v); };
    // 대비가 모자라면 toward 쪽으로 밀고 "보정됨"으로 표시한다
    auto adjust = [&](T t, QColor v, const QColor &toward, const QColor &bg, double minimum) {
        if (fix && contrast(v, bg) < minimum) {
            v = pushUntil(v, toward, bg, minimum);
            out.adjusted.set(indexOf(t));
        }
        set(t, v);
    };
    auto changed = [&](T t) { return c[t] != builtin[t]; };

    // ① 시드 토큰
    for (const SeedRole r : {SeedRole::Win, SeedRole::Surface, SeedRole::Line, SeedRole::Fg, SeedRole::InvCur,
                             SeedRole::Warn, SeedRole::Danger, SeedRole::Ok}) {
        if (const auto s = seed(r))
            set(seedRoleInfo(r).seedToken(), *s);
    }
    if (!vs.selFollowsAccent && seed(SeedRole::Sel))
        set(T::Sel, *seed(SeedRole::Sel));
    if (!vs.invSelFollowsAccent && seed(SeedRole::InvSel))
        set(T::InvSel, *seed(SeedRole::InvSel));

    // ② 역할별 파생 — 규칙은 시드 토큰만 읽으므로 순환이 없다
    const QColor win = c[T::Win];
    const QColor surface = c[T::Surface];
    const QColor fg = c[T::Fg];
    const QColor line = c[T::Line];

    if (changed(T::Win)) {
        set(T::Foot, watercolor ? win : shiftLightness(win, -3));
        set(T::Head, watercolor ? win : light ? mix(win, surface, 0.5) : shiftLightness(win, 4));
        if (watercolor)
            set(T::Btn, win);
    }
    if (changed(T::Surface) || changed(T::Fg) || (!light && changed(T::Win))) {
        set(T::Alt, mix(surface, fg, 0.02));
        set(T::Field, (light || watercolor) ? surface : mix(surface, win, 0.5));
        if (!watercolor)
            set(T::Btn, light ? surface : shiftLightness(surface, 8));
        set(T::Tint, mix(surface, fg, 0.05));
    }
    if (changed(T::Line) || changed(T::Surface)) {
        set(T::BtnLine, stepAway(line, surface, watercolor ? 12 : 4));
        set(T::Grid, mix(line, surface, 0.5));
    }
    if (changed(T::Fg) || changed(T::Surface)) {
        set(T::Fg2, mix(fg, surface, 0.25));
        set(T::Fg3, towardKeeping(fg, surface, 4.5));
    }

    // 강조색 — 변형별 강조색이 있으면 그것, 없으면 공통 강조색(다크는 흰색 8 %)
    const std::optional<QColor> variantAccent = seed(SeedRole::Accent);
    const bool accentSeeded = variantAccent.has_value() || (seeds.accent && seeds.accent->isValid());
    if (accentSeeded || changed(T::Surface) || changed(T::Fg) || changed(T::InvCur)) {
        const QColor seedAccent = variantAccent ? *variantAccent
                                : accentSeeded  ? (light ? *seeds.accent : mix(*seeds.accent, kWhite, 0.08))
                                                : c[T::Accent];
        if (accentSeeded)
            adjust(T::Accent, seedAccent, kBlack, kWhite, 4.5);  // 흰 글자 대비가 모자라면 어둡게
        const QColor accent = c[T::Accent];
        if (accentSeeded || changed(T::Surface)) {
            adjust(T::AccentFg, light ? mix(seedAccent, kBlack, 0.10) : mix(seedAccent, kWhite, 0.45), light ? kBlack : kWhite,
                   surface, 4.5);
            set(T::AccentSoft, mix(surface, accent, light ? 0.13 : 0.28));
        }
        if (accentSeeded) {
            if (!watercolor)  // 워터컬러의 포커스는 글자색 점선
                set(T::Focus, light ? accent : mix(accent, kWhite, 0.30));
            set(T::OnAccent, fix ? bestTextOn(accent) : kWhite);
        }
        if (vs.selFollowsAccent && (accentSeeded || changed(T::Surface) || changed(T::Fg))) {
            if (watercolor)
                set(T::Sel, accent);  // 강조 채움 + 강조 위 글자
            else
                adjust(T::Sel, mix(surface, accent, light ? 0.20 : 0.40), surface, fg, 4.5);
        }
        if (vs.invSelFollowsAccent && accentSeeded)
            set(T::InvSel, accent);
        if (accentSeeded || changed(T::InvCur)) {
            adjust(T::InvCurSel, light ? mix(accent, kWhite, 0.55) : mix(accent, kBlack, 0.25), light ? kWhite : kBlack,
                   c[T::InvCur], 4.5);
        }
    }
    if (changed(T::Sel) && !watercolor)
        set(T::SelIn, desaturate(c[T::Sel]));
    if (changed(T::InvCur))
        set(T::OnInvCur, bestTextOn(c[T::InvCur]));
    if (changed(T::InvSel)) {
        set(T::OnInvSel, fix ? bestTextOn(c[T::InvSel]) : kWhite);
        if (!watercolor)
            set(T::InvSelIn, desaturate(c[T::InvSel]));
    }
    if (changed(T::Warn) || changed(T::Surface)) {
        set(T::WarnBg, mix(surface, c[T::Warn], 0.12));
        set(T::WarnLine, mix(surface, c[T::Warn], 0.45));
    }
    if (changed(T::Danger) || changed(T::Surface)) {
        const QColor danger = c[T::Danger];
        if (changed(T::Danger))
            adjust(T::DangerFill, danger, kBlack, kWhite, 4.5);
        set(T::DangerBg, mix(surface, danger, 0.09));
        set(T::DangerLine, mix(surface, danger, 0.30));
        set(T::OnDanger, bestTextOn(c[T::DangerFill]));
    }
    if (changed(T::Ok) || changed(T::Surface))
        set(T::OkBg, mix(surface, c[T::Ok], 0.11));

    // ③ 직접 지정
    for (std::size_t i = 0; i < overrides.size(); ++i) {
        if (overrides[i]) {
            c.setColor(static_cast<Token>(i), *overrides[i]);
            out.adjusted.reset(i);
        }
    }
    return out;
}

QColor seedColor(SeedRole role, const ThemeColors &derived, const ThemeSeeds &seeds)
{
    if (role == SeedRole::Accent) {
        const VariantSeeds &vs = seedsFor(seeds, derived.design(), derived.variant());
        if (const auto &a = vs.seed[static_cast<std::size_t>(SeedRole::Accent)]; a && a->isValid())
            return *a;
        if (seeds.accent && seeds.accent->isValid())
            return derived.isDark() ? mix(*seeds.accent, kWhite, 0.08) : *seeds.accent;
        return derived[T::Accent];
    }
    return derived[seedRoleInfo(role).seedToken()];
}

// ------------------------------------------------------------------------------------- ColorScheme

bool ColorScheme::isPristine() const
{
    return sameColors(ColorScheme());
}

bool ColorScheme::sameColors(const ColorScheme &other) const
{
    return seeds == other.seeds && overrides == other.overrides;
}

QJsonObject ColorScheme::toJson() const
{
    QJsonObject root;
    root[u"format"_s] = u"fm-color-scheme"_s;
    root[u"version"_s] = 1;
    root[u"id"_s] = id;
    root[u"name"_s] = name;
    root[u"fixContrast"_s] = seeds.fixContrast;
    QJsonObject accent;
    accent[u"color"_s] = seeds.accent ? QJsonValue(colorHex(*seeds.accent)) : QJsonValue(QJsonValue::Null);
    accent[u"useSystem"_s] = seeds.useSystemAccent;
    root[u"accent"_s] = accent;

    QJsonObject designs;
    for (const Design d : {Design::Standard, Design::Watercolor}) {
        QJsonObject variants;
        for (const Variant v : {Variant::Light, Variant::Dark, Variant::Navy}) {
            if (d == Design::Standard && v == Variant::Navy)
                continue;
            const VariantSeeds &vs = seeds.variantSeeds(d, v);
            const TokenOverrides &ov = overridesFor(d, v);
            QJsonObject seedObj;
            for (const SeedRoleInfo &info : kRoles) {
                if (const auto &s = vs.seed[static_cast<std::size_t>(info.role)])
                    seedObj[QString::fromLatin1(info.id)] = colorHex(*s);
            }
            QJsonObject overrideObj;
            for (std::size_t i = 0; i < ov.size(); ++i) {
                if (ov[i])
                    overrideObj[tokenCssName(static_cast<Token>(i))] = colorHex(*ov[i]);
            }
            if (seedObj.isEmpty() && overrideObj.isEmpty() && vs.selFollowsAccent && vs.invSelFollowsAccent)
                continue;
            QJsonObject variantObj;
            variantObj[u"seeds"_s] = seedObj;
            variantObj[u"follow"_s] = QJsonObject{{u"sel"_s, vs.selFollowsAccent}, {u"invsel"_s, vs.invSelFollowsAccent}};
            variantObj[u"overrides"_s] = overrideObj;
            variants[variantKey(v)] = variantObj;
        }
        if (!variants.isEmpty())
            designs[designKey(d)] = variants;
    }
    root[u"designs"_s] = designs;
    return root;
}

std::optional<ColorScheme> ColorScheme::fromJson(const QJsonObject &json, QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<ColorScheme> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (json.value(u"format"_s).toString() != u"fm-color-scheme")
        return fail(u"색 구성표 파일이 아닙니다"_s);
    if (json.value(u"version"_s).toInt(1) > 1)
        return fail(u"더 새로운 버전의 색 구성표입니다"_s);
    ColorScheme scheme;
    scheme.id = json.value(u"id"_s).toString(u"custom"_s);
    scheme.name = json.value(u"name"_s).toString();
    scheme.seeds.fixContrast = json.value(u"fixContrast"_s).toBool(true);
    const QJsonObject accent = json.value(u"accent"_s).toObject();
    if (const QColor a = parseColorHex(accent.value(u"color"_s).toString()); a.isValid())
        scheme.seeds.accent = a;
    scheme.seeds.useSystemAccent = accent.value(u"useSystem"_s).toBool(false);

    const QJsonObject designs = json.value(u"designs"_s).toObject();
    for (const Design d : {Design::Standard, Design::Watercolor}) {
        const QJsonObject variants = designs.value(designKey(d)).toObject();
        for (const Variant v : {Variant::Light, Variant::Dark, Variant::Navy}) {
            const QJsonObject variantObj = variants.value(variantKey(v)).toObject();
            if (variantObj.isEmpty())
                continue;
            VariantSeeds &vs = scheme.seeds.variantSeeds(d, v);
            const QJsonObject seedObj = variantObj.value(u"seeds"_s).toObject();
            for (const SeedRoleInfo &info : kRoles) {
                if (const QColor s = parseColorHex(seedObj.value(QString::fromLatin1(info.id)).toString()); s.isValid())
                    vs.seed[static_cast<std::size_t>(info.role)] = s;
            }
            const QJsonObject follow = variantObj.value(u"follow"_s).toObject();
            vs.selFollowsAccent = follow.value(u"sel"_s).toBool(true);
            vs.invSelFollowsAccent = follow.value(u"invsel"_s).toBool(true);
            const QJsonObject overrideObj = variantObj.value(u"overrides"_s).toObject();
            for (auto it = overrideObj.begin(); it != overrideObj.end(); ++it) {
                const auto token = tokenByCss(it.key());
                const QColor color = parseColorHex(it.value().toString());
                if (token && color.isValid())
                    scheme.overridesFor(d, v)[indexOf(*token)] = color;
            }
        }
    }
    return scheme;
}

} // namespace fm::style
