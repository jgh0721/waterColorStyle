#pragma once

// UI와 분리한 규칙 — 이름 검사, 이름 변경 / 이동 해석(02 §3), 새 폴더 계획(02 §6). 단위 테스트 대상.

#include "fmdialogs/FileOpContext.h"

#include <fmwidgets/DialogWidgets.h>

#include <QString>

namespace fm::dialogs {

// ---------------------------------------------------------------- 이름 검사(Windows 규칙)

/// \ / : * ? " < > | 가운데 첫 글자(없으면 null).
QChar firstInvalidChar(const QString &name);
/// CON · PRN · AUX · NUL · COM1–9 · LPT1–9 (첫 점 앞부분, 대소문자 무시).
bool isReservedName(const QString &name);
/// 끝이 마침표나 공백.
bool hasTrailingDotOrSpace(const QString &name);

// ---------------------------------------------------------------- 이름 변경 / 이동

struct MoveRenamePlan
{
    enum Operation { Rename, Move, MoveRename, NoChange };
    enum Issue { None, Empty, InvalidChar, Reserved, TrailingDotSpace, Exists, ExtensionChange, NewFolder, MissingFolder };

    QString directory;       // 대상 폴더(끝 \ 포함)
    QString name;            // 새 이름
    Operation operation = NoChange;
    bool sameVolume = true;
    Issue issue = None;
    QString detail;          // 잘못된 문자 · 예약 이름 · ".pdf → .txt"
    bool canProceed = false; // 확인 단추를 켤지

    QString operationText() const;   // "이동" · "이름 변경" · "이동 · 이름 변경" · "변경 없음"
    QString volumeText() const;      // "같은 볼륨 · 즉시 처리" · "다른 볼륨 · 복사 후 삭제"
    QString statusText() const;      // "이 이름을 쓸 수 있습니다" 등
};

struct MoveRenameOptions
{
    bool createDirectories = true;   // 대상 폴더가 없으면 만들기
    bool allowExtensionChange = false;
};

/// 입력을 해석한다. \ 가 있으면 이동(상대 경로는 원본 폴더 기준), 없으면 같은 폴더에서 이름 변경.
/// 검사 순서: 빈 이름 → 잘못된 문자 → 예약 이름 → 끝 마침표 · 공백 → 이미 있음 → 확장자 변경 → 대상 폴더 없음.
MoveRenamePlan planMoveRename(const QString &input, const QString &sourceDir, const QString &originalName,
                              const MoveRenameOptions &options, const FileSystemProbe &probe);

// ---------------------------------------------------------------- 새 폴더

struct FolderPlan
{
    QList<fm::ui::FolderPlanView::Node> nodes;   // 현재 위치 → 조각들
    QString relativePath;                         // 정리한 상대 경로("platform\\win32\\shim")
    bool canCreate = false;
    QString help;                                 // 도움말(비면 기본 문구)
    bool helpIsError = false;                     // --danger(아니면 --warn)
};

/// \ 또는 / 로 나눈 조각마다 이미 있는지 · 새로 만드는지. 한 번 새로 만드는 조각이 나오면 그 아래는 모두 새로 만듦.
/// 앞뒤 공백 · 빈 조각은 정리하고, ".." · 절대 경로는 쓸 수 없는 이름으로 표시한다.
FolderPlan planFolders(const QString &baseDir, const QString &input, const FileSystemProbe &probe);

} // namespace fm::dialogs
