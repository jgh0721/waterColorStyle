#pragma once

#include "fmfilelist/ColumnValues.h"
#include "fmfilelist/FileListModel.h"

#include <QAbstractProxyModel>
#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>

class QFileSystemModel;

namespace fm::filelist {

class ThumbnailProvider;

/// 읽기 전용 QFileSystemModel의 폴더 하나를 평면 목록으로 보여 주는 프록시.
/// 현재 폴더의 자식만 매핑하고, 맨 위에 합성 ".." 행을 두고(드라이브 루트 제외), FileRoles 값을 계산한다.
/// 표시(MarkedRole)는 파일 이름 집합으로 보관한다 — 폴더를 바꾸면 비운다.
class FileSystemListProxy : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit FileSystemListProxy(QFileSystemModel *source, QObject *parent = nullptr);

    QFileSystemModel *fileSystemModel() const noexcept { return m_fs; }

    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }
    bool hasUpRow() const noexcept { return m_hasUp; }

    void setThumbnailProvider(ThumbnailProvider *provider);

    /// 크기 · 날짜 표시 형식. 바꾸면 모든 행의 dataChanged를 낸다.
    const DisplayFormat &displayFormat() const noexcept { return m_format; }
    void setDisplayFormat(const DisplayFormat &format);
    /// 열 세트의 추가 열(모델 열 ColumnCount부터). Windows 속성은 reader가 읽는 대로 그 행을 다시 알린다.
    const ExtraColumns &extraColumns() const noexcept { return m_extra; }
    void setExtraColumns(const ExtraColumns &extra);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

Q_SIGNALS:
    /// 현재 폴더의 목록을 다 읽었다(QFileSystemModel::directoryLoaded).
    void loaded();

private:
    int upOffset() const noexcept { return m_hasUp ? 1 : 0; }
    bool isUpRow(int row) const noexcept { return m_hasUp && row == 0; }
    QModelIndex sourceIndex(int row) const;
    const FileEntry &entryAt(int row) const;
    QVariant thumbnailData(const FileEntry &entry, int role) const;

    void connectSource();
    void thumbnailReady(const QString &path);

    QFileSystemModel *m_fs = nullptr;
    ThumbnailProvider *m_thumbnails = nullptr;
    QPersistentModelIndex m_root;
    QString m_rootPath;
    bool m_hasUp = false;
    bool m_resetting = false;
    DisplayFormat m_format;
    ExtraColumns m_extra;
    QSet<QString> m_marked;                         // 파일 이름
    mutable QHash<QString, FileEntry> m_entries;    // 파일 이름 → 계산한 항목
    FileEntry m_up;
    QModelIndexList m_layoutProxy;
    QList<QPersistentModelIndex> m_layoutSource;
};

/// 실제 폴더 원본: 읽기 전용 QFileSystemModel(변경 감시 사용) + FileSystemListProxy + 섬네일 생성기.
/// 이름 변경 · 끌어 놓기 · 삭제는 막는다. 파일 작업 대화상자는 실제 이름 · 크기로 열리지만 실행은 시뮬레이터로만 한다.
class LocalFileSource : public QObject
{
    Q_OBJECT
public:
    explicit LocalFileSource(QObject *parent = nullptr);
    ~LocalFileSource() override;

    FileSystemListProxy *model() const noexcept { return m_proxy; }
    ThumbnailProvider *thumbnails() const noexcept { return m_thumbnails; }
    /// 열 세트의 Windows 속성(작업 스레드 + 캐시).
    PropertyReader *properties() const noexcept { return m_properties; }

    QString path() const;
    void setPath(const QString &path);
    /// 상위 폴더로. 드라이브 루트면 false.
    bool cdUp();
    /// 목록 모델의 인덱스(프록시를 거쳐도 된다)를 연다 — 폴더면 들어가고 ".."면 올라간다. 파일은 false.
    bool open(const QModelIndex &index);

    /// 숨김 · 시스템 속성 파일을 읽을지(QDir::Hidden | QDir::System). 시스템 파일만 따로 거르는 것은 FileSortProxy::showSystem.
    bool showHidden() const noexcept { return m_showHidden; }
    void setShowHidden(bool on);

    /// "D:" · "새 볼륨" · "여유 312 GB / 1.82 TB" — 현재 폴더가 있는 드라이브.
    QString driveName() const;
    QString volumeLabel() const;
    QString freeSpaceText() const;

Q_SIGNALS:
    /// 폴더가 바뀌었다. previousChild는 위로 올라갔을 때 방금 나온 폴더 이름(커서를 그 폴더에 둘 때 쓴다).
    void pathChanged(const QString &path, const QString &previousChild);

private:
    void applyFilter();

    QFileSystemModel *m_fs = nullptr;
    FileSystemListProxy *m_proxy = nullptr;
    ThumbnailProvider *m_thumbnails = nullptr;
    PropertyReader *m_properties = nullptr;
    bool m_showHidden = false;
};

} // namespace fm::filelist
