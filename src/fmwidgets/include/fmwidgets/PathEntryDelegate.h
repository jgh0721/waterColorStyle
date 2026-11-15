#pragma once
#include <QStyledItemDelegate>
namespace fm::ui {
// The owner resolves paths asynchronously and supplies these roles. Painting
// never expands environment variables or touches the filesystem.
class PathEntryDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Role { ExpandedPathRole = Qt::UserRole + 710, PathStatusRole, PathWarningRole };
    enum Status { Pending, Exists, Missing, Unresolved };
    explicit PathEntryDelegate(QObject* Parent = nullptr);
    void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
    QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
};
}