#pragma once
#include <QWidget>
#include <QModelIndex>
#include <memory>
class QAbstractItemModel;
class QItemSelectionModel;
namespace fm::filelist {
// A three-column ordered list backed by the owner's model. Every reordering
// gesture calls that model's moveRows; the widget never keeps a second value list.
class PathValueEditor final : public QWidget
{
    Q_OBJECT
public:
    explicit PathValueEditor(QWidget* Parent = nullptr);
    ~PathValueEditor() override;
    void setModel(QAbstractItemModel* Model);
    QModelIndex currentIndex() const;
    void setCurrentIndex(const QModelIndex& Index);
    QItemSelectionModel* selectionModel() const;
    bool MoveRow(int From, int To);
protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
private:
    struct Private;
    std::unique_ptr<Private> d_;
};
}
