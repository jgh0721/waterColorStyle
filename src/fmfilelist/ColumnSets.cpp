#include "fmfilelist/ColumnSets.h"

using namespace Qt::StringLiterals;

namespace fm::filelist {

namespace {

using K = ColumnDef::Kind;
using B = ColumnDef::BandPos;
using E = ColumnDef::Empty;
using S = ColumnDef::Sort;

ColumnDef def(const QString &id, const QString &title, K kind, const QString &source, int width, Qt::Alignment align,
              bool single, B pos, S sort, const QString &sample = {}, E empty = E::Dash)
{
    ColumnDef d;
    d.id = id;
    d.title = title;
    d.kind = kind;
    d.source = source;
    d.width = width;
    d.align = align;
    d.showInSingleLine = single;
    d.bandPos = pos;
    d.sort = sort;
    d.sample = sample;
    d.empty = empty;
    return d;
}

ColumnDef nameColumn(const QString &sample)
{
    return def(u"name"_s, u"이름"_s, K::BuiltinField, u"이름 (확장자 포함)"_s, -1, Qt::AlignLeft, true, B::Row0Full, S::NaturalName,
               sample);
}

QString knownFolderLabel(const QString &id)
{
    if (id == u"Downloads")
        return u"다운로드"_s;
    if (id == u"Documents")
        return u"문서"_s;
    if (id == u"Pictures")
        return u"사진"_s;
    if (id == u"Videos")
        return u"동영상"_s;
    if (id == u"Music")
        return u"음악"_s;
    if (id == u"Desktop")
        return u"바탕 화면"_s;
    return id;
}

} // namespace

ColumnSettings ColumnSettings::defaults()
{
    ColumnSettings s;

    ColumnSet standard;
    standard.id = u"default"_s;
    standard.name = u"기본"_s;
    standard.rule.type = ColumnSetRule::Type::Default;
    standard.columns = {
        nameColumn(u"release-notes.md"_s),
        def(u"ext"_s, u"확장자"_s, K::BuiltinField, u"확장자"_s, 64, Qt::AlignLeft, true, B::Hidden, S::Text, u"md"_s),
        def(u"type"_s, u"종류"_s, K::BuiltinField, u"종류"_s, 140, Qt::AlignLeft, false, B::Row1, S::Text, u"Markdown 문서"_s),
        def(u"size"_s, u"크기"_s, K::BuiltinField, u"크기"_s, 84, Qt::AlignRight, true, B::Row1, S::Number, u"12.4 KB"_s),
        def(u"modified"_s, u"수정한 날짜"_s, K::BuiltinField, u"수정한 날짜"_s, 136, Qt::AlignLeft, true, B::Row1, S::Date,
            u"2026-09-26 18:02"_s),
        def(u"attr"_s, u"속성"_s, K::BuiltinField, u"속성"_s, 52, Qt::AlignLeft, true, B::Row1, S::Text, u"A"_s),
    };

    // 사진 · 영상 — 05 §2.2.3 표 샘플 그대로
    ColumnSet photos;
    photos.id = u"photos"_s;
    photos.name = u"사진 · 영상"_s;
    photos.autoApply = true;
    photos.rule.type = ColumnSetRule::Type::GroupRatio;
    photos.rule.groupId = u"media"_s;
    photos.rule.ratioPercent = 60;
    ColumnDef taken = def(u"taken"_s, u"촬영 날짜"_s, K::WindowsProperty, u"System.Photo.DateTaken"_s, 136, Qt::AlignLeft, true,
                          B::Row1, S::Date, u"2026-09-14 10:15"_s, E::Fallback);
    taken.fallbackColumnId = u"modified"_s;
    taken.folderValues = {u"2026-09-14 10:15"_s, u"2026-09-14 15:02"_s, u"(영상 없음)"_s};
    ColumnDef size = def(u"size"_s, u"크기"_s, K::BuiltinField, u"크기"_s, 84, Qt::AlignRight, true, B::Row1, S::Number, u"4.8 MB"_s);
    size.folderValues = {u"4.8 MB"_s, u"24.6 MB"_s, u"312 MB"_s};
    ColumnDef resolution = def(u"resolution"_s, u"해상도"_s, K::Expression,
                               u"[System.Image.HorizontalSize] × [System.Image.VerticalSize]"_s, 104, Qt::AlignHCenter, true,
                               B::Row1, S::NumberTuple, u"4032 × 3024"_s, E::Blank);
    resolution.custom = true;
    resolution.folderValues = {u"4032 × 3024"_s, u"7008 × 4672"_s, u"3840 × 2160"_s, u"—"_s};
    ColumnDef duration = def(u"duration"_s, u"재생 시간"_s, K::WindowsProperty, u"System.Media.Duration"_s, 72, Qt::AlignRight, true,
                             B::Row1, S::Number, u"—"_s);
    duration.folderValues = {u"—"_s, u"—"_s, u"0:42 (MP4)"_s};
    ColumnDef camera = def(u"camera"_s, u"카메라"_s, K::WindowsProperty, u"System.Photo.CameraModel"_s, 140, Qt::AlignLeft, false,
                           B::Row1, S::Text, u"ILCE-7M4"_s, E::Blank);
    camera.folderValues = {u"ILCE-7M4"_s, u"Pixel 9"_s, u"(없음)"_s};
    ColumnDef modified = def(u"modified"_s, u"수정한 날짜"_s, K::BuiltinField, u"수정한 날짜"_s, 136, Qt::AlignLeft, false, B::Hidden,
                             S::Date, u"2026-09-14 10:15"_s);
    modified.folderValues = {u"2026-09-14 10:15"_s, u"2026-09-15 08:26"_s};
    ColumnDef name = nameColumn(u"2026-09-14_제주_001.jpg"_s);
    name.folderValues = {u"2026-09-14_제주_001.jpg"_s, u"DSC04417.arw"_s, u"…"_s};
    photos.columns = {name, taken, size, resolution, duration, camera, modified};

    ColumnSet source;
    source.id = u"source"_s;
    source.name = u"소스 코드"_s;
    source.autoApply = true;
    source.rule.type = ColumnSetRule::Type::PathWildcard;
    source.rule.pathPattern = u"D:\\Work\\*"_s;
    source.columns = {
        nameColumn(u"BandedPanelView.cpp"_s),
        def(u"size"_s, u"크기"_s, K::BuiltinField, u"크기"_s, 84, Qt::AlignRight, true, B::Row1, S::Number, u"24.1 KB"_s),
        def(u"modified"_s, u"수정한 날짜"_s, K::BuiltinField, u"수정한 날짜"_s, 136, Qt::AlignLeft, true, B::Row1, S::Date,
            u"2026-09-27 21:40"_s),
        def(u"type"_s, u"종류"_s, K::BuiltinField, u"종류"_s, 140, Qt::AlignLeft, false, B::Row1, S::Text, u"C++ 소스"_s),
    };

    ColumnSet downloads;
    downloads.id = u"downloads"_s;
    downloads.name = u"다운로드"_s;
    downloads.autoApply = true;
    downloads.rule.type = ColumnSetRule::Type::KnownFolder;
    downloads.rule.knownFolder = u"Downloads"_s;
    downloads.columns = {
        nameColumn(u"vc_redist.x64.exe"_s),
        def(u"size"_s, u"크기"_s, K::BuiltinField, u"크기"_s, 84, Qt::AlignRight, true, B::Row1, S::Number, u"24.4 MB"_s),
        def(u"modified"_s, u"수정한 날짜"_s, K::BuiltinField, u"수정한 날짜"_s, 136, Qt::AlignLeft, true, B::Row1, S::Date,
            u"2026-09-26 18:02"_s),
        def(u"origin"_s, u"받은 곳"_s, K::WindowsProperty, u"System.Link.TargetUrl"_s, 180, Qt::AlignLeft, false, B::Row1, S::Text,
            u"aka.ms/vs/17"_s, E::Blank),
    };

    ColumnSet review;
    review.id = u"review"_s;
    review.name = u"설치 패키지 검토"_s;
    review.rule.type = ColumnSetRule::Type::Manual;
    review.columns = {
        nameColumn(u"QtitanDataGrid-9.3.0-Setup.exe"_s),
        def(u"version"_s, u"버전"_s, K::WindowsProperty, u"System.Software.ProductVersion"_s, 96, Qt::AlignLeft, true, B::Row1, S::Text,
            u"9.3.0.0"_s),
        def(u"company"_s, u"게시자"_s, K::WindowsProperty, u"System.Company"_s, 160, Qt::AlignLeft, true, B::Row1, S::Text,
            u"Developer Machines"_s),
        def(u"size"_s, u"크기"_s, K::BuiltinField, u"크기"_s, 84, Qt::AlignRight, true, B::Row1, S::Number, u"186 MB"_s),
    };

    s.sets = {standard, photos, source, downloads, review};
    return s;
}

QString columnSetTag(const ColumnSet &set)
{
    if (set.isDefault())
        return u"모든 폴더"_s;
    return set.autoApply && set.rule.type != ColumnSetRule::Type::Manual ? u"자동"_s : u"수동"_s;
}

QString columnSetRuleSummary(const ColumnSet &set, const QStringList &groupNames, const QStringList &groupIds)
{
    if (set.isDefault())
        return u"다른 세트가 맞지 않을 때"_s;
    if (!set.autoApply || set.rule.type == ColumnSetRule::Type::Manual)
        return u"탭에서 직접 고를 때만"_s;
    switch (set.rule.type) {
    case ColumnSetRule::Type::GroupRatio: {
        const qsizetype i = groupIds.indexOf(set.rule.groupId);
        const QString group = i >= 0 && i < groupNames.size() ? groupNames.at(i) : set.rule.groupId;
        return u"%1 그룹이 %2% 이상"_s.arg(group).arg(set.rule.ratioPercent);
    }
    case ColumnSetRule::Type::PathWildcard:
        return u"경로가 %1 와 일치"_s.arg(set.rule.pathPattern);
    case ColumnSetRule::Type::KnownFolder:
        return u"경로가 사용자 %1 폴더"_s.arg(knownFolderLabel(set.rule.knownFolder));
    default:
        return u"탭에서 직접 고를 때만"_s;
    }
}

QString columnKindLabel(ColumnDef::Kind kind)
{
    switch (kind) {
    case K::BuiltinField:    return u"기본 필드"_s;
    case K::WindowsProperty: return u"Windows 속성"_s;
    case K::Expression:      return u"식"_s;
    }
    return {};
}

QString columnEmptyLabel(const ColumnDef &d, const QList<ColumnDef> &all)
{
    switch (d.empty) {
    case E::Dash:  return u"—"_s;
    case E::Blank: return u"비워 둠"_s;
    case E::Fallback: {
        for (const ColumnDef &other : all) {
            if (other.id == d.fallbackColumnId)
                return u"%1로 대신"_s.arg(other.title);
        }
        return u"다른 열로 대신"_s;
    }
    }
    return {};
}

QString columnSortLabel(ColumnDef::Sort sort)
{
    switch (sort) {
    case S::NaturalName: return u"이름 · 자연 정렬"_s;
    case S::Date:        return u"날짜"_s;
    case S::Number:      return u"숫자"_s;
    case S::NumberTuple: return u"숫자 · 가로 × 세로"_s;
    case S::Text:        return u"문자"_s;
    }
    return {};
}

QString columnAlignLabel(Qt::Alignment align)
{
    if (align & Qt::AlignRight)
        return u"오른쪽"_s;
    if (align & Qt::AlignHCenter)
        return u"가운데"_s;
    return u"왼쪽"_s;
}

QString bandPosLabel(ColumnDef::BandPos pos)
{
    switch (pos) {
    case B::Row0Full: return u"행 0 · 전체"_s;
    case B::Row1:     return u"행 1"_s;
    case B::Hidden:   return u"숨김"_s;
    }
    return {};
}

ListColumnLayout bandPreviewLayout(const ColumnSet &set, QList<int> *rowColumns)
{
    using R = ListColumn::Role;
    using L = ListColumn::TwoLine;
    ListColumnLayout layout;
    layout.columns.append({0, R::Icon, QString(), 16, 40, Qt::AlignCenter, false, L::Row0Full});
    QString nameTitle = u"이름"_s;
    for (const ColumnDef &d : set.columns) {
        if (d.bandPos == B::Row0Full)
            nameTitle = d.title;
    }
    layout.columns.append({1, R::Name, nameTitle, 0, 0, Qt::AlignLeft, true, L::Row0Full});
    int modelColumn = 2;
    if (rowColumns)
        rowColumns->clear();
    for (int i = 0; i < set.columns.size(); ++i) {
        const ColumnDef &d = set.columns.at(i);
        if (d.bandPos != B::Row1)
            continue;
        ListColumn c;
        c.modelColumn = modelColumn++;
        c.role = R::Meta;
        c.caption = d.title;
        c.width = d.width > 0 ? d.width : 100;
        c.align = d.align;
        c.oneLine = d.showInSingleLine;
        c.twoLine = L::Row1;
        c.tabular = d.sort == S::Number || d.sort == S::Date || d.sort == S::NumberTuple;
        layout.columns.append(c);
        if (rowColumns)
            rowColumns->append(i);
    }
    layout.columns.append({modelColumn, R::Filler, QString(), 0, 0, Qt::AlignLeft, false, L::Row1});
    return layout;
}

} // namespace fm::filelist
