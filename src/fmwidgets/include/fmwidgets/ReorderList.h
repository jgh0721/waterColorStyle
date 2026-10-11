#pragma once

#include <QListView>
#include <QListWidget>

namespace fm::ui {
class ReorderListInteraction;

// Vertical single-selection lists with a visible grip. Dragging the grip shows
// a floating row and insertion line; only release commits a move. Alt+Up/Down
// provides the same operation without a mouse. Models must implement moveRows.
class ReorderListView : public QListView
{
    Q_OBJECT
public:
    explicit ReorderListView(QWidget* Parent = nullptr);
    ~ReorderListView() override;
    QRect HandleRect(int Row) const;
    bool MoveRow(int From, int To);
    void setModel(QAbstractItemModel* Model) override;
private:
    ReorderListInteraction* interaction_ = nullptr;
};

class ReorderListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit ReorderListWidget(QWidget* Parent = nullptr);
    ~ReorderListWidget() override;
    QRect HandleRect(int Row) const;
    bool MoveRow(int From, int To);
private:
    ReorderListInteraction* interaction_ = nullptr;
};
}
