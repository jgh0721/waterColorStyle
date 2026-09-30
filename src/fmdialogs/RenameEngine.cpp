#include "fmdialogs/RenameEngine.h"

#include "fmdialogs/Planners.h"

#include <QHash>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

void split(const QString &name, QString *stem, QString *ext)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0) {
        *stem = name;
        ext->clear();
        return;
    }
    *stem = name.left(dot);
    *ext = name.mid(dot + 1);
}

QString applyCase(const QString &text, NameCase mode)
{
    switch (mode) {
    case NameCase::Keep:  return text;
    case NameCase::Lower: return text.toLower();
    case NameCase::Upper: return text.toUpper();
    case NameCase::FirstUpper:
        return text.isEmpty() ? text : text.left(1).toUpper() + text.mid(1).toLower();
    case NameCase::TitleCase: {
        QString out = text.toLower();
        bool start = true;
        for (QChar &c : out) {
            if (start && c.isLetter())
                c = c.toUpper();
            start = !c.isLetterOrNumber();
        }
        return out;
    }
    }
    return text;
}

QString counter(int value, int digits)
{
    const QString number = QString::number(std::abs(value)).rightJustified(std::max(1, digits), u'0');
    return value < 0 ? u'-' + number : number;
}

} // namespace

QString RenamePreviewRow::stateText() const
{
    switch (state) {
    case Ok:        return QObject::tr("정상");
    case Same:      return QObject::tr("변경 없음");
    case Invalid:   return QObject::tr("잘못된 이름");
    case Exists:    return QObject::tr("충돌 · 이미 있음");
    case Duplicate: return QObject::tr("충돌 · 중복");
    }
    return QString();
}

QString RenameSummary::text() const
{
    return QObject::tr("%1개 중 %2개 변경 · 충돌/오류 %3 · 변경 없음 %4").arg(total).arg(changed).arg(bad).arg(same);
}

QString applyRenameRules(const RenameFile &file, int index, const RenameRules &rules, const QString &folderName)
{
    QString stem, ext;
    split(file.name, &stem, &ext);
    const QDateTime date = (rules.dateSource == DateSource::Captured && file.captured.isValid()) ? file.captured : file.modified;
    auto expand = [&](QString mask) {
        mask.replace(u"[N]"_s, stem);
        mask.replace(u"[E]"_s, ext);
        mask.replace(u"[C]"_s, counter(rules.counterStart + index * rules.counterStep, rules.counterDigits));
        mask.replace(u"[P]"_s, folderName);
        mask.replace(u"[Y]"_s, date.isValid() ? date.toString(u"yyyy"_s) : QString());
        mask.replace(u"[M]"_s, date.isValid() ? date.toString(u"MM"_s) : QString());
        mask.replace(u"[D]"_s, date.isValid() ? date.toString(u"dd"_s) : QString());
        mask.replace(u"[t]"_s, date.isValid() ? date.toString(u"HHmmss"_s) : QString());
        return mask;
    };
    // 1. 이름 마스크
    QString newStem = expand(rules.mask);
    // 2. 찾기 · 바꾸기
    if (!rules.find.isEmpty()) {
        if (rules.regex) {
            QRegularExpression re(rules.find, rules.caseSensitive ? QRegularExpression::NoPatternOption
                                                                  : QRegularExpression::CaseInsensitiveOption);
            if (re.isValid()) {
                QString replacement = rules.replace;
                // $1 → \1. QString::replace()의 after는 "\숫자"만 캡처로 바꾸고 다른 \는 그대로 두므로
                // after = "\\1"(글자 \ + 캡처 1)이면 "$1"이 "\1"이 된다.
                replacement.replace(QRegularExpression(u"\\$(\\d)"_s), u"\\\\1"_s);
                newStem.replace(re, replacement);
            }
        } else {
            newStem.replace(rules.find, rules.replace, rules.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        }
    }
    // 3. 이름 대소문자
    newStem = applyCase(newStem, rules.nameCase);
    // 4. 확장자 마스크 · 대소문자
    QString newExt = expand(rules.extMask);
    switch (rules.extCase) {
    case ExtCase::Keep: break;
    case ExtCase::Lower: newExt = newExt.toLower(); break;
    case ExtCase::Upper: newExt = newExt.toUpper(); break;
    }
    return newExt.isEmpty() ? newStem : newStem + u'.' + newExt;
}

QList<RenamePreviewRow> previewRename(const QList<RenameFile> &files, const RenameRules &rules, const QString &folderName,
                                      const QSet<QString> &existingOthers, Qt::CaseSensitivity comparison)
{
    auto key = [comparison](const QString &name) { return comparison == Qt::CaseInsensitive ? name.toLower() : name; };
    QList<RenamePreviewRow> rows;
    rows.reserve(files.size());
    for (int i = 0; i < files.size(); ++i) {
        const RenameFile &f = files.at(i);
        RenamePreviewRow row;
        row.index = i + 1;
        row.original = f.name;
        row.newName = applyRenameRules(f, i, rules, folderName);
        row.size = f.size;
        row.date = (rules.dateSource == DateSource::Captured && f.captured.isValid()) ? f.captured : f.modified;
        row.isLong = row.original.size() > 40 || row.newName.size() > 40;
        rows.append(row);
    }
    // 차지된 이름: 목록 밖 파일 + 이번에 이름이 그대로인 파일
    QSet<QString> occupied;
    for (const QString &name : existingOthers)
        occupied.insert(key(name));
    for (const RenamePreviewRow &row : std::as_const(rows)) {
        if (row.newName == row.original)
            occupied.insert(key(row.original));
    }
    QHash<QString, int> counts;
    for (const RenamePreviewRow &row : std::as_const(rows))
        ++counts[key(row.newName)];

    for (RenamePreviewRow &row : rows) {
        // 판정 순서(목업): 잘못된 이름 → 변경 없음 → 충돌 · 이미 있음 → 충돌 · 중복 → 정상
        if (row.newName.isEmpty() || !firstInvalidChar(row.newName).isNull() || row.newName.startsWith(u'.')
            || isReservedName(row.newName) || hasTrailingDotOrSpace(row.newName)) {
            row.state = RenamePreviewRow::Invalid;
        } else if (row.newName == row.original) {
            row.state = RenamePreviewRow::Same;
        } else if (occupied.contains(key(row.newName)) && key(row.newName) != key(row.original)) {
            row.state = RenamePreviewRow::Exists;
        } else if (counts.value(key(row.newName)) > 1) {
            row.state = RenamePreviewRow::Duplicate;
        } else {
            row.state = RenamePreviewRow::Ok;
        }
    }
    return rows;
}

RenameSummary summarizeRename(const QList<RenamePreviewRow> &rows)
{
    RenameSummary s;
    s.total = int(rows.size());
    for (const RenamePreviewRow &row : rows) {
        if (row.isLong)
            ++s.longCount;
        if (row.state == RenamePreviewRow::Ok)
            ++s.changed;
        else if (row.state == RenamePreviewRow::Same)
            ++s.same;
        else
            ++s.bad;
    }
    return s;
}

namespace RenameSample {

QList<RenameFile> files()
{
    constexpr qint64 MB = 1024 * 1024;
    auto f = [](const QString &name, double mb, const QString &date) {
        RenameFile file;
        file.name = name;
        file.size = qint64(mb * MB);
        file.captured = QDateTime::fromString(date, u"yyyy-MM-dd HH:mm:ss"_s);
        file.modified = file.captured.addDays(2);
        return file;
    };
    return {
        f(u"제주 여행 2026-09-14 성산일출봉 일출 타임랩스 원본 (4K 60fps HDR · 편집 전).MP4"_s, 312, u"2026-09-14 06:12:40"_s),
        f(u"IMG_20260914_101522.JPG"_s, 4.8, u"2026-09-14 10:15:22"_s),
        f(u"KakaoTalk_20260914_103047215_01_애월 해안도로 카페 창가에서 본 바다.jpg"_s, 1.9, u"2026-09-14 10:30:47"_s),
        f(u"IMG_20260914_104410.JPG"_s, 4.6, u"2026-09-14 10:44:10"_s),
        f(u"DSC04417.ARW"_s, 24.6, u"2026-09-14 15:02:41"_s),
        f(u"DSC04418.ARW"_s, 24.3, u"2026-09-14 15:02:44"_s),
        f(u"IMG_20260914_171233.JPG"_s, 5.3, u"2026-09-14 17:12:33"_s),
        f(u"Screenshot_20260915-082610_지도 앱_제주공항에서 렌터카 하우스 가는 길 (도보 안내).png"_s, 1.2, u"2026-09-15 08:26:10"_s),
        f(u"IMG_20260915_090158.JPG"_s, 5.0, u"2026-09-15 09:01:58"_s),
        f(u"PXL_20260915_112034567.MP4"_s, 188, u"2026-09-15 11:20:34"_s),
        f(u"2026-09-15_제주_011.jpg"_s, 4.7, u"2026-09-15 13:45:02"_s),
        f(u"한라산 영실코스 단체사진_보정본_최종_진짜최종 (인화용 300dpi).jpg"_s, 8.4, u"2026-09-15 14:08:22"_s),
    };
}

QString folder()
{
    return u"D:\\Photos\\2026-09 제주"_s;
}

QString folderName()
{
    return u"2026-09 제주"_s;
}

QSet<QString> existing()
{
    return {u"2026-09-14_제주_007.jpg"_s};
}

RenameRules boardRules()
{
    RenameRules r;
    r.mask = u"[Y]-[M]-[D]_제주_[C]"_s;
    r.extCase = ExtCase::Lower;
    return r;
}

} // namespace RenameSample

} // namespace fm::dialogs
