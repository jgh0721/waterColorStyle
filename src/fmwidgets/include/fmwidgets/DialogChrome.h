#pragma once

#include "fmwidgets/Glyph.h"

class QDialog;

namespace fm::ui {

struct DialogChromeOptions
{
    glyph::Glyph icon = glyph::App;  // 창 아이콘 — 관리자 권한 창은 Shield
    bool minimizeButton = false;     // 진행 창
    bool maximizeButton = false;     // 다중 이름 변경
    bool fixedSize = true;           // 목업 클라이언트 크기로 고정(다중 이름 변경은 false)
};

/// .ui로 만든 대화상자에 공통 창 설정을 건다 — 창 플래그(도움말 단추 없음), 창 아이콘(테마가 바뀌면
/// 다시 만든다), 대화상자 밀도(fmDensity = dialog), 고정 크기. 별도 기반 클래스 없이 uic의 Ui:: 클래스와 함께 쓴다.
void setupDialogChrome(QDialog *dialog, const DialogChromeOptions &options = {});

} // namespace fm::ui
