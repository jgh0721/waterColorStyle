#include "fmfilelist/LocalFileSource.h"

#include "fmfilelist/ThumbnailProvider.h"

#include <QDir>
#include <QFileSystemModel>
#include <QStorageInfo>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

using namespace Qt::StringLiterals;

namespace fm::filelist {

namespace {

// QFileSystemModel 열: 0 이름 · 1 크기 · 2 종류 · 3 수정한 날짜
int sourceColumnFor(int column)
{
    switch (column) {
    case SizeColumn:     return 1;
    case TypeColumn:     return 2;
    case ModifiedColumn: return 3;
    default:             return 0;
    }
}

int columnFromSource(int sourceColumn)
{
    switch (sourceColumn) {
    case 1:  return SizeColumn;
    case 2:  return TypeColumn;
    case 3:  return ModifiedColumn;
    default: return NameColumn;
    }
}

int attributesOf(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    const DWORD a = GetFileAttributesW(reinterpret_cast<const wchar_t *>(native.utf16()));
    if (a == INVALID_FILE_ATTRIBUTES)
        return 0;
    return ((a & FILE_ATTRIBUTE_READONLY) ? ReadOnly : 0) | ((a & FILE_ATTRIBUTE_ARCHIVE) ? Archive : 0)
         | ((a & FILE_ATTRIBUTE_HIDDEN) ? Hidden : 0) | ((a & FILE_ATTRIBUTE_SYSTEM) ? System : 0)
         | ((a & FILE_ATTRIBUTE_REPARSE_POINT) ? ReparsePoint : 0);
#else
    const QFileInfo info(path);
    return (info.isHidden() ? Hidden : 0) | (info.isWritable() ? 0 : ReadOnly);
#endif
}

} // namespace

// ---------------------------------------------------------------- FileSystemListProxy

FileSystemListProxy::FileSystemListProxy(QFileSystemModel *source, QObject *parent)
    : QAbstractProxyModel(parent)
    , m_fs(source)
{
    m_up.stem = u".."_s;
    m_up.kind = Kind::Up;
    m_up.typeName = u"상위 폴더"_s;
    QAbstractProxyModel::setSourceModel(source);
    connectSource();
}

void FileSystemListProxy::setThumbnailProvider(ThumbnailProvider *provider)
{
    if (m_thumbnails)
        disconnect(m_thumbnails, nullptr, this, nullptr);
    m_thumbnails = provider;
    if (m_thumbnails) {
        connect(m_thumbnails, &ThumbnailProvider::ready, this, &FileSystemListProxy::thumbnailReady);
        connect(m_thumbnails, &ThumbnailProvider::settingsChanged, this, [this] {
            if (rowCount() > 0)
                Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {ArtRole, ThumbnailRole, AspectRole, BadgeRole});
        });
    }
}

void FileSystemListProxy::setRootPath(const QString &path)
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    m_resetting = true;
    beginResetModel();
    m_rootPath = clean;
    m_root = m_fs->setRootPath(clean);
    m_hasUp = !QDir(clean).isRoot();
    m_marked.clear();
    m_entries.clear();
    endResetModel();
    m_resetting = false;
    if (m_root.isValid() && m_fs->canFetchMore(m_root))
        m_fs->fetchMore(m_root);
}

void FileSystemListProxy::connectSource()
{
    auto underRoot = [this](const QModelIndex &parent) { return !m_resetting && m_root.isValid() && parent == m_root; };

    connect(m_fs, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this, underRoot](const QModelIndex &parent, int first, int last) {
                if (underRoot(parent))
                    beginInsertRows({}, first + upOffset(), last + upOffset());
            });
    connect(m_fs, &QAbstractItemModel::rowsInserted, this, [this, underRoot](const QModelIndex &parent) {
        if (underRoot(parent))
            endInsertRows();
    });
    connect(m_fs, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this, underRoot](const QModelIndex &parent, int first, int last) {
                if (!underRoot(parent))
                    return;
                for (int r = first; r <= last; ++r)
                    m_entries.remove(m_fs->fileName(m_fs->index(r, 0, parent)));
                beginRemoveRows({}, first + upOffset(), last + upOffset());
            });
    connect(m_fs, &QAbstractItemModel::rowsRemoved, this, [this, underRoot](const QModelIndex &parent) {
        if (underRoot(parent))
            endRemoveRows();
    });
    connect(m_fs, &QAbstractItemModel::dataChanged, this,
            [this, underRoot](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                if (!underRoot(topLeft.parent()))
                    return;
                for (int r = topLeft.row(); r <= bottomRight.row(); ++r)
                    m_entries.remove(m_fs->fileName(m_fs->index(r, 0, topLeft.parent())));
                Q_EMIT dataChanged(index(topLeft.row() + upOffset(), 0),
                                   index(bottomRight.row() + upOffset(), columnCount() - 1));
            });

    // 정렬 · 이동은 배치 변경으로 전한다. 우리 프록시의 영구 인덱스를 원본 영구 인덱스로 옮겨 심는다.
    auto aboutToChangeLayout = [this] {
        if (m_resetting)
            return;
        Q_EMIT layoutAboutToBeChanged();
        m_layoutProxy = persistentIndexList();
        m_layoutSource.clear();
        m_layoutSource.reserve(m_layoutProxy.size());
        for (const QModelIndex &i : std::as_const(m_layoutProxy))
            m_layoutSource.append(QPersistentModelIndex(mapToSource(i)));
    };
    auto layoutDone = [this] {
        if (m_resetting)
            return;
        QModelIndexList to;
        to.reserve(m_layoutProxy.size());
        for (qsizetype i = 0; i < m_layoutProxy.size(); ++i) {
            const QModelIndex &from = m_layoutProxy.at(i);
            if (isUpRow(from.row()) && !m_layoutSource.at(i).isValid()) {
                to.append(from);
                continue;
            }
            const QModelIndex mapped = mapFromSource(m_layoutSource.at(i));
            to.append(mapped.isValid() ? index(mapped.row(), from.column()) : QModelIndex());
        }
        changePersistentIndexList(m_layoutProxy, to);
        m_layoutProxy.clear();
        m_layoutSource.clear();
        Q_EMIT layoutChanged();
    };
    connect(m_fs, &QAbstractItemModel::layoutAboutToBeChanged, this, aboutToChangeLayout);
    connect(m_fs, &QAbstractItemModel::layoutChanged, this, layoutDone);
    connect(m_fs, &QAbstractItemModel::rowsAboutToBeMoved, this, aboutToChangeLayout);
    connect(m_fs, &QAbstractItemModel::rowsMoved, this, layoutDone);

    connect(m_fs, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        if (!m_resetting)
            beginResetModel();
    });
    connect(m_fs, &QAbstractItemModel::modelReset, this, [this] {
        if (m_resetting)
            return;
        m_root = m_fs->index(m_rootPath);
        m_entries.clear();
        endResetModel();
    });
    connect(m_fs, &QFileSystemModel::directoryLoaded, this, [this](const QString &path) {
        if (QDir::cleanPath(path) == m_rootPath)
            Q_EMIT loaded();
    });
}

void FileSystemListProxy::thumbnailReady(const QString &path)
{
    const QModelIndex source = m_fs->index(path);
    if (!source.isValid() || source.parent() != m_root)
        return;
    const int row = source.row() + upOffset();
    Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1), {ArtRole, ThumbnailRole, AspectRole});
}

QModelIndex FileSystemListProxy::sourceIndex(int row) const
{
    return m_fs->index(row - upOffset(), 0, m_root);
}

const FileEntry &FileSystemListProxy::entryAt(int row) const
{
    if (isUpRow(row))
        return m_up;
    const QModelIndex src = sourceIndex(row);
    const QString name = m_fs->fileName(src);
    auto it = m_entries.find(name);
    if (it == m_entries.end()) {
        FileEntry e;
        const bool dir = m_fs->isDir(src);
        splitFileName(name, dir, &e.stem, &e.ext);
        e.path = m_fs->filePath(src);
        e.attributes = attributesOf(e.path);
        e.kind = dir ? Kind::Folder : kindForExtension(e.ext, (e.attributes & System) != 0);
        e.size = dir ? -1 : m_fs->size(src);
        e.modified = m_fs->lastModified(src);
        const QFileInfo info = m_fs->fileInfo(src);
        e.created = info.birthTime();
        e.accessed = info.lastRead();
        e.typeName = dir ? u"파일 폴더"_s : m_fs->type(src);
        it = m_entries.insert(name, e);
    }
    it->marked = m_marked.contains(name);
    return *it;
}

QVariant FileSystemListProxy::thumbnailData(const FileEntry &e, int role) const
{
    const bool target = !e.isDir() && m_thumbnails && m_thumbnails->isTarget(e.ext);  // 설정 › 섬네일 보기 › 대상
    if (!target)
        return role == ArtRole ? QVariant(int(Art::None)) : QVariant();
    const QImage image = m_thumbnails->thumbnail(e.path, e.modified, e.size);
    switch (role) {
    case ArtRole:
        if (!image.isNull())
            return int(Art::Image);
        return int(m_thumbnails->isPending(e.path, e.modified, e.size) ? Art::Loading : Art::None);
    case ThumbnailRole:
        return image;
    case AspectRole:
        return image.isNull() ? 0.0 : qreal(image.width()) / qreal(image.height());
    case BadgeRole:
        return image.isNull() ? QString() : e.ext.toUpper();
    default:
        return QVariant();
    }
}

QModelIndex FileSystemListProxy::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || column < 0 || row >= rowCount() || column >= columnCount())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex FileSystemListProxy::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int FileSystemListProxy::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_root.isValid())
        return 0;
    return m_fs->rowCount(m_root) + upOffset();
}

int FileSystemListProxy::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount + m_extra.count();
}

bool FileSystemListProxy::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() && rowCount() > 0;
}

QModelIndex FileSystemListProxy::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || isUpRow(proxyIndex.row()))
        return QModelIndex();
    return m_fs->index(proxyIndex.row() - upOffset(), sourceColumnFor(proxyIndex.column()), m_root);
}

QModelIndex FileSystemListProxy::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || !m_root.isValid() || sourceIndex.parent() != m_root)
        return QModelIndex();
    return index(sourceIndex.row() + upOffset(), columnFromSource(sourceIndex.column()));
}

QVariant FileSystemListProxy::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    const FileEntry &e = entryAt(index.row());
    if (index.column() >= ColumnCount) {
        if (const std::optional<QVariant> v = m_extra.data(e, index.column(), role, m_format))
            return *v;
    }
    switch (role) {
    case ArtRole:
    case ThumbnailRole:
    case AspectRole:
    case BadgeRole:
        return thumbnailData(e, role);
    default:
        return fileEntryData(e, index.column(), role, m_format);
    }
}

void FileSystemListProxy::setExtraColumns(const ExtraColumns &extra)
{
    if (m_extra == extra)
        return;
    if (m_extra.reader)
        disconnect(m_extra.reader, nullptr, this, nullptr);
    beginResetModel();
    m_extra = extra;
    endResetModel();
    if (m_extra.reader) {
        // Windows 속성을 다 읽은 파일의 추가 열만 다시 그린다
        connect(m_extra.reader, &PropertyReader::ready, this, [this](const QString &path) {
            const QModelIndex source = m_fs->index(path);
            if (!source.isValid() || source.parent() != m_root || m_extra.count() == 0)
                return;
            const int row = source.row() + upOffset();
            Q_EMIT dataChanged(index(row, ColumnCount), index(row, columnCount() - 1), {Qt::DisplayRole});
        });
    }
}

void FileSystemListProxy::setDisplayFormat(const DisplayFormat &format)
{
    if (m_format == format)
        return;
    m_format = format;
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole, SizeTextRole, DateTextRole});
}

bool FileSystemListProxy::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != MarkedRole || !index.isValid() || isUpRow(index.row()))
        return false;
    const QString name = m_fs->fileName(sourceIndex(index.row()));
    const bool marked = value.toBool();
    if (m_marked.contains(name) == marked)
        return true;
    if (marked)
        m_marked.insert(name);
    else
        m_marked.remove(name);
    Q_EMIT dataChanged(this->index(index.row(), 0), this->index(index.row(), columnCount() - 1), {MarkedRole});
    return true;
}

QVariant FileSystemListProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return section >= ColumnCount ? m_extra.headerData(section, role) : QVariant(fileColumnTitle(section));
    return QVariant();
}

Qt::ItemFlags FileSystemListProxy::flags(const QModelIndex &index) const
{
    // 읽기 전용: 편집 · 끌기 · 놓기 없음
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

bool FileSystemListProxy::canFetchMore(const QModelIndex &parent) const
{
    return !parent.isValid() && m_root.isValid() && m_fs->canFetchMore(m_root);
}

void FileSystemListProxy::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid() && m_root.isValid())
        m_fs->fetchMore(m_root);
}

// ---------------------------------------------------------------- LocalFileSource

LocalFileSource::LocalFileSource(QObject *parent)
    : QObject(parent)
    , m_fs(new QFileSystemModel(this))
    , m_thumbnails(new ThumbnailProvider(this))
    , m_properties(new PropertyReader(this))
{
    m_fs->setReadOnly(true);
    m_fs->setOption(QFileSystemModel::DontUseCustomDirectoryIcons, true);
    m_fs->setOption(QFileSystemModel::DontResolveSymlinks, true);
    applyFilter();
    m_proxy = new FileSystemListProxy(m_fs, this);
    m_proxy->setThumbnailProvider(m_thumbnails);
}

LocalFileSource::~LocalFileSource() = default;

QString LocalFileSource::path() const
{
    return QDir::toNativeSeparators(m_proxy->rootPath());
}

void LocalFileSource::setPath(const QString &path)
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (clean == m_proxy->rootPath())
        return;
    m_proxy->setRootPath(clean);
    Q_EMIT pathChanged(this->path(), QString());
}

bool LocalFileSource::cdUp()
{
    QDir dir(m_proxy->rootPath());
    if (dir.isRoot())
        return false;
    const QString child = dir.dirName();
    if (!dir.cdUp())
        return false;
    m_proxy->setRootPath(dir.absolutePath());
    Q_EMIT pathChanged(path(), child);
    return true;
}

bool LocalFileSource::open(const QModelIndex &index)
{
    if (!index.isValid())
        return false;
    if (index.data(IsUpRole).toBool())
        return cdUp();
    if (index.data(IsDirRole).toBool()) {
        setPath(index.data(FilePathRole).toString());
        return true;
    }
    return false;
}

void LocalFileSource::setShowHidden(bool on)
{
    if (m_showHidden == on)
        return;
    m_showHidden = on;
    applyFilter();
}

void LocalFileSource::applyFilter()
{
    QDir::Filters filters = QDir::AllEntries | QDir::AllDirs | QDir::NoDotAndDotDot;
    if (m_showHidden)
        filters |= QDir::Hidden | QDir::System;
    m_fs->setFilter(filters);
}

QString LocalFileSource::driveName() const
{
    const QString root = QStorageInfo(m_proxy->rootPath()).rootPath();
    if (root.size() >= 2 && root.at(1) == u':')
        return root.left(2).toUpper();
    return QDir::toNativeSeparators(root);
}

QString LocalFileSource::volumeLabel() const
{
    const QStorageInfo storage(m_proxy->rootPath());
    return storage.name().isEmpty() ? u"로컬 디스크"_s : storage.name();
}

QString LocalFileSource::freeSpaceText() const
{
    const QStorageInfo storage(m_proxy->rootPath());
    if (!storage.isValid())
        return QString();
    return u"여유 %1 / %2"_s.arg(formatSize(storage.bytesAvailable()), formatSize(storage.bytesTotal()));
}

} // namespace fm::filelist
