#pragma once

#include "fmfilelist/FileRoles.h"

#include <QColor>
#include <QRectF>

class QPainter;

namespace fm::style {
class ThemeColors;
}

namespace fm::filelist {

/// 목록 · 섬네일의 파일 · 폴더 · 상위 폴더 아이콘(01 §1.4 경로, viewBox 16).
/// 선 색(line)은 상태에 따라 바뀌고(역상 · 워터컬러 선택은 흰색), 종류 띠 · 폴더 채움 색은 그대로다.
/// 선 굵기: 16 px 1.2 · 20 px 1.0(viewBox 단위), 44 px 이상은 목업처럼 화면 1.5 px(non-scaling-stroke).
namespace FileIconPainter {

void paint(QPainter *painter, const QRectF &rect, Kind kind, const QColor &line,
           const fm::style::ThemeColors &colors, qreal opacity = 1.0);

} // namespace FileIconPainter

} // namespace fm::filelist
