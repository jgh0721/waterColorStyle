#include "fmfilelist/FileListStats.h"

#include "fmfilelist/FileRoles.h"

#include <QAbstractItemModel>

using namespace Qt::StringLiterals;

namespace fm::filelist {

FileListStats FileListStats::compute(const QAbstractItemModel *model)
{
    FileListStats s;
    if (!model)
        return s;
    for (int r = 0; r < model->rowCount(); ++r) {
        const QModelIndex i = model->index(r, NameColumn);
        if (i.data(IsUpRole).toBool())
            continue;
        const bool marked = i.data(MarkedRole).toBool();
        if (i.data(IsDirRole).toBool()) {
            ++s.folders;
            if (marked)
                ++s.markedFolders;
            continue;
        }
        const qint64 size = std::max<qint64>(0, i.data(SizeBytesRole).toLongLong());
        ++s.files;
        s.bytes += size;
        if (marked) {
            ++s.markedFiles;
            s.markedBytes += size;
        }
        if (i.data(HiddenRole).toBool())
            ++s.hiddenFiles;
    }
    return s;
}

QString FileListStats::selectionText() const
{
    return u"파일 %1 / %2개 선택 · %3 / %4"_s.arg(markedFiles).arg(files).arg(formatSize(markedBytes), formatSize(bytes));
}

QString FileListStats::secondaryText() const
{
    if (hiddenFiles > 0)
        return u"숨김 %1개 표시 중"_s.arg(hiddenFiles);
    return u"폴더 %1개"_s.arg(folders);
}

} // namespace fm::filelist
