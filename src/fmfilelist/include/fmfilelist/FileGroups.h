#pragma once

// 파일 그룹 · 색상(설정 › 파일 그룹 · 색상, docs/specs/05 §2.1 · §4.2) — 조건으로 파일을 묶고 그룹마다 글자색 · 배경색 ·
// 글꼴 효과를 준다. 매처는 설정이 바뀔 때 한 번 컴파일하고, 목록 모델이 GroupStyleRole로 해석된 스타일을 돌려준다.

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace fm::filelist {

struct GroupCondition
{
    enum class Field : std::uint8_t { Extension, Name, Attributes, MimeType, Size, Modified, Created };
    enum class Op : std::uint8_t {
        AnyOf, NoneOf,                         // 확장자
        Wildcard, Regex, Contains, Equals,     // 이름
        Has, HasNot,                           // 속성
        MimeStartsWith, MimeEquals,            // 파일 형식
        AtLeast, AtMost, SizeEquals, Between,  // 크기
        Within, OlderThan,                     // 날짜
    };

    Field field = Field::Extension;
    Op op = Op::AnyOf;
    QString value;  // 원문 그대로 저장, 매칭할 때 해석한다

    bool operator==(const GroupCondition &) const = default;
};

struct GroupStyle
{
    std::optional<QColor> textLight, textDark, backLight, backDark;
    bool darkFromLight = false;  // '라이트 색에서 자동'(그룹별)
    bool bold = false, italic = false, underline = false, strike = false;

    bool hasEffect() const noexcept { return bold || italic || underline || strike; }
    bool operator==(const GroupStyle &) const = default;
};

struct FileGroup
{
    QString id;
    QString name;
    bool builtin = false;   // 이름 · 조건 잠금, 삭제 불가
    bool matchAll = false;  // false = 하나라도, true = 모두
    QList<GroupCondition> conditions;
    GroupStyle style;

    bool operator==(const FileGroup &) const = default;
};

struct FileGroupSettings
{
    enum class Merge : std::uint8_t { FirstOnly, PerProperty };
    QList<FileGroup> groups;  // 순서 = 우선순위
    Merge merge = Merge::PerProperty;

    /// 첫 실행 기본값 — 목업의 10개(숨김 · 시스템, 실행 파일, 문서 … 최근 24시간에 바뀜).
    static FileGroupSettings defaults();
    bool operator==(const FileGroupSettings &) const = default;
};

/// 매칭에 쓰는 파일 정보.
struct FileFacts
{
    QString name;          // 확장자 포함
    QString ext;           // 점 없음
    int attributes = 0;    // fm::filelist::Attribute
    qint64 size = -1;      // 폴더 -1
    QDateTime modified;
    QDateTime created;
    bool isDir = false;
};

/// 해석된 그룹 스타일(목록이 그린다). 색은 라이트 · 다크 둘 다 — 그리는 쪽이 테마 변형에 맞춰 고른다.
struct ResolvedGroupStyle
{
    std::optional<QColor> textLight, textDark, backLight, backDark;
    bool bold = false, italic = false, underline = false, strike = false;
    QStringList groups;  // 실제로 무언가를 준 그룹 이름(우선순위 순)

    bool isEmpty() const noexcept { return groups.isEmpty(); }
    std::optional<QColor> text(bool dark) const { return dark ? textDark : textLight; }
    std::optional<QColor> background(bool dark) const { return dark ? backDark : backLight; }
    bool operator==(const ResolvedGroupStyle &) const = default;
};

class FileGroupMatcher
{
public:
    FileGroupMatcher() = default;
    explicit FileGroupMatcher(const FileGroupSettings &settings);

    /// 맞는 그룹 인덱스(우선순위 순).
    QList<int> matchingGroups(const FileFacts &facts, const QDateTime &now = QDateTime::currentDateTime()) const;
    /// 합치기 규칙: 글자색 · 배경색 = 그 항목을 정한 첫 그룹, 효과 = 맞는 그룹 중 하나라도 켠 것(OR).
    /// "위 그룹 하나만"이면 첫 그룹만 남긴다.
    ResolvedGroupStyle resolve(const FileFacts &facts, const QDateTime &now = QDateTime::currentDateTime()) const;
    bool isEmpty() const noexcept { return m_groups.isEmpty(); }
    const FileGroupSettings &settings() const noexcept { return m_settings; }

private:
    struct Compiled
    {
        GroupCondition::Field field;
        GroupCondition::Op op;
        bool valid = false;
        QSet<QString> extensions;
        QList<QRegularExpression> patterns;
        QString text;
        QStringList mimes;
        int attributes = 0;
        qint64 low = 0, high = 0;
        qint64 seconds = 0;
    };
    struct CompiledGroup
    {
        bool matchAll = false;
        QList<Compiled> conditions;
    };
    bool matches(const Compiled &c, const FileFacts &facts, const QDateTime &now) const;

    FileGroupSettings m_settings;
    QList<CompiledGroup> m_groups;
};

/// 조건 값 해석 — 잘못되면 false와 사유(경고 테두리 · 도구 설명).
bool validateCondition(const GroupCondition &condition, QString *error = nullptr);
/// "1 GB" · "0 바이트" · "512KB" → 바이트(1024 단위).
std::optional<qint64> parseSizeText(const QString &text);
/// "24시간 이내" · "7일" · "2주" · "30분" → 초.
std::optional<qint64> parseDurationText(const QString &text);
/// 조건에서 만든 요약 줄 — "*.exe  *.msi  *.cmd  *.bat  *.ps1", 효과만 있으면 "· 밑줄만" 꼬리(05 §2.1.1).
QString groupSummary(const FileGroup &group);
/// 다크 색 파생('라이트 색에서 자동') — 밝기 반전 후 bg 대비 4.5 보정.
QColor darkFromLightColor(const QColor &light, const QColor &darkBackground);

QString conditionFieldLabel(GroupCondition::Field field);
QString conditionOpLabel(GroupCondition::Op op);
QList<GroupCondition::Op> conditionOps(GroupCondition::Field field);

} // namespace fm::filelist

Q_DECLARE_METATYPE(fm::filelist::ResolvedGroupStyle)
