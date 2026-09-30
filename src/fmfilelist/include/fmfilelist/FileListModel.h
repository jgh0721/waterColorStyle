#pragma once

#include "fmfilelist/FileRoles.h"

#include <QAbstractTableModel>
#include <QImage>
#include <QList>

namespace fm::filelist {

/// 목록의 항목 하나. 샘플 원본은 이 값을 그대로 보관하고, 실제 폴더 원본은 행마다 만들어 캐시한다.
struct FileEntry
{
    QString stem;
    QString ext;              // 점 없음
    QString typeName;
    Kind kind = Kind::Other;
    qint64 size = -1;         // 폴더 · 상위 폴더는 -1
    QDateTime modified;
    int attributes = 0;
    bool marked = false;
    QString path;
    Art art = Art::None;      // 섬네일 그림 종류
    qreal aspect = 0;         // 그림의 가로 / 세로
    QString badge;
    QImage thumbnail;         // Art::Image일 때

    bool isUp() const noexcept { return kind == Kind::Up; }
    bool isDir() const noexcept { return kind == Kind::Folder || kind == Kind::Up; }
    bool isHidden() const noexcept { return (attributes & (Hidden | System)) != 0; }
    QString fullName() const { return ext.isEmpty() ? stem : stem + u'.' + ext; }
};

/// 항목 하나의 열 · 역할 값 — 두 원본이 같은 규칙을 쓰도록 모았다.
QVariant fileEntryData(const FileEntry &entry, int column, int role);

/// 샘플 데이터용 표 모델. 표시(MarkedRole)만 바꿀 수 있다.
class FileListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit FileListModel(QObject *parent = nullptr);
    explicit FileListModel(const QList<FileEntry> &entries, QObject *parent = nullptr);

    void setEntries(const QList<FileEntry> &entries);
    const QList<FileEntry> &entries() const noexcept { return m_entries; }
    const FileEntry &entry(int row) const { return m_entries.at(row); }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QList<FileEntry> m_entries;
};

/// 열 머리글 문자열(두 원본 공통): "이름", "확장자", "종류", "크기", "수정한 날짜", "속성".
QString fileColumnTitle(int column);

} // namespace fm::filelist
