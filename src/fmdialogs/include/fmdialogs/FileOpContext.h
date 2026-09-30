#pragma once

// 파일 작업 대화상자에 넘기는 값 — 선택 항목, 원본 · 대상 폴더, 파일 시스템 조회(실제 폴더 또는 샘플).

#include <fmwidgets/DialogWidgets.h>

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace fm::dialogs {

struct FileItem
{
    QString name;
    qint64 size = 0;                  // 폴더는 0
    bool isDir = false;
    bool readOnly = false;
    fm::ui::ChoiceCard::FileKind kind = fm::ui::ChoiceCard::DocKind;
    QDateTime modified;
};

/// 볼륨 정보 — "E: 백업 · NTFS · 여유 공간 812 GB / 1.82 TB".
struct VolumeInfo
{
    bool valid = false;
    QString drive;      // "E:"
    QString label;      // "백업"
    QString fileSystem; // "NTFS"
    qint64 available = 0;
    qint64 total = 0;
    bool hasRecycleBin = true;
};

/// 파일 시스템 조회. 실제 폴더는 QFileInfo · QStorageInfo, 샘플은 가상 드라이브(D: · E: · \\nas01).
class FileSystemProbe
{
public:
    virtual ~FileSystemProbe() = default;
    virtual bool exists(const QString &path) const = 0;
    virtual VolumeInfo volume(const QString &path) const = 0;
    /// 같은 볼륨인지(이동이 즉시 처리되는지).
    bool sameVolume(const QString &a, const QString &b) const;
};

/// 실제 파일 시스템(읽기 전용 조회만).
class LocalProbe final : public FileSystemProbe
{
public:
    bool exists(const QString &path) const override;
    VolumeInfo volume(const QString &path) const override;
};

/// 샘플 드라이브 — 목업 문구가 나오도록 고정 값(02 §2 · §3 · §6).
class MockProbe final : public FileSystemProbe
{
public:
    MockProbe();
    bool exists(const QString &path) const override;
    VolumeInfo volume(const QString &path) const override;
    void addPath(const QString &path);

private:
    QStringList m_paths;  // 소문자 · 끝 \ 없음
};

/// 대화상자 입력.
struct FileOpContext
{
    QList<FileItem> items;
    QString sourceDir;             // "D:\\Downloads"
    QString targetDir;             // 반대쪽 패널 — 복사 · 이동 대상 기본값
    QStringList recentTargets;     // 최근 대상(복사 · 이동)
    QString suggestedName;         // 새 파일 · 새 폴더 처음 값(비면 "새 파일" · "새 폴더")
    const FileSystemProbe *probe = nullptr;

    qint64 totalSize() const;
    /// 항목 수 문구의 이름 — 1개면 그 이름, 여러 개면 "{첫 이름} 외 {n−1}개".
    QString countLabel() const;
};

/// 목업 보드의 상태로 채운 값(데모 · 스냅숏용).
namespace BoardContext {
FileOpContext copy();         // D:\Downloads 선택 3개 → E:\Backup\Installers
FileOpContext moveRename();   // PDF 보고서 하나
FileOpContext remove();       // 3개(삭제)
FileOpContext newFile();      // D:\Work\fm-core\src\panel · BandedPanelView
FileOpContext newFolder();    // D:\Work\fm-core\src · platform\win32\shim
const MockProbe &probe();
} // namespace BoardContext

/// "3.95 GB" — 목업 형식(fmfilelist::formatSize와 같음).
QString formatBytes(qint64 bytes);
/// 확장자로 파일 종류(아이콘 띠 색).
fm::ui::ChoiceCard::FileKind fileKindFor(const QString &name, bool isDir);

} // namespace fm::dialogs
