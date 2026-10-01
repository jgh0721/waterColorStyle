#pragma once

// 열 세트(설정 › 열 · 사용자 정의 열, docs/specs/05 §2.2 · §4.3) — 폴더 종류마다 열 목록을 두고, 각 열이 1줄 · 2줄(밴드)
// 표시에서 어디에 놓일지 정한다. 2줄 배치 미리보기는 이 정의를 ListColumnLayout으로 바꿔 FileListView로 그린다.

#include "fmfilelist/ListColumns.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace fm::filelist {

struct ColumnDef
{
    enum class Kind : std::uint8_t { BuiltinField, WindowsProperty, Expression };
    enum class BandPos : std::uint8_t { Row0Full, Row1, Hidden };
    enum class Empty : std::uint8_t { Dash, Blank, Fallback };
    enum class Sort : std::uint8_t { NaturalName, Date, Number, NumberTuple, Text };

    QString id;
    QString title;
    Kind kind = Kind::BuiltinField;
    QString source;                 // 필드 이름 | "System.Photo.DateTaken" | 식 원문
    int width = 100;                // -1 = 나머지(늘어남 — 이름 열만)
    Qt::Alignment align = Qt::AlignLeft;
    bool showInSingleLine = true;
    BandPos bandPos = BandPos::Row1;
    Empty empty = Empty::Dash;
    QString fallbackColumnId;       // empty == Fallback일 때
    Sort sort = Sort::Text;
    bool custom = false;            // '사용자' 태그
    QString sample;                 // 예시 값(목업 · 미리보기)
    QStringList folderValues;       // "이 폴더의 값" 샘플

    bool operator==(const ColumnDef &) const = default;
};

struct ColumnSetRule
{
    enum class Type : std::uint8_t { Default, GroupRatio, PathWildcard, KnownFolder, Manual };
    Type type = Type::Manual;
    QString groupId;                // GroupRatio — 파일 그룹 id
    int ratioPercent = 60;
    QString pathPattern;            // PathWildcard — "D:\\Work\\*"
    QString knownFolder;            // KnownFolder — "Downloads"

    bool operator==(const ColumnSetRule &) const = default;
};

struct ColumnSet
{
    QString id;
    QString name;
    bool autoApply = false;
    ColumnSetRule rule;
    QList<ColumnDef> columns;       // 순서 = 1줄 왼쪽→오른쪽 = 2줄 행 1 안의 순서

    bool isDefault() const noexcept { return rule.type == ColumnSetRule::Type::Default; }
    bool operator==(const ColumnSet &) const = default;
};

struct ColumnSettings
{
    QList<ColumnSet> sets;          // [0] = 기본(삭제 불가), 나머지 순서 = 자동 적용 우선순위

    /// 목업의 5개 세트(기본 · 사진 · 영상 · 소스 코드 · 다운로드 · 설치 패키지 검토) — 사진 · 영상은 표 샘플 7열.
    static ColumnSettings defaults();
    bool operator==(const ColumnSettings &) const = default;
};

/// "그룹 · 이미지 · 영상이 60% 이상" 같은 적용 조건 요약(세트 목록 둘째 줄).
QString columnSetRuleSummary(const ColumnSet &set, const QStringList &groupNames = {}, const QStringList &groupIds = {});
/// 태그 — "모든 폴더" · "자동" · "수동".
QString columnSetTag(const ColumnSet &set);

QString columnKindLabel(ColumnDef::Kind kind);
QString columnEmptyLabel(const ColumnDef &def, const QList<ColumnDef> &all = {});
QString columnSortLabel(ColumnDef::Sort sort);
QString columnAlignLabel(Qt::Alignment align);
QString bandPosLabel(ColumnDef::BandPos pos);

/// 2줄 배치 미리보기용 열 배치 — 모델 열 0 = 아이콘, 1 = 이름, 2.. = 행 1에 놓이는 열(순서대로). 폭은 열 정의의 폭.
/// rowColumns에 모델 열 2부터 대응하는 열 정의 인덱스를 돌려준다.
ListColumnLayout bandPreviewLayout(const ColumnSet &set, QList<int> *rowColumns = nullptr);

} // namespace fm::filelist
