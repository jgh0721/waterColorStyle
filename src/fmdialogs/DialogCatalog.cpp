#include "fmdialogs/DialogCatalog.h"

#include "fmdialogs/FileOpDialogs.h"
#include "fmdialogs/MultiRenameDialog.h"
#include "fmdialogs/ProgressDialog.h"

using namespace Qt::StringLiterals;

namespace fm::dialogs {

QList<DialogVariant> dialogVariants()
{
    const QSize copy(600, 444), move(600, 434), del(540, 424), file(560, 484), folder(520, 434);
    const QSize progress(640, 524), compact(520, 0);  // 간단히는 높이가 내용에 따라 정해진다
    const QSize rename(1120, 764);
    return {
        {u"copy.default"_s, u"copy"_s, u"복사"_s, true, copy},
        {u"copy.historyOpen"_s, u"copy"_s, u"최근 대상 목록"_s, false, copy},
        {u"copy.single"_s, u"copy"_s, u"1개 항목"_s, false, copy},
        {u"copy.destInvalid"_s, u"copy"_s, u"대상 없음"_s, false, copy},
        {u"copy.lowSpace"_s, u"copy"_s, u"여유 공간 부족"_s, false, copy},
        {u"copy.sameFolder"_s, u"copy"_s, u"같은 폴더"_s, false, copy},
        {u"move.sameVolume"_s, u"move"_s, u"같은 볼륨으로 이동"_s, true, move},
        {u"move.renameOnly"_s, u"move"_s, u"이름 변경만"_s, false, move},
        {u"move.crossVolume"_s, u"move"_s, u"다른 볼륨"_s, false, move},
        {u"move.conflict"_s, u"move"_s, u"같은 이름 있음"_s, false, move},
        {u"move.invalid"_s, u"move"_s, u"잘못된 문자"_s, false, move},
        {u"move.extChange"_s, u"move"_s, u"확장자 변경"_s, false, move},
        {u"move.newFolder"_s, u"move"_s, u"새 폴더"_s, false, move},
        {u"delete.trash"_s, u"delete"_s, u"휴지통"_s, true, del},
        {u"delete.permanent"_s, u"delete"_s, u"영구 삭제"_s, true, del},
        {u"delete.single"_s, u"delete"_s, u"1개 항목"_s, false, del},
        {u"delete.many"_s, u"delete"_s, u"128개 항목"_s, false, del},
        {u"delete.noRecycleBin"_s, u"delete"_s, u"휴지통 없는 드라이브"_s, false, del},
        {u"delete.readOnlyIncluded"_s, u"delete"_s, u"읽기 전용 포함"_s, false, del},
        {u"newfile.default"_s, u"newfile"_s, u"새 파일(C++ 헤더)"_s, true, file},
        {u"newfile.tpl0"_s, u"newfile"_s, u"빈 파일"_s, true, file},
        {u"newfile.tpl1"_s, u"newfile"_s, u"텍스트 문서"_s, true, file},
        {u"newfile.tpl2"_s, u"newfile"_s, u"Markdown"_s, true, file},
        {u"newfile.tpl3"_s, u"newfile"_s, u"C++ 소스"_s, true, file},
        {u"newfile.tpl5"_s, u"newfile"_s, u"CMake"_s, true, file},
        {u"newfile.tpl6"_s, u"newfile"_s, u"JSON"_s, true, file},
        {u"newfile.tpl7"_s, u"newfile"_s, u"Qt 디자이너 폼"_s, true, file},
        {u"newfile.exists"_s, u"newfile"_s, u"같은 이름 있음"_s, false, file},
        {u"newfile.invalid"_s, u"newfile"_s, u"잘못된 문자"_s, false, file},
        {u"newfile.empty"_s, u"newfile"_s, u"이름 없음"_s, false, file},
        {u"newfolder.nested"_s, u"newfolder"_s, u"하위 폴더까지"_s, true, folder},
        {u"newfolder.single"_s, u"newfolder"_s, u"한 단계"_s, false, folder},
        {u"newfolder.allExist"_s, u"newfolder"_s, u"이미 있음"_s, false, folder},
        {u"newfolder.invalid"_s, u"newfolder"_s, u"잘못된 이름"_s, false, folder},
        {u"newfolder.deep"_s, u"newfolder"_s, u"8단계"_s, false, folder},
        {u"progress.copy.detail.running"_s, u"progress"_s, u"복사 · 자세히"_s, true, progress},
        {u"progress.copy.detail.paused"_s, u"progress"_s, u"복사 · 자세히 · 일시 정지"_s, true, progress},
        {u"progress.copy.compact.running"_s, u"progress"_s, u"복사 · 간단히"_s, false, compact},
        {u"progress.move.compact.paused"_s, u"progress"_s, u"이동 · 일시 정지"_s, true, compact},
        {u"progress.delete.compact.running"_s, u"progress"_s, u"휴지통으로 삭제"_s, true, compact},
        {u"progress.copy.compact.elevated"_s, u"progress"_s, u"관리자 권한 복사"_s, true, compact},
        {u"progress.delete.compact.permanent"_s, u"progress"_s, u"영구 삭제"_s, false, compact},
        {u"multirename.default"_s, u"multirename"_s, u"다중 이름 변경(자동 → 2줄)"_s, true, rename},
        {u"multirename.oneLine"_s, u"multirename"_s, u"1줄 레코드"_s, true, rename},
        {u"multirename.twoLine"_s, u"multirename"_s, u"2줄 레코드"_s, true, rename},
        {u"multirename.extKeep"_s, u"multirename"_s, u"확장자 유지"_s, true, rename},
        {u"multirename.extUpper"_s, u"multirename"_s, u"확장자 대문자"_s, true, rename},
        {u"multirename.tokenAppend"_s, u"multirename"_s, u"토큰 [t] 덧붙임"_s, true, rename},
        {u"multirename.duplicates"_s, u"multirename"_s, u"중복 충돌"_s, false, rename},
        {u"multirename.invalid"_s, u"multirename"_s, u"잘못된 이름"_s, false, rename},
        {u"multirename.autoOneLine"_s, u"multirename"_s, u"짧은 이름(자동 → 1줄)"_s, false, rename},
        {u"multirename.blocked"_s, u"multirename"_s, u"충돌 시 막기"_s, false, rename},
        {u"multirename.findReplace"_s, u"multirename"_s, u"찾기 · 바꾸기"_s, false, rename},
        {u"multirename.noUndo"_s, u"multirename"_s, u"되돌릴 기록 없음"_s, false, rename},
    };
}

QDialog *createDialog(const QString &requested, QWidget *parent)
{
    QString id = requested;
    if (!id.contains(u'.')) {
        for (const DialogVariant &v : dialogVariants()) {
            if (v.dialog == id) {
                id = v.id;
                break;
            }
        }
    }
    const QString group = id.section(u'.', 0, 0);
    if (group == u"copy") {
        auto *d = new CopyDialog(BoardContext::copy(), parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"move") {
        auto *d = new MoveRenameDialog(BoardContext::moveRename(), false, parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"delete") {
        auto *d = new DeleteDialog(BoardContext::remove(), id == u"delete.permanent", parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"newfile") {
        auto *d = new NewFileDialog(BoardContext::newFile(), parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"newfolder") {
        auto *d = new NewFolderDialog(BoardContext::newFolder(), parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"multirename") {
        auto *d = new MultiRenameDialog(MultiRenameDialog::boardContext(), parent);
        d->applyVariant(id);
        return d;
    }
    if (group == u"progress") {
        auto *d = new ProgressDialog(ProgressDialog::boardCopy(), parent);
        d->setDemoLoop(true);
        d->applyVariant(id);
        return d;
    }
    return nullptr;
}

} // namespace fm::dialogs
