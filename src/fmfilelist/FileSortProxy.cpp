#include "fmfilelist/FileSortProxy.h"

#include "fmfilelist/FileGroups.h"
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

void FileSortProxy::setGroupMatcher(std::shared_ptr<const FileGroupMatcher> matcher)
{
    m_groups = std::move(matcher);
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {GroupStyleRole});
}

QVariant FileSortProxy::data(const QModelIndex &index, int role) const
{
    if (role != GroupStyleRole)
        return QSortFilterProxyModel::data(index, role);
    if (!m_groups || m_groups->isEmpty() || index.data(IsUpRole).toBool())
        return QVariant();
    FileFacts facts;
    facts.name = index.data(FullNameRole).toString();
    facts.ext = index.data(ExtRole).toString();
    facts.attributes = index.data(AttributesRole).toInt();
    facts.size = index.data(SizeBytesRole).toLongLong();
    facts.modified = index.data(ModifiedRole).toDateTime();
    facts.isDir = index.data(IsDirRole).toBool();
    const ResolvedGroupStyle style = m_groups->resolve(facts);
    return style.isEmpty() ? QVariant() : QVariant::fromValue(style);
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
        if (left.column() >= ColumnCount)  // 열 세트의 추가 열 — 표시 글자를 자연 정렬(숫자는 숫자 크기로)
            c = m_collator.compare(left.data().toString(), right.data().toString());
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
