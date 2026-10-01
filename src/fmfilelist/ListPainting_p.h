#pragma once

// 목록 그리기 공통 규칙 — 레코드 치수(디자인 · 1줄/2줄 · 구분 방식), 상태별 글자색, 레코드 바탕 · 커서.
// FileListView(Qtitan 레코드 그리기 · 셀 델리게이트)와 섬네일 그리기가 함께 쓴다.

#include "fmfilelist/ListAppearance.h"

#include <fmstyle/ThemeColors.h>

#include <QColor>
#include <QRectF>

class QPainter;
class QWidget;

namespace fm::filelist::detail {

/// 위젯 또는 조상의 fmPaneActive.
bool paneActive(const QWidget *widget);

/// 1줄 보기의 고정 열 폭(확장자 64 · 크기 84 · 수정한 날짜 128 · 속성 52 + 오른쪽 행 여백 8).
inline constexpr int kExtWidth = 64;
inline constexpr int kSizeWidth = 84;
inline constexpr int kDateWidth1 = 128;
inline constexpr int kAttrWidth1 = 52 + 8;
inline constexpr int kRowGutter = 8;     // 1줄 좌우 행 여백
inline constexpr int kCellPad = 8;       // 칸 좌우 여백
/// 2줄 보기 메타 줄 열 폭(종류 140 · 크기 84 · 수정한 날짜 136 · 속성 52), 아이콘 밴드 40.
inline constexpr int kIconBand = 40;
inline constexpr int kTypeWidth = 140;
inline constexpr int kDateWidth2 = 136;
inline constexpr int kAttrWidth2 = 52;

/// 레코드 치수.
struct RecordGeometry
{
    bool twoLine = false;
    bool watercolor = false;
    bool nameBelow = false;
    RecordSeparator separator = RecordSeparator::None;
    int pitch = 24;           // 레코드 전체 높이(여백 방식의 아래 틈 포함) — Qtitan 셀 높이 × 줄 수
    int gap = 0;              // 여백 방식: 레코드 아래 틈 2
    int sideMargin = 0;       // 여백 방식: 좌우 바깥 여백 6
    qreal padTop = 0;         // 2줄: 블록 안 위 여백
    int nameHeight = 22;      // 2줄 이름 줄
    int metaHeight = 18;      // 2줄 메타 줄
    int headerHeight = 27;    // 머리글(아래 선 포함)
    int oneLineMetaWidth = kExtWidth + kSizeWidth + kDateWidth1 + kAttrWidth1;  // 1줄 메타 열 폭 합(틴트 띠 — 열 배치가 정함)
    qreal radius = 0;         // 여백 방식 블록 모서리

    static RecordGeometry make(bool watercolor, bool twoLine, const ListAppearance &appearance);

    /// 바탕 블록(여백 방식이면 좌우 · 아래를 줄인 사각형).
    QRectF block(const QRect &record) const;
    /// 2줄: 이름 줄 · 메타 줄의 논리 사각형(목업 좌표). 1줄은 블록 전체.
    QRectF nameLine(const QRect &record) const;
    QRectF metaLine(const QRect &record) const;
    int linesPerRecord() const noexcept { return twoLine ? 2 : 1; }
    int cellHeight() const noexcept { return pitch / linesPerRecord(); }
};

/// 레코드 상태.
struct RecordState
{
    bool active = false;   // 활성 패널
    bool marked = false;   // 표시(선택)
    bool cursor = false;   // 커서
    bool hidden = false;   // 숨김 · 시스템
};

/// 상태별 글자 · 아이콘 선 색.
struct TextColors
{
    QColor name;
    QColor ext;
    QColor meta;
    QColor iconLine;
    bool bold = false;
};

bool invertedCursor(const ListAppearance &a, const RecordState &s);
TextColors textColors(const fm::style::ThemeColors &tc, const ListAppearance &a, const RecordState &s,
                      bool twoLine, RecordSeparator separator);

/// 레코드 바탕(교차 배경 · 선택 · 역상 · 틴트 띠 · 구분선). alternate = 교차 배경 대상 레코드.
void paintRecordBackground(QPainter *p, const QRect &record, const RecordGeometry &g, const ListAppearance &a,
                           const RecordState &s, bool alternate, const fm::style::ThemeColors &tc,
                           const QColor &groupBackground = QColor());
/// 커서 틀(레코드 둘레 하나).
void paintRecordCursor(QPainter *p, const QRect &record, const RecordGeometry &g, const ListAppearance &a,
                       const RecordState &s, const fm::style::ThemeColors &tc);

/// 커서 틀 펜 — 시안1 실선 / 비활성 점선(dashed), 시안2 점선(dotted). 그리지 않으면 NoPen.
QPen cursorPen(const fm::style::ThemeColors &tc, InactiveCursor inactiveCursor, const RecordState &s);

} // namespace fm::filelist::detail
