#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

namespace fm::filelist {

/// 목록 정렬 · 찾기 필터. ".."은 정렬 방향과 상관없이 맨 위, 폴더를 먼저(선택), 이름은 자연 정렬(숫자 크기 · 대소문자 무시).
/// 정렬 열이 -1이면 원본 순서를 그대로 둔다 — 샘플 데이터는 보드의 순서가 이름순이 아니다(01 §11).
/// 목록과 섬네일이 같은 프록시를 공유하므로 정렬 · 필터 · 표시가 보기 전환에도 유지된다.
class FileSortProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool foldersFirst READ foldersFirst WRITE setFoldersFirst)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden)
    Q_PROPERTY(QString quickFilter READ quickFilter WRITE setQuickFilter)

public:
    explicit FileSortProxy(QObject *parent = nullptr);

    bool foldersFirst() const noexcept { return m_foldersFirst; }
    void setFoldersFirst(bool on);

    /// 샘플 원본의 숨김 항목 보이기. 실제 폴더는 원본(QFileSystemModel)의 필터로 거른다.
    bool showHidden() const noexcept { return m_showHidden; }
    void setShowHidden(bool on);

    /// 시스템 속성(S) 항목 보이기 — 설정 › 파일 패널 › "보호된 운영 체제 파일 표시". 두 원본 모두 여기서 거른다.
    bool showSystem() const noexcept { return m_showSystem; }
    void setShowSystem(bool on);

    /// 이름에 이 글자가 들어간 항목만(대소문자 무시). ".."은 항상 남긴다.
    QString quickFilter() const { return m_quickFilter; }
    void setQuickFilter(const QString &text);

    /// 표시(MarkedRole)를 모든 행에 적용 · 반전한다(".." 제외).
    void markAll(bool marked);
    void invertMarks();

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    int compareNames(const QModelIndex &left, const QModelIndex &right) const;

    bool m_foldersFirst = true;
    bool m_showHidden = true;
    bool m_showSystem = true;
    QString m_quickFilter;
    QCollator m_collator;
};

} // namespace fm::filelist
