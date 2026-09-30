#include "fmdialogs/RenamePreview.h"

#include "fmdialogs/FileOpContext.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>

#include <QtnGrid.h>
#include <QtnGridBandedTableView.h>

#include <QEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

using fm::style::Token;

namespace {

// 02 §9.3 치수
constexpr int kPitch1 = 26;       // 1줄 행(아래 선 포함)
constexpr int kPitch2 = 46;       // 2줄 행(아래 선 포함) — 줄 21 + 21, 위아래 1.5
constexpr int kHeader1 = 28;
constexpr int kHeader2 = 45;
constexpr int kPad = 10;          // 셀 좌우 여백
constexpr int kIndexWidth = 40;
constexpr int kArrowWidth = 28;
constexpr int kSizeWidth = 80;
constexpr int kDateWidth = 140;
constexpr int kStatusWidth = 132;
constexpr qreal kLine2 = 21.0;

fm::style::Tone toneFor(RenamePreviewRow::State state)
{
    switch (state) {
    case RenamePreviewRow::Ok:   return fm::style::Tone::Ok;
    case RenamePreviewRow::Same: return fm::style::Tone::Mute;
    default:                     return fm::style::Tone::Bad;
    }
}

} // namespace

// ---------------------------------------------------------------------------------------------
// RenamePreviewModel

RenamePreviewModel::RenamePreviewModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void RenamePreviewModel::setRows(const QList<RenamePreviewRow> &rows)
{
    if (rows.size() == m_rows.size() && !rows.isEmpty()) {
        m_rows = rows;
        Q_EMIT dataChanged(index(0, 0), index(int(m_rows.size()) - 1, ColumnCount - 1));
        return;
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

int RenamePreviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int RenamePreviewModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant RenamePreviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const RenamePreviewRow &r = m_rows.at(index.row());
    switch (role) {
    case StateRole:
        return int(r.state);
    case IsLongRole:
        return r.isLong;
    case NewNameRole:
        return r.newName;
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column()) {
        case IndexColumn:    return r.index;
        case OriginalColumn: return r.original;
        case NewNameColumn:  return r.newName;
        case SizeColumn:     return formatBytes(r.size);
        case DateColumn:     return r.date.toString(u"yyyy-MM-dd HH:mm"_s);
        case StatusColumn:   return r.stateText();
        default:             return {};
        }
    default:
        return {};
    }
}

QVariant RenamePreviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case IndexColumn:    return u"#"_s;
    case OriginalColumn: return tr("원래 이름");
    case NewNameColumn:  return tr("새 이름");
    case SizeColumn:     return tr("크기");
    case DateColumn:     return tr("촬영 날짜");
    case StatusColumn:   return tr("상태");
    default:             return QString();
    }
}

// ---------------------------------------------------------------------------------------------
// RenamePreviewView

class PreviewRecordPainter final : public Qtitan::GridRecordPainter
{
public:
    explicit PreviewRecordPainter(RenamePreviewViewPrivate *d) : m_d(d) {}
    void paintBackground(QPainter &p, const Record &record, QWidget *widget) override;
    void paintOverlay(QPainter &p, const Record &record, QWidget *widget) override;

private:
    RenamePreviewViewPrivate *m_d;
};

class PreviewCellDelegate final : public QStyledItemDelegate
{
public:
    PreviewCellDelegate(RenamePreviewViewPrivate *d, QObject *parent) : QStyledItemDelegate(parent), m_d(d) {}
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    RenamePreviewViewPrivate *m_d;
};

class RenamePreviewViewPrivate
{
public:
    explicit RenamePreviewViewPrivate(RenamePreviewView *q) : q(q) {}

    const fm::style::ThemeColors &colors() const { return fm::style::themeColorsFor(q); }
    Qtitan::GridBandedTableColumn *column(int modelColumn) const
    {
        return static_cast<Qtitan::GridBandedTableColumn *>(view->getColumnByModelColumn(modelColumn));
    }
    int pitch() const { return twoLine ? kPitch2 : kPitch1; }
    int headerHeight() const { return twoLine ? kHeader2 : kHeader1; }

    void setupGrid();
    bool ensureColumns();
    void applyLayout();
    void applyTheme();
    void evaluate();

    RenamePreviewView *q;
    Qtitan::Grid *grid = nullptr;
    Qtitan::GridBandedTableView *view = nullptr;
    RenamePreviewModel *model = nullptr;
    std::unique_ptr<PreviewRecordPainter> recordPainter;
    PreviewCellDelegate *delegate = nullptr;
    fm::filelist::ViewMode mode = fm::filelist::ViewMode::Auto;
    bool twoLine = false;
    bool columnsReady = false;
    bool applying = false;
};

void PreviewRecordPainter::paintBackground(QPainter &p, const Record &record, QWidget *)
{
    const fm::style::ThemeColors &tc = m_d->colors();
    const auto state = RenamePreviewRow::State(record.index.data(RenamePreviewModel::StateRole).toInt());
    const bool bad = state == RenamePreviewRow::Invalid || state == RenamePreviewRow::Exists
                     || state == RenamePreviewRow::Duplicate;
    const QRect r = record.rect;
    p.fillRect(r, bad ? tc[Token::DangerBg] : tc[Token::Surface]);
    // 행 아래 선 — 1줄은 --grid, 2줄은 레코드 경계가 보이게 --line
    p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), m_d->twoLine ? tc[Token::Line] : tc[Token::Grid]);
}

void PreviewRecordPainter::paintOverlay(QPainter &p, const Record &record, QWidget *)
{
    // 목업에는 선택이 없다 — 키보드로 스크롤할 때만 포커스 행에 1 px 틀.
    if (!(record.state & QStyle::State_HasFocus) || !m_d->grid->hasFocus())
        return;
    const fm::style::ThemeColors &tc = m_d->colors();
    QPen pen(tc[Token::Focus], 1.0);
    if (tc.isWatercolor()) {
        pen.setStyle(Qt::DotLine);
        pen.setCosmetic(true);
    }
    p.save();
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(record.rect).adjusted(0.5, 0.5, -0.5, -1.5));
    p.restore();
}

void PreviewCellDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
    const QModelIndex idx = source.isValid() ? source : index;
    const int column = idx.column();
    if (column == RenamePreviewModel::ArrowColumn && m_d->twoLine)
        return;

    const fm::style::ThemeColors &tc = m_d->colors();
    const bool two = m_d->twoLine;
    const QRect cell = option.rect;
    const auto state = RenamePreviewRow::State(idx.data(RenamePreviewModel::StateRole).toInt());

    // 셀이 속한 레코드의 세로 범위(2줄에서 새 이름 칸은 아래 줄, 번호 · 상태는 두 줄 걸침)
    int recordTop = cell.top();
    if (two && column == RenamePreviewModel::NewNameColumn)
        recordTop = cell.top() - kPitch2 / 2;
    const int pitch = m_d->pitch();
    // 글줄: 1줄은 아래 선을 뺀 25 px, 2줄은 21 px 두 줄(위 1.5)
    const QRectF whole(cell.left(), recordTop, cell.width(), pitch - 1);
    const QRectF line0 = two ? QRectF(cell.left(), recordTop + 1.5, cell.width(), kLine2) : whole;
    const QRectF line1 = two ? QRectF(cell.left(), recordTop + 1.5 + kLine2, cell.width(), kLine2) : whole;

    const QFont base = fm::style::pixelFont(m_d->q->font(), 13);
    const QFont meta = fm::style::withTabularNumbers(fm::style::pixelFont(m_d->q->font(), 12));
    p->save();
    switch (column) {
    case RenamePreviewModel::IndexColumn: {
        p->setFont(meta);
        p->setPen(tc[Token::Fg3]);
        p->drawText(whole.adjusted(kPad, 0, -kPad, 0), Qt::AlignRight | Qt::AlignVCenter, idx.data().toString());
        break;
    }
    case RenamePreviewModel::OriginalColumn: {
        p->setFont(base);
        p->setPen(tc[Token::Fg2]);
        const QRectF r = line0.adjusted(kPad, 0, -kPad, 0);
        // 파일 이름은 확장자가 보이게 가운데를 줄인다(02 §9.7)
        p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(base).elidedText(idx.data().toString(), Qt::ElideMiddle, int(r.width())));
        break;
    }
    case RenamePreviewModel::ArrowColumn: {
        const QRectF g(whole.center().x() - 7, whole.center().y() - 7, 14, 14);
        fm::style::paintGlyph(p, fm::style::Glyph::ArrowRight, g, tc[Token::Fg3]);
        break;
    }
    case RenamePreviewModel::NewNameColumn: {
        QFont font = base;
        const bool same = state == RenamePreviewRow::Same;
        font.setWeight(same ? QFont::Normal : QFont::DemiBold);
        p->setFont(font);
        QRectF r = line1.adjusted(kPad, 0, -kPad, 0);
        if (two) {
            fm::style::paintGlyph(p, fm::style::Glyph::ReturnArrow, QRectF(r.left(), r.center().y() - 7, 14, 14),
                                  tc[Token::Fg3]);
            r.setLeft(r.left() + 14 + 6);
        }
        p->setPen(same ? tc[Token::Fg3] : tc[Token::Fg]);
        p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(font).elidedText(idx.data().toString(), Qt::ElideMiddle, int(r.width())));
        break;
    }
    case RenamePreviewModel::SizeColumn:
    case RenamePreviewModel::DateColumn: {
        p->setFont(meta);
        p->setPen(tc[Token::Fg2]);
        const Qt::Alignment align = column == RenamePreviewModel::SizeColumn ? Qt::AlignRight : Qt::AlignLeft;
        p->drawText(line0.adjusted(kPad, 0, -kPad, 0), align | Qt::AlignVCenter, idx.data().toString());
        break;
    }
    case RenamePreviewModel::StatusColumn: {
        const QString text = idx.data().toString();
        const fm::style::Tone tone = toneFor(state);
        const QSize size = fm::style::tagSize(text, tone, true, false);
        const QRectF tag(whole.left() + kPad, whole.center().y() - size.height() / 2.0, size.width(), size.height());
        fm::style::paintTag(p, tag, text, tone, tc, true);
        break;
    }
    default:
        break;
    }
    p->restore();
}

void RenamePreviewViewPrivate::setupGrid()
{
    grid = new Qtitan::Grid(q);
    grid->setViewType(Qtitan::Grid::BandedTableView);
    view = grid->view<Qtitan::GridBandedTableView>();
    recordPainter = std::make_unique<PreviewRecordPainter>(this);
    delegate = new PreviewCellDelegate(this, q);

    view->beginUpdate();
    Qtitan::GridViewOptions &o = view->options();
    o.setGroupsHeader(false);
    o.setMainMenuDisabled(true);
    o.setGridLines(Qtitan::LinesNone);
    o.setSelectionPolicy(Qtitan::GridViewOptions::IgnoreSelection);
    o.setAlternatingRowColors(false);
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
}

bool RenamePreviewViewPrivate::ensureColumns()
{
    if (!model)
        return false;
    if (columnsReady && view->getColumnCount() >= RenamePreviewModel::ColumnCount)
        return true;
    view->beginUpdate();
    view->setModel(model);
    for (int c = 0; c < RenamePreviewModel::ColumnCount; ++c) {
        auto *col = column(c);
        if (!col)
            continue;
        col->setCaption(model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
        const bool right = c == RenamePreviewModel::IndexColumn || c == RenamePreviewModel::SizeColumn;
        col->setTextAlignment((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        col->setEditorType(Qtitan::GridEditor::DelegateAdapter);
        static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(col->editorRepository())->setDelegate(delegate);
        col->editorRepository()->setEditable(false);
        col->setSortEnabled(false);
        col->setMenuButtonVisible(false);
        col->setHidingEnabled(false);
        col->setMovingEnabled(false);
    }
    view->endUpdate();
    columnsReady = true;
    return true;
}

void RenamePreviewViewPrivate::applyLayout()
{
    if (applying || !ensureColumns())
        return;
    for (int c = 0; c < RenamePreviewModel::ColumnCount; ++c) {
        if (!column(c))
            return;  // 모델 초기화 중(Qtitan이 열 연결을 다시 잇기 전)
    }
    applying = true;
    view->beginUpdate();
    view->removeBands();
    Qtitan::GridTableBand *indexBand = view->addBand(QString());
    Qtitan::GridTableBand *mainBand = view->addBand(QString());
    Qtitan::GridTableBand *statusBand = view->addBand(QString());

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
            col->setMinWidth(40);
            col->setMaxWidth(100000);
        }
        col->setVisible(visible);
    };
    const int span = twoLine ? 2 : 1;
    place(RenamePreviewModel::IndexColumn, indexBand, 0, span, kIndexWidth, true);
    place(RenamePreviewModel::OriginalColumn, mainBand, 0, 1, 0, true);
    place(RenamePreviewModel::ArrowColumn, mainBand, 0, 1, kArrowWidth, !twoLine);
    place(RenamePreviewModel::NewNameColumn, mainBand, twoLine ? 1 : 0, 1, 0, true);
    place(RenamePreviewModel::SizeColumn, mainBand, 0, 1, kSizeWidth, true);
    place(RenamePreviewModel::DateColumn, mainBand, 0, 1, kDateWidth, true);
    place(RenamePreviewModel::StatusColumn, statusBand, 0, span, kStatusWidth, true);
    // 1줄은 원래 이름 · → · 새 이름 · 크기 · 날짜 순서. 2줄은 아래 줄에 새 이름만.
    column(RenamePreviewModel::OriginalColumn)->setVisualIndex(0);
    column(RenamePreviewModel::ArrowColumn)->setVisualIndex(1);
    column(RenamePreviewModel::NewNameColumn)->setVisualIndex(2);
    column(RenamePreviewModel::SizeColumn)->setVisualIndex(3);
    column(RenamePreviewModel::DateColumn)->setVisualIndex(4);

    auto *newName = column(RenamePreviewModel::NewNameColumn);
    const fm::style::ThemeColors &tc = colors();
    if (twoLine) {
        newName->setCaption(model->headerData(RenamePreviewModel::NewNameColumn, Qt::Horizontal, Qt::DisplayRole).toString());
        newName->setIcon(fm::style::glyphIcon(fm::style::Glyph::ReturnArrow, tc[Token::Fg3], 14));
    } else {
        newName->setIcon(QIcon());
    }

    Qtitan::GridViewOptions &o = view->options();
    o.setCellHeight(twoLine ? kPitch2 / 2 : kPitch1);
    // 머리글 — 파일 목록과 같이 Qtitan 높이는 줄 하나 기준, 시안1은 아래 선 1 px를 뺀다.
    const int h = headerHeight() - (tc.isWatercolor() ? 0 : 1);
    o.setColumnHeight(twoLine ? h / 2 : h);
    view->endUpdate();
    grid->viewport()->update();
    applying = false;
}

void RenamePreviewViewPrivate::applyTheme()
{
    const fm::style::ThemeColors &tc = colors();
    Qtitan::GridViewOptions &o = view->options();
    o.setColumnPen(QPen(tc.isWatercolor() ? tc[Token::Fg] : tc[Token::Fg2]));
    o.setColumnFont(fm::style::pixelFont(q->font(), 12));
    o.setCellFont(fm::style::pixelFont(q->font(), 13));
    o.setBackgroundColor(tc[Token::Surface]);
    applyLayout();
}

void RenamePreviewViewPrivate::evaluate()
{
    bool want = false;
    switch (mode) {
    case fm::filelist::ViewMode::OneLine:
        want = false;
        break;
    case fm::filelist::ViewMode::TwoLine:
        want = true;
        break;
    default:
        want = model && summarizeRename(model->rows()).longCount > 0;
        break;
    }
    if (want == twoLine && columnsReady)
        return;
    const bool changed = want != twoLine;
    twoLine = want;
    applyLayout();
    if (changed)
        Q_EMIT q->twoLineChanged(twoLine);
}

RenamePreviewView::RenamePreviewView(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<RenamePreviewViewPrivate>(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    d->setupGrid();
    layout->addWidget(d->grid);
    setFocusProxy(d->grid);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] { d->applyTheme(); });
    d->applyTheme();
}

RenamePreviewView::~RenamePreviewView()
{
    // 모델보다 먼저 Qtitan 뷰를 없앤다(Qtitan은 모델이 뷰보다 오래 살아야 한다).
    if (d->view)
        d->view->setRecordPainter(nullptr);
    delete d->grid;
    d->grid = nullptr;
    d->view = nullptr;
}

void RenamePreviewView::setModel(RenamePreviewModel *model)
{
    if (d->model == model)
        return;
    if (d->model)
        disconnect(d->model, nullptr, this, nullptr);
    d->model = model;
    d->columnsReady = false;
    d->evaluate();
    // Qtitan이 먼저 연결되게 뷰에 모델을 건 뒤에 연결한다 — Qtitan은 modelAboutToBeReset에서 열 연결을 풀고
    // modelReset에서 다시 잇는다. 그 사이에 열을 만지면 getColumnByModelColumn()이 null이다.
    if (model) {
        auto changed = [this] { d->evaluate(); };
        connect(model, &QAbstractItemModel::modelReset, this, changed);
        connect(model, &QAbstractItemModel::dataChanged, this, changed);
    }
}

RenamePreviewModel *RenamePreviewView::model() const
{
    return d->model;
}

fm::filelist::ViewMode RenamePreviewView::viewMode() const noexcept
{
    return d->mode;
}

void RenamePreviewView::setViewMode(fm::filelist::ViewMode mode)
{
    if (d->mode == mode)
        return;
    d->mode = mode;
    d->evaluate();
    Q_EMIT viewModeChanged(mode);
}

bool RenamePreviewView::isTwoLine() const noexcept
{
    return d->twoLine;
}

int RenamePreviewView::preferredHeight(int rows) const
{
    return d->headerHeight() + rows * d->pitch();
}

QSize RenamePreviewView::sizeHint() const
{
    return QSize(1000, preferredHeight(8));
}

void RenamePreviewView::changeEvent(QEvent *event)
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

} // namespace fm::dialogs
