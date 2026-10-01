#include "fmfilelist/ListColumns.h"

#include "fmfilelist/FileRoles.h"
#include "ListPainting_p.h"

namespace fm::filelist {

using namespace detail;

ListColumnLayout ListColumnLayout::standard()
{
    using R = ListColumn::Role;
    using L = ListColumn::TwoLine;
    ListColumnLayout layout;
    layout.columns = {
        {IconColumn, R::Icon, QString(), 16, kIconBand, Qt::AlignCenter, false, L::Row0Full},
        {NameColumn, R::Name, QString(), 0, 0, Qt::AlignLeft, true, L::Row0Full},
        {ExtColumn, R::Extension, QString(), kExtWidth, -1, Qt::AlignLeft, true, L::Hidden},
        {TypeColumn, R::Meta, QString(), kTypeWidth, -1, Qt::AlignLeft, false, L::Row1},
        {SizeColumn, R::Meta, QString(), kSizeWidth, -1, Qt::AlignRight, true, L::Row1, false, true},
        {ModifiedColumn, R::Meta, QString(), kDateWidth1, kDateWidth2, Qt::AlignLeft, true, L::Row1, false, true},
        {AttrColumn, R::Meta, QString(), kAttrWidth2, kAttrWidth2, Qt::AlignLeft, true, L::Row1, true, false},
        {FillerColumn, R::Filler, QString(), 0, 0, Qt::AlignLeft, false, L::Row1},
    };
    return layout;
}

const ListColumn *ListColumnLayout::find(int modelColumn) const
{
    for (const ListColumn &c : columns) {
        if (c.modelColumn == modelColumn)
            return &c;
    }
    return nullptr;
}

int ListColumnLayout::nameColumn() const
{
    for (const ListColumn &c : columns) {
        if (c.role == ListColumn::Role::Name)
            return c.modelColumn;
    }
    return 0;
}

int ListColumnLayout::requiredColumns() const
{
    int count = 0;
    for (const ListColumn &c : columns)
        count = std::max(count, c.modelColumn + 1);
    return count;
}

} // namespace fm::filelist
