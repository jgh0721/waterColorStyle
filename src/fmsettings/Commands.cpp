#include "fmsettings/Commands.h"

using namespace Qt::StringLiterals;

namespace fm::settings {

namespace {

QKeySequence key(QKeyCombination combination)
{
    return QKeySequence(combination);
}

} // namespace

const QList<CommandDef> &commands()
{
    using C = CommandCategory;
    using S = CommandScope;
    static const QList<CommandDef> list = {
        {u"copy"_s, u"복사"_s, C::FileOps, S::FileList, {key(Qt::Key_F5)}},
        {u"move"_s, u"이동 · 이름 변경"_s, C::FileOps, S::FileList, {key(Qt::Key_F6)}},
        {u"rename"_s, u"그 자리에서 이름 변경"_s, C::FileOps, S::FileList, {key(Qt::Key_F2)}},
        {u"newFolder"_s, u"새 폴더"_s, C::FileOps, S::FileList, {key(Qt::Key_F7)}},
        {u"newFile"_s, u"새 파일"_s, C::FileOps, S::FileList, {key(Qt::SHIFT | Qt::Key_F4)}},
        {u"delete"_s, u"삭제"_s, C::FileOps, S::FileList, {key(Qt::Key_F8), key(Qt::Key_Delete)}},
        {u"deletePermanent"_s, u"영구 삭제"_s, C::FileOps, S::FileList, {key(Qt::SHIFT | Qt::Key_Delete)}},
        {u"multiRename"_s, u"다중 이름 변경"_s, C::FileOps, S::FileList, {key(Qt::CTRL | Qt::Key_M)}},
        {u"viewOneLine"_s, u"표시 1줄"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::SHIFT | Qt::Key_1)}},
        {u"viewTwoLine"_s, u"표시 2줄"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::SHIFT | Qt::Key_2)}},
        {u"viewAuto"_s, u"표시 자동"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::SHIFT | Qt::Key_3)}},
        {u"viewThumbnails"_s, u"표시 섬네일"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::SHIFT | Qt::Key_4)}},
        {u"columnSet"_s, u"열 세트 바꾸기"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::SHIFT | Qt::Key_C)}},
        {u"showHidden"_s, u"숨김 파일 표시"_s, C::Panel, S::Panel, {key(Qt::CTRL | Qt::Key_H)}},
        {u"switchPanel"_s, u"패널 전환"_s, C::Navigation, S::Window, {key(Qt::Key_Tab)}},
        {u"settings"_s, u"설정 열기"_s, C::Navigation, S::Window, {key(Qt::CTRL | Qt::Key_Comma)}},
        {u"find"_s, u"찾기"_s, C::Navigation, S::Window, {key(Qt::CTRL | Qt::Key_F)}},
        {u"newTab"_s, u"새 탭"_s, C::Navigation, S::Window, {key(Qt::CTRL | Qt::Key_T)}},
        {u"closeTab"_s, u"탭 닫기"_s, C::Navigation, S::Window, {key(Qt::CTRL | Qt::Key_W)}},
        {u"back"_s, u"뒤로"_s, C::Navigation, S::Window, {key(Qt::ALT | Qt::Key_Left)}},
        {u"forward"_s, u"앞으로"_s, C::Navigation, S::Window, {key(Qt::ALT | Qt::Key_Right)}},
        {u"queue"_s, u"대기열에 추가"_s, C::Dialog, S::CopyMoveDialog, {key(Qt::Key_F2)}},
    };
    return list;
}

const CommandDef *findCommand(const QString &id)
{
    for (const CommandDef &c : commands()) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

QStringList keyLayouts()
{
    return {u"default"_s, u"totalcmd"_s, u"explorer"_s};
}

QString keyLayoutLabel(const QString &layout)
{
    if (layout == u"totalcmd")
        return u"Total Commander 호환"_s;
    if (layout == u"explorer")
        return u"Windows 탐색기 호환"_s;
    return u"기본 (F키 중심)"_s;
}

QList<QKeySequence> layoutKeys(const QString &commandId, const QString &layout)
{
    const CommandDef *c = findCommand(commandId);
    if (!c)
        return {};
    if (layout == u"totalcmd") {
        // Total Commander: 이름 변경 Shift+F6, 다중 이름 변경 Ctrl+M, 삭제 F8 · Del
        if (commandId == u"rename")
            return {key(Qt::SHIFT | Qt::Key_F6)};
    } else if (layout == u"explorer") {
        // Windows 탐색기: 새 폴더 Ctrl+Shift+N, 삭제 Del, 찾기 Ctrl+E · F3, 설정 없음
        if (commandId == u"newFolder")
            return {key(Qt::CTRL | Qt::SHIFT | Qt::Key_N)};
        if (commandId == u"delete")
            return {key(Qt::Key_Delete)};
        if (commandId == u"find")
            return {key(Qt::CTRL | Qt::Key_E), key(Qt::Key_F3)};
        if (commandId == u"copy" || commandId == u"move")
            return {};
    }
    return c->defaultKeys;
}

QList<QKeySequence> effectiveKeys(const KeyBindingSettings &keys, const QString &commandId)
{
    const auto it = keys.overrides.constFind(commandId);
    if (it != keys.overrides.cend())
        return it.value();
    return layoutKeys(commandId, keys.layout);
}

bool scopesOverlap(CommandScope a, CommandScope b)
{
    const bool dialogA = a == CommandScope::CopyMoveDialog;
    const bool dialogB = b == CommandScope::CopyMoveDialog;
    if (dialogA || dialogB)
        return a == b;
    return true;  // 창 ⊃ 패널 ⊃ 파일 목록 — 모두 조상–자손 관계
}

std::optional<QString> findConflict(const KeyBindingSettings &keys, const QString &commandId, const QKeySequence &key)
{
    const CommandDef *self = findCommand(commandId);
    if (!self || key.isEmpty())
        return std::nullopt;
    for (const CommandDef &other : commands()) {
        if (other.id == commandId || !scopesOverlap(self->scope, other.scope))
            continue;
        if (effectiveKeys(keys, other.id).contains(key))
            return other.id;
    }
    return std::nullopt;
}

bool isForbiddenKey(const QKeySequence &sequence)
{
    if (sequence.isEmpty())
        return true;
    const QKeyCombination c = sequence[0];
    const Qt::Key k = c.key();
    const Qt::KeyboardModifiers m = c.keyboardModifiers();
    if (k == Qt::Key_Control || k == Qt::Key_Shift || k == Qt::Key_Alt || k == Qt::Key_Meta || k == Qt::Key_unknown)
        return true;
    if (m & Qt::MetaModifier)
        return true;
    if (k == Qt::Key_Print)
        return true;
    if (m == Qt::AltModifier && (k == Qt::Key_F4 || k == Qt::Key_Tab))
        return true;
    if (m == Qt::NoModifier && k == Qt::Key_F10)
        return true;
    if ((m & (Qt::ControlModifier | Qt::AltModifier)) == (Qt::ControlModifier | Qt::AltModifier) && k == Qt::Key_Delete)
        return true;
    return false;
}

QString categoryLabel(CommandCategory category)
{
    switch (category) {
    case CommandCategory::FileOps:    return u"파일 작업"_s;
    case CommandCategory::Panel:      return u"패널"_s;
    case CommandCategory::Navigation: return u"탐색"_s;
    case CommandCategory::Dialog:     return u"대화상자"_s;
    }
    return {};
}

QString scopeLabel(CommandScope scope)
{
    switch (scope) {
    case CommandScope::Window:         return u"창"_s;
    case CommandScope::Panel:          return u"패널"_s;
    case CommandScope::FileList:       return u"파일 목록"_s;
    case CommandScope::CopyMoveDialog: return u"복사 · 이동 대화상자"_s;
    }
    return {};
}

QString keysText(const QList<QKeySequence> &keys)
{
    QStringList parts;
    for (const QKeySequence &k : keys)
        parts.append(k.toString(QKeySequence::NativeText));
    return parts.join(u" · "_s);
}

} // namespace fm::settings
