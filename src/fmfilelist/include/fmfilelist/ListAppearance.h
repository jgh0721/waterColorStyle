#pragma once

// 목록 · 섬네일의 표시 설정(설정 › 파일 패널 · 섬네일 보기의 값)과 디자인별 치수.

#include <QObject>

#include <algorithm>
#include <cstdint>

namespace fm::filelist {

Q_NAMESPACE

/// 레코드 표시 방식(보기 세그먼트 1줄 · 2줄 · 자동 · 섬네일).
enum class ViewMode : std::uint8_t { OneLine, TwoLine, Auto, Thumbnails };
Q_ENUM_NS(ViewMode)

/// 레코드 구분 방식(01 §1.9).
enum class RecordSeparator : std::uint8_t { None, Zebra, Line, Space, Tint };
Q_ENUM_NS(RecordSeparator)

/// 비활성 패널의 커서(설정 › 파일 패널 › 커서와 선택).
enum class InactiveCursor : std::uint8_t { Dashed, Hidden, SameAsActive };
Q_ENUM_NS(InactiveCursor)

/// 넘치는 이름(설정 › 파일 패널).
enum class NameElide : std::uint8_t { MiddleKeepExtension, End, Middle };
Q_ENUM_NS(NameElide)

/// 목록(1줄 · 2줄)의 표시 설정.
struct ListAppearance
{
    RecordSeparator oneLineSeparator = RecordSeparator::None;
    RecordSeparator twoLineSeparator = RecordSeparator::Line;  // Main 보드 기본(PLAN §11)
    bool nameBelow = false;                                    // 2줄: 이름을 아래 줄에
    bool invertCursor = false;                                 // 역상 커서
    bool invertSelection = false;                              // 역상 선택
    bool boldSelection = true;                                 // 선택 항목 이름 굵게(시안1)
    InactiveCursor inactiveCursor = InactiveCursor::Dashed;
    NameElide nameElide = NameElide::MiddleKeepExtension;
    int autoWidth = 640;                                       // 자동: 패널 폭이 이보다 작으면 2줄
    int autoPercent = 25;                                      // 자동: 잘린 이름 비율이 이 이상이면 2줄

    bool operator==(const ListAppearance &) const = default;
};

/// 섬네일 보기의 표시 설정.
struct ThumbnailAppearance
{
    enum class Info : std::uint8_t { None, Size, Date, SizeDate };

    int size = 96;               // 64 · 96 · 160 · 256
    int nameLines = 2;           // 1 · 2 · 0(전체)
    Info info = Info::Size;
    bool fill = false;           // 채움(가운데를 잘라 정사각형) — 끄면 맞춤
    bool badges = true;          // 종류 배지(보통 크기 이상)
    bool invertCursor = false;
    bool invertSelection = false;
    bool boldSelection = true;
    InactiveCursor inactiveCursor = InactiveCursor::Dashed;

    /// 타일 폭 — max(크기 + 28, 100): 96 → 124(Main 보드, PLAN §11).
    int tileWidth() const noexcept { return std::max(size + 28, 100); }

    bool operator==(const ThumbnailAppearance &) const = default;
};

/// 섬네일 크기 단계(Ctrl+휠).
inline constexpr int kThumbnailSizes[] = {64, 96, 160, 256};

} // namespace fm::filelist
