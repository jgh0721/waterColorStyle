#pragma once

// 관리자 권한 요청(docs/specs/03) — 대화상자 구성 데이터와 8종 팩토리. 문구 · 동사 · 규칙을 이 한곳에 둔다.
// 대화상자는 구성(PromptSpec)을 받아 고정 순서(머리 → 배너 → 키-값 → 목록 → 선택지 → 대안 → 체크 상자)로 조립한다.

#include <fmwidgets/Banner.h>
#include <fmwidgets/DialogCards.h>
#include <fmwidgets/DialogHeader.h>

#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace fm::dialogs::elev {

enum class Operation : std::uint8_t { Copy, Move, Delete, Rename, CreateFolder, CreateFile };

/// 대화상자에서 고를 수 있는 결과 — 단추 · 라디오 · 대안 카드가 모두 이 값 하나를 낸다(03 §3.2).
enum class Choice : std::uint8_t {
    Elevate,             // 관리자 권한으로 계속 / 이동 / 삭제 / 이름 바꾸기 / 만들기, 사전 확인 '모두'
    SkipNeedingAdmin,    // 사전 확인: 권한이 필요한 항목만 건너뛰고 나머지 처리
    Skip,                // 건너뛰기(이 항목)
    CopyOnly,            // 이동: 복사만 하기
    Retry,               // 실패: 다시 시도
    TakeOwnership,       // 소유권: 소유권 가져오고 삭제
    UseUserFolder,       // 새로 만들기: 대신 사용자 폴더에
    ChooseOtherLocation, // 새로 만들기: 다른 위치…
    EditName,            // 이름 변경: 이름 다시 입력
    Cancel,              // 취소 · 작업 취소 · Esc · 닫기
};

/// "Elevate" 등 — 기록 · 테스트용.
QString choiceName(Choice choice);

struct OptionSpec
{
    Choice choice = Choice::Elevate;
    QString text;               // "관리자 권한으로 이동(&M)"
    QString description;        // 비면 한 줄 라디오, 있으면 설명 있는 라디오(OptionRadio)
    QString primaryText;        // 이 선택일 때 기본 단추 글자
    bool primaryShield = false;
};

struct ButtonSpec
{
    enum Placement : std::uint8_t { Leading, Trailing };
    Choice choice = Choice::Cancel;
    QString text;               // followsOption이면 비워 두고 선택지의 primaryText를 쓴다
    Placement placement = Trailing;
    bool primary = false;       // Button::Primary
    bool isDefault = false;     // Enter · 처음 포커스
    bool shield = false;
    bool followsOption = false; // 라디오 선택에 따라 글자 · 방패 · 결과가 바뀜
    QString toolTip;            // "Esc" · "Enter"
};

struct BannerSpec
{
    fm::ui::Banner::Tone tone = fm::ui::Banner::Warn;
    fm::ui::glyph::Glyph glyph = fm::ui::glyph::Warning;
    QString text;               // 서식 있는 글(<b> 허용)
    bool alignTop = true;
};

struct AltActionSpec
{
    Choice choice = Choice::UseUserFolder;
    fm::ui::IconSpec icon = fm::ui::IconSpec::folder();
    QString title;              // "대신 사용자 폴더에 만들기(&U) — 권한 필요 없음"
    QString detail;             // "%LOCALAPPDATA%\FM Tools\plugins"
};

struct CheckSpec
{
    QString text;
    bool checked = false;
};

struct PromptSpec
{
    QString windowTitle;
    fm::ui::DialogHeader::Tone badgeTone = fm::ui::DialogHeader::Warn;
    fm::ui::glyph::Glyph badgeGlyph = fm::ui::glyph::Shield;
    QString heading;
    QString description;
    std::optional<BannerSpec> banner;
    QList<fm::ui::KeyValueCard::Row> rows;
    QList<fm::ui::ItemListCard::Item> items;
    QList<OptionSpec> options;
    int defaultOption = 0;
    std::optional<AltActionSpec> altAction;
    std::optional<CheckSpec> check;
    QList<ButtonSpec> buttons;
    QSize designSize{560, 370};  // 목업 창 크기(제목 표시줄 36 · 테두리 포함) → 클라이언트 최소 558 × (H − 38)
};

struct Result
{
    Choice choice = Choice::Cancel;
    bool checked = false;        // '같은 선택 적용' 또는 실패 창의 '남은 항목 건너뛰기'
};

/// 엔진이 채우는 항목 정보.
struct ItemInfo
{
    QString name;
    bool isDir = false;
    qint64 size = -1;            // -1이면 크기를 표시하지 않는다
    fm::style::Token kindTint = fm::style::Token::KDoc;  // 파일 종류 띠 색(파일 목록과 같은 규칙)
};

namespace prompts {

PromptSpec preflight(Operation op, int total, const QList<fm::ui::ItemListCard::Item> &needAdmin,
                     const QString &reason = {});
PromptSpec copyDenied(const ItemInfo &item, const QString &targetDir, int remaining, bool applyDefault);
PromptSpec moveDenied(const ItemInfo &item, const QString &sourceDir, const QString &targetDir, int remaining,
                      bool applyDefault);
PromptSpec deleteDenied(const ItemInfo &item, const QString &dir, bool toRecycleBin, int remaining, bool applyDefault);
PromptSpec renameDenied(const ItemInfo &item, const QString &newName, const QString &dir);
PromptSpec createDenied(bool folder, const QString &name, const QString &dir, const QString &protectedRoot,
                        const QString &userFolderDisplay);
PromptSpec ownershipDenied(Operation op, const ItemInfo &item, const QString &dir, const QString &owner,
                           bool isWindowsApps, Choice defaultChoice = Choice::Skip);
PromptSpec elevationFailed(Operation op, const ItemInfo &item, const QString &targetDir);

/// 목업 보드 상태(03 §2): preflight · preflight.skip · copy · move · move.copyOnly · delete · rename ·
/// create.folder · create.file · ownership · failed. 선택지를 바꾼 상태(.skip · .copyOnly)는 defaultOption으로 나타낸다.
QStringList boardIds();
PromptSpec board(const QString &id);

} // namespace prompts

} // namespace fm::dialogs::elev
