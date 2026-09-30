#pragma once

#include <QString>

class QAbstractItemModel;

namespace fm::filelist {

/// 패널 상태 줄의 수치 — "파일 2 / 11개 선택 · 9.9 KB / 41.1 KB" · "폴더 8개" / "숨김 1개 표시 중".
struct FileListStats
{
    int files = 0;
    int markedFiles = 0;
    qint64 bytes = 0;
    qint64 markedBytes = 0;
    int folders = 0;           // ".." 제외
    int markedFolders = 0;
    int hiddenFiles = 0;       // 보이는 숨김 · 시스템 파일(폴더 제외)

    static FileListStats compute(const QAbstractItemModel *model);

    /// 왼쪽 문구 "파일 {선택} / {전체}개 선택 · {선택 크기} / {전체 크기}".
    QString selectionText() const;
    /// 오른쪽 문구 — 보이는 숨김 파일이 있으면 "숨김 N개 표시 중", 아니면 "폴더 N개"(01 §14-25의 기본값).
    QString secondaryText() const;
};

} // namespace fm::filelist
