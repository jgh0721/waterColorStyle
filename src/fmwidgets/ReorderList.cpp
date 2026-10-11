#include "fmwidgets/ReorderList.h"

#include <QApplication>
#include <fmstyle/ThemeManager.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>

namespace fm::ui {
namespace {
constexpr int GripWidth = 30;
QRect GripRect(const QRect& Row, Qt::LayoutDirection Direction)
{
    return QRect(Direction == Qt::RightToLeft ? Row.right() - GripWidth + 1 : Row.left(),
        Row.top(), GripWidth, Row.height());
}
QStyleOptionViewItem ContentOption(QStyleOptionViewItem Option)
{
    if (Option.direction == Qt::RightToLeft) Option.rect.adjust(0, 0, -GripWidth, 0);
    else Option.rect.adjust(GripWidth, 0, 0, 0);
    return Option;
}
class GripDelegate final : public QStyledItemDelegate
{
public:
    explicit GripDelegate(QObject* Parent) : QStyledItemDelegate(Parent) {}
    QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override
    {
        const auto Size = QStyledItemDelegate::sizeHint(Option, Index);
        return {Size.width() + GripWidth, qMax(Size.height() + 8, 34)};
    }
    void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override
    {
        auto Background = Option; initStyleOption(&Background, Index);
        Background.text.clear(); Background.icon = {}; Background.features = {};
        const auto* Style = Option.widget ? Option.widget->style() : QApplication::style();
        Style->drawControl(QStyle::CE_ItemViewItem, &Background, Painter, Option.widget);
        QStyledItemDelegate::paint(Painter, ContentOption(Option), Index);
        const auto Grip = GripRect(Option.rect, Option.direction);
        Painter->save();
        Painter->setPen(QPen(Option.palette.color(!(Option.state & QStyle::State_Enabled) ? QPalette::Disabled : (Option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive),
            Option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text), 1.5));
        for (int Offset : {-4, 0, 4}) Painter->drawLine(Grip.center().x() - 6, Grip.center().y() + Offset, Grip.center().x() + 6, Grip.center().y() + Offset);
        Painter->restore();
    }
    bool editorEvent(QEvent* Event, QAbstractItemModel* Model, const QStyleOptionViewItem& Option, const QModelIndex& Index) override
    {
        return QStyledItemDelegate::editorEvent(Event, Model, ContentOption(Option), Index);
    }
};

class DragOverlay final : public QWidget
{
public:
    explicit DragOverlay(QWidget* Parent) : QWidget(Parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }
    QPixmap ghost_;
    QPoint position_;
    int line_ = 0;
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter Painter(this);
        Painter.setOpacity(0.82); Painter.drawPixmap(position_, ghost_); Painter.setOpacity(1);
        Painter.setPen(QPen(fm::style::themeColorsFor(this)[fm::style::Token::Accent], 3));
        Painter.drawLine(2, line_, width() - 3, line_);
    }
};
}

class ReorderListInteraction final : public QObject
{
public:
    explicit ReorderListInteraction(QListView* View) : QObject(View), view_(View)
    {
        View->setViewMode(QListView::ListMode);
        View->setFlow(QListView::TopToBottom);
        View->setWrapping(false);
        View->setSelectionMode(QAbstractItemView::SingleSelection);
        View->setDragDropMode(QAbstractItemView::NoDragDrop);
        View->setEditTriggers(QAbstractItemView::NoEditTriggers);
        View->setMouseTracking(true); View->viewport()->setMouseTracking(true);
        View->setItemDelegate(new GripDelegate(View));
        View->setToolTip(tr("끌기 핸들로 순서 변경 · Alt+↑/↓ · Esc로 끌기 취소"));
        View->setAccessibleDescription(View->toolTip());
        View->installEventFilter(this); View->viewport()->installEventFilter(this);
        overlay_ = new DragOverlay(View->viewport());
        timer_.setInterval(40);
        connect(&timer_, &QTimer::timeout, this, [this] {
            if (!dragging_ || !source_.isValid()) { cancel(); return; }
            const int Margin = qMax(24, view_->fontMetrics().height());
            const int Delta = pointer_.y() < Margin ? -10 : pointer_.y() >= view_->viewport()->height() - Margin ? 10 : 0;
            if (Delta) view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->value() + Delta);
            updateTarget();
        });
        BindModel();
    }
    ~ReorderListInteraction() override { cancel(); delete overlay_; }
    void BindModel()
    {
        for (const auto& Connection : modelConnections_) disconnect(Connection);
        modelConnections_.clear(); cancel();
        if (!view_->model()) return;
        modelConnections_.append(connect(view_->model(), &QAbstractItemModel::modelAboutToBeReset, this, [this] { cancel(); }));
        modelConnections_.append(connect(view_->model(), &QAbstractItemModel::rowsAboutToBeRemoved, this, [this] { cancel(); }));
        modelConnections_.append(connect(view_->model(), &QAbstractItemModel::layoutAboutToBeChanged, this, [this] { cancel(); }));
    }
    QRect HandleRect(int Row) const
    {
        if (!view_->model()) return {};
        const auto Index = view_->model()->index(Row, view_->modelColumn(), view_->rootIndex());
        return Index.isValid() ? GripRect(view_->visualRect(Index), view_->layoutDirection()) : QRect{};
    }
    bool MoveRow(int From, int To)
    {
        auto* Model = view_->model(); const auto Root = view_->rootIndex();
        if (!Model || From < 0 || To < 0 || From == To || From >= Model->rowCount(Root) || To >= Model->rowCount(Root)) return false;
        const QPersistentModelIndex Selected(Model->index(From, view_->modelColumn(), Root));
        // Never rebuild rows or copy their data: moveRows retains item identity,
        // checks, user roles and persistent selection, including QListWidget items.
        if (!Model->moveRows(Root, From, 1, Root, To > From ? To + 1 : To)) return false;
        view_->setCurrentIndex(Selected); view_->scrollTo(Selected);
        return true;
    }
protected:
    bool eventFilter(QObject* Object, QEvent* Event) override
    {
        if (Event->type() == QEvent::Hide || Event->type() == QEvent::WindowDeactivate || Event->type() == QEvent::UngrabMouse) cancel();
        if (Object == view_ && Event->type() == QEvent::KeyPress) {
            auto* Key = static_cast<QKeyEvent*>(Event);
            if (Key->key() == Qt::Key_Escape && source_.isValid()) { cancel(); return true; }
            if (Key->modifiers() == Qt::AltModifier && (Key->key() == Qt::Key_Up || Key->key() == Qt::Key_Down)) {
                cancel(); MoveRow(view_->currentIndex().row(), view_->currentIndex().row() + (Key->key() == Qt::Key_Up ? -1 : 1)); return true;
            }
        }
        if (Object != view_->viewport()) return false;
        if (Event->type() == QEvent::Resize) overlay_->setGeometry(view_->viewport()->rect());
        if (Event->type() == QEvent::MouseButtonPress) {
            auto* Mouse = static_cast<QMouseEvent*>(Event); const auto Index = view_->indexAt(Mouse->position().toPoint());
            if (Mouse->button() == Qt::LeftButton && Index.isValid() && (Index.flags() & Qt::ItemIsEnabled) && HandleRect(Index.row()).contains(Mouse->position().toPoint())) {
                view_->setFocus(); view_->setCurrentIndex(Index); source_ = Index;
                start_ = pointer_ = Mouse->position().toPoint(); return true;
            }
        }
        if (Event->type() == QEvent::MouseMove) {
            auto* Mouse = static_cast<QMouseEvent*>(Event); pointer_ = Mouse->position().toPoint();
            if (source_.isValid() && (Mouse->buttons() & Qt::LeftButton)) {
                if (!dragging_ && (pointer_ - start_).manhattanLength() >= QApplication::startDragDistance()) {
                    const auto Row = view_->visualRect(source_);
                    overlay_->ghost_ = view_->viewport()->grab(Row);
                    dragging_ = true; overlay_->setGeometry(view_->viewport()->rect()); overlay_->show(); overlay_->raise(); timer_.start();
                }
                if (dragging_) updateTarget();
                view_->viewport()->setCursor(Qt::ClosedHandCursor); return true;
            }
            const auto Index = view_->indexAt(pointer_);
            view_->viewport()->setCursor(Index.isValid() && HandleRect(Index.row()).contains(pointer_) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
        if (Event->type() == QEvent::MouseButtonRelease) {
            auto* Mouse = static_cast<QMouseEvent*>(Event);
            if (Mouse->button() == Qt::LeftButton && source_.isValid()) {
                pointer_ = Mouse->position().toPoint(); if (dragging_) updateTarget();
                const int From = source_.row(), To = insertion_ > From ? insertion_ - 1 : insertion_;
                const bool Commit = dragging_ && view_->viewport()->rect().contains(pointer_);
                cancel(); if (Commit) MoveRow(From, To); return true;
            }
        }
        return false;
    }
private:
    void updateTarget()
    {
        if (!source_.isValid() || !view_->model()) { cancel(); return; }
        const int Count = view_->model()->rowCount(view_->rootIndex());
        const QPoint Bounded(qBound(0, pointer_.x(), view_->viewport()->width() - 1), qBound(0, pointer_.y(), view_->viewport()->height() - 1));
        const auto Hover = view_->indexAt(Bounded);
        if (Hover.isValid()) {
            const auto Bounds = view_->visualRect(Hover);
            insertion_ = Hover.row() + (Bounded.y() >= Bounds.center().y() ? 1 : 0);
        } else {
            const auto First = view_->visualRect(view_->model()->index(0, view_->modelColumn(), view_->rootIndex()));
            insertion_ = Bounded.y() <= First.top() ? 0 : Count;
        }
        const auto Anchor = view_->model()->index(qMin(insertion_, Count - 1), view_->modelColumn(), view_->rootIndex());
        const auto Bounds = view_->visualRect(Anchor);
        overlay_->line_ = qBound(2, insertion_ == Count ? Bounds.bottom() + 1 : Bounds.top(), view_->viewport()->height() - 2);
        overlay_->position_ = QPoint(0, qBound(0, pointer_.y() - qRound(overlay_->ghost_.deviceIndependentSize().height()) / 2, qMax(0, view_->viewport()->height() - qRound(overlay_->ghost_.deviceIndependentSize().height()))));
        overlay_->update();
    }
    void cancel()
    {
        timer_.stop(); dragging_ = false; source_ = QPersistentModelIndex{};
        if (overlay_) overlay_->hide();
        view_->viewport()->unsetCursor();
    }
    QListView* view_;
    DragOverlay* overlay_ = nullptr;
    QTimer timer_;
    QList<QMetaObject::Connection> modelConnections_;
    QPersistentModelIndex source_;
    QPoint start_, pointer_;
    int insertion_ = 0;
    bool dragging_ = false;
};

ReorderListView::ReorderListView(QWidget* Parent) : QListView(Parent), interaction_(new ReorderListInteraction(this)) {}
ReorderListView::~ReorderListView() { delete interaction_; }
void ReorderListView::setModel(QAbstractItemModel* Model) { QListView::setModel(Model); if (interaction_) interaction_->BindModel(); }
QRect ReorderListView::HandleRect(int Row) const { return interaction_->HandleRect(Row); }
bool ReorderListView::MoveRow(int From, int To) { return interaction_->MoveRow(From, To); }
ReorderListWidget::ReorderListWidget(QWidget* Parent) : QListWidget(Parent), interaction_(new ReorderListInteraction(this)) { setSortingEnabled(false); }
ReorderListWidget::~ReorderListWidget() { delete interaction_; }
QRect ReorderListWidget::HandleRect(int Row) const { return interaction_->HandleRect(Row); }
bool ReorderListWidget::MoveRow(int From, int To) { return interaction_->MoveRow(From, To); }
}
