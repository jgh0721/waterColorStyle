#include "fmfilelist/FileSortProxy.h"

#include "fmfilelist/FileRoles.h"

#include <QDateTime>

namespace fm::filelist {

FileSortProxy::FileSortProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void FileSortProxy::setFoldersFirst(bool on)
{
    if (m_foldersFirst == on)
        return;
    m_foldersFirst = on;
    invalidate();
}

void FileSortProxy::setShowHidden(bool on)
{
    if (m_showHidden == on)
        return;
    beginFilterChange();
    m_showHidden = on;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void FileSortProxy::setShowSystem(bool on)
{
    if (m_showSystem == on)
        return;
    beginFilterChange();
    m_showSystem = on;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void FileSortProxy::setQuickFilter(const QString &text)
{
    if (m_quickFilter == text)
        return;
    beginFilterChange();
    m_quickFilter = text;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void FileSortProxy::markAll(bool marked)
{
    for (int r = 0; r < rowCount(); ++r) {
        const QModelIndex i = index(r, NameColumn);
        if (!i.data(IsUpRole).toBool() && i.data(MarkedRole).toBool() != marked)
            setData(i, marked, MarkedRole);
    }
}

void FileSortProxy::invertMarks()
{
    for (int r = 0; r < rowCount(); ++r) {
        const QModelIndex i = index(r, NameColumn);
        if (!i.data(IsUpRole).toBool())
            setData(i, !i.data(MarkedRole).toBool(), MarkedRole);
    }
}

int FileSortProxy::compareNames(const QModelIndex &left, const QModelIndex &right) const
{
    return m_collator.compare(left.data(FullNameRole).toString(), right.data(FullNameRole).toString());
}

bool FileSortProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // QSortFilterProxyModel은 내림차순에서 결과를 뒤집는다. 고정 순서(.. · 폴더 먼저)는 방향에 맞춰 미리 뒤집어 둔다.
    const bool ascending = sortOrder() == Qt::AscendingOrder;
    const bool leftUp = left.data(IsUpRole).toBool();
    const bool rightUp = right.data(IsUpRole).toBool();
    if (leftUp != rightUp)
        return ascending ? leftUp : rightUp;
    if (m_foldersFirst) {
        const bool leftDir = left.data(IsDirRole).toBool();
        const bool rightDir = right.data(IsDirRole).toBool();
        if (leftDir != rightDir)
            return ascending ? leftDir : rightDir;
    }

    int c = 0;
    switch (left.column()) {
    case SizeColumn: {
        const qint64 a = left.data(SizeBytesRole).toLongLong();
        const qint64 b = right.data(SizeBytesRole).toLongLong();
        c = a < b ? -1 : (a > b ? 1 : 0);
        break;
    }
    case ModifiedColumn: {
        const QDateTime a = left.data(ModifiedRole).toDateTime();
        const QDateTime b = right.data(ModifiedRole).toDateTime();
        c = a < b ? -1 : (a > b ? 1 : 0);
        break;
    }
    case ExtColumn:
        c = m_collator.compare(left.data(ExtRole).toString(), right.data(ExtRole).toString());
        break;
    case TypeColumn:
        c = m_collator.compare(left.data(TypeNameRole).toString(), right.data(TypeNameRole).toString());
        break;
    case AttrColumn:
        c = left.data(AttributesRole).toInt() - right.data(AttributesRole).toInt();
        break;
    default:
        break;
    }
    if (c == 0)
        c = compareNames(left, right);
    if (c == 0)
        return left.row() < right.row();
    return c < 0;
}

bool FileSortProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex i = sourceModel()->index(sourceRow, NameColumn, sourceParent);
    if (i.data(IsUpRole).toBool())
        return true;
    if (!m_showHidden && i.data(HiddenRole).toBool())
        return false;
    if (!m_showSystem && (i.data(AttributesRole).toInt() & System))
        return false;
    if (!m_quickFilter.isEmpty() && !i.data(FullNameRole).toString().contains(m_quickFilter, Qt::CaseInsensitive))
        return false;
    return true;
}

} // namespace fm::filelist
