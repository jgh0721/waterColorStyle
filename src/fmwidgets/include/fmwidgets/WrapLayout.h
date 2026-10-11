#pragma once
#include <QLayout>
#include <QList>
#include <QStyle>
#include <QWidget>
#include <algorithm>

namespace fm::ui {
// Keep native widget sizes and keyboard order when a control row becomes narrow.
// Hidden interaction choices must reserve neither width nor spacing.
class WrapLayout final : public QLayout
{
public:
    explicit WrapLayout(QWidget* Parent = nullptr) : QLayout(Parent)
    {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(8);
    }
    ~WrapLayout() override
    {
        while (auto* Item = takeAt(0)) delete Item;
    }
    void addItem(QLayoutItem* Item) override { items_.append(Item); invalidate(); }
    int count() const override { return static_cast<int>(items_.size()); }
    QLayoutItem* itemAt(int Index) const override { return items_.value(Index); }
    QLayoutItem* takeAt(int Index) override
    {
        if (Index < 0 || Index >= count()) return nullptr;
        auto* Item = items_.takeAt(Index); invalidate(); return Item;
    }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int Width) const override { return arrange(QRect(0, 0, Width, 0), false); }
    QSize minimumSize() const override
    {
        QSize Size;
        for (auto* Item : items_) if (!Item->isEmpty()) Size = Size.expandedTo(preferredSize(Item));
        const auto Margins = contentsMargins();
        return Size + QSize(Margins.left() + Margins.right(), Margins.top() + Margins.bottom());
    }
    QSize sizeHint() const override { return minimumSize(); }
    void setGeometry(const QRect& Rect) override { QLayout::setGeometry(Rect); arrange(Rect, true); }
private:
    QSize preferredSize(QLayoutItem* Item) const
    {
        return Item->sizeHint().expandedTo(Item->minimumSize()).boundedTo(Item->maximumSize());
    }
    int arrange(const QRect& Rect, bool Apply) const
    {
        const auto Margins = contentsMargins();
        const auto Area = Rect.marginsRemoved(Margins);
        const int Gap = std::max(0, spacing());
        int Used = 0, Top = Area.top(), RowHeight = 0;
        for (auto* Item : items_) {
            if (Item->isEmpty()) continue;
            const auto Size = preferredSize(Item);
            if (Used && Used + Gap + Size.width() > Area.width()) {
                Top += RowHeight + Gap; Used = 0; RowHeight = 0;
            }
            if (Used) Used += Gap;
            if (Apply) {
                const QRect Position(Area.left() + Used, Top, Size.width(), Size.height());
                Item->setGeometry(QStyle::visualRect(parentWidget() ? parentWidget()->layoutDirection() : Qt::LeftToRight, Area, Position));
            }
            Used += Size.width(); RowHeight = std::max(RowHeight, Size.height());
        }
        return Margins.top() + Top - Area.top() + RowHeight + Margins.bottom();
    }
    QList<QLayoutItem*> items_;
};
} // namespace fm::ui
