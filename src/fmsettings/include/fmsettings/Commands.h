#pragma once

// 명령 목록(설정 › 키보드, docs/specs/05 §2.5.4) — 명령 id · 이름 · 범주 · 범위 · 배열별 기본 키.
// id는 메인 창 QAction의 objectName과 같다. 충돌 판정은 범위를 본다(창 ⊃ 패널 ⊃ 파일 목록, 대화상자는 독립).

#include "fmsettings/AppSettings.h"

#include <QKeySequence>
#include <QList>
#include <QString>

#include <cstdint>
#include <optional>

namespace fm::settings {

enum class CommandCategory : std::uint8_t { FileOps, Panel, Navigation, Dialog };
enum class CommandScope : std::uint8_t { Window, Panel, FileList, CopyMoveDialog };

struct CommandDef
{
    QString id;
    QString name;
    CommandCategory category;
    CommandScope scope;
    QList<QKeySequence> defaultKeys;  // "default" 배열
};

const QList<CommandDef> &commands();
const CommandDef *findCommand(const QString &id);

/// 배열 — default(F키 중심) · totalcmd(Total Commander 호환) · explorer(Windows 탐색기 호환).
QStringList keyLayouts();
QString keyLayoutLabel(const QString &layout);
QList<QKeySequence> layoutKeys(const QString &commandId, const QString &layout);
/// 설정을 반영한 지금 키(배열 기본값 + 사용자 지정).
QList<QKeySequence> effectiveKeys(const KeyBindingSettings &keys, const QString &commandId);
/// 두 범위가 동시에 활성일 수 있는가(조상–자손 관계). 대화상자 범위는 서로 · 창과 독립.
bool scopesOverlap(CommandScope a, CommandScope b);
/// key를 이미 쓰는 다른 명령(범위가 겹치는 것만).
std::optional<QString> findConflict(const KeyBindingSettings &keys, const QString &commandId, const QKeySequence &key);
/// 쓸 수 없는 키(수식키 단독, Win 조합, Alt+F4, Alt+Tab, F10 단독, PrintScreen).
bool isForbiddenKey(const QKeySequence &key);

QString categoryLabel(CommandCategory category);
QString scopeLabel(CommandScope scope);
/// "F7 · Shift+F4" — 칩 표시 문자열.
QString keysText(const QList<QKeySequence> &keys);

} // namespace fm::settings
