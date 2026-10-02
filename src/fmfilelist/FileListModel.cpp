#include "fmfilelist/FileListModel.h"

using namespace Qt::StringLiterals;

namespace fm::filelist {

QString fileColumnTitle(int column)
{
    switch (column) {
    case NameColumn:     return u"이름"_s;
    case ExtColumn:      return u"확장자"_s;
    case TypeColumn:     return u"종류"_s;
    case SizeColumn:     return u"크기"_s;
    case ModifiedColumn: return u"수정한 날짜"_s;
    case AttrColumn:     return u"속성"_s;
    default:             return QString();
    }
}

QVariant fileEntryData(const FileEntry &e, int column, int role, const DisplayFormat &format)
{
    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case NameColumn:     return e.fullName();
        case ExtColumn:      return e.ext;
        case TypeColumn:     return e.typeName;
        case SizeColumn:     return e.isDir() ? QString() : formatSize(e.size, format.sizeUnit);
        case ModifiedColumn: return formatDate(e.modified, format.dateFormat);
        case AttrColumn:     return e.isUp() ? QString() : attributeText(e.attributes);
        default:             return QVariant();
        }
    case Qt::ToolTipRole:
        return column == NameColumn || column == IconColumn ? QVariant(e.fullName()) : QVariant();
    case Qt::TextAlignmentRole:
        return column == SizeColumn ? int(Qt::AlignRight | Qt::AlignVCenter) : int(Qt::AlignLeft | Qt::AlignVCenter);
    case StemRole:       return e.stem;
    case ExtRole:        return e.ext;
    case FullNameRole:   return e.fullName();
    case KindRole:       return int(e.kind);
    case IsDirRole:      return e.isDir();
    case IsUpRole:       return e.isUp();
    case HiddenRole:     return e.isHidden();
    case MarkedRole:     return e.marked;
    case SizeBytesRole:  return e.size;
    case ModifiedRole:   return e.modified;
    case AttributesRole: return e.attributes;
    case TypeNameRole:   return e.typeName;
    case FilePathRole:   return e.path;
    case ArtRole:        return int(e.art);
    case ThumbnailRole:  return e.thumbnail;
    case AspectRole:     return e.aspect;
    case BadgeRole:      return e.badge;
    case NoSortRole:     return QVariant();
    case SizeTextRole:   return e.isDir() ? QString() : formatSize(e.size, format.sizeUnit);
    case DateTextRole:   return formatDay(e.modified, format.dateFormat);
    default:             return QVariant();
    }
}

FileListModel::FileListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

FileListModel::FileListModel(const QList<FileEntry> &entries, QObject *parent)
    : QAbstractTableModel(parent)
    , m_entries(entries)
{
}

void FileListModel::setEntries(const QList<FileEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void FileListModel::replaceEntries(int first, const QList<FileEntry> &entries)
{
    const int count = std::min<int>(int(entries.size()), int(m_entries.size()) - first);
    if (first < 0 || count <= 0)
        return;
    for (int i = 0; i < count; ++i)
        m_entries[first + i] = entries.at(i);
    Q_EMIT dataChanged(index(first, 0), index(first + count - 1, columnCount() - 1));
}

void FileListModel::setDisplayFormat(const DisplayFormat &format)
{
    if (m_format == format)
        return;
    m_format = format;
    if (!m_entries.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole, SizeTextRole, DateTextRole});
}

void FileListModel::setExtraColumns(const ExtraColumns &extra)
{
    if (m_extra == extra)
        return;
    beginResetModel();
    m_extra = extra;
    endResetModel();
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

int FileListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount + m_extra.count();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return QVariant();
    if (index.column() >= ColumnCount) {
        if (const std::optional<QVariant> v = m_extra.data(m_entries.at(index.row()), index.column(), role, m_format))
            return *v;
    }
    return fileEntryData(m_entries.at(index.row()), index.column(), role, m_format);
}

bool FileListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != MarkedRole || !index.isValid() || index.row() >= m_entries.size())
        return false;
    FileEntry &e = m_entries[index.row()];
    const bool marked = value.toBool() && !e.isUp();
    if (e.marked == marked)
        return true;
    e.marked = marked;
    // 레코드 전체(모든 열)를 다시 그리게 한다.
    Q_EMIT dataChanged(this->index(index.row(), 0), this->index(index.row(), columnCount() - 1), {MarkedRole});
    return true;
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return section >= ColumnCount ? m_extra.headerData(section, role) : QVariant(fileColumnTitle(section));
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags FileListModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

} // namespace fm::filelist
