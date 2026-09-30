#pragma once

// 목업의 SVG 아이콘 — 경로는 캔버스 보드의 <svg>를 그대로 옮겼다(viewBox 16, 일부 10 · 12).
// 색은 그릴 때 정한다(토큰 색). 두 색 아이콘(방패 · 파일 띠)은 두 번째 색을 받는다.
// 위젯의 Designer 속성으로는 fm::ui::glyph::Glyph(fmwidgets/Glyph.h, 값이 같다)를 쓴다.

#include <QColor>
#include <QIcon>
#include <QRectF>

#include <cstdint>

class QPainter;

namespace fm::style {

class ThemeColors;

enum class Glyph : std::uint8_t {
    None,
    App,            // 창 아이콘 — 둥근 사각형 두 개 (강조색, 오른쪽 50 %)
    Shield,         // 관리자 권한 — 두 색(--shield / --shield-2)
    Copy,
    Rename,
    Trash,
    NewFile,
    NewFolder,
    Pause,
    Play,
    Clock,
    More,
    ArrowRight,
    Check,
    Info,
    Warning,
    File,           // 파일 — 선 + 종류 색 띠(두 번째 색)
    Folder,         // 채운 폴더 (--folder)
    FolderOutline,
    PlusSmall,
    ChevronDown,
    ChevronUp,
    Undo,
    ReturnArrow,
    Close,

    Count
};

/// 아이콘을 rect에 그린다. secondary가 유효하지 않으면 아이콘 기본 규칙(예: App의 50 % 투명)을 쓴다.
void paintGlyph(QPainter *painter, Glyph glyph, const QRectF &rect, const QColor &primary,
                const QColor &secondary = QColor());

/// 한 색(또는 두 색) 아이콘. px는 논리 크기, 고해상도 화면용으로 2배 픽스맵도 넣는다.
QIcon glyphIcon(Glyph glyph, const QColor &primary, int px = 16, const QColor &secondary = QColor());

/// 방패 아이콘(--shield / --shield-2). QStyle::SP_VistaShield로도 얻는다.
QIcon shieldIcon(const ThemeColors &colors, int px = 16);

/// viewBox 크기 — 대부분 16, PlusSmall · ChevronUp 12, ChevronDown 10.
int glyphViewBox(Glyph glyph) noexcept;

} // namespace fm::style
