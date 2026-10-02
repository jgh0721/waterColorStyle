#pragma once

// 열 세트를 메인 창 목록에 적용(설정 › 열 · 사용자 정의 열 — docs/specs/05 §2.2 · §4.3).
// 폴더마다 세트를 고르고(자동 적용 규칙), 세트의 열을 목록 열 배치로 바꾼다. 기본 열(이름 · 확장자 · 종류 · 크기 ·
// 수정한 날짜 · 속성)은 모델의 기본 열을 그대로 쓰고, 그 밖의 열(Windows 속성 · 식 · 만든 날짜 등)은 모델의 추가 열
// (ColumnCount부터)로 보인다. Windows 속성은 작업 스레드에서 읽어 캐시한다(05 §6-10).

#include "fmfilelist/ColumnSets.h"
#include "fmfilelist/FileRoles.h"
#include "fmfilelist/ListColumns.h"

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>

#include <optional>

class QAbstractItemModel;

namespace fm::filelist {

class FileGroupMatcher;
struct FileEntry;

/// Windows 속성(IPropertyStore) — 파일마다 필요한 속성을 한 번에 읽어 경로 · 수정 시각 기준으로 캐시한다.
class PropertyReader : public QObject
{
    Q_OBJECT
public:
    explicit PropertyReader(QObject *parent = nullptr);
    ~PropertyReader() override;

    /// 표시 값(Windows가 지역화한 글자 — "4032 x 3024", "0:00:42"). 아직 없으면 요청하고 빈 문자열 — 끝나면 ready(path).
    QString value(const QString &path, const QDateTime &modified, const QString &canonicalName);
    /// 식에 넣을 값 — 숫자는 단위 없이("4032"), 그 밖은 표시 값.
    QString rawValue(const QString &path, const QDateTime &modified, const QString &canonicalName);
    bool isPending(const QString &path, const QDateTime &modified) const;
    /// 앞으로 읽을 속성 이름(세트가 바뀌면 다시 정한다 — 캐시는 비운다).
    void setProperties(const QStringList &canonicalNames);
    QStringList properties() const { return m_names; }

Q_SIGNALS:
    void ready(const QString &path);

private:
    struct Values
    {
        QHash<QString, QString> display;
        QHash<QString, QString> raw;
    };
    const Values *values(const QString &path, const QDateTime &modified);

    QThreadPool m_pool;
    QCache<QString, Values> m_cache;
    QSet<QString> m_pending;
    QStringList m_names;
};

/// 폴더에 쓸 세트(05 §2.2.2) — 자동 적용 세트를 순서대로 보고, 맞는 것이 없으면 기본 세트(0).
/// rows는 목록 모델(파일 그룹 비율을 셀 때 — 폴더 · ".."는 빼고 센다). local이 아니면(샘플) 알려진 폴더는 맞지 않는다.
int resolveColumnSet(const ColumnSettings &settings, const QString &folder, bool local, const FileGroupMatcher *groups,
                     const QAbstractItemModel *rows);

/// 메인 창 목록의 열 배치. 기본 열은 모델 기본 열로, 그 밖의 열은 extras에 넣고 모델 열 ColumnCount + i로 둔다.
/// 기본 세트(설정 기본값 그대로)면 ListColumnLayout::standard()와 같다.
ListColumnLayout mainLayoutForSet(const ColumnSet &set, QList<ColumnDef> *extras);

/// 기본 필드 이름("크기" · "만든 날짜" · "경로" …) → 모델 기본 열(없으면 -1 — 추가 열로 계산).
int builtinFieldColumn(const QString &field);

/// 추가 열 값 — 기본 필드(만든 날짜 · 접근한 날짜 · 경로 등) · Windows 속성 · 식(대괄호 안의 속성 · 기본 필드를 값으로).
/// 값이 없으면 열의 빈 값 규칙(— · 비움 · 다른 열로 대신). reader가 없으면(샘플) Windows 속성은 빈 값.
QString extraColumnText(const ColumnDef &def, const QList<ColumnDef> &all, const FileEntry &entry, PropertyReader *reader,
                        const DisplayFormat &format);

/// 모델의 추가 열(두 원본이 같이 쓴다) — 모델 열 ColumnCount + i = defs[i].
struct ExtraColumns
{
    QList<ColumnDef> defs;
    QList<ColumnDef> all;        // 세트의 모든 열(빈 값 규칙 "다른 열로 대신")
    PropertyReader *reader = nullptr;

    int count() const noexcept { return int(defs.size()); }
    /// 표시 · 정렬 · 머리글 외의 역할은 nullopt — 모델이 이름 열의 값을 준다.
    std::optional<QVariant> data(const FileEntry &entry, int column, int role, const DisplayFormat &format) const;
    QVariant headerData(int column, int role) const;
    bool operator==(const ExtraColumns &) const = default;
};

} // namespace fm::filelist
