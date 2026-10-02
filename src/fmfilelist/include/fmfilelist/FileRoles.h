#pragma once

// 파일 목록 모델 규약 — 샘플 원본(FileListModel)과 실제 폴더 원본(FileSystemListProxy)이 같은 열 · 역할을 준다.
// 목록(FileListView) · 섬네일(ThumbnailView)은 원본을 구분하지 않고 이 규약만 읽는다.

#include <fmstyle/ThemeTokens.h>

#include <QDateTime>
#include <QString>
#include <QVariant>

#include <cstdint>

namespace fm::filelist {

/// 열. 1줄 보기는 Name · Ext · Size · Modified · Attr, 2줄 보기는 Icon · Name · Type · Size · Modified · Attr · Filler.
enum Column {
    IconColumn,
    NameColumn,
    ExtColumn,
    TypeColumn,
    SizeColumn,
    ModifiedColumn,
    AttrColumn,
    FillerColumn,   // 2줄 보기 메타 줄의 남는 폭(빈 칸)
    ColumnCount
};

enum Role {
    StemRole = Qt::UserRole + 1,  // QString — 확장자를 뺀 이름. 점으로 시작하는 파일 · 폴더는 전체 이름
    ExtRole,                      // QString — 점 없는 확장자("exe"), 없으면 빈 문자열
    FullNameRole,                 // QString
    KindRole,                     // int(Kind)
    IsDirRole,                    // bool — 폴더 · 상위 폴더
    IsUpRole,                     // bool — ".."
    HiddenRole,                   // bool — 숨김 또는 시스템 속성(흐리게 그림)
    MarkedRole,                   // bool — 표시(선택). setData로 바꾼다
    SizeBytesRole,                // qint64 — 폴더는 -1
    ModifiedRole,                 // QDateTime
    AttributesRole,               // int(Attribute 조합)
    TypeNameRole,                 // QString — "응용 프로그램" 등
    FilePathRole,                 // QString — 절대 경로(샘플은 가상 경로)
    ArtRole,                      // int(Art) — 섬네일 그림의 종류
    ThumbnailRole,                // QImage — 실제 섬네일(Art::Image일 때)
    AspectRole,                   // qreal — 그림의 가로 / 세로, 모르면 0
    BadgeRole,                    // QString — 섬네일 배지("PNG", "1:42:08")
    NoSortRole,                   // 항상 빈 값 — Qtitan 자체 정렬이 모델 순서를 그대로 두게 한다(FileListView)
    GroupStyleRole,               // QVariant(ResolvedGroupStyle) — 파일 그룹의 글자색 · 배경색 · 글꼴 효과(FileGroups.h)
    PreviewStateRole,             // int(PreviewState 조합) — 미리보기가 행마다 상태를 정한다(설정 › 테마 색상의 행 상태)
    SizeTextRole,                 // QString — 크기 글자(모델의 표시 형식), 폴더는 빈 문자열
    DateTextRole,                 // QString — 날짜만(모델의 표시 형식에서 시각을 뺀 것) — 섬네일 정보 줄
};

/// 종류 — 아이콘 모양과 종류 띠 색(--k-*)을 정한다.
enum class Kind : std::uint8_t { Up, Folder, Exe, Pdf, Img, Zip, Code, Doc, Sys, Other };

/// 섬네일 그림 — 샘플은 목업의 가짜 그림(Shot · Video · Pdf · Photo), 실제 폴더는 Image 또는 Loading.
enum class Art : std::uint8_t { None, Shot, Video, Pdf, Photo, Loading, Image };

enum Attribute : int { ReadOnly = 0x1, Archive = 0x2, Hidden = 0x4, System = 0x8, ReparsePoint = 0x10 };

/// 미리보기 행 상태(PreviewStateRole) — 설정의 테마 미리보기가 한 목록에 여러 상태를 함께 보인다.
enum PreviewState : int { PreviewMarked = 0x1, PreviewCursor = 0x2, PreviewInvertCursor = 0x4, PreviewInvertSelection = 0x8 };

/// 종류 띠 색 토큰. 폴더는 --folder, 상위 폴더 · 기타는 --k-doc.
fm::style::Token kindToken(Kind kind) noexcept;

/// 확장자(점 없음)로 종류를 정한다 — 목업 매핑(01 §11.3) + 흔한 확장자.
Kind kindForExtension(const QString &ext, bool system = false);

/// 이름을 stem과 확장자로 나눈다. 점으로 시작하는 파일(".gitignore")과 폴더는 전체가 stem이다.
void splitFileName(const QString &name, bool isDir, QString *stem, QString *ext);

/// 목업 형식의 크기 — "412 B", "1.3 KB", "186 MB", "3.74 GB", "1.82 TB" (1024 단위).
QString formatSize(qint64 bytes);

/// 목업 형식의 날짜 "yyyy-MM-dd HH:mm".
QString formatDate(const QDateTime &time);

/// 크기 열의 단위(설정 › 파일 패널).
enum class SizeUnit : std::uint8_t { Auto, Bytes, KB, MB };

/// 목록 · 섬네일의 크기 · 날짜 표시 형식(설정 › 파일 패널). 모델마다 두어 설정 미리보기는 적용 전 값을 쓴다.
struct DisplayFormat
{
    static constexpr const char16_t *kSystemShort = u"system-short";
    static constexpr const char16_t *kSystemLong = u"system-long";

    SizeUnit sizeUnit = SizeUnit::Auto;
    QString dateFormat = QStringLiteral("yyyy-MM-dd HH:mm");  // Qt 형식 문자열 또는 kSystemShort · kSystemLong

    bool operator==(const DisplayFormat &) const = default;
};

/// 단위를 정한 크기 — 자동은 formatSize(), 바이트 "1,234,567 B", KB "1,206 KB"(올림), MB "1.2 MB"(0.1 MB 올림).
QString formatSize(qint64 bytes, SizeUnit unit);
/// 표시 형식의 날짜 · 시각. 시스템 형식은 지금 로캘의 짧은 · 긴 형식.
QString formatDate(const QDateTime &time, const QString &format);
/// 표시 형식에서 시각 부분을 뺀 날짜만(섬네일 정보 줄) — "yyyy-MM-dd HH:mm" → "yyyy-MM-dd".
QString formatDay(const QDateTime &time, const QString &format);

/// 속성 네 글자 "rahs"(읽기 전용 · 보관 · 숨김 · 시스템), 없으면 '-'.
QString attributeText(int attributes);

/// 섬네일 대상인 이미지 확장자(Qt 이미지 읽기 지원 형식)인지.
bool isImageExtension(const QString &ext);
/// Windows 셸 섬네일이 기대되는 동영상 · PDF 확장자인지.
bool isShellThumbnailExtension(const QString &ext);

} // namespace fm::filelist
