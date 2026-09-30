#pragma once

#include "fmfilelist/FileListModel.h"

#include <QStringList>

namespace fm::filelist {

/// 캔버스 보드의 샘플 폴더 — 항목, 커서, 탭, 경로, 여유 공간 문구.
struct MockFolder
{
    QString path;              // "D:\\Downloads"
    QStringList tabs;
    int currentTab = 0;
    QList<FileEntry> entries;  // 보드의 순서 그대로(이름순 아님, 01 §11)
    int cursor = 0;
    QString volumeLabel;       // "새 볼륨"
    QString freeText;          // "여유 312 GB / 1.82 TB"
};

/// 목업 샘플 데이터(01 §11, 04 §2.3 · §2.4). 목업과 대조 · 스냅숏의 기준이다.
namespace MockFileSource {

/// Main 보드 왼쪽 패널 — D:\Work\fm-core, 20행, 커서 7(src), 선택 2개.
MockFolder left();
/// Main 보드 오른쪽 패널 — D:\Downloads, 12행, 커서 3(PDF 보고서), 선택 3개, 숨김 1개.
MockFolder right();
/// 설정 › 파일 패널 미리보기 — 5개(선택 1 · 3 · 4, 커서 2, 숨김 5).
MockFolder settingsPanelPreview();
/// 설정 › 섬네일 보기 미리보기 — 9개(선택 2개, 커서 3번째, 만드는 중 1개, 숨김 1개).
MockFolder thumbnailPreview();

} // namespace MockFileSource

} // namespace fm::filelist
