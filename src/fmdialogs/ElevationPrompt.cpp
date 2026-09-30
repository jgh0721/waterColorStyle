#include "fmdialogs/ElevationPrompt.h"

#include "fmdialogs/FileOpContext.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace fm::dialogs::elev {

using fm::style::Token;
using fm::ui::KeyValueCard;
namespace glyph = fm::ui::glyph;

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("fm::dialogs::elev", text);
}

/// 창 제목 접두사(03 §3.4 표).
QString titlePrefix(Operation op)
{
    switch (op) {
    case Operation::Copy:         return tr("복사");
    case Operation::Move:         return tr("이동");
    case Operation::Delete:       return tr("삭제");
    case Operation::Rename:       return tr("이름 변경");
    case Operation::CreateFolder: return tr("새 폴더");
    case Operation::CreateFile:   return tr("새 파일");
    }
    return QString();
}

/// 사전 확인 · 소유권 단추의 동사.
QString verb(Operation op)
{
    switch (op) {
    case Operation::Copy:   return tr("복사");
    case Operation::Move:   return tr("이동");
    case Operation::Delete: return tr("삭제");
    case Operation::Rename: return tr("이름 변경");
    case Operation::CreateFolder:
    case Operation::CreateFile: return tr("만들기");
    }
    return QString();
}

/// 실패 창 설명의 끝 문장.
QString notDoneYet(Operation op)
{
    switch (op) {
    case Operation::Copy:   return tr("이 항목은 아직 복사되지 않았습니다.");
    case Operation::Move:   return tr("이 항목은 아직 이동되지 않았습니다.");
    case Operation::Delete: return tr("이 항목은 아직 삭제되지 않았습니다.");
    case Operation::Rename: return tr("이름이 아직 바뀌지 않았습니다.");
    case Operation::CreateFolder:
    case Operation::CreateFile: return tr("아직 만들어지지 않았습니다.");
    }
    return QString();
}

QString failedStatus(Operation op)
{
    switch (op) {
    case Operation::Copy:   return tr("대기 중 · 복사 안 됨");
    case Operation::Move:   return tr("대기 중 · 이동 안 됨");
    case Operation::Delete: return tr("대기 중 · 삭제 안 됨");
    case Operation::Rename: return tr("대기 중 · 이름 바뀌지 않음");
    case Operation::CreateFolder:
    case Operation::CreateFile: return tr("대기 중 · 만들어지지 않음");
    }
    return QString();
}

QString withSlash(QString dir)
{
    if (!dir.isEmpty() && !dir.endsWith(u'\\'))
        dir += u'\\';
    return dir;
}

KeyValueCard::Row itemRow(const QString &label, const ItemInfo &item, Qt::TextElideMode elide = Qt::ElideRight)
{
    KeyValueCard::Row row;
    row.label = label;
    row.value = item.name;
    row.icon = item.isDir ? fm::ui::IconSpec::folder() : fm::ui::IconSpec::file(item.kindTint);
    if (item.size >= 0 && !item.isDir)
        row.trailing = formatBytes(item.size);
    row.elide = elide;
    return row;
}

KeyValueCard::Row pathRow(const QString &label, const QString &path)
{
    KeyValueCard::Row row;
    row.label = label;
    row.value = withSlash(path);
    row.style = KeyValueCard::Mono;
    row.elide = Qt::ElideMiddle;
    return row;
}

KeyValueCard::Row textRow(const QString &label, const QString &value, KeyValueCard::ValueStyle style = KeyValueCard::Normal)
{
    KeyValueCard::Row row;
    row.label = label;
    row.value = value;
    row.style = style;
    return row;
}

ButtonSpec button(Choice choice, const QString &text, bool primary = false, bool shield = false)
{
    ButtonSpec b;
    b.choice = choice;
    b.text = text;
    b.primary = primary;
    b.isDefault = primary;
    b.shield = shield;
    return b;
}

ButtonSpec cancelButton(const QString &text = QString())
{
    ButtonSpec b = button(Choice::Cancel, text.isEmpty() ? tr("취소") : text);
    b.toolTip = u"Esc"_s;
    return b;
}

std::optional<CheckSpec> applyCheck(int remaining, bool checked)
{
    // 남은 항목이 없으면 체크 상자를 두지 않는다(03 §1.9)
    if (remaining <= 0)
        return std::nullopt;
    return CheckSpec{tr("남은 %1개 항목에도 같은 선택 적용(&A)").arg(remaining), checked};
}

} // namespace

QString choiceName(Choice choice)
{
    switch (choice) {
    case Choice::Elevate:             return u"Elevate"_s;
    case Choice::SkipNeedingAdmin:    return u"SkipNeedingAdmin"_s;
    case Choice::Skip:                return u"Skip"_s;
    case Choice::CopyOnly:            return u"CopyOnly"_s;
    case Choice::Retry:               return u"Retry"_s;
    case Choice::TakeOwnership:       return u"TakeOwnership"_s;
    case Choice::UseUserFolder:       return u"UseUserFolder"_s;
    case Choice::ChooseOtherLocation: return u"ChooseOtherLocation"_s;
    case Choice::EditName:            return u"EditName"_s;
    case Choice::Cancel:              return u"Cancel"_s;
    }
    return QString();
}

namespace prompts {

PromptSpec preflight(Operation op, int total, const QList<fm::ui::ItemListCard::Item> &needAdmin, const QString &reason)
{
    const int need = int(needAdmin.size());
    const int rest = std::max(0, total - need);
    PromptSpec s;
    s.windowTitle = tr("%1 — 권한 확인").arg(titlePrefix(op));
    s.heading = tr("%1개 항목 중 %2개는 관리자 권한이 필요합니다").arg(total).arg(need);
    s.description = tr("시작하기 전에 권한을 확인했습니다.");
    if (!reason.isEmpty())
        s.description += u' ' + reason;
    s.items = needAdmin;
    s.options = {
        {Choice::Elevate, tr("모두 %1(&A) — 관리자 권한을 한 번만 요청").arg(verb(op)), QString(), tr("계속"), true},
        {Choice::SkipNeedingAdmin, tr("권한이 필요한 %1개는 건너뛰고 %2개만 %3(&K)").arg(need).arg(rest).arg(verb(op)),
         QString(), tr("%1개 %2").arg(rest).arg(verb(op)), false},
    };
    ButtonSpec primary = button(Choice::Elevate, QString(), true);
    primary.followsOption = true;
    s.buttons = {primary, cancelButton()};
    s.designSize = {560, 400};
    return s;
}

PromptSpec copyDenied(const ItemInfo &item, const QString &targetDir, int remaining, bool applyDefault)
{
    PromptSpec s;
    s.windowTitle = tr("복사 — 관리자 권한 필요");
    s.heading = tr("대상 폴더에 쓰려면 관리자 권한이 필요합니다");
    s.description = tr("이 폴더는 관리자만 바꿀 수 있습니다. 계속하면 Windows 사용자 계정 컨트롤(UAC) 확인 창이 열립니다.");
    s.rows = {itemRow(tr("항목"), item), pathRow(tr("대상"), targetDir),
              textRow(tr("필요한 권한"), item.isDir ? tr("이 폴더에 하위 폴더 만들기") : tr("이 폴더에 파일 만들기 (쓰기)"))};
    s.check = applyCheck(remaining, applyDefault);
    s.buttons = {button(Choice::Elevate, tr("관리자 권한으로 계속(&C)"), true, true), button(Choice::Skip, tr("건너뛰기(&S)")),
                 cancelButton()};
    s.designSize = {560, 370};
    return s;
}

PromptSpec moveDenied(const ItemInfo &item, const QString &sourceDir, const QString &targetDir, int remaining,
                      bool applyDefault)
{
    PromptSpec s;
    s.windowTitle = tr("이동 — 관리자 권한 필요");
    s.heading = tr("원본을 옮기려면 관리자 권한이 필요합니다");
    s.description = tr("대상에 복사할 수는 있지만, 원본 위치에서 파일을 지울 권한이 없습니다.");
    s.rows = {itemRow(tr("항목"), item), pathRow(tr("원본"), sourceDir), pathRow(tr("대상"), targetDir),
              textRow(tr("필요한 권한"), tr("원본 위치에서 삭제"))};
    s.options = {
        {Choice::Elevate, tr("관리자 권한으로 이동(&M)"), tr("대상에 복사한 뒤 원본을 지웁니다."), tr("관리자 권한으로 이동"), true},
        {Choice::CopyOnly, tr("복사만 하기(&C)"), tr("원본은 그대로 둡니다. 관리자 권한이 필요 없습니다."), tr("복사만 하기"), false},
    };
    s.check = applyCheck(remaining, applyDefault);
    ButtonSpec primary = button(Choice::Elevate, QString(), true);
    primary.followsOption = true;
    s.buttons = {primary, button(Choice::Skip, tr("건너뛰기(&S)")), cancelButton()};
    s.designSize = {560, 480};
    return s;
}

PromptSpec deleteDenied(const ItemInfo &item, const QString &dir, bool toRecycleBin, int remaining, bool applyDefault)
{
    PromptSpec s;
    s.windowTitle = tr("삭제 — 관리자 권한 필요");
    s.heading = item.isDir ? tr("이 폴더를 삭제하려면 관리자 권한이 필요합니다") : tr("이 파일을 삭제하려면 관리자 권한이 필요합니다");
    s.description = tr("이 위치의 파일은 관리자만 지울 수 있습니다. 계속하면 사용자 계정 컨트롤(UAC) 확인 창이 열립니다.");
    s.rows = {itemRow(tr("항목"), item), pathRow(tr("위치"), dir),
              textRow(tr("삭제 방식"), toRecycleBin ? tr("휴지통으로 이동") : tr("영구 삭제")), textRow(tr("필요한 권한"), tr("삭제"))};
    s.check = applyCheck(remaining, applyDefault);
    s.buttons = {button(Choice::Elevate, tr("관리자 권한으로 삭제(&D)"), true, true), button(Choice::Skip, tr("건너뛰기(&S)")),
                 cancelButton()};
    s.designSize = {560, 390};
    return s;
}

PromptSpec renameDenied(const ItemInfo &item, const QString &newName, const QString &dir)
{
    PromptSpec s;
    s.windowTitle = tr("이름 변경 — 관리자 권한 필요");
    s.heading = tr("이름을 바꾸려면 관리자 권한이 필요합니다");
    s.description = tr("이 폴더의 항목은 관리자만 이름을 바꿀 수 있습니다.");
    s.rows = {itemRow(tr("현재 이름"), item), textRow(tr("새 이름"), newName, KeyValueCard::Strong), pathRow(tr("위치"), dir),
              textRow(tr("필요한 권한"), tr("이 폴더에서 삭제 · 파일 만들기"))};
    ButtonSpec edit = button(Choice::EditName, tr("이름 다시 입력(&E)"));
    edit.placement = ButtonSpec::Leading;
    s.buttons = {edit, button(Choice::Elevate, tr("관리자 권한으로 이름 바꾸기(&R)"), true, true), cancelButton()};
    s.designSize = {560, 360};
    return s;
}

PromptSpec createDenied(bool folder, const QString &name, const QString &dir, const QString &protectedRoot,
                        const QString &userFolderDisplay)
{
    PromptSpec s;
    s.windowTitle = folder ? tr("새 폴더 — 관리자 권한 필요") : tr("새 파일 — 관리자 권한 필요");
    s.heading = folder ? tr("이 위치에 폴더를 만들려면 관리자 권한이 필요합니다") : tr("이 위치에 파일을 만들려면 관리자 권한이 필요합니다");
    s.description = tr("%1 아래는 관리자만 바꿀 수 있습니다. 앱이 쓰는 데이터라면 사용자 폴더에 두는 편이 안전합니다.").arg(protectedRoot);
    ItemInfo item;
    item.name = name;
    item.isDir = folder;
    item.kindTint = Token::KCode;
    s.rows = {itemRow(tr("만들 항목"), item), pathRow(tr("위치"), dir),
              textRow(tr("필요한 권한"), folder ? tr("이 폴더에 하위 폴더 만들기") : tr("이 폴더에 파일 만들기"))};
    s.altAction = AltActionSpec{Choice::UseUserFolder, fm::ui::IconSpec::folder(),
                                tr("대신 사용자 폴더에 만들기(&U) — 권한 필요 없음"), userFolderDisplay};
    s.buttons = {button(Choice::Elevate, tr("관리자 권한으로 만들기(&C)"), true, true),
                 button(Choice::ChooseOtherLocation, tr("다른 위치(&O)…")), cancelButton()};
    s.designSize = {560, 400};
    return s;
}

PromptSpec ownershipDenied(Operation op, const ItemInfo &item, const QString &dir, const QString &owner,
                           bool isWindowsApps, Choice defaultChoice)
{
    PromptSpec s;
    s.windowTitle = tr("%1 — 액세스 거부").arg(titlePrefix(op));
    s.badgeTone = fm::ui::DialogHeader::Danger;
    s.badgeGlyph = glyph::LockKeyhole;
    s.heading = tr("관리자 권한으로도 액세스가 거부되었습니다");
    const QString ownerName = owner.section(u'\\', -1);
    s.description = tr("이 %1의 소유자는 %2이며 관리자에게도 %3 권한을 주지 않습니다. 소유권을 가져온 뒤 권한을 부여해야 계속할 수 있습니다.")
                        .arg(item.isDir ? tr("폴더") : tr("파일"), ownerName, verb(op));
    if (isWindowsApps) {
        s.banner = BannerSpec{fm::ui::Banner::Warn, glyph::Warning,
                              tr("WindowsApps 폴더의 소유권이나 권한을 바꾸면 스토어 앱 설치와 업데이트가 실패할 수 있습니다. "
                                 "앱은 설정 › 앱에서 제거하는 편이 안전합니다."),
                              true};
    }
    s.rows = {itemRow(tr("항목"), item, Qt::ElideMiddle), pathRow(tr("위치"), dir),
              textRow(tr("현재 소유자"), owner, KeyValueCard::Mono),
              textRow(tr("바뀌는 내용"), tr("소유자를 Administrators로 바꾸고 모든 권한 부여"))};
    // 위험한 동작은 왼쪽 · 보통 역할, Enter는 건너뛰기(설정 "대화상자 기본 버튼"으로 바꿀 수 있다 — 03 §2.7)
    const bool takeDefault = defaultChoice == Choice::TakeOwnership;
    ButtonSpec take = button(Choice::TakeOwnership, tr("소유권 가져오고 %1(&T)").arg(verb(op)), takeDefault, true);
    take.placement = ButtonSpec::Leading;
    ButtonSpec skip = button(Choice::Skip, tr("건너뛰기(&S)"), !takeDefault);
    if (!takeDefault)
        skip.toolTip = u"Enter"_s;
    s.buttons = {take, skip, cancelButton()};
    s.designSize = {560, 440};
    return s;
}

PromptSpec elevationFailed(Operation op, const ItemInfo &item, const QString &targetDir)
{
    PromptSpec s;
    s.windowTitle = tr("%1 — 관리자 권한을 얻지 못함").arg(titlePrefix(op));
    s.badgeTone = fm::ui::DialogHeader::Danger;
    s.badgeGlyph = glyph::Warning;
    s.heading = tr("관리자 권한을 얻지 못했습니다");
    s.description = tr("사용자 계정 컨트롤 창에서 ‘아니요’를 골랐거나 응답 시간이 지났습니다. ") + notDoneYet(op);
    KeyValueCard::Row status;
    status.label = tr("상태");
    status.tagText = failedStatus(op);
    status.tagTone = fm::ui::Tag::Mute;
    s.rows = {itemRow(tr("항목"), item), pathRow(op == Operation::Delete ? tr("위치") : tr("대상"), targetDir), status};
    s.check = CheckSpec{tr("권한이 필요한 남은 항목은 묻지 않고 건너뛰기(&A)"), false};
    s.buttons = {button(Choice::Retry, tr("다시 시도(&R)"), true, true), button(Choice::Skip, tr("건너뛰기(&S)")),
                 cancelButton(tr("작업 취소"))};
    s.designSize = {560, 370};
    return s;
}

QStringList boardIds()
{
    return {u"preflight"_s, u"preflight.skip"_s, u"copy"_s, u"move"_s, u"move.copyOnly"_s, u"delete"_s, u"rename"_s,
            u"create.folder"_s, u"create.file"_s, u"ownership"_s, u"failed"_s};
}

PromptSpec board(const QString &id)
{
    constexpr qint64 MB = 1024 * 1024;
    const ItemInfo redist{u"vc_redist.x64.exe"_s, false, qint64(24.4 * MB), Token::KExe};
    const QString redistDir = u"C:\\Program Files\\FM Tools\\redist\\"_s;

    if (id.startsWith(u"preflight")) {
        const QList<fm::ui::ItemListCard::Item> items = {
            {u"build\\msi-staging\\FM Tools\\fm.exe"_s, u"Administrators"_s},
            {u"build\\msi-staging\\FM Tools\\Qt6Core.dll"_s, u"Administrators"_s},
            {u"build\\msi-staging\\install-elevated.log"_s, u"Administrators"_s},
        };
        PromptSpec s = preflight(Operation::Delete, 42, items, tr("아래 항목은 관리자 계정으로 실행한 설치 과정에서 만들어졌습니다."));
        if (id == u"preflight.skip")
            s.defaultOption = 1;
        return s;
    }
    if (id == u"copy")
        return copyDenied(redist, redistDir, 2, true);
    if (id.startsWith(u"move")) {
        const ItemInfo item{u"session-export-2026-09.json"_s, false, qint64(2.4 * MB), Token::KCode};
        // 목업은 체크 상자 처음 값이 꺼져 있다(03 R6 — 설정값은 켬)
        PromptSpec s = moveDenied(item, u"C:\\ProgramData\\FM Tools\\exports\\"_s, u"D:\\Archive\\2026-Q3\\"_s, 4, false);
        if (id == u"move.copyOnly")
            s.defaultOption = 1;
        return s;
    }
    if (id == u"delete") {
        const ItemInfo item{u"thumbs-v2.db"_s, false, 128 * MB, Token::KSys};
        return deleteDenied(item, u"C:\\ProgramData\\FM Tools\\cache\\"_s, true, 4, true);
    }
    if (id == u"rename") {
        const ItemInfo item{u"settings.ini"_s, false, -1, Token::KDoc};
        return renameDenied(item, u"settings.ini.bak"_s, u"C:\\Program Files\\FM Tools\\"_s);
    }
    if (id == u"create.folder" || id == u"create.file") {
        const bool folder = id == u"create.folder";
        return createDenied(folder, folder ? u"plugins"_s : u"plugins.json"_s, u"C:\\Program Files\\FM Tools\\"_s,
                            u"C:\\Program Files"_s,
                            folder ? u"%LOCALAPPDATA%\\FM Tools\\plugins"_s : u"%LOCALAPPDATA%\\FM Tools\\plugins.json"_s);
    }
    if (id == u"ownership") {
        const ItemInfo item{u"Fabrikam.PhotoTools_3.2.14.0_x64__8h2k1d9x7q3aa"_s, true, -1, Token::KDoc};
        return ownershipDenied(Operation::Delete, item, u"C:\\Program Files\\WindowsApps\\"_s,
                               u"NT SERVICE\\TrustedInstaller"_s, true);
    }
    if (id == u"failed")
        return elevationFailed(Operation::Copy, redist, redistDir);
    return copyDenied(redist, redistDir, 2, true);
}

} // namespace prompts

} // namespace fm::dialogs::elev
