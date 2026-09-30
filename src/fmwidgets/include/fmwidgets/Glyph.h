#pragma once

// 위젯 속성(Qt Designer에서 고르는 값)으로 쓰는 아이콘 이름. 값은 fm::style::Glyph와 같고,
// 그리기는 fmstyle(Glyphs.h)이 맡는다. .ui에는 fm::ui::glyph::Copy 처럼 저장된다.

#include <fmstyle/Glyphs.h>

#include <QObject>

namespace fm::ui::glyph {
Q_NAMESPACE

// Qt Designer · uic 호환을 위해 범위 없는 enum (이름 충돌을 막으려고 별도 네임스페이스에 둔다).
enum Glyph {
    None, App, Shield, Copy, Rename, Trash, NewFile, NewFolder, Pause, Play, Clock, More, ArrowRight,
    Check, Info, Warning, File, Folder, FolderOutline, PlusSmall, ChevronDown, ChevronUp, Undo, ReturnArrow,
    Close, Back, Forward, ArrowUp, Refresh, Move, MultiRename, Search, Drive, ChevronRight, ViewOneLine,
    ViewTwoLine, ViewThumbnails, Lock, LockKeyhole
};
Q_ENUM_NS(Glyph)

static_assert(int(LockKeyhole) + 1 == int(fm::style::Glyph::Count), "fm::ui::glyph::Glyph must mirror fm::style::Glyph");

inline fm::style::Glyph toStyle(Glyph g) { return static_cast<fm::style::Glyph>(g); }

} // namespace fm::ui::glyph
