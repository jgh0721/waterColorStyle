#include "fmfilelist/FileListView.h"

#include "fmfilelist/FileGroups.h"
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
    int nameColumn() const { return layout.nameColumn(); }

    /// 행 상태 — 미리보기 모델(PreviewStateRole)이 있으면 그 행의 상태 · 역상 여부를 그대로 쓴다.
    RecordState stateFor(const QModelIndex &index, bool cursor, ListAppearance *appearanceOut) const
    {
        RecordState s;
        s.active = paneActive(q);
        s.marked = index.data(MarkedRole).toBool();
        s.hidden = index.data(HiddenRole).toBool();
        s.cursor = cursor;
        ListAppearance a = appearance;
        const QVariant forced = index.data(PreviewStateRole);
        if (forced.isValid()) {
            const int flags = forced.toInt();
            s.active = true;
            s.marked = flags & PreviewMarked;
            s.cursor = flags & PreviewCursor;
            a.invertCursor = flags & PreviewInvertCursor;
            a.invertSelection = flags & PreviewInvertSelection;
        }
        if (appearanceOut)
            *appearanceOut = a;
        return s;
    }
    ResolvedGroupStyle groupStyleFor(const QModelIndex &index) const
    {
        const QVariant v = index.data(GroupStyleRole);
        return v.canConvert<ResolvedGroupStyle>() ? v.value<ResolvedGroupStyle>() : ResolvedGroupStyle();
    }
    /// 1줄에서 보이는 마지막 열(행 여백 8이 붙는다).
    int lastOneLineColumn() const
    {
        int last = -1;
        for (const ListColumn &c : layout.columns) {
            if (c.oneLine && c.role != ListColumn::Role::Icon && c.role != ListColumn::Role::Filler)
                last = c.modelColumn;
        }
        return last;
    }
    int oneLineMetaWidth() const
    {
        int width = 0;
        for (const ListColumn &c : layout.columns) {
            if (c.oneLine && (c.role == ListColumn::Role::Meta || c.role == ListColumn::Role::Extension))
                width += c.width;
        }
        return width + kRowGutter;
    }

    void setupGrid();
    void setupColumns();
    bool ensureColumns();
    void applyLayout();
    void applyTheme();
    void scheduleTheme();
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
    ListColumnLayout layout = ListColumnLayout::standard();
    RecordGeometry geometry;
    int sortColumn = -1;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool syncingSort = false;
    bool applying = false;
    bool columnsReady = false;
    bool themePending = false;
};

void FileRecordPainter::paintBackground(QPainter &p, const Record &record, QWidget *)
{
    ListAppearance a;
    const RecordState s = m_d->stateFor(record.index, record.state & QStyle::State_HasFocus, &a);
    const fm::style::ThemeColors &tc = m_d->colors();
    const auto back = m_d->groupStyleFor(record.index).background(tc.isDark());
    paintRecordBackground(&p, record.rect, m_d->geometry, a, s, record.alternate, tc, back.value_or(QColor()));
}

void FileRecordPainter::paintOverlay(QPainter &p, const Record &record, QWidget *)
{
    ListAppearance a;
    const RecordState s = m_d->stateFor(record.index, record.state & QStyle::State_HasFocus, &a);
    if (!s.cursor)
        return;
    paintRecordCursor(&p, record.rect, m_d->geometry, a, s, m_d->colors());
}

void FileCellDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // DelegateAdapter는 내부 대리 모델 인덱스를 넘긴다. 원래 모델 인덱스는 QueryIndexRole로 얻는다.
    const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
    const QModelIndex idx = source.isValid() ? source : index;
    const int column = idx.column();
    const ListColumn *spec = m_d->layout.find(column);
    if (!spec || spec->role == ListColumn::Role::Filler)
        return;

    const fm::style::ThemeColors &tc = m_d->colors();
    const RecordGeometry &g = m_d->geometry;
    const QModelIndex nameIndex = idx.siblingAtColumn(m_d->nameColumn());
    ListAppearance a;
    const RecordState s = m_d->stateFor(nameIndex, idx.row() == m_d->q->cursorRow(), &a);
    TextColors c = textColors(tc, a, s, g.twoLine, g.separator);
    const QRect cell = option.rect;

    // 파일 그룹: 글자색은 선택 · 역상 표시가 아닐 때만, 글꼴 효과는 늘
    const ResolvedGroupStyle group = m_d->groupStyleFor(nameIndex);
    const bool inverted = invertedCursor(a, s) || (a.invertSelection && s.marked) || (tc.isWatercolor() && s.marked && s.active);
    if (!inverted) {
        if (const auto text = group.text(tc.isDark())) {
            c.name = *text;
            c.ext = *text;
        }
    }

    // 셀이 속한 레코드 사각형 — 2줄에서 줄마다 셀이 따로 온다(아이콘은 두 줄 걸침).
    QRect record = cell;
    if (g.twoLine && spec->role != ListColumn::Role::Icon) {
        const bool onNameLine = spec->role == ListColumn::Role::Name;
        const int line = onNameLine == g.nameBelow ? 1 : 0;
        record = QRect(cell.left(), cell.top() - line * g.cellHeight(), cell.width(), g.pitch);
    }

    const QFont base = m_d->q->font();
    const QFont metaFont = fm::style::pixelFont(base, 12);
    p->save();

    if (spec->role == ListColumn::Role::Icon) {
        const QRectF block = g.block(record);
        const qreal centerY = block.top() + g.padTop + (g.nameHeight + g.metaHeight) / 2.0;
        const Kind kind = Kind(nameIndex.data(KindRole).toInt());
        FileIconPainter::paint(p, QRectF(cell.center().x() + 0.5 - 10, centerY - 10, 20, 20), kind, c.iconLine, tc,
                               s.hidden ? 0.6 : 1.0);
        p->restore();
        return;
    }

    if (spec->role == ListColumn::Role::Name) {
        QFont font = base;
        if (c.bold || group.bold)
            font.setWeight(QFont::DemiBold);
        font.setItalic(group.italic);
        font.setUnderline(group.underline);
        font.setStrikeOut(group.strike);
        p->setFont(font);
        const QFontMetrics fm(font);
        const QRectF line = g.nameLine(record);
        const Kind kind = Kind(idx.data(KindRole).toInt());
        const QString stem = idx.data(StemRole).toString();
        const bool first = !g.twoLine;  // 1줄에서 이름은 첫 열(행 여백 + 아이콘 16)
        if (first) {
            const qreal left = cell.left() + kRowGutter + kCellPad + g.sideMargin;
            FileIconPainter::paint(p, QRectF(left, line.center().y() - 8, 16, 16), kind, c.iconLine, tc, s.hidden ? 0.6 : 1.0);
            const qreal textLeft = left + 22;
            const int width = int(cell.right() + 1 - kCellPad - textLeft);
            const Qt::TextElideMode mode = a.nameElide == NameElide::End ? Qt::ElideRight : Qt::ElideMiddle;
            // 확장자 열이 없는 배치(미리보기)는 이름 뒤에 확장자를 --fg3로 붙인다
            const QString ext = idx.data(ExtRole).toString();
            const bool extColumn = std::any_of(m_d->layout.columns.cbegin(), m_d->layout.columns.cend(), [](const ListColumn &col) {
                return col.role == ListColumn::Role::Extension && col.oneLine;
            });
            if (extColumn || ext.isEmpty()) {
                p->setPen(c.name);
                p->drawText(QRectF(textLeft, line.top(), width, line.height()), Qt::AlignLeft | Qt::AlignVCenter,
                            fm.elidedText(stem, mode, width));
            } else {
                const QString dotExt = u'.' + ext;
                const int extWidth = fm.horizontalAdvance(dotExt);
                const QString shown = fm.elidedText(stem, mode, std::max(0, width - extWidth));
                const int stemWidth = fm.horizontalAdvance(shown);
                p->setPen(c.name);
                p->drawText(QRectF(textLeft, line.top(), stemWidth + 1, line.height()), Qt::AlignLeft | Qt::AlignVCenter, shown);
                p->setPen(c.ext);
                p->drawText(QRectF(textLeft + stemWidth, line.top(), extWidth + 1, line.height()), Qt::AlignLeft | Qt::AlignVCenter,
                            dotExt);
            }
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

    // 메타 칸: 12 px, 숫자 폭 고정(크기 · 날짜) · 고정폭(속성)은 열 배치가 정한다
    QFont font = spec->mono ? fm::style::monoFont(12) : metaFont;
    if (spec->tabular)
        font = fm::style::withTabularNumbers(font);
    p->setFont(font);
    QString text = idx.data(Qt::DisplayRole).toString();
    if (column == SizeColumn && !g.twoLine && idx.data(IsDirRole).toBool() && !idx.data(IsUpRole).toBool())
        text = u"폴더"_s;
    const QRectF line = g.metaLine(record);
    const bool last = !g.twoLine && column == m_d->lastOneLineColumn();
    const qreal left = cell.left() + kCellPad;
    const qreal right = cell.right() + 1 - kCellPad - (last ? kRowGutter + g.sideMargin : 0);
    const Qt::Alignment align = (spec->align & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter;
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
        Q_EMIT q->activated(args->row().modelIndex(nameColumn()));
    });
    QObject::connect(view, &Qtitan::GridViewBase::sortingChanged, q, [this] { syncSortFromGrid(); });

    grid->installEventFilter(q);
}

// 모델의 열이 아직 없을 수 있다(원본이 없는 프록시). 열이 생기면 Qtitan 열을 다시 만들고 설정한다.
bool FileListViewPrivate::ensureColumns()
{
    const int required = layout.requiredColumns();
    if (!model || model->columnCount() < required)
        return false;
    if (columnsReady && view->getColumnCount() >= required) {
        bool all = true;
        for (const ListColumn &c : layout.columns)
            all = all && column(c.modelColumn);
        if (all)
            return true;
    }
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
    for (int c = 0; c < view->getColumnCount(); ++c) {
        auto *col = static_cast<Qtitan::GridBandedTableColumn *>(view->getColumn(c));
        if (!col)
            continue;
        const int modelColumn = col->dataBinding() ? col->dataBinding()->column() : -1;
        const ListColumn *spec = layout.find(modelColumn);
        col->setCaption(spec && !spec->caption.isEmpty() ? spec->caption
                        : model                          ? model->headerData(modelColumn, Qt::Horizontal).toString()
                                                         : QString());
        col->setTextAlignment(((spec ? spec->align : Qt::AlignLeft) & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter);
        col->setEditorType(Qtitan::GridEditor::DelegateAdapter);
        static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(col->editorRepository())->setDelegate(delegate);
        col->editorRepository()->setEditable(false);
        // Qtitan 정렬은 모델 순서를 그대로 둔다 — 실제 정렬은 FileSortProxy가 한다.
        col->dataBinding()->setSortRole(Qt::ItemDataRole(NoSortRole));
        col->setSortEnabled(spec && (spec->role == ListColumn::Role::Name || spec->role == ListColumn::Role::Meta
                                    || spec->role == ListColumn::Role::Extension));
        col->setMenuButtonVisible(false);
        col->setHidingEnabled(false);
        col->setMovingEnabled(false);
        if (!spec)
            col->setVisible(false);
    }
    view->endUpdate();
}

void FileListViewPrivate::applyLayout()
{
    if (!model || applying || !ensureColumns())
        return;
    applying = true;
    geometry = RecordGeometry::make(colors().isWatercolor(), twoLine, appearance);
    geometry.oneLineMetaWidth = oneLineMetaWidth();
    const RecordGeometry &g = geometry;

    view->beginUpdate();
    view->removeBands();
    Qtitan::GridTableBand *iconBand = view->addBand(QString());
    Qtitan::GridTableBand *fileBand = view->addBand(u"파일"_s);
    iconBand->setVisible(g.twoLine);

    auto place = [&](int c, Qtitan::GridTableBand *band, int row, int span, int width, bool visible) {
        auto *col = column(c);
        if (!col)
            return;
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
    const int last = lastOneLineColumn();
    for (const ListColumn &c : std::as_const(layout.columns)) {
        using R = ListColumn::Role;
        switch (c.role) {
        case R::Icon:
            place(c.modelColumn, g.twoLine ? iconBand : fileBand, 0, g.twoLine ? 2 : 1, g.twoLine ? kIconBand : 16, g.twoLine);
            break;
        case R::Name:
            place(c.modelColumn, fileBand, g.twoLine ? nameRow : 0, 1, 0, true);
            break;
        case R::Meta:
        case R::Extension:
            if (!g.twoLine) {
                const int width = c.width + (c.modelColumn == last ? kRowGutter : 0);
                place(c.modelColumn, fileBand, 0, 1, width, c.oneLine);
            } else {
                place(c.modelColumn, fileBand, metaRow, 1, c.widthFor(true), c.twoLine == ListColumn::TwoLine::Row1);
            }
            break;
        case R::Filler:
            place(c.modelColumn, fileBand, metaRow, 1, 0, g.twoLine);
            break;
        }
        if (auto *col = column(c.modelColumn)) {
            if (c.role == R::Icon || c.role == R::Filler)
                col->setCaption(QString());
        }
    }
    if (auto *name = column(nameColumn()))
        name->setCaptionIndent(g.twoLine ? 0 : kRowGutter + g.sideMargin);  // 머리글 글자를 셀 글자(x = 16)에 맞춘다

    Qtitan::GridViewOptions &o = view->options();
    o.setCellHeight(g.cellHeight());
    // 머리글: 1줄 27/22(아래 선 포함), 2줄 45/40(22 + 22 + 1 / 20 + 20) — Qtitan 머리글 높이는 줄 하나 기준
    o.setColumnHeight(g.twoLine ? (g.headerHeight - (g.watercolor ? 0 : 1)) / 2 : g.headerHeight - (g.watercolor ? 0 : 1));
    view->endUpdate();
    grid->viewport()->update();
    applying = false;
}

/// 팔레트 · 글꼴 · 스타일 변경 이벤트는 QApplication::setStyle() · setPalette()가 미리 모은 위젯 목록을 돌며 보낸다.
/// 그 안에서 밴드 · 열을 다시 만들면 Qtitan이 열 선택 팝업의 체크 상자를 바로 지워, 목록에 지운 위젯이 남아
/// 앱이 죽는다(설정 창에서 디자인을 바꿔 적용할 때). 다시 배치는 이벤트 루프로 미루고 한 번으로 모은다.
void FileListViewPrivate::scheduleTheme()
{
    if (themePending)
        return;
    themePending = true;
    QMetaObject::invokeMethod(
        q,
        [this] {
            themePending = false;
            if (view)
                applyTheme();
        },
        Qt::QueuedConnection);
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
    for (const ListColumn &spec : std::as_const(layout.columns)) {
        auto *col = this->column(spec.modelColumn);
        if (col && col->sortOrder() != Qtitan::SortNone) {
            column = spec.modelColumn;
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
        const QPersistentModelIndex keep = cursor >= 0 ? model->index(cursor, nameColumn()) : QModelIndex();
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
    const QModelIndex i = model->index(row, nameColumn());
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
            if (!d->columnsReady || d->view->getColumnCount() < d->layout.requiredColumns()) {
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
                    if (roles.contains(GroupStyleRole) || roles.contains(PreviewStateRole) || roles.isEmpty())
                        d->grid->viewport()->update();
                });
    }
    d->scheduleAuto();
}

QAbstractItemModel *FileListView::model() const
{
    return d->model;
}

const ListColumnLayout &FileListView::columnLayout() const noexcept
{
    return d->layout;
}

void FileListView::setColumnLayout(const ListColumnLayout &layout)
{
    if (d->layout == layout)
        return;
    d->layout = layout;
    d->columnsReady = false;
    if (d->model) {
        d->setupColumns();
        d->applyLayout();
    }
    d->scheduleAuto();
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
    // GridViewOptions::setScrollBars는 9.2에서 값만 저장한다 — 스크롤 영역의 정책으로 숨긴다
    const Qt::ScrollBarPolicy policy = preview ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded;
    d->grid->setVerticalScrollBarPolicy(policy);
    d->grid->setHorizontalScrollBarPolicy(policy);
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
    return d->model && row >= 0 ? d->model->index(row, d->nameColumn()) : QModelIndex();
}

void FileListView::setSortIndicator(int column, Qt::SortOrder order, bool apply)
{
    d->syncingSort = !apply;
    d->view->beginUpdate();
    for (const ListColumn &spec : std::as_const(d->layout.columns)) {
        auto *col = d->column(spec.modelColumn);
        if (!col)
            continue;
        if (spec.modelColumn == column)
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
    const QModelIndex i = d->model->index(row, d->nameColumn());
    if (!i.data(IsUpRole).toBool())
        d->model->setData(i, !i.data(MarkedRole).toBool(), MarkedRole);
}

double FileListView::truncatedNameRatio(int width) const
{
    if (!d->model)
        return 0;
    const int available = width - d->oneLineMetaWidth() - kCellPad - 22 - kCellPad;
    const QFontMetrics fm(font());
    const int rows = std::min(d->model->rowCount(), 400);
    int counted = 0;
    int truncated = 0;
    for (int r = 0; r < rows; ++r) {
        const QModelIndex i = d->model->index(r, d->nameColumn());
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
        d->scheduleTheme();
        break;
    default:
        break;
    }
}

} // namespace fm::filelist
