#pragma once

#include "fmfilelist/FileRoles.h"
#include "fmfilelist/ListAppearance.h"

#include <QFont>
#include <QRect>

class QPainter;
class QModelIndex;

namespace fm::style {
class ThemeColors;
}

namespace fm::filelist {

/// 섬네일 타일 그리기 — Qt 목록 구현과 Qtitan 카드 구현이 같은 그리기를 쓴다(차이는 담는 컨테이너뿐).
/// 타일(01 §1.7 · 04 §2.4): 안쪽 여백 6, 그림 상자 크기 × 크기, 간격 4, 캡션(이름 12.5 px/16 · 정보 11.5 px/15).
namespace ThumbnailPainter {

struct TileState
{
    bool active = false;
    bool cursor = false;
};

/// 타일 높이 — 6 + 크기 + 4 + (1 + 이름 줄 × 16 + [1 + 15] + 2) + 6. 96 · 이름 2줄 · 정보 있음 = 163.
/// 이름 줄 수 "전체"(0)는 3줄로 잡는다(균일 높이).
int tileHeight(const ThumbnailAppearance &appearance);

/// rect 안 가운데에 타일 폭 · 타일 높이로 그린다.
void paintTile(QPainter *painter, const QRect &rect, const QModelIndex &index, const TileState &state,
               const ThumbnailAppearance &appearance, const fm::style::ThemeColors &colors, const QFont &baseFont);

/// 목업의 가짜 섬네일(04 §2.4, 01 §9.4) — 테마와 무관한 고정 색.
void paintMockArt(QPainter *painter, const QRectF &rect, Art art);

} // namespace ThumbnailPainter

} // namespace fm::filelist
