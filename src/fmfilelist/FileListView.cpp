#include "fmfilelist/FileListView.h"

#include "fmfilelist/FileIconPainter.h"
#include "fmfilelist/FileRoles.h"
#include "ListPainting_p.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QtnGrid.h>
#include <QtnGridBandedTableView.h>

#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::filelist {

using namespace detail;
using fm::style::Token;

// ---------------------------------------------------------------- 레코드 그리기 (패치 Q3)
class FileRecordPainter final : public Qtitan::GridRecordPainter
{
public:
    explicit FileRecordPainter(FileListViewPrivate *d) : m_d(d) {}
    void paintBackground(QPainter &p, const Record &record, QWidget *widget) override;
    void paintOverlay(QPainter &p, const Record &record, QWidget *widget) override;

private:
    FileListViewPrivate *m_d;
};

// ---------------------------------------------------------------- 셀 글자 (DelegateAdapter)
class FileCellDelegate final : public QStyledItemDelegate
{
public:
    FileCellDelegate(FileListViewPrivate *d, QObject *parent) : QStyledItemDelegate(parent), m_d(d) {}
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    FileListViewPrivate *m_d;
};

class FileListViewPrivate
{
public:
    explicit FileListViewPrivate(FileListView *q) : q(q) {}

    const fm::style::ThemeColors &colors() const { return fm::style::themeColorsFor(q); }
    Qtitan::GridBandedTableColumn *column(int modelColumn) const
    {
        return static_cast<Qtitan::GridBandedTableColumn *>(view->getColumnByModelColumn(modelColumn));
    }
    RecordState stateFor(const QModelIndex &index, bool cursor) const
    {
        RecordState s;
        s.active = paneActive(q);
        s.marked = index.data(MarkedRole).toBool();
        s.hidden = index.data(HiddenRole).toBool();
        s.cursor = cursor;
        return s;
    }

    void setupGrid();
    void setupColumns();
    bool ensureColumns();
    void applyLayout();
    void applyTheme();
    void scheduleAuto();
    void evaluateAuto();
    void syncSortFromGrid();
    void moveCursor(int delta);
    void markRange(int row, bool marked);

    FileListView *q;
    Qtitan::Grid *grid = nullptr;
    Qtitan::GridBandedTableView *view = nullptr;
    QAbstractItemModel *model = nullptr;
    std::unique_ptr<FileRecordPainter> recordPainter;
    FileCellDelegate *delegate = nullptr;
    QTimer *autoTimer = nullptr;
    ViewMode mode = ViewMode::Auto;
    bool twoLine = false;
    bool preview = false;
    ListAppearance appearance;
    RecordGeometry geometry;
    int sortColumn = -1;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool syncingSort = false;
    bool applying = false;
    bool columnsReady = false;
};

void FileRecordPainter::paintBackground(QPainter &p, const Record &record, QWidget *)
{
    const RecordState s = m_d->stateFor(record.index, record.state & QStyle::State_HasFocus);
    paintRecordBackground(&p, record.rect, m_d->geometry, m_d->appearance, s, record.alternate, m_d->colors());
}

void FileRecordPainter::paintOverlay(QPainter &p, const Record &record, QWidget *)
{
    if (!(record.state & QStyle::State_HasFocus))
        return;
    const RecordState s = m_d->stateFor(record.index, true);
    paintRecordCursor(&p, record.rect, m_d->geometry, m_d->appearance, s, m_d->colors());
}

void FileCellDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // DelegateAdapter는 내부 대리 모델 인덱스를 넘긴다. 원래 모델 인덱스는 QueryIndexRole로 얻는다.
    const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
    const QModelIndex idx = source.isValid() ? source : index;
    const int column = idx.column();
    if (column == FillerColumn)
        return;

    const fm::style::ThemeColors &tc = m_d->colors();
    const RecordGeometry &g = m_d->geometry;
    const ListAppearance &a = m_d->appearance;
    const RecordState s = m_d->stateFor(idx, idx.row() == m_d->q->cursorRow());
    const TextColors c = textColors(tc, a, s, g.twoLine, g.separator);
    const QRect cell = option.rect;

    // 셀이 속한 레코드 사각형 — 2줄에서 줄마다 셀이 따로 온다(아이콘은 두 줄 걸침).
    QRect record = cell;
    if (g.twoLine && column != IconColumn) {
        const bool onNameLine = column == NameColumn;
        const int line = onNameLine == g.nameBelow ? 1 : 0;
        record = QRect(cell.left(), cell.top() - line * g.cellHeight(), cell.width(), g.pitch);
    }

    const QFont base = m_d->q->font();
    const QFont metaFont = fm::style::pixelFont(base, 12);
    p->save();

    if (column == IconColumn) {
        const QRectF block = g.block(record);
        const qreal centerY = block.top() + g.padTop + (g.nameHeight + g.metaHeight) / 2.0;
        const Kind kind = Kind(idx.data(KindRole).toInt());
        FileIconPainter::paint(p, QRectF(cell.center().x() + 0.5 - 10, centerY - 10, 20, 20), kind, c.iconLine, tc,
                               s.hidden ? 0.6 : 1.0);
        p->restore();
        return;
    }

    if (column == NameColumn) {
        QFont font = base;
        if (c.bold)
            font.setWeight(QFont::DemiBold);
        p->setFont(font);
        const QFontMetrics fm(font);
        const QRectF line = g.nameLine(record);
        const Kind kind = Kind(idx.data(KindRole).toInt());
        const QString stem = idx.data(StemRole).toString();
        if (!g.twoLine) {
            // 1줄: 행 여백 8 + 칸 여백 8, 아이콘 16 + 간격 6 + stem(확장자는 확장자 열에)
            const qreal left = cell.left() + kRowGutter + kCellPad + g.sideMargin;
            FileIconPainter::paint(p, QRectF(left, line.center().y() - 8, 16, 16), kind, c.iconLine, tc,
                                   s.hidden ? 0.6 : 1.0);
            const qreal textLeft = left + 22;
            const int width = int(cell.right() + 1 - kCellPad - textLeft);
            const Qt::TextElideMode mode = a.nameElide == NameElide::End ? Qt::ElideRight : Qt::ElideMiddle;
            p->setPen(c.name);
            p->drawText(QRectF(textLeft, line.top(), width, line.height()), Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(stem, mode, width));
        } else {
            // 2줄: stem(--fg) + ".ext"(--fg3, 줄이지 않음). 넘치면 stem 가운데를 줄인다.
            const qreal left = cell.left() + kCellPad;
            const int width = int(cell.right() + 1 - kRowGutter - kCellPad - g.sideMargin - left);
            const QString ext = idx.data(ExtRole).toString();
            const QString dotExt = ext.isEmpty() ? QString() : u'.' + ext;
            QString shownStem = stem;
            QString shownExt = dotExt;
            if (fm.horizontalAdvance(stem + dotExt) > width) {
                switch (a.nameElide) {
                case NameElide::MiddleKeepExtension:
                    shownStem = fm.elidedText(stem, Qt::ElideMiddle, std::max(0, width - fm.horizontalAdvance(dotExt)));
                    break;
                case NameElide::End:
                    shownStem = fm.elidedText(stem + dotExt, Qt::ElideRight, width);
                    shownExt.clear();
                    break;
                case NameElide::Middle:
                    shownStem = fm.elidedText(stem + dotExt, Qt::ElideMiddle, width);
                    shownExt.clear();
                    break;
                }
            }
            const int stemWidth = fm.horizontalAdvance(shownStem);
            p->setPen(c.name);
            p->drawText(QRectF(left, line.top(), stemWidth + 1, line.height()), Qt::AlignLeft | Qt::AlignVCenter, shownStem);
            if (!shownExt.isEmpty()) {
                p->setPen(s.hidden && c.ext == tc[Token::Fg3] ? tc[Token::Fg3] : c.ext);
                p->drawText(QRectF(left + stemWidth, line.top(), fm.horizontalAdvance(shownExt) + 1, line.height()),
                            Qt::AlignLeft | Qt::AlignVCenter, shownExt);
            }
        }
        p->restore();
        return;
    }

    // 메타 칸: 12 px, 크기 · 날짜 숫자 폭 고정, 속성 고정폭
    QFont font = metaFont;
    if (column == SizeColumn || column == ModifiedColumn)
        font = fm::style::withTabularNumbers(font);
    else if (column == AttrColumn)
        font = fm::style::monoFont(12);
    p->setFont(font);
    QString text = idx.data(Qt::DisplayRole).toString();
    if (column == SizeColumn && !g.twoLine && idx.data(IsDirRole).toBool() && !idx.data(IsUpRole).toBool())
        text = u"폴더"_s;
    const QRectF line = g.metaLine(record);
    const qreal left = cell.left() + kCellPad;
    const qreal right = cell.right() + 1 - kCellPad - (!g.twoLine && column == AttrColumn ? kRowGutter + g.sideMargin : 0);
    const Qt::Alignment align = (column == SizeColumn ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
    p->setPen(c.meta);
    const int width = int(right - left);
    p->drawText(QRectF(left, line.top(), width, line.height()), int(align),
                QFontMetrics(font).elidedText(text, Qt::ElideRight, width));
    p->restore();
}

// ---------------------------------------------------------------- FileListViewPrivate

void FileListViewPrivate::setupGrid()
{
    grid = new Qtitan::Grid(q);
    grid->setViewType(Qtitan::Grid::BandedTableView);
    view = grid->view<Qtitan::GridBandedTableView>();
    recordPainter = std::make_unique<FileRecordPainter>(this);
    delegate = new FileCellDelegate(this, q);

    view->beginUpdate();
    Qtitan::GridViewOptions &o = view->options();
    o.setGroupsHeader(false);
    o.setMainMenuDisabled(true);
    o.setGridLines(Qtitan::LinesNone);
    o.setSelectionPolicy(Qtitan::GridViewOptions::IgnoreSelection);
    o.setAlternatingRowColors(true);
    o.setFocusFrameEnabled(false);
    o.setColumnHidingEnabled(false);
    o.setColumnMovingEnabled(false);
    o.setFilterEnabled(false);
    o.setFindEnabled(false);
    o.setFieldChooserEnabled(false);
    o.setZoomEnabled(false);
    o.setDragEnabled(false);
    o.setDropEnabled(false);
    o.setShowWaitCursor(false);
    o.setSortIndicatorStyled(true);  // 패치 Q5
    Qtitan::GridTableViewOptions &t = view->tableOptions();
    t.setColumnAutoWidth(true);
    t.setRowsQuickSelection(false);
    t.setColumnsQuickCustomization(false);
    t.setColumnsQuickMenuVisible(false);
    t.setFrozenPlaceQuickSelection(false);
    view->bandedOptions().setBandsHeader(false);
    view->bandedOptions().setBandsQuickCustomization(false);
    view->setRecordPainter(recordPainter.get());
    view->endUpdate();

    QObject::connect(view, &Qtitan::GridViewBase::focusRowChanged, q, [this](int, int row) {
        Q_EMIT q->cursorRowChanged(row);
    });
    QObject::connect(view, &Qtitan::GridViewBase::rowClicked, q, [this](Qtitan::RowClickEventArgs *) {
        Q_EMIT q->paneActivated();
    });
    QObject::connect(view, &Qtitan::GridViewBase::rowDblClicked, q, [this](Qtitan::RowClickEventArgs *args) {
        Q_EMIT q->activated(args->row().modelIndex(NameColumn));
    });
    QObject::connect(view, &Qtitan::GridViewBase::sortingChanged, q, [this] { syncSortFromGrid(); });

    grid->installEventFilter(q);
}

// 모델의 열이 아직 없을 수 있다(원본이 없는 프록시). 열이 생기면 Qtitan 열을 다시 만들고 설정한다.
bool FileListViewPrivate::ensureColumns()
{
    if (!model || model->columnCount() < ColumnCount)
        return false;
    if (columnsReady && view->getColumnCount() >= ColumnCount && column(FillerColumn))
        return true;
    view->beginUpdate();
    view->setModel(model);
    view->endUpdate();
    setupColumns();
    columnsReady = true;
    return true;
}

void FileListViewPrivate::setupColumns()
{
    view->beginUpdate();
    for (int c = 0; c < ColumnCount; ++c) {
        auto *col = column(c);
        if (!col)
            continue;
        col->setCaption(model ? model->headerData(c, Qt::Horizontal).toString() : QString());
        col->setTextAlignment((c == SizeColumn ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        col->setEditorType(Qtitan::GridEditor::DelegateAdapter);
        static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(col->editorRepository())->setDelegate(delegate);
        col->editorRepository()->setEditable(false);
        // Qtitan 정렬은 모델 순서를 그대로 둔다 — 실제 정렬은 FileSortProxy가 한다.
        col->dataBinding()->setSortRole(Qt::ItemDataRole(NoSortRole));
        col->setSortEnabled(c != IconColumn && c != FillerColumn);
        col->setMenuButtonVisible(false);
        col->setHidingEnabled(false);
        col->setMovingEnabled(false);
    }
    view->endUpdate();
}

void FileListViewPrivate::applyLayout()
{
    if (!model || applying || !ensureColumns())
        return;
    applying = true;
    geometry = RecordGeometry::make(colors().isWatercolor(), twoLine, appearance);
    const RecordGeometry &g = geometry;

    view->beginUpdate();
    view->removeBands();
    Qtitan::GridTableBand *iconBand = view->addBand(QString());
    Qtitan::GridTableBand *fileBand = view->addBand(u"파일"_s);
    iconBand->setVisible(g.twoLine);

    auto place = [&](int c, Qtitan::GridTableBand *band, int row, int span, int width, bool visible) {
        auto *col = column(c);
        col->setBandIndex(band->index());
        col->setRowIndex(row);
        col->setRowSpan(span);
        if (width > 0) {
            col->setWidth(width);
            col->setMinWidth(width);
            col->setMaxWidth(width);
        } else {
            col->setMinWidth(24);
            col->setMaxWidth(100000);
        }
        col->setVisible(visible);
    };
    const int nameRow = g.nameBelow ? 1 : 0;
    const int metaRow = g.twoLine ? 1 - nameRow : 0;
    if (!g.twoLine) {
        place(IconColumn, fileBand, 0, 1, 16, false);
        place(NameColumn, fileBand, 0, 1, 0, true);
        place(ExtColumn, fileBand, 0, 1, kExtWidth, true);
        place(TypeColumn, fileBand, 0, 1, kTypeWidth, false);
        place(SizeColumn, fileBand, 0, 1, kSizeWidth, true);
        place(ModifiedColumn, fileBand, 0, 1, kDateWidth1, true);
        place(AttrColumn, fileBand, 0, 1, kAttrWidth1, true);
        place(FillerColumn, fileBand, 0, 1, 0, false);
        column(NameColumn)->setCaptionIndent(kRowGutter + g.sideMargin);  // 머리글 글자를 셀 글자(x = 16)에 맞춘다
    } else {
        place(IconColumn, iconBand, 0, 2, kIconBand, true);
        place(NameColumn, fileBand, nameRow, 1, 0, true);
        place(ExtColumn, fileBand, metaRow, 1, kExtWidth, false);
        place(TypeColumn, fileBand, metaRow, 1, kTypeWidth, true);
        place(SizeColumn, fileBand, metaRow, 1, kSizeWidth, true);
        place(ModifiedColumn, fileBand, metaRow, 1, kDateWidth2, true);
        place(AttrColumn, fileBand, metaRow, 1, kAttrWidth2, true);
        place(FillerColumn, fileBand, metaRow, 1, 0, true);
        column(NameColumn)->setCaptionIndent(0);
    }
    column(IconColumn)->setCaption(QString());
    column(FillerColumn)->setCaption(QString());

    Qtitan::GridViewOptions &o = view->options();
    o.setCellHeight(g.cellHeight());
    // 머리글: 1줄 27/22(아래 선 포함), 2줄 45/40(22 + 22 + 1 / 20 + 20) — Qtitan 머리글 높이는 줄 하나 기준
    o.setColumnHeight(g.twoLine ? (g.headerHeight - (g.watercolor ? 0 : 1)) / 2 : g.headerHeight - (g.watercolor ? 0 : 1));
    view->endUpdate();
    grid->viewport()->update();
    applying = false;
}

void FileListViewPrivate::applyTheme()
{
    const fm::style::ThemeColors &tc = colors();
    Qtitan::GridViewOptions &o = view->options();
    o.setColumnPen(QPen(tc.isWatercolor() ? tc[Token::Fg] : tc[Token::Fg2]));
    o.setColumnFont(fm::style::pixelFont(q->font(), 12));
    o.setCellFont(q->font());
    o.setBackgroundColor(tc[Token::Surface]);
    applyLayout();
}

void FileListViewPrivate::scheduleAuto()
{
    if (mode == ViewMode::Auto || mode == ViewMode::Thumbnails)
        autoTimer->start();
}

void FileListViewPrivate::evaluateAuto()
{
    if (mode != ViewMode::Auto && mode != ViewMode::Thumbnails)
        return;
    const int width = q->width();
    const int threshold = appearance.autoWidth;
    const double percent = appearance.autoPercent / 100.0;
    bool want = twoLine;
    if (!twoLine)
        want = width < threshold || q->truncatedNameRatio(width) >= percent;
    else
        want = !(width >= threshold + 80 && q->truncatedNameRatio(width - 80) < percent);
    if (want == twoLine)
        return;
    twoLine = want;
    applyLayout();
    Q_EMIT q->twoLineChanged(twoLine);
}

void FileListViewPrivate::syncSortFromGrid()
{
    int column = -1;
    Qt::SortOrder order = Qt::AscendingOrder;
    for (int c = 0; c < ColumnCount; ++c) {
        auto *col = this->column(c);
        if (col && col->sortOrder() != Qtitan::SortNone) {
            column = c;
            order = col->sortOrder() == Qtitan::SortDescending ? Qt::DescendingOrder : Qt::AscendingOrder;
            break;
        }
    }
    sortColumn = column;
    sortOrder = order;
    if (syncingSort)
        return;
    if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(model)) {
        const int cursor = q->cursorRow();
        const QPersistentModelIndex keep = cursor >= 0 ? model->index(cursor, NameColumn) : QModelIndex();
        proxy->sort(column, order);
        if (keep.isValid())
            q->setCursorRow(keep.row());
    }
    Q_EMIT q->sortChanged(column, order);
}

void FileListViewPrivate::moveCursor(int delta)
{
    if (!model || model->rowCount() == 0)
        return;
    const int row = std::clamp(q->cursorRow() + delta, 0, model->rowCount() - 1);
    q->setCursorRow(row);
}

void FileListViewPrivate::markRange(int row, bool marked)
{
    if (!model || row < 0 || row >= model->rowCount())
        return;
    const QModelIndex i = model->index(row, NameColumn);
    if (!i.data(IsUpRole).toBool())
        model->setData(i, marked, MarkedRole);
}

// ---------------------------------------------------------------- FileListView

FileListView::FileListView(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<FileListViewPrivate>(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    d->setupGrid();
    layout->addWidget(d->grid);
    setFocusProxy(d->grid);

    d->autoTimer = new QTimer(this);
    d->autoTimer->setSingleShot(true);
    d->autoTimer->setInterval(100);
    connect(d->autoTimer, &QTimer::timeout, this, [this] { d->evaluateAuto(); });

    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] { d->applyTheme(); });
    d->applyTheme();
}

FileListView::~FileListView()
{
    // Qtitan 뷰가 레코드 그리기 객체를 쓰지 않게 먼저 뗀다.
    if (d->view)
        d->view->setRecordPainter(nullptr);
}

void FileListView::setModel(QAbstractItemModel *model)
{
    if (d->model == model)
        return;
    if (d->model)
        disconnect(d->model, nullptr, this, nullptr);
    d->model = model;
    d->columnsReady = false;
    d->view->beginUpdate();
    d->view->setModel(model);
    d->view->endUpdate();
    d->applyLayout();
    if (model) {
        auto changed = [this] { d->scheduleAuto(); };
        connect(model, &QAbstractItemModel::modelReset, this, [this] {
            if (!d->columnsReady || d->view->getColumnCount() < ColumnCount) {
                d->columnsReady = false;
                d->applyLayout();
            }
            d->scheduleAuto();
        });
        connect(model, &QAbstractItemModel::rowsInserted, this, changed);
        connect(model, &QAbstractItemModel::rowsRemoved, this, changed);
        connect(model, &QAbstractItemModel::layoutChanged, this, changed);
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                    if (roles.contains(MarkedRole))
                        Q_EMIT marksChanged();
                });
    }
    d->scheduleAuto();
}

QAbstractItemModel *FileListView::model() const
{
    return d->model;
}

ViewMode FileListView::viewMode() const noexcept
{
    return d->mode;
}

void FileListView::setViewMode(ViewMode mode)
{
    if (d->mode == mode)
        return;
    d->mode = mode;
    const bool before = d->twoLine;
    if (mode == ViewMode::OneLine)
        d->twoLine = false;
    else if (mode == ViewMode::TwoLine)
        d->twoLine = true;
    if (before != d->twoLine) {
        d->applyLayout();
        Q_EMIT twoLineChanged(d->twoLine);
    }
    if (mode == ViewMode::Auto || mode == ViewMode::Thumbnails)
        d->evaluateAuto();
    Q_EMIT viewModeChanged(mode);
}

bool FileListView::isTwoLine() const noexcept
{
    return d->twoLine;
}

const ListAppearance &FileListView::appearance() const noexcept
{
    return d->appearance;
}

void FileListView::setAppearance(const ListAppearance &appearance)
{
    if (d->appearance == appearance)
        return;
    d->appearance = appearance;
    d->applyLayout();
    d->scheduleAuto();
}

bool FileListView::isPaneActive() const
{
    return paneActive(this);
}

void FileListView::setPaneActive(bool active)
{
    fm::style::setPaneActive(this, active);
    d->grid->viewport()->update();
    d->grid->update();
}

bool FileListView::isPreviewMode() const noexcept
{
    return d->preview;
}

void FileListView::setPreviewMode(bool preview)
{
    if (d->preview == preview)
        return;
    d->preview = preview;
    d->grid->setFocusPolicy(preview ? Qt::NoFocus : Qt::StrongFocus);
    d->view->options().setScrollBars(preview ? Qtitan::ScrollNone : Qtitan::ScrollAuto);
    updateGeometry();
}

int FileListView::cursorRow() const
{
    return d->view->focusedRowIndex();
}

void FileListView::setCursorRow(int row)
{
    d->view->setFocusedRowIndex(row);
}

QModelIndex FileListView::cursorIndex() const
{
    const int row = cursorRow();
    return d->model && row >= 0 ? d->model->index(row, NameColumn) : QModelIndex();
}

void FileListView::setSortIndicator(int column, Qt::SortOrder order, bool apply)
{
    d->syncingSort = !apply;
    d->view->beginUpdate();
    for (int c = 0; c < ColumnCount; ++c) {
        auto *col = d->column(c);
        if (!col)
            continue;
        if (c == column)
            col->setSortOrder(order == Qt::DescendingOrder ? Qtitan::SortDescending : Qtitan::SortAscending);
        else if (col->sortOrder() != Qtitan::SortNone)
            col->setSortOrder(Qtitan::SortNone);
    }
    d->view->endUpdate();
    d->syncingSort = false;
    d->sortColumn = column;
    d->sortOrder = order;
}

int FileListView::sortColumn() const
{
    return d->sortColumn;
}

Qt::SortOrder FileListView::sortOrder() const
{
    return d->sortOrder;
}

void FileListView::toggleMark(int row)
{
    if (!d->model || row < 0 || row >= d->model->rowCount())
        return;
    const QModelIndex i = d->model->index(row, NameColumn);
    if (!i.data(IsUpRole).toBool())
        d->model->setData(i, !i.data(MarkedRole).toBool(), MarkedRole);
}

double FileListView::truncatedNameRatio(int width) const
{
    if (!d->model)
        return 0;
    const int available = width - (kExtWidth + kSizeWidth + kDateWidth1 + kAttrWidth1) - kRowGutter - kCellPad - 22 - kCellPad;
    const QFontMetrics fm(font());
    const int rows = std::min(d->model->rowCount(), 400);
    int counted = 0;
    int truncated = 0;
    for (int r = 0; r < rows; ++r) {
        const QModelIndex i = d->model->index(r, NameColumn);
        if (i.data(IsUpRole).toBool())
            continue;
        ++counted;
        if (fm.horizontalAdvance(i.data(StemRole).toString()) > available)
            ++truncated;
    }
    return counted ? double(truncated) / counted : 0.0;
}

int FileListView::preferredHeight(int rows) const
{
    return d->geometry.headerHeight + rows * d->geometry.pitch + 2;
}

QSize FileListView::sizeHint() const
{
    if (d->preview && d->model)
        return QSize(480, preferredHeight(d->model->rowCount()));
    return QSize(720, 480);
}

QSize FileListView::minimumSizeHint() const
{
    return QSize(160, d->geometry.headerHeight + d->geometry.pitch * 2);
}

bool FileListView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->grid) {
        switch (event->type()) {
        case QEvent::FocusIn:
        case QEvent::MouseButtonPress:
            Q_EMIT paneActivated();
            break;
        case QEvent::KeyPress: {
            auto *key = static_cast<QKeyEvent *>(event);
            const Qt::KeyboardModifiers mods = key->modifiers() & ~Qt::KeypadModifier;
            const int row = cursorRow();
            switch (key->key()) {
            case Qt::Key_Insert:
            case Qt::Key_Space:
                if (mods == Qt::NoModifier) {
                    toggleMark(row);
                    d->moveCursor(1);
                    return true;
                }
                break;
            case Qt::Key_Up:
            case Qt::Key_Down:
                if (mods == Qt::ShiftModifier) {
                    toggleMark(row);
                    d->moveCursor(key->key() == Qt::Key_Down ? 1 : -1);
                    return true;
                }
                break;
            case Qt::Key_Left:
            case Qt::Key_Right:
                return true;  // 칸 이동 없음(레코드 단위)
            case Qt::Key_A:
                if (mods == Qt::ControlModifier) {
                    for (int r = 0; d->model && r < d->model->rowCount(); ++r)
                        d->markRange(r, true);
                    return true;
                }
                break;
            case Qt::Key_Asterisk:
                if (key->modifiers() & Qt::KeypadModifier) {
                    for (int r = 0; d->model && r < d->model->rowCount(); ++r)
                        toggleMark(r);
                    return true;
                }
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                Q_EMIT activated(cursorIndex());
                return true;
            case Qt::Key_Backspace:
                Q_EMIT upRequested();
                return true;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FileListView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    d->scheduleAuto();
}

void FileListView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
        if (d->view)
            d->applyTheme();
        break;
    default:
        break;
    }
}

} // namespace fm::filelist
