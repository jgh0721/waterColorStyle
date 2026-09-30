#pragma once

// 다중 이름 변경 규칙 엔진(02 §9.4) — 마스크 · 찾기/바꾸기 · 대소문자 · 카운터 · 날짜.
// 목업 샘플 12개의 요약 "12개 중 10개 변경 · 충돌/오류 1 · 변경 없음 1"이 단위 테스트 기준이다.

#include <QDateTime>
#include <QList>
#include <QSet>
#include <QString>

namespace fm::dialogs {

struct RenameFile
{
    QString name;
    qint64 size = 0;
    QDateTime captured;   // 촬영 날짜(EXIF) — 없으면 수정한 날짜를 쓴다
    QDateTime modified;
};

enum class NameCase { Keep, Lower, Upper, FirstUpper, TitleCase };
enum class ExtCase { Keep, Lower, Upper };
enum class DateSource { Captured, Modified };

struct RenameRules
{
    QString mask = QStringLiteral("[N]");
    QString extMask = QStringLiteral("[E]");
    QString find;
    QString replace;
    bool regex = false;
    bool caseSensitive = false;
    NameCase nameCase = NameCase::Keep;
    ExtCase extCase = ExtCase::Keep;
    int counterStart = 1;
    int counterStep = 1;
    int counterDigits = 3;
    DateSource dateSource = DateSource::Captured;

    bool operator==(const RenameRules &) const = default;
};

struct RenamePreviewRow
{
    enum State { Ok, Same, Invalid, Exists, Duplicate };

    int index = 0;           // 1부터
    QString original;
    QString newName;
    State state = Ok;
    bool isLong = false;     // 원래 이름 또는 새 이름이 40자 초과
    qint64 size = 0;
    QDateTime date;

    bool isBad() const noexcept { return state == Invalid || state == Exists || state == Duplicate; }
    QString stateText() const;  // "정상" · "변경 없음" · "잘못된 이름" · "충돌 · 이미 있음" · "충돌 · 중복"
};

struct RenameSummary
{
    int total = 0;
    int changed = 0;   // 정상
    int bad = 0;       // 잘못된 이름 + 충돌
    int same = 0;
    int longCount = 0;

    /// "12개 중 10개 변경 · 충돌/오류 1 · 변경 없음 1"
    QString text() const;
};

/// 한 파일의 새 이름(규칙 적용 순서: 마스크 → 찾기 · 바꾸기 → 이름 대소문자 → 확장자 마스크 → 확장자 대소문자).
QString applyRenameRules(const RenameFile &file, int index, const RenameRules &rules, const QString &folderName);

/// 미리보기 행. existingOthers는 폴더에서 이번 목록에 들지 않은 이름들(충돌 판정),
/// 비교는 기본으로 Windows처럼 대소문자를 무시한다("변경 없음"만 정확히 같을 때).
QList<RenamePreviewRow> previewRename(const QList<RenameFile> &files, const RenameRules &rules, const QString &folderName,
                                      const QSet<QString> &existingOthers,
                                      Qt::CaseSensitivity comparison = Qt::CaseInsensitive);

RenameSummary summarizeRename(const QList<RenamePreviewRow> &rows);

/// 목업 데이터(02 §9.5) — D:\Photos\2026-09 제주의 12개, 폴더에 미리 있는 이름 "2026-09-14_제주_007.jpg".
namespace RenameSample {
QList<RenameFile> files();
QString folder();          // "D:\\Photos\\2026-09 제주"
QString folderName();      // "2026-09 제주"
QSet<QString> existing();
RenameRules boardRules();  // 마스크 "[Y]-[M]-[D]_제주_[C]", 확장자 소문자
} // namespace RenameSample

} // namespace fm::dialogs
