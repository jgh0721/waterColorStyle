#include "fmfilelist/FileGroups.h"

#include "fmfilelist/FileRoles.h"

#include <QMimeDatabase>

#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::filelist {

namespace {

using Field = GroupCondition::Field;
using Op = GroupCondition::Op;

QStringList splitList(const QString &text, QChar separator)
{
    QStringList out;
    for (const QString &part : text.split(separator)) {
        const QString t = part.trimmed();
        if (!t.isEmpty())
            out.append(t);
    }
    return out;
}

QSet<QString> extensionSet(const QString &text)
{
    QSet<QString> out;
    for (QString part : splitList(text, u',')) {
        while (part.startsWith(u'.') || part.startsWith(u'*'))
            part.remove(0, 1);
        if (!part.isEmpty())
            out.insert(part.toLower());
    }
    return out;
}

int attributeFlags(const QString &text)
{
    int flags = 0;
    const QString t = text.toLower();
    if (t.contains(u"(h)") || t.contains(u"숨김"))
        flags |= Hidden;
    if (t.contains(u"(s)") || t.contains(u"시스템"))
        flags |= System;
    if (t.contains(u"(r)") || t.contains(u"읽기 전용"))
        flags |= ReadOnly;
    if (t.contains(u"(a)") || t.contains(u"보관"))
        flags |= Archive;
    if (t.contains(u"재분석") || t.contains(u"링크") || t.contains(u"정션"))
        flags |= ReparsePoint;
    return flags;
}

QString attributeSummary(int flags)
{
    QStringList parts;
    if (flags & Hidden)
        parts.append(u"H"_s);
    if (flags & System)
        parts.append(u"S"_s);
    if (flags & ReadOnly)
        parts.append(u"R"_s);
    if (flags & Archive)
        parts.append(u"A"_s);
    if (flags & ReparsePoint)
        parts.append(u"링크 · 정션"_s);
    return u"속성에 "_s + parts.join(u" 또는 "_s);
}

const QMimeDatabase &mimeDatabase()
{
    static const QMimeDatabase db;
    return db;
}

QColor invertLightness(const QColor &c)
{
    const QColor hsl = c.toHsl();
    return QColor::fromHslF(hsl.hslHueF(), hsl.hslSaturationF(), 1.0 - hsl.lightnessF()).toRgb();
}

GroupCondition condition(Field field, Op op, const QString &value)
{
    return {field, op, value};
}

FileGroup group(const QString &id, const QString &name, QList<GroupCondition> conditions)
{
    FileGroup g;
    g.id = id;
    g.name = name;
    g.conditions = std::move(conditions);
    return g;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// 기본값 (05 §2.1.1 샘플 = 첫 실행 기본값)

FileGroupSettings FileGroupSettings::defaults()
{
    FileGroupSettings s;
    FileGroup hid = group(u"hid"_s, u"숨김 · 시스템"_s, {condition(Field::Attributes, Op::Has, u"숨김(H) 또는 시스템(S)"_s)});
    hid.builtin = true;
    hid.style.textLight = QColor(0x6B, 0x71, 0x7C);
    hid.style.textDark = QColor(0x8A, 0x90, 0x99);
    hid.style.italic = true;

    FileGroup exe = group(u"exe"_s, u"실행 파일"_s, {condition(Field::Extension, Op::AnyOf, u"exe, msi, cmd, bat, ps1, com"_s)});
    exe.style.textLight = QColor(0x1D, 0x5B, 0xC7);
    exe.style.textDark = QColor(0x82, 0xAE, 0xF6);

    FileGroup doc = group(u"doc"_s, u"문서"_s,
                          {condition(Field::Extension, Op::AnyOf, u"pdf, docx, xlsx, pptx, hwp, hwpx, md, txt"_s)});
    doc.style.textLight = QColor(0xA8, 0x32, 0x1F);
    doc.style.textDark = QColor(0xF0, 0x8A, 0x7A);

    FileGroup media = group(u"media"_s, u"이미지 · 영상"_s,
                            {condition(Field::Extension, Op::AnyOf, u"png, jpg, jpeg, heic, webp, arw, mp4, mkv, mov"_s),
                             condition(Field::MimeType, Op::MimeStartsWith, u"image/, video/"_s)});
    media.style.textLight = QColor(0x0F, 0x7A, 0x6E);
    media.style.textDark = QColor(0x4F, 0xD1, 0xBF);

    FileGroup zip = group(u"zip"_s, u"압축 파일"_s, {condition(Field::Extension, Op::AnyOf, u"zip, 7z, rar, tar, gz, zst, cab"_s)});
    zip.style.textLight = QColor(0x6D, 0x3F, 0xC0);
    zip.style.textDark = QColor(0xB3, 0x94, 0xF0);

    FileGroup src = group(u"src"_s, u"소스 코드"_s,
                          {condition(Field::Extension, Op::AnyOf, u"cpp, cxx, h, hpp, inl, ixx, ui, qrc"_s),
                           condition(Field::Name, Op::Wildcard, u"CMakeLists.txt; *.cmake"_s)});
    src.style.textLight = QColor(0x9A, 0x5B, 0x00);
    src.style.textDark = QColor(0xF0, 0xB2, 0x4D);
    src.style.bold = true;

    FileGroup link = group(u"link"_s, u"링크 · 바로 가기"_s,
                           {condition(Field::Extension, Op::AnyOf, u"lnk, url"_s),
                            condition(Field::Attributes, Op::Has, u"재분석 지점 (링크 · 정션)"_s)});
    link.style.underline = true;

    FileGroup big = group(u"big"_s, u"큰 파일"_s, {condition(Field::Size, Op::AtLeast, u"1 GB"_s)});
    big.style.textLight = QColor(0xA3, 0x24, 0x6B);
    big.style.textDark = QColor(0xF0, 0x7D, 0xB8);

    FileGroup empty = group(u"empty"_s, u"빈 파일"_s, {condition(Field::Size, Op::SizeEquals, u"0 바이트"_s)});
    empty.style.strike = true;

    FileGroup recent = group(u"new"_s, u"최근 24시간에 바뀜"_s, {condition(Field::Modified, Op::Within, u"24시간 이내"_s)});
    recent.style.backLight = QColor(0xFF, 0xF1, 0xC9);
    recent.style.backDark = QColor(0x3A, 0x2F, 0x10);

    s.groups = {hid, exe, doc, media, zip, src, link, big, empty, recent};
    s.merge = Merge::PerProperty;
    return s;
}

// ---------------------------------------------------------------------------------------------
// 값 해석

std::optional<qint64> parseSizeText(const QString &text)
{
    static const QRegularExpression re(u"^\\s*([0-9]+(?:[.,][0-9]+)?)\\s*(바이트|b|kb|mb|gb|tb|k|m|g|t)?\\s*$"_s,
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    QString number = m.captured(1);
    number.replace(u',', u'.');
    const double value = number.toDouble();
    const QString unit = m.captured(2).toLower();
    double scale = 1;
    if (unit == u"kb" || unit == u"k")
        scale = 1024.0;
    else if (unit == u"mb" || unit == u"m")
        scale = 1024.0 * 1024;
    else if (unit == u"gb" || unit == u"g")
        scale = 1024.0 * 1024 * 1024;
    else if (unit == u"tb" || unit == u"t")
        scale = 1024.0 * 1024 * 1024 * 1024;
    return qint64(std::llround(value * scale));
}

std::optional<qint64> parseDurationText(const QString &text)
{
    static const QRegularExpression re(u"^\\s*([0-9]+)\\s*(분|시간|일|주|개월)?\\s*(이내|전|이전)?\\s*$"_s);
    const auto m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    const qint64 n = m.captured(1).toLongLong();
    const QString unit = m.captured(2);
    qint64 scale = 86400;  // 단위가 없으면 일
    if (unit == u"분")
        scale = 60;
    else if (unit == u"시간")
        scale = 3600;
    else if (unit == u"주")
        scale = 7 * 86400;
    else if (unit == u"개월")
        scale = 30 * 86400;
    return n * scale;
}

bool validateCondition(const GroupCondition &c, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (c.value.trimmed().isEmpty())
        return fail(u"값을 입력하세요"_s);
    switch (c.field) {
    case Field::Extension:
        return extensionSet(c.value).isEmpty() ? fail(u"확장자를 쉼표로 나눠 적으세요"_s) : true;
    case Field::Name:
        if (c.op == Op::Regex) {
            const QRegularExpression re(c.value);
            if (!re.isValid())
                return fail(u"정규식 오류: "_s + re.errorString());
        }
        return true;
    case Field::Attributes:
        return attributeFlags(c.value) ? true : fail(u"숨김(H) · 시스템(S) · 읽기 전용(R) · 보관(A) · 링크 중에서 적으세요"_s);
    case Field::MimeType:
        return splitList(c.value, u',').isEmpty() ? fail(u"MIME 형식을 적으세요"_s) : true;
    case Field::Size:
        if (c.op == Op::Between) {
            const QStringList parts = c.value.split(u'~');
            if (parts.size() != 2 || !parseSizeText(parts.at(0)) || !parseSizeText(parts.at(1)))
                return fail(u"\"1 MB ~ 10 MB\"처럼 적으세요"_s);
            return true;
        }
        return parseSizeText(c.value) ? true : fail(u"\"1 GB\" · \"0 바이트\"처럼 적으세요"_s);
    case Field::Modified:
    case Field::Created:
        return parseDurationText(c.value) ? true : fail(u"\"24시간 이내\" · \"7일\"처럼 적으세요"_s);
    }
    return true;
}

QColor darkFromLightColor(const QColor &light, const QColor &darkBackground)
{
    const QColor hsl = light.toHsl();
    QColor dark = QColor::fromHslF(hsl.hslHueF(), hsl.hslSaturationF(), 1.0 - hsl.lightnessF()).toRgb();
    auto luminance = [](const QColor &c) {
        auto ch = [](int v) {
            const double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
    };
    auto contrast = [&](const QColor &a, const QColor &b) {
        const double x = luminance(a), y = luminance(b);
        return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
    };
    for (int i = 0; i < 30 && contrast(dark, darkBackground) < 4.5; ++i) {
        dark = QColor(qRound(dark.red() + (255 - dark.red()) * 0.06), qRound(dark.green() + (255 - dark.green()) * 0.06),
                      qRound(dark.blue() + (255 - dark.blue()) * 0.06));
    }
    return dark;
}

QString conditionFieldLabel(Field field)
{
    switch (field) {
    case Field::Extension:  return u"확장자"_s;
    case Field::Name:       return u"이름"_s;
    case Field::Attributes: return u"속성"_s;
    case Field::MimeType:   return u"파일 형식"_s;
    case Field::Size:       return u"크기"_s;
    case Field::Modified:   return u"수정한 날짜"_s;
    case Field::Created:    return u"만든 날짜"_s;
    }
    return {};
}

QString conditionOpLabel(Op op)
{
    switch (op) {
    case Op::AnyOf:          return u"다음 중 하나"_s;
    case Op::NoneOf:         return u"다음이 아닌 것"_s;
    case Op::Wildcard:       return u"와일드카드"_s;
    case Op::Regex:          return u"정규식"_s;
    case Op::Contains:       return u"포함"_s;
    case Op::Equals:         return u"같음"_s;
    case Op::Has:            return u"포함"_s;
    case Op::HasNot:         return u"포함 안 함"_s;
    case Op::MimeStartsWith: return u"MIME 시작"_s;
    case Op::MimeEquals:     return u"MIME 같음"_s;
    case Op::AtLeast:        return u"이상"_s;
    case Op::AtMost:         return u"이하"_s;
    case Op::SizeEquals:     return u"같음"_s;
    case Op::Between:        return u"사이"_s;
    case Op::Within:         return u"최근"_s;
    case Op::OlderThan:      return u"이전"_s;
    }
    return {};
}

QList<Op> conditionOps(Field field)
{
    switch (field) {
    case Field::Extension:  return {Op::AnyOf, Op::NoneOf};
    case Field::Name:       return {Op::Wildcard, Op::Regex, Op::Contains, Op::Equals};
    case Field::Attributes: return {Op::Has, Op::HasNot};
    case Field::MimeType:   return {Op::MimeStartsWith, Op::MimeEquals};
    case Field::Size:       return {Op::AtLeast, Op::AtMost, Op::SizeEquals, Op::Between};
    case Field::Modified:
    case Field::Created:    return {Op::Within, Op::OlderThan};
    }
    return {};
}

QString groupSummary(const FileGroup &g)
{
    QStringList parts;
    for (const GroupCondition &c : g.conditions) {
        switch (c.field) {
        case Field::Extension: {
            const QStringList exts = splitList(c.value, u',');
            QStringList shown;
            for (int i = 0; i < std::min<qsizetype>(5, exts.size()); ++i)
                shown.append(u"*."_s + exts.at(i).toLower());
            QString text = shown.join(u"  "_s);
            if (exts.size() > 5)
                text += u" …"_s;
            parts.append(c.op == Op::NoneOf ? text + u" 제외"_s : text);
            break;
        }
        case Field::Name:
            parts.append(c.value.trimmed());
            break;
        case Field::Attributes:
            parts.append((c.op == Op::HasNot ? u"속성 없음: "_s : QString()) + attributeSummary(attributeFlags(c.value)));
            break;
        case Field::MimeType:
            parts.append(c.value.trimmed());
            break;
        case Field::Size: {
            const QString v = c.value.trimmed();
            switch (c.op) {
            case Op::AtLeast: parts.append(u"크기 %1 이상"_s.arg(v)); break;
            case Op::AtMost:  parts.append(u"크기 %1 이하"_s.arg(v)); break;
            case Op::Between: parts.append(u"크기 %1"_s.arg(v)); break;
            default:          parts.append(u"크기 %1"_s.arg(v)); break;
            }
            break;
        }
        case Field::Modified:
        case Field::Created: {
            const QString what = c.field == Field::Modified ? u"수정한 날짜"_s : u"만든 날짜"_s;
            parts.append(c.op == Op::OlderThan ? u"%1 %2보다 이전"_s.arg(what, c.value.trimmed())
                                               : u"%1 %2"_s.arg(what, c.value.trimmed()));
            break;
        }
        }
    }
    QString summary = parts.join(u"  ·  "_s);
    const GroupStyle &s = g.style;
    const bool text = s.textLight || s.textDark;
    const bool back = s.backLight || s.backDark;
    if (!text && !back && s.hasEffect()) {
        QStringList effects;
        if (s.bold)
            effects.append(u"굵게"_s);
        if (s.italic)
            effects.append(u"기울임"_s);
        if (s.underline)
            effects.append(u"밑줄"_s);
        if (s.strike)
            effects.append(u"취소선"_s);
        summary += u" · "_s + effects.join(u" · "_s) + u"만"_s;
    } else if (!text && back && !s.hasEffect()) {
        summary += u" · 배경만"_s;
    }
    return summary;
}

// ---------------------------------------------------------------------------------------------
// FileGroupMatcher

FileGroupMatcher::FileGroupMatcher(const FileGroupSettings &settings)
    : m_settings(settings)
{
    for (const FileGroup &g : settings.groups) {
        CompiledGroup cg;
        cg.matchAll = g.matchAll;
        for (const GroupCondition &c : g.conditions) {
            Compiled k;
            k.field = c.field;
            k.op = c.op;
            k.valid = validateCondition(c);
            if (k.valid) {
                switch (c.field) {
                case Field::Extension:
                    k.extensions = extensionSet(c.value);
                    break;
                case Field::Name:
                    if (c.op == Op::Wildcard) {
                        for (const QString &p : splitList(c.value, u';'))
                            k.patterns.append(QRegularExpression::fromWildcard(p, Qt::CaseInsensitive));
                    } else if (c.op == Op::Regex) {
                        k.patterns.append(QRegularExpression(c.value, QRegularExpression::CaseInsensitiveOption));
                    } else {
                        k.text = c.value.trimmed();
                    }
                    break;
                case Field::Attributes:
                    k.attributes = attributeFlags(c.value);
                    break;
                case Field::MimeType:
                    for (const QString &m : splitList(c.value, u','))
                        k.mimes.append(m.toLower());
                    break;
                case Field::Size:
                    if (c.op == Op::Between) {
                        const QStringList parts = c.value.split(u'~');
                        k.low = *parseSizeText(parts.at(0));
                        k.high = *parseSizeText(parts.at(1));
                        if (k.low > k.high)
                            std::swap(k.low, k.high);
                    } else {
                        k.low = *parseSizeText(c.value);
                    }
                    break;
                case Field::Modified:
                case Field::Created:
                    k.seconds = *parseDurationText(c.value);
                    break;
                }
            }
            cg.conditions.append(k);
        }
        m_groups.append(cg);
    }
}

bool FileGroupMatcher::matches(const Compiled &c, const FileFacts &f, const QDateTime &now) const
{
    switch (c.field) {
    case Field::Extension: {
        if (f.isDir)
            return false;
        const bool in = c.extensions.contains(f.ext.toLower());
        return c.op == Op::NoneOf ? !in : in;
    }
    case Field::Name:
        switch (c.op) {
        case Op::Wildcard:
        case Op::Regex:
            return std::any_of(c.patterns.cbegin(), c.patterns.cend(),
                               [&f](const QRegularExpression &re) { return re.match(f.name).hasMatch(); });
        case Op::Contains: return f.name.contains(c.text, Qt::CaseInsensitive);
        default:           return f.name.compare(c.text, Qt::CaseInsensitive) == 0;
        }
    case Field::Attributes: {
        const bool any = (f.attributes & c.attributes) != 0;
        return c.op == Op::HasNot ? !any : any;
    }
    case Field::MimeType: {
        if (f.isDir)
            return false;
        const QString mime = mimeDatabase().mimeTypeForFile(f.name, QMimeDatabase::MatchExtension).name().toLower();
        return std::any_of(c.mimes.cbegin(), c.mimes.cend(), [&](const QString &m) {
            return c.op == Op::MimeEquals ? mime == m : mime.startsWith(m);
        });
    }
    case Field::Size:
        if (f.isDir || f.size < 0)
            return false;
        switch (c.op) {
        case Op::AtLeast: return f.size >= c.low;
        case Op::AtMost:  return f.size <= c.low;
        case Op::Between: return f.size >= c.low && f.size <= c.high;
        default:          return f.size == c.low;
        }
    case Field::Modified:
    case Field::Created: {
        const QDateTime &t = c.field == Field::Modified ? f.modified : f.created;
        if (!t.isValid())
            return false;
        const qint64 age = t.secsTo(now);
        return c.op == Op::OlderThan ? age > c.seconds : age >= 0 && age <= c.seconds;
    }
    }
    return false;
}

QList<int> FileGroupMatcher::matchingGroups(const FileFacts &facts, const QDateTime &now) const
{
    QList<int> out;
    for (int i = 0; i < m_groups.size(); ++i) {
        const CompiledGroup &g = m_groups.at(i);
        int valid = 0;
        int hits = 0;
        for (const Compiled &c : g.conditions) {
            if (!c.valid)
                continue;  // 해석되지 않는 조건은 매칭에서 빠진다
            ++valid;
            if (matches(c, facts, now))
                ++hits;
        }
        if (valid > 0 && (g.matchAll ? hits == valid : hits > 0))
            out.append(i);
    }
    return out;
}

ResolvedGroupStyle FileGroupMatcher::resolve(const FileFacts &facts, const QDateTime &now) const
{
    ResolvedGroupStyle r;
    QList<int> hits = matchingGroups(facts, now);
    if (m_settings.merge == FileGroupSettings::Merge::FirstOnly && hits.size() > 1)
        hits = {hits.first()};
    static const QColor kDarkSurface(0x16, 0x18, 0x1B);
    for (const int i : std::as_const(hits)) {
        const FileGroup &g = m_settings.groups.at(i);
        const GroupStyle &s = g.style;
        bool contributed = false;
        if (!r.textLight && s.textLight) {
            r.textLight = s.textLight;
            contributed = true;
        }
        std::optional<QColor> textDark = s.darkFromLight && s.textLight ? std::optional(darkFromLightColor(*s.textLight, kDarkSurface))
                                                                       : s.textDark;
        if (!r.textDark && textDark) {
            r.textDark = textDark;
            contributed = true;
        }
        if (!r.backLight && s.backLight) {
            r.backLight = s.backLight;
            contributed = true;
        }
        // 배경은 밝기만 뒤집는다(글자 대비 보정은 글자색에만)
        std::optional<QColor> backDark = s.darkFromLight && s.backLight ? std::optional(invertLightness(*s.backLight)) : s.backDark;
        if (!r.backDark && backDark) {
            r.backDark = backDark;
            contributed = true;
        }
        if (s.hasEffect()) {
            r.bold |= s.bold;
            r.italic |= s.italic;
            r.underline |= s.underline;
            r.strike |= s.strike;
            contributed = true;
        }
        if (contributed)
            r.groups.append(g.name);
    }
    return r;
}

} // namespace fm::filelist
