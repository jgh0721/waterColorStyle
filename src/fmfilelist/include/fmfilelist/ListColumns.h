#pragma once

// 목록 열 배치 — 1줄에서 보일 열과 폭, 2줄(밴드)에서 놓일 줄(BandSpec: 아이콘 = 두 줄 걸침 40, 이름 = 행 0 전체,
// 나머지 = 행 1). 기본 배치는 파일 목록 열(FileRoles)이고, 설정의 미리보기(그룹 · 열 세트)는 다른 모델과 배치를 쓴다.

#include <QList>
#include <QString>

#include <cstdint>

namespace fm::filelist {

struct ListColumn
{
    // Extension: 1줄 확장자 열(있으면 이름 칸에 확장자를 붙이지 않는다). 그리기는 Meta와 같다.
    enum class Role : std::uint8_t { Icon, Name, Meta, Extension, Filler };
    enum class TwoLine : std::uint8_t { Hidden, Row1, Row0Full };

    int modelColumn = 0;
    Role role = Role::Meta;
    QString caption;             // 비면 모델 머리글
    int width = 100;             // 1줄 폭(이름은 0 = 늘어남). 1줄 마지막 열에는 행 여백 8이 더해진다
    int twoLineWidth = -1;       // 2줄 메타 줄 폭(-1 = width)
    Qt::Alignment align = Qt::AlignLeft;
    bool oneLine = true;
    TwoLine twoLine = TwoLine::Row1;
    bool mono = false;           // 고정폭 12 px(속성)
    bool tabular = false;        // 숫자 폭 고정(크기 · 날짜)

    int widthFor(bool twoLineLayout) const noexcept { return twoLineLayout && twoLineWidth >= 0 ? twoLineWidth : width; }
    bool operator==(const ListColumn &) const = default;
};

struct ListColumnLayout
{
    QList<ListColumn> columns;

    /// 파일 목록 기본 배치(01 §1.6 · BandSpec): 아이콘 · 이름 · 확장자(1줄) · 종류(2줄) · 크기 · 수정한 날짜 · 속성 · 채움(2줄).
    static ListColumnLayout standard();

    const ListColumn *find(int modelColumn) const;
    /// 이름 열의 모델 열(없으면 0).
    int nameColumn() const;
    /// 모델에 있어야 하는 열 수.
    int requiredColumns() const;

    bool operator==(const ListColumnLayout &) const = default;
};

} // namespace fm::filelist
