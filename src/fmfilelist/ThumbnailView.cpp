#include "fmfilelist/ThumbnailView.h"

#include "fmfilelist/FileRoles.h"
#include "fmfilelist/ThumbnailPainter.h"
#include "ListPainting_p.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QtnCardGrid.h>
#include <QtnGridCardView.h>

#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::filelist {

using fm::style::Token;

namespace {

void markAll(QAbstractItemModel *model)
{
    for (int r = 0; model && r < model->rowCount(); ++r) {
        const QModelIndex i = model->index(r, NameColumn);
        if (!i.data(IsUpRole).toBool())
            model->setData(i, true, MarkedRole);
    }
}

void toggleMark(QAbstractItemModel *model, int row)
{
    if (!model || row < 0 || row >= model->rowCount())
        return;
    const QModelIndex i = model->index(row, NameColumn);
    if (!i.data(IsUpRole).toBool())
        model->setData(i, !i.data(MarkedRole).toBool(), MarkedRole);
}

// ---------------------------------------------------------------- Qt 목록 타일 델리게이트
class ListTileDelegate final : public QStyledItemDelegate
{
public:
    explicit ListTileDelegate(ThumbnailListView *view) : QStyledItemDelegate(view), m_view(view) {}

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const ThumbnailAppearance &a = m_view->appearance();
        const int tw = a.tileWidth();
        // space-between: 칸 폭 = 보기 폭 / 열 수, 타일 i의 x = i × (보기 폭 − 타일 폭) / (열 수 − 1). 타일은 칸 안에 들어간다.
        const int viewWidth = std::max(tw, m_view->layoutWidth());
        const int cell = std::max(1, m_view->gridSize().width());
        const int columns = std::max(1, viewWidth / cell);
        const int column = std::clamp(option.rect.left() / cell, 0, columns - 1);
        const int left = columns > 1 ? int(std::lround(double(column) * (viewWidth - tw) / (columns - 1))) : 0;
        ThumbnailPainter::TileState state;
        state.active = detail::paneActive(m_view);
        state.cursor = index.row() == m_view->currentIndex().row();
        ThumbnailPainter::paintTile(p, QRect(left, option.rect.top(), tw, option.rect.height()), index, state, a,
                                    fm::style::themeColorsFor(m_view), m_view->font());
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        const ThumbnailAppearance &a = m_view->appearance();
        return QSize(a.tileWidth(), ThumbnailPainter::tileHeight(a));
    }

private:
    ThumbnailListView *m_view;
};

} // namespace

// ---------------------------------------------------------------- ThumbnailListView

ThumbnailListView::ThumbnailListView(QWidget *parent)
    : QListView(parent)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::NoSelection);  // 선택은 모델의 MarkedRole
    setSpacing(0);
    setFrameShape(QFrame::NoFrame);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDragEnabled(false);
    setItemDelegate(new ListTileDelegate(this));
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 안쪽 여백 8 — 여백 칸은 목록 바탕색으로 칠한다
    setViewportMargins(8, 8, 8, 0);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    updateGrid();
}

void ThumbnailListView::setModel(QAbstractItemModel *model)
{
    QListView::setModel(model);
    updateGrid();
}

void ThumbnailListView::setAppearance(const ThumbnailAppearance &appearance)
{
    if (m_appearance == appearance)
        return;
    m_appearance = appearance;
    updateGrid();
    viewport()->update();
}

int ThumbnailListView::layoutWidth() const
{
    // QListViewPrivate::prepareItemsLayout과 같은 계산
    int width = maximumViewportSize().width();
    const QScrollBar *bar = verticalScrollBar();
    if (verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded && !style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, bar))
        width -= style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, bar);
    return width;
}

void ThumbnailListView::updateGrid()
{
    const int tw = m_appearance.tileWidth();
    const int width = std::max(tw, layoutWidth());
    const int columns = std::max(1, (width + 4) / (tw + 4));  // 최소 간격 4
    setGridSize(QSize(width / columns, ThumbnailPainter::tileHeight(m_appearance) + 4));
}

int ThumbnailListView::cursorRow() const
{
    return currentIndex().row();
}

void ThumbnailListView::setCursorRow(int row)
{
    if (!model() || row < 0 || row >= model()->rowCount())
        return;
    // 원본이 없는 프록시에 건 setModelColumn은 무시되므로 지금 보기 열을 쓴다(역할 값은 모든 열이 같다).
    setCurrentIndex(model()->index(row, modelColumn()));
    scrollTo(currentIndex());
}

void ThumbnailListView::setPreviewMode(bool preview)
{
    setFocusPolicy(preview ? Qt::NoFocus : Qt::StrongFocus);
    setVerticalScrollBarPolicy(preview ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ThumbnailListView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    updateGrid();
}

void ThumbnailListView::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    switch (event->key()) {
    case Qt::Key_Insert:
    case Qt::Key_Space:
        if (mods == Qt::NoModifier) {
            toggleMark(model(), cursorRow());
            setCursorRow(cursorRow() + 1);
            return;
        }
        break;
    case Qt::Key_A:
        if (mods == Qt::ControlModifier) {
            markAll(model());
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        Q_EMIT rowActivated(currentIndex());
        return;
    case Qt::Key_Backspace:
        Q_EMIT upRequested();
        return;
    default:
        break;
    }
    QListView::keyPressEvent(event);
}

void ThumbnailListView::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT paneActivated();
    QListView::mousePressEvent(event);
}

void ThumbnailListView::focusInEvent(QFocusEvent *event)
{
    Q_EMIT paneActivated();
    QListView::focusInEvent(event);
}

void ThumbnailListView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        Q_EMIT sizeStepRequested(event->angleDelta().y() > 0 ? 1 : -1);
        event->accept();
        return;
    }
    QListView::wheelEvent(event);
}

void ThumbnailListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);
    Q_EMIT cursorRowChanged(current.row());
}

// ---------------------------------------------------------------- ThumbnailCardView (Qtitan)

class ThumbnailCardView::TileDelegate final : public QStyledItemDelegate
{
public:
    explicit TileDelegate(ThumbnailCardView *view) : QStyledItemDelegate(view), m_view(view) {}

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
        const QModelIndex idx = source.isValid() ? source : index;
        ThumbnailPainter::TileState state;
        state.active = detail::paneActive(m_view);
        state.cursor = idx.row() == m_view->cursorRow();
        ThumbnailPainter::paintTile(p, option.rect, idx, state, m_view->appearance(), fm::style::themeColorsFor(m_view),
                                    m_view->font());
    }

private:
    ThumbnailCardView *m_view;
};

// 카드 바탕만 목록 바탕색으로 — 선택 · 커서는 타일 그리기가 맡는다(Qtitan 기본 선택 · 포커스 그리기를 끈다).
class ThumbnailCardView::TileRecordPainter final : public Qtitan::GridRecordPainter
{
public:
    explicit TileRecordPainter(ThumbnailCardView *view) : m_view(view) {}
    void paintBackground(QPainter &p, const Record &record, QWidget *) override
    {
        p.fillRect(record.rect, fm::style::themeColorsFor(m_view)[Token::Surface]);
    }

private:
    ThumbnailCardView *m_view;
};

ThumbnailCardView::ThumbnailCardView(QWidget *parent)
    : QWidget(parent)
    , m_recordPainter(std::make_unique<TileRecordPainter>(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_grid = new Qtitan::CardGrid(this);
    m_grid->setViewType(Qtitan::CardGrid::CardView);  // 가로 우선 배치(CardViewVertical은 세로 우선)
    m_view = m_grid->view<Qtitan::GridCardView>();
    m_delegate = new TileDelegate(this);

    m_view->beginUpdate();
    Qtitan::GridViewOptions &o = m_view->options();
    o.setGroupsHeader(false);
    o.setMainMenuDisabled(true);
    o.setSelectionPolicy(Qtitan::GridViewOptions::IgnoreSelection);
    o.setFocusFrameEnabled(false);
    o.setFilterEnabled(false);
    o.setFindEnabled(false);
    o.setZoomEnabled(false);
    o.setDragEnabled(false);
    o.setShowWaitCursor(false);
    Qtitan::GridCardViewOptions &c = m_view->cardOptions();
    c.setItemTitleHeight(0);
    c.setColumnCaptionsVisible(false);  // 패치 Q7
    c.setItemMargin(4);
    c.setItemPadding(0);
    c.setMaximumItemCount(1000);  // 한 줄의 카드 수 상한(-1은 Qtitan이 처리하지 않는다)
    m_view->setRecordPainter(m_recordPainter.get());
    m_view->endUpdate();
    layout->addWidget(m_grid);
    setFocusProxy(m_grid);

    connect(m_view, &Qtitan::GridViewBase::focusRowChanged, this, [this](int, int row) { Q_EMIT cursorRowChanged(row); });
    connect(m_view, &Qtitan::GridViewBase::rowClicked, this, [this](Qtitan::RowClickEventArgs *) { Q_EMIT paneActivated(); });
    connect(m_view, &Qtitan::GridViewBase::rowDblClicked, this, [this](Qtitan::RowClickEventArgs *args) {
        Q_EMIT rowActivated(args->row().modelIndex(NameColumn));
    });
    m_grid->installEventFilter(this);
    applyAppearance();
}

ThumbnailCardView::~ThumbnailCardView()
{
    if (m_view)
        m_view->setRecordPainter(nullptr);
}

void ThumbnailCardView::setModel(QAbstractItemModel *model)
{
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    m_view->beginUpdate();
    m_view->setModel(model);
    m_view->endUpdate();
    if (model) {
        // 원본이 없는 프록시는 열이 없다 — 원본이 붙어 열이 생기면 다시 설정한다.
        // Qtitan이 먼저 초기화를 처리하도록 Qtitan 다음에 연결한다.
        connect(model, &QAbstractItemModel::modelReset, this, [this] { configureColumns(); });
    }
    configureColumns();
}

void ThumbnailCardView::configureColumns()
{
    if (!m_model || m_model->columnCount() < ColumnCount)
        return;
    if (m_view->getColumnCount() < ColumnCount) {
        m_view->beginUpdate();
        m_view->setModel(m_model);  // Qtitan은 모델을 줄 때만 열을 만든다
        m_view->endUpdate();
    }
    m_view->beginUpdate();
    for (int col = 0; col < m_view->getColumnCount(); ++col) {
        auto *column = static_cast<Qtitan::GridColumn *>(m_view->getColumn(col));
        column->setVisible(false);
    }
    if (auto *name = static_cast<Qtitan::GridColumn *>(m_view->getColumnByModelColumn(NameColumn))) {
        name->setVisible(true);
        name->setEditorType(Qtitan::GridEditor::DelegateAdapter);
        static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(name->editorRepository())->setDelegate(m_delegate);
        name->editorRepository()->setEditable(false);
        name->dataBinding()->setSortRole(Qt::ItemDataRole(NoSortRole));
    }
    m_view->endUpdate();
    applyAppearance();
}

void ThumbnailCardView::setAppearance(const ThumbnailAppearance &appearance)
{
    if (m_appearance == appearance)
        return;
    m_appearance = appearance;
    applyAppearance();
}

void ThumbnailCardView::applyAppearance()
{
    m_view->beginUpdate();
    Qtitan::GridViewOptions &o = m_view->options();
    o.setCellWidth(m_appearance.tileWidth());
    o.setCellHeight(ThumbnailPainter::tileHeight(m_appearance));
    o.setBackgroundColor(fm::style::themeColorsFor(this)[Token::Surface]);
    m_view->cardOptions().setItemWidth(m_appearance.tileWidth());
    m_view->endUpdate();
    m_grid->viewport()->update();
}

int ThumbnailCardView::columnsPerRow() const
{
    const int tw = m_appearance.tileWidth();
    return std::max(1, (m_grid->viewport()->width() - 4) / (tw + 4));
}

int ThumbnailCardView::cursorRow() const
{
    return m_view->focusedRowIndex();
}

void ThumbnailCardView::setCursorRow(int row)
{
    m_view->setFocusedRowIndex(row);
}

void ThumbnailCardView::setPreviewMode(bool preview)
{
    m_grid->setFocusPolicy(preview ? Qt::NoFocus : Qt::StrongFocus);
    m_view->options().setScrollBars(preview ? Qtitan::ScrollNone : Qtitan::ScrollAuto);
}

bool ThumbnailCardView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_grid)
        return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::MouseButtonPress:
        Q_EMIT paneActivated();
        break;
    case QEvent::Wheel: {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            Q_EMIT sizeStepRequested(wheel->angleDelta().y() > 0 ? 1 : -1);
            return true;
        }
        break;
    }
    case QEvent::KeyPress: {
        auto *key = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers mods = key->modifiers() & ~Qt::KeypadModifier;
        const int rows = m_model ? m_model->rowCount() : 0;
        const int row = cursorRow();
        auto moveTo = [&](int target) {
            if (rows > 0)
                setCursorRow(std::clamp(target, 0, rows - 1));
            return true;
        };
        switch (key->key()) {
        case Qt::Key_Insert:
        case Qt::Key_Space:
            if (mods == Qt::NoModifier) {
                toggleMark(m_model, row);
                return moveTo(row + 1);
            }
            break;
        case Qt::Key_A:
            if (mods == Qt::ControlModifier) {
                markAll(m_model);
                return true;
            }
            break;
        // 2차원 이동: 가로 우선 배치이므로 위 · 아래는 한 줄의 카드 수만큼
        case Qt::Key_Left:
            return moveTo(row - 1);
        case Qt::Key_Right:
            return moveTo(row + 1);
        case Qt::Key_Up:
            return moveTo(row - columnsPerRow());
        case Qt::Key_Down:
            return moveTo(row + columnsPerRow());
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_model && row >= 0)
                Q_EMIT rowActivated(m_model->index(row, NameColumn));
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
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------- ThumbnailInfoBar

ThumbnailInfoBar::ThumbnailInfoBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ThumbnailInfoBar::setSortText(const QString &text)
{
    m_sort = text;
    update();
}

void ThumbnailInfoBar::setSizeText(const QString &text)
{
    m_size = text;
    update();
}

QSize ThumbnailInfoBar::sizeHint() const
{
    return QSize(320, 27);
}

QSize ThumbnailInfoBar::minimumSizeHint() const
{
    return QSize(120, 27);
}

void ThumbnailInfoBar::paintEvent(QPaintEvent *)
{
    // 27 px(아래 1 --line), 좌우 12, 간격 8, 바탕 --head(시안2 --x-g2), 12 px --fg2
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    const QColor bg = tc.isWatercolor() ? fm::style::watercolorChrome(tc.variant()).g2 : tc[Token::Head];
    p.fillRect(rect(), bg);
    p.fillRect(QRect(0, height() - 1, width(), 1), tc[Token::Line]);
    const QFont font = fm::style::withTabularNumbers(fm::style::pixelFont(this->font(), 12));
    p.setFont(font);
    const QFontMetrics fm(font);
    const QRect text = rect().adjusted(12, 0, -12, -1);
    const QString label = u"정렬"_s;
    p.setPen(tc[Token::Fg2]);
    p.drawText(text, Qt::AlignLeft | Qt::AlignVCenter, label);
    const int sortLeft = text.left() + fm.horizontalAdvance(label) + 8;
    const int sizeWidth = fm.horizontalAdvance(m_size);
    p.setPen(tc[Token::Fg]);
    p.drawText(QRect(sortLeft, text.top(), std::max(0, text.right() - sizeWidth - 16 - sortLeft), text.height()),
               Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(m_sort, Qt::ElideRight, std::max(0, text.right() - sizeWidth - 16 - sortLeft)));
    p.setPen(tc[Token::Fg2]);
    p.drawText(text, Qt::AlignRight | Qt::AlignVCenter, m_size);
}

// ---------------------------------------------------------------- ThumbnailView

ThumbnailView::ThumbnailView(QWidget *parent)
    : QWidget(parent)
    , m_info(new ThumbnailInfoBar(this))
    , m_stack(new QStackedWidget(this))
    , m_list(new ThumbnailListView(this))
    , m_cards(new ThumbnailCardView(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_info);
    layout->addWidget(m_stack, 1);
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_cards);
    m_info->setSortText(u"이름 ↑ · 폴더 먼저"_s);
    setFocusProxy(m_list);

    connect(m_list, &ThumbnailListView::cursorRowChanged, this, [this](int row) {
        if (m_backend == QtList)
            Q_EMIT cursorRowChanged(row);
    });
    connect(m_cards, &ThumbnailCardView::cursorRowChanged, this, [this](int row) {
        if (m_backend == QtitanCards)
            Q_EMIT cursorRowChanged(row);
    });
    connect(m_list, &ThumbnailListView::rowActivated, this, &ThumbnailView::activated);
    connect(m_cards, &ThumbnailCardView::rowActivated, this, &ThumbnailView::activated);
    connect(m_list, &ThumbnailListView::upRequested, this, &ThumbnailView::upRequested);
    connect(m_cards, &ThumbnailCardView::upRequested, this, &ThumbnailView::upRequested);
    connect(m_list, &ThumbnailListView::paneActivated, this, &ThumbnailView::paneActivated);
    connect(m_cards, &ThumbnailCardView::paneActivated, this, &ThumbnailView::paneActivated);
    connect(m_list, &ThumbnailListView::sizeStepRequested, this, &ThumbnailView::stepSize);
    connect(m_cards, &ThumbnailCardView::sizeStepRequested, this, &ThumbnailView::stepSize);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] {
        m_list->viewport()->update();
        m_cards->setAppearance(m_appearance);
        update();
    });
    m_list->setAppearance(m_appearance);
    m_cards->setAppearance(m_appearance);
    updateInfo();
}

ThumbnailView::~ThumbnailView() = default;

ThumbnailBackend *ThumbnailView::current() const
{
    return m_backend == QtList ? static_cast<ThumbnailBackend *>(m_list) : static_cast<ThumbnailBackend *>(m_cards);
}

void ThumbnailView::setModel(QAbstractItemModel *model)
{
    m_model = model;
    m_list->setModel(model);
    m_cards->setModel(model);
}

void ThumbnailView::setBackend(Backend backend)
{
    if (m_backend == backend)
        return;
    const int row = cursorRow();
    const bool hadFocus = m_stack->currentWidget() && m_stack->currentWidget()->hasFocus();
    m_backend = backend;
    m_stack->setCurrentWidget(current()->widget());
    setFocusProxy(current()->widget());
    current()->setCursorRow(row);
    if (hadFocus)
        current()->widget()->setFocus();
    Q_EMIT backendChanged(backend);
}

void ThumbnailView::setAppearance(const ThumbnailAppearance &appearance)
{
    if (m_appearance == appearance)
        return;
    m_appearance = appearance;
    m_list->setAppearance(appearance);
    m_cards->setAppearance(appearance);
    updateInfo();
}

bool ThumbnailView::isPaneActive() const
{
    return detail::paneActive(this);
}

void ThumbnailView::setPaneActive(bool active)
{
    fm::style::setPaneActive(this, active);
    m_list->viewport()->update();
    const auto children = m_cards->findChildren<QWidget *>();
    for (QWidget *w : children)
        w->update();
}

bool ThumbnailView::isInfoBarVisible() const
{
    return m_info->isVisibleTo(this);
}

void ThumbnailView::setInfoBarVisible(bool visible)
{
    m_info->setVisible(visible);
}

void ThumbnailView::setSortText(const QString &text)
{
    m_info->setSortText(text);
}

void ThumbnailView::setPreviewMode(bool preview)
{
    m_list->setPreviewMode(preview);
    m_cards->setPreviewMode(preview);
}

int ThumbnailView::cursorRow() const
{
    return current()->cursorRow();
}

void ThumbnailView::setCursorRow(int row)
{
    current()->setCursorRow(row);
}

void ThumbnailView::stepSize(int delta)
{
    const auto *it = std::find(std::begin(kThumbnailSizes), std::end(kThumbnailSizes), m_appearance.size);
    int index = it == std::end(kThumbnailSizes) ? 1 : int(it - std::begin(kThumbnailSizes));
    index = std::clamp(index + delta, 0, int(std::size(kThumbnailSizes)) - 1);
    if (kThumbnailSizes[index] == m_appearance.size)
        return;
    ThumbnailAppearance a = m_appearance;
    a.size = kThumbnailSizes[index];
    setAppearance(a);
    Q_EMIT sizeChanged(a.size);
}

void ThumbnailView::updateInfo()
{
    QString name;
    switch (m_appearance.size) {
    case 64:  name = u"작게"_s; break;
    case 96:  name = u"보통"_s; break;
    case 160: name = u"크게"_s; break;
    case 256: name = u"아주 크게"_s; break;
    default:  name = u"사용자"_s; break;
    }
    m_info->setSizeText(u"%1 · %2 px · Ctrl+휠"_s.arg(name).arg(m_appearance.size));
}

} // namespace fm::filelist
