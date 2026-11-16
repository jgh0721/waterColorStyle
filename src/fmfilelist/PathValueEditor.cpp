#include <fmfilelist/PathValueEditor.h>
#include <fmwidgets/PathEntryDelegate.h>
#include <fmwidgets/Label.h>
#include <QAbstractTableModel>
#include <QItemSelectionModel>
#include <QPointer>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#if FM_WITH_QTITAN
#include <QtitanGrid.h>
#endif

namespace fm::filelist {
class PathColumns final : public QAbstractTableModel
{
public:
    explicit PathColumns(QObject* Parent) : QAbstractTableModel(Parent) {}
    void SetSource(QAbstractItemModel* Model)
    {
        beginResetModel();
        if (source_) disconnect(source_, nullptr, this, nullptr);
        source_ = Model;
        endResetModel();
        if (!source_) return;
        connect(source_, &QAbstractItemModel::modelAboutToBeReset, this, [this] { beginResetModel(); });
        connect(source_, &QAbstractItemModel::modelReset, this, [this] { endResetModel(); });
        connect(source_, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](auto, int First, int Last) { beginInsertRows({}, First, Last); });
        connect(source_, &QAbstractItemModel::rowsInserted, this, [this] { endInsertRows(); ranksChanged(); });
        connect(source_, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](auto, int First, int Last) { beginRemoveRows({}, First, Last); });
        connect(source_, &QAbstractItemModel::rowsRemoved, this, [this] { endRemoveRows(); ranksChanged(); });
        connect(source_, &QAbstractItemModel::rowsAboutToBeMoved, this, [this](auto, int First, int Last, auto, int To) { beginMoveRows({}, First, Last, {}, To); });
        connect(source_, &QAbstractItemModel::rowsMoved, this, [this] { endMoveRows(); ranksChanged(); });
        connect(source_, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& First, const QModelIndex& Last) { emit dataChanged(index(First.row(), 0), index(Last.row(), 2)); });
        connect(source_, &QObject::destroyed, this, [this] { beginResetModel(); source_ = nullptr; endResetModel(); });
    }
    int rowCount(const QModelIndex& Parent = {}) const override { return !Parent.isValid() && source_ ? source_->rowCount() : 0; }
    int columnCount(const QModelIndex& = {}) const override { return 3; }
    QVariant data(const QModelIndex& Index, int Role) const override
    {
        if (!source_ || !Index.isValid()) return {};
        const auto Source = source_->index(Index.row(), 0);
        if (Role != Qt::DisplayRole) return Role == Qt::EditRole ? QVariant() : Source.data(Role);
        if (Index.column() == 0) return QStringLiteral("☰ %1").arg(Index.row() + 1);
        if (Index.column() == 1) return Source.data();
        const auto Warning = Source.data(fm::ui::PathEntryDelegate::PathWarningRole).toString();
        if (!Warning.isEmpty()) return Warning;
        return QStringList{tr("확인 중"), tr("있음"), tr("없음"), tr("미확인")}.value(Source.data(fm::ui::PathEntryDelegate::PathStatusRole).toInt());
    }
    Qt::ItemFlags flags(const QModelIndex& Index) const override { return Index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags; }
private:
    void ranksChanged() { if (rowCount()) emit dataChanged(index(0, 0), index(rowCount() - 1, 0)); }
    QPointer<QAbstractItemModel> source_;
};
struct PathValueEditor::Private
{
    QPointer<QAbstractItemModel> model;
    QItemSelectionModel* selection = nullptr;
    PathColumns* columns = nullptr;
    bool syncing = false, dragging = false;
    QPersistentModelIndex dragSource;
    QPoint press;
#if FM_WITH_QTITAN
    Qtitan::TreeGrid* grid = nullptr;
    Qtitan::GridTreeView* view = nullptr;
#endif
};
PathValueEditor::PathValueEditor(QWidget* Parent) : QWidget(Parent), d_(std::make_unique<Private>())
{
    auto* Layout = new QVBoxLayout(this); Layout->setContentsMargins(0, 0, 0, 0);
    d_->columns = new PathColumns(this);
#if FM_WITH_QTITAN
    d_->grid = new Qtitan::TreeGrid(this); d_->grid->setObjectName("fm.pathValueEditor.grid");
    d_->grid->setViewType(Qtitan::TreeGrid::TreeView); d_->view = d_->grid->view<Qtitan::GridTreeView>();
    auto* View = d_->view; View->options().setAutoCreateColumns(false); View->setModel(d_->columns);
    View->options().setGroupsHeader(false); View->tableOptions().setColumnsHeader(false);
    View->options().setGridLines(Qtitan::LinesNone); View->options().setDragEnabled(false); View->options().setDropEnabled(false);
    View->options().setFocusFollowsMouse(false); View->options().setKeepFocusedRow(true);
    View->tableOptions().setRowsQuickSelection(false); View->tableOptions().setColumnsQuickCustomization(false);
    View->tableOptions().setColumnsQuickMenuVisible(false); View->setFileManagerMode(true);
    for (int I = 0; I < 3; ++I) {
        auto* Column = static_cast<Qtitan::GridTableColumn*>(View->addColumn(I, QStringList{tr("순서"), tr("폴더"), tr("점검")}[I]));
        Column->setWidth(I == 0 ? 62 : I == 1 ? 320 : 130);
    }
    for (auto* Action : View->actions()) { Action->disconnect(); Action->setShortcut({}); }
    d_->grid->installEventFilter(this); d_->grid->viewport()->installEventFilter(this);
    d_->grid->viewport()->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(d_->grid->viewport()); Layout->addWidget(d_->grid);
    connect(View, &Qtitan::GridViewBase::focusRowChanged, this, [this] {
        if (d_->syncing || !d_->selection || !d_->model) return;
        const auto Row = d_->view->focusedRow();
        if (Row.isValid()) d_->selection->setCurrentIndex(d_->model->index(Row.modelIndex(0).row(), 0), QItemSelectionModel::ClearAndSelect);
    });
#else
    Layout->addWidget(new fm::ui::Label(tr("Qtitan TreeGrid가 필요합니다."), fm::ui::Label::Help, this));
    setEnabled(false);
#endif
}
PathValueEditor::~PathValueEditor() = default;
void PathValueEditor::setModel(QAbstractItemModel* Model)
{
    d_->syncing = true;
    delete d_->selection; d_->selection = nullptr; d_->model = Model;
    d_->columns->SetSource(Model);
    if (Model) {
        d_->selection = new QItemSelectionModel(Model, this);
        connect(d_->selection, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& Index) {
#if FM_WITH_QTITAN
            if (d_->syncing || !Index.isValid()) return;
            d_->syncing = true;
            const auto Row = d_->view->getRow(d_->columns->index(Index.row(), 0));
            if (Row.isValid()) { d_->view->setFocusedRowIndex(Row.rowIndex()); d_->view->deselectAll(); d_->view->selectRow(Row.rowIndex()); }
            d_->syncing = false;
#endif
        });
    }
    d_->syncing = false;
}
QModelIndex PathValueEditor::currentIndex() const { return d_->selection ? d_->selection->currentIndex() : QModelIndex(); }
QItemSelectionModel* PathValueEditor::selectionModel() const { return d_->selection; }
void PathValueEditor::setCurrentIndex(const QModelIndex& Index)
{
    if (d_->selection && Index.model() == d_->model) d_->selection->setCurrentIndex(Index, QItemSelectionModel::ClearAndSelect);
}
bool PathValueEditor::MoveRow(int From, int To)
{
    if (!d_->model || From < 0 || To < 0 || From >= d_->model->rowCount() || To >= d_->model->rowCount() || From == To) return false;
    d_->syncing = true;
    const bool Moved = d_->model->moveRows({}, From, 1, {}, To > From ? To + 1 : To);
    d_->syncing = false;
    if (Moved) {
        // Force the focus synchronization even when the persistent source cursor
        // already followed the moved row and QItemSelectionModel emits no change.
        d_->selection->clearCurrentIndex(); setCurrentIndex(d_->model->index(To, 0));
    }
    return Moved;
}
bool PathValueEditor::eventFilter(QObject* Watched, QEvent* Event)
{
#if FM_WITH_QTITAN
    if (Event->type() == QEvent::KeyPress || Event->type() == QEvent::ShortcutOverride) {
        auto* Key = static_cast<QKeyEvent*>(Event);
        if ((Key->key() == Qt::Key_Up || Key->key() == Qt::Key_Down) &&
            (Key->modifiers() == Qt::NoModifier || Key->modifiers() == Qt::ControlModifier)) {
            Key->accept(); if (Event->type() == QEvent::ShortcutOverride) return true;
            const int Row = currentIndex().row(), Next = Row + (Key->key() == Qt::Key_Up ? -1 : 1);
            if (Key->modifiers() == Qt::ControlModifier) MoveRow(Row, Next);
            else if (d_->model && Next >= 0 && Next < d_->model->rowCount()) setCurrentIndex(d_->model->index(Next, 0));
            return true;
        }
    }
    if (Watched == d_->grid->viewport() && Event->type() == QEvent::MouseButtonPress) {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        const auto Hit = d_->grid->hitInfo(Mouse->position().toPoint());
        if (Mouse->button() == Qt::LeftButton && Hit.columnIndex() == 0 && Hit.row().isValid() && d_->model) {
            d_->dragSource = d_->model->index(Hit.modelIndex().row(), 0); d_->press = Mouse->position().toPoint();
            setCurrentIndex(d_->dragSource); return true;
        }
    }
    if (Watched == d_->grid->viewport() && Event->type() == QEvent::MouseMove && d_->dragSource.isValid()) {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        if ((Mouse->position().toPoint() - d_->press).manhattanLength() >= QApplication::startDragDistance()) {
            d_->dragging = true; d_->grid->viewport()->setCursor(Qt::ClosedHandCursor);
        }
        return true;
    }
    if (Event->type() == QEvent::MouseButtonRelease && d_->dragSource.isValid()) {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        const auto Hit = d_->grid->hitInfo(d_->grid->viewport()->mapFromGlobal(Mouse->globalPosition().toPoint()));
        if (d_->dragging && Hit.row().isValid()) MoveRow(d_->dragSource.row(), Hit.modelIndex().row());
        d_->dragSource = {}; d_->dragging = false; d_->grid->viewport()->unsetCursor(); return true;
    }
    if (Event->type() == QEvent::Hide || Event->type() == QEvent::WindowDeactivate) {
        d_->dragSource = {}; d_->dragging = false; d_->grid->viewport()->unsetCursor();
    }
#endif
    return QWidget::eventFilter(Watched, Event);
}
}
