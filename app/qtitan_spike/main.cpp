// QtitanDataGrid 시험 앱 (P1) — 패치한 Qtitan 그리드에 FmStyle · WatercolorStyle이 어디까지 적용되는지 판정한다.
//   qtitan_spike                                    창으로 실행 (위쪽에서 디자인 · 색 구성표 · 보기 전환)
//   qtitan_spike --design watercolor --scheme dark  시작 상태 지정
//   qtitan_spike --mode 2 --sep tint --name-below   2줄 레코드 · 메타 행 틴트 · 이름 아래
//   qtitan_spike --thumb                            섬네일(Qtitan 카드 보기 + 패치 Q7)
//   qtitan_spike --toggle-design-at 400             실행 중 디자인 전환 확인
//   qtitan_spike --shot out.png                     스크린샷을 저장하고 끝냄
//
// 왼쪽 패널은 활성(fmPaneActive = true), 오른쪽은 비활성이다. 두 패널 모두 1 · 4 · 5번 표시(선택), 2번 커서(RecordLayouts 보드와 같음).
// 선택은 Qtitan 선택이 아니라 모델의 MarkedRole이다(TC 방식 — 커서 이동과 표시가 따로). 레코드 바탕 · 선택 · 커서 틀은
// 패치 Q3의 GridRecordPainter가, 글자는 DelegateAdapter에 건 델리게이트가 그린다.

#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QtnGrid.h>
#include <QtnCardGrid.h>
#include <QtnGridBandedTableView.h>
#include <QtnGridCardView.h>

#include <QApplication>
#include <QComboBox>
#include <QCommandLineParser>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
namespace fs = fm::style;
using T = fs::Token;

namespace {

enum Column { ColIcon, ColName, ColType, ColSize, ColDate, ColAttr, ColFill, ColumnCount };
constexpr int MarkedRole = Qt::UserRole + 1;
constexpr int TintRole = Qt::UserRole + 2;
constexpr int ExtRole = Qt::UserRole + 3;

enum class Separator { Zebra, Line, Space, Tint };

struct Sample
{
    QString stem;
    QString ext;
    QString type;
    QString size;
    QString date;
    fs::Token tint;
    bool marked;
};

const QList<Sample> &samples()
{
    static const QList<Sample> list = {
        {u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols"_s, u".exe"_s, u"응용 프로그램"_s, u"3.74 GB"_s, u"2026-09-26 18:02"_s, T::KExe, true},
        {u"2026년 3분기 보안 감사 보고서 — 커널 미니필터 드라이버 서명 정책 검토 (최종본 v3)"_s, u".pdf"_s, u"PDF 문서"_s, u"12.4 MB"_s, u"2026-09-25 16:11"_s, T::KPdf, false},
        {u"Screenshot 2026-09-27 231455 - 듀얼 패널 레이아웃 비교 (밴드 2줄 vs 단일 행)"_s, u".png"_s, u"PNG 이미지"_s, u"2.1 MB"_s, u"2026-09-27 23:14"_s, T::KImg, false},
        {u"vc_redist.x64"_s, u".exe"_s, u"응용 프로그램"_s, u"24.4 MB"_s, u"2026-09-22 11:07"_s, T::KExe, true},
        {u"sysinternals-suite_process-monitor_procexp_autoruns_2026-09"_s, u".zip"_s, u"압축(ZIP) 폴더"_s, u"51.3 MB"_s, u"2026-09-20 09:33"_s, T::KZip, true},
        {u"CMakeLists"_s, u".txt"_s, u"텍스트 문서"_s, u"6.8 KB"_s, u"2026-09-27 22:31"_s, T::KCode, false},
        {u"README"_s, u".md"_s, u"Markdown 문서"_s, u"8.3 KB"_s, u"2026-09-24 21:15"_s, T::KDoc, false},
        {u"desktop"_s, u".ini"_s, u"구성 설정"_s, u"282 B"_s, u"2026-09-01 08:00"_s, T::KSys, false},
    };
    return list;
}

// 목업의 문서 아이콘(모서리 접힌 종이 + 종류 색 띠)
QIcon fileIcon(const QColor &line, const QColor &tint)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(20, 20) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(20.0 / 16.0, 20.0 / 16.0);
    p.setPen(QPen(line, 1.0, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    QPainterPath page;
    page.moveTo(3.5, 1.5);
    page.lineTo(9.1, 1.5);
    page.lineTo(12.5, 4.9);
    page.lineTo(12.5, 14.5);
    page.lineTo(3.5, 14.5);
    page.closeSubpath();
    p.drawPath(page);
    p.drawPolyline(QPolygonF{{9.0, 1.6}, {9.0, 5.0}, {12.4, 5.0}});
    p.setPen(Qt::NoPen);
    p.setBrush(tint);
    p.drawRoundedRect(QRectF(5.0, 8.6, 6.0, 3.6), 0.6, 0.6);
    return QIcon(pm);
}

QStandardItemModel *createModel(QObject *parent)
{
    auto *model = new QStandardItemModel(0, ColumnCount, parent);
    model->setHorizontalHeaderLabels({QString(), u"이름"_s, u"종류"_s, u"크기"_s, u"수정한 날짜"_s, u"속성"_s, QString()});
    for (const Sample &s : samples()) {
        QList<QStandardItem *> row;
        for (int c = 0; c < ColumnCount; ++c)
            row << new QStandardItem;
        row[ColName]->setText(s.stem + s.ext);
        row[ColName]->setData(s.ext, ExtRole);
        row[ColType]->setText(s.type);
        row[ColSize]->setText(s.size);
        row[ColDate]->setText(s.date);
        row[ColAttr]->setText(u"-a--"_s);
        row[ColIcon]->setData(int(s.tint), TintRole);
        for (QStandardItem *item : row)
            item->setData(s.marked, MarkedRole);
        model->appendRow(row);
    }
    return model;
}

void refreshIcons(QStandardItemModel *model)
{
    const fs::ThemeColors &tc = fs::ThemeManager::instance().colors();
    for (int r = 0; r < model->rowCount(); ++r) {
        QStandardItem *item = model->item(r, ColIcon);
        item->setIcon(fileIcon(tc[T::Fg3], tc[fs::Token(item->data(TintRole).toInt())]));
    }
}

bool paneActive(const QWidget *w)
{
    for (; w; w = w->parentWidget()) {
        const QVariant v = w->property(fs::props::kPaneActive);
        if (v.isValid())
            return v.toBool();
    }
    return false;
}

bool watercolor() { return fs::ThemeManager::instance().design() == fs::Design::Watercolor; }

struct ViewSettings
{
    int mode = 2;          // 1줄 · 2줄
    bool nameBelow = false;
    Separator separator = Separator::Line;
};

// ---------------------------------------------------------------- 레코드 그리기 (패치 Q3)
class RecordPainter final : public Qtitan::GridRecordPainter
{
public:
    explicit RecordPainter(const ViewSettings &settings) : m_settings(settings) {}

    void paintBackground(QPainter &p, const Record &record, QWidget *widget) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(widget);
        const bool active = paneActive(widget);
        const bool marked = record.index.data(MarkedRole).toBool();
        const bool two = m_settings.mode == 2;
        const QColor sel = active ? tc[T::Sel] : tc[T::SelIn];
        QRect r = record.rect;
        p.fillRect(r, tc[T::Surface]);

        switch (m_settings.separator) {
        case Separator::Zebra:
            if (record.alternate)
                p.fillRect(r, tc[T::Alt]);
            if (marked)
                p.fillRect(r, sel);
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Grid]);
            break;
        case Separator::Line:
            if (marked)
                p.fillRect(r, sel);
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), two ? tc[T::Line] : tc[T::Grid]);
            break;
        case Separator::Space:
            if (marked) {
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(Qt::NoPen);
                p.setBrush(sel);
                p.drawRoundedRect(QRectF(r).adjusted(6, 1, -6, -1), 6, 6);
            }
            break;
        case Separator::Tint: {
            if (marked)
                p.fillRect(r, sel);
            const int meta = m_settings.nameBelow ? 0 : 1;
            if (two && record.lines.size() > meta && !record.lines[meta].isNull()) {
                QRectF band = QRectF(record.lines[meta]).adjusted(2, 1, 0, -1);
                const QColor tint = marked ? (watercolor() ? QColor(255, 255, 255, 41) : tc[T::TintSel]) : tc[T::Tint];
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(Qt::NoPen);
                p.setBrush(tint);
                p.drawRoundedRect(band, 4, 4);
            }
            break;
        }
        }
    }

    void paintOverlay(QPainter &p, const Record &record, QWidget *widget) override
    {
        if (!(record.state & QStyle::State_HasFocus))
            return;
        const fs::ThemeColors &tc = fs::themeColorsFor(widget);
        const bool active = paneActive(widget);
        QPen pen;
        if (watercolor()) {
            if (!active)
                return;  // 워터컬러: 비활성 패널에는 커서가 없다
            pen = QPen(tc[T::Focus], 1.0, Qt::DotLine);
        } else {
            pen = active ? QPen(tc[T::Focus], 1.0) : QPen(tc[T::Fg3], 1.0, Qt::DashLine);
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        const QRectF r = QRectF(record.rect).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_settings.separator == Separator::Space) {
            p.setRenderHint(QPainter::Antialiasing);
            p.drawRoundedRect(r.adjusted(6, 1, -6, -1), 6, 6);
        } else {
            p.drawRect(r);
        }
    }

private:
    const ViewSettings &m_settings;
};

// ---------------------------------------------------------------- 글자 (DelegateAdapter)
class TextDelegate final : public QStyledItemDelegate
{
public:
    TextDelegate(QWidget *pane, const ViewSettings &settings, QObject *parent)
        : QStyledItemDelegate(parent), m_pane(pane), m_settings(settings) {}

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(m_pane);
        const bool active = paneActive(m_pane);
        // DelegateAdapter는 내부 대리 모델의 인덱스를 넘긴다. 원래 모델 인덱스는 QueryIndexRole로 얻는다.
        const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
        const QModelIndex &idx = source.isValid() ? source : index;
        const bool marked = idx.data(MarkedRole).toBool();
        const bool onAccent = marked && active && watercolor();  // 워터컬러 활성 패널의 선택 = 흰 글자
        const QRect r = option.rect.adjusted(8, 0, -8, 0);
        p->save();
        if (idx.column() == ColName) {
            QFont font = option.font;
            if (marked)
                font.setWeight(QFont::DemiBold);
            p->setFont(font);
            const QFontMetrics fm(font);
            const QString ext = idx.data(ExtRole).toString();
            const QString full = idx.data(Qt::DisplayRole).toString();
            QString stem = full.left(full.size() - ext.size());
            const int extWidth = fm.horizontalAdvance(ext);
            if (fm.horizontalAdvance(full) > r.width())
                stem = fm.elidedText(stem, Qt::ElideMiddle, qMax(0, r.width() - extWidth));
            const int stemWidth = fm.horizontalAdvance(stem);
            p->setPen(onAccent ? tc[T::OnAccent] : tc[T::Fg]);
            p->drawText(QRect(r.left(), r.top(), stemWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, stem);
            p->setPen(onAccent ? tc[T::OnAccent] : tc[T::Fg3]);
            p->drawText(QRect(r.left() + stemWidth, r.top(), extWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, ext);
        } else {
            QFont font = option.font;
            const bool meta = m_settings.mode == 2;
            if (meta)
                font.setPixelSize(12);
            p->setFont(font);
            QColor color = meta ? (marked ? tc[T::Fg2] : tc[T::Fg3]) : tc[T::Fg];
            if (onAccent)
                color = tc[T::OnAccent];
            p->setPen(color);
            const Qt::Alignment align = (idx.column() == ColSize ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
            p->drawText(r, int(align), QFontMetrics(font).elidedText(idx.data().toString(), Qt::ElideRight, r.width()));
        }
        p->restore();
    }

private:
    QWidget *m_pane;
    const ViewSettings &m_settings;
};

// ---------------------------------------------------------------- 패널
class Pane : public QWidget
{
public:
    Pane(QAbstractItemModel *model, bool active, const ViewSettings &settings, QWidget *parent)
        : QWidget(parent), m_settings(settings), m_recordPainter(settings)
    {
        fs::setPaneActive(this, active);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_grid = new Qtitan::Grid(this);
        m_grid->setViewType(Qtitan::Grid::BandedTableView);
        m_view = m_grid->view<Qtitan::GridBandedTableView>();
        m_delegate = new TextDelegate(this, settings, this);

        m_view->beginUpdate();
        m_view->setModel(model);
        Qtitan::GridViewOptions &o = m_view->options();
        o.setGroupsHeader(false);
        o.setMainMenuDisabled(true);
        o.setGridLines(Qtitan::LinesNone);
        o.setSelectionPolicy(Qtitan::GridViewOptions::IgnoreSelection);
        o.setAlternatingRowColors(true);
        o.setFocusFrameEnabled(false);
        o.setColumnHidingEnabled(false);
        o.setColumnMovingEnabled(false);
        o.setFilterEnabled(false);
        Qtitan::GridTableViewOptions &t = m_view->tableOptions();
        t.setColumnAutoWidth(true);
        t.setRowsQuickSelection(false);
        t.setColumnsQuickCustomization(false);
        t.setColumnsQuickMenuVisible(false);
        t.setFrozenPlaceQuickSelection(false);
        m_view->bandedOptions().setBandsHeader(false);
        m_view->bandedOptions().setBandsQuickCustomization(false);
        for (int c = 0; c < ColumnCount; ++c) {
            auto *col = static_cast<Qtitan::GridColumn *>(m_view->getColumnByModelColumn(c));
            const Qt::Alignment align = (c == ColSize ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
            col->setTextAlignment(align);
            if (c == ColIcon) {
                col->editorRepository()->setAlignment(Qt::AlignCenter);
                continue;
            }
            col->setEditorType(Qtitan::GridEditor::DelegateAdapter);
            static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(col->editorRepository())->setDelegate(m_delegate);
        }
        m_view->getColumnByModelColumn(ColFill)->setCaption(QString());
        m_view->setRecordPainter(&m_recordPainter);
        m_view->endUpdate();
        layout->addWidget(m_grid);
    }

    void apply()
    {
        const ViewSettings &s = m_settings;
        m_view->beginUpdate();
        while (m_view->bandCount() > 0)
            m_view->removeBand(0);
        Qtitan::GridTableBand *iconBand = m_view->addBand(QString());
        Qtitan::GridTableBand *fileBand = m_view->addBand(u"파일"_s);
        auto column = [this](int modelColumn) {
            return static_cast<Qtitan::GridBandedTableColumn *>(m_view->getColumnByModelColumn(modelColumn));
        };
        const bool two = s.mode == 2;
        const int nameRow = two && s.nameBelow ? 1 : 0;
        const int metaRow = two ? 1 - nameRow : 0;

        auto *icon = column(ColIcon);
        icon->setBandIndex(two ? iconBand->index() : fileBand->index());
        icon->setRowIndex(0);
        icon->setRowSpan(two ? 2 : 1);
        icon->setWidth(two ? 40 : 28);
        icon->setMinWidth(icon->width());
        icon->setMaxWidth(icon->width());
        icon->setCaption(QString());
        iconBand->setVisible(two);

        auto *name = column(ColName);
        name->setBandIndex(fileBand->index());
        name->setRowIndex(nameRow);
        name->setRowSpan(1);

        const int widths[] = {0, 0, 140, 84, 136, 52, 0};
        for (int c : {ColType, ColSize, ColDate, ColAttr, ColFill}) {
            auto *col = column(c);
            col->setBandIndex(fileBand->index());
            col->setRowIndex(metaRow);
            col->setRowSpan(1);
            if (widths[c] > 0)
                col->setWidth(widths[c]);
            col->setVisible(two || c != ColFill);
        }
        m_view->options().setCellHeight(two ? 22 : 24);
        m_view->endUpdate();
        m_view->setFocusedRowIndex(1);  // RecordLayouts: 2번 커서
        m_grid->viewport()->update();
    }

private:
    const ViewSettings &m_settings;
    RecordPainter m_recordPainter;
    TextDelegate *m_delegate = nullptr;
    Qtitan::Grid *m_grid = nullptr;
    Qtitan::GridBandedTableView *m_view = nullptr;
};

// ---------------------------------------------------------------- 섬네일 (Qtitan CardGrid · 패치 Q7)
constexpr int kTileWidth = 124;
constexpr int kTileImage = 96;

// 타일 한 장: 그림 칸(가짜 섬네일 = 종류 색 판 + 확장자 배지) + 이름 두 줄. Main 보드의 섬네일 보기를 따른다.
class TileDelegate final : public QStyledItemDelegate
{
public:
    TileDelegate(QWidget *pane, QObject *parent) : QStyledItemDelegate(parent), m_pane(pane) {}

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QModelIndex source = index.data(Qtitan::QueryIndexRole).value<QModelIndex>();
        const QModelIndex idx = (source.isValid() ? source : index).siblingAtColumn(ColName);
        const fs::ThemeColors &tc = fs::themeColorsFor(m_pane);
        const bool marked = idx.data(MarkedRole).toBool();
        const bool onAccent = marked && paneActive(m_pane) && watercolor();
        const fs::Token tint = fs::Token(idx.siblingAtColumn(ColIcon).data(TintRole).toInt());
        const QRect r = option.rect;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF art(r.center().x() - kTileImage / 2.0, r.top() + 6, kTileImage, kTileImage);
        QColor fill = tc[tint];
        fill.setAlpha(46);
        p->setPen(QPen(tc[T::Line], 1.0));
        p->setBrush(fill);
        p->drawRoundedRect(art.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        // 확장자 배지(96 px 이상에서만 — 설정 › 섬네일 보기)
        const QString ext = idx.data(ExtRole).toString().mid(1).toUpper();
        QFont badgeFont = option.font;
        badgeFont.setPixelSize(10);
        badgeFont.setWeight(QFont::DemiBold);
        p->setFont(badgeFont);
        const QFontMetrics bfm(badgeFont);
        const QRectF badge(art.right() - bfm.horizontalAdvance(ext) - 12, art.bottom() - 20, bfm.horizontalAdvance(ext) + 8, 16);
        p->setPen(Qt::NoPen);
        p->setBrush(tc[tint]);
        p->drawRoundedRect(badge, 3, 3);
        p->setPen(tc[T::OnAccent]);
        p->drawText(badge, Qt::AlignCenter, ext);
        // 이름 두 줄 가운데 정렬
        QFont font = option.font;
        if (marked)
            font.setWeight(QFont::DemiBold);
        p->setFont(font);
        p->setPen(onAccent ? tc[T::OnAccent] : tc[T::Fg]);
        const QRect text(r.left() + 4, int(art.bottom()) + 6, r.width() - 8, QFontMetrics(font).lineSpacing() * 2);
        const QString name = idx.data().toString();
        QString elided = name;
        const QFontMetrics fm(font);
        if (fm.boundingRect(text, Qt::AlignHCenter | Qt::TextWrapAnywhere, name).height() > text.height())
            elided = fm.elidedText(name, Qt::ElideMiddle, text.width() * 2 - 12);
        p->drawText(text, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWrapAnywhere, elided);
        p->restore();
    }

private:
    QWidget *m_pane;
};

class TileRecordPainter final : public Qtitan::GridRecordPainter
{
public:
    void paintBackground(QPainter &p, const Record &record, QWidget *widget) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(widget);
        if (!record.index.siblingAtColumn(ColName).data(MarkedRole).toBool())
            return;
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(paneActive(widget) ? tc[T::Sel] : tc[T::SelIn]);
        p.drawRoundedRect(QRectF(record.rect).adjusted(1, 1, -1, -1), 6, 6);
    }

    void paintOverlay(QPainter &p, const Record &record, QWidget *widget) override
    {
        if (!(record.state & QStyle::State_HasFocus) || !paneActive(widget))
            return;
        const fs::ThemeColors &tc = fs::themeColorsFor(widget);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(tc[T::Focus], 1.0, watercolor() ? Qt::DotLine : Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(record.rect).adjusted(1.5, 1.5, -1.5, -1.5), 6, 6);
    }
};

class ThumbPane : public QWidget
{
public:
    ThumbPane(QAbstractItemModel *model, bool active, QWidget *parent) : QWidget(parent)
    {
        fs::setPaneActive(this, active);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_grid = new Qtitan::CardGrid(this);
        m_grid->setViewType(Qtitan::CardGrid::CardView);  // 가로 우선 배치(CardViewVertical은 세로 우선)
        auto *view = m_grid->view<Qtitan::GridCardView>();
        view->beginUpdate();
        view->setModel(model);
        Qtitan::GridViewOptions &o = view->options();
        o.setGroupsHeader(false);
        o.setMainMenuDisabled(true);
        o.setSelectionPolicy(Qtitan::GridViewOptions::IgnoreSelection);
        o.setFocusFrameEnabled(false);
        o.setFilterEnabled(false);
        o.setCellWidth(kTileWidth);
        o.setCellHeight(20);
        Qtitan::GridCardViewOptions &c = view->cardOptions();
        c.setItemTitleHeight(0);
        c.setColumnCaptionsVisible(false);
        c.setItemMargin(4);
        c.setItemPadding(0);
        c.setMaximumItemCount(64);
        for (int col = 0; col < ColumnCount; ++col)
            view->getColumnByModelColumn(col)->setVisible(col == ColName);
        auto *name = static_cast<Qtitan::GridColumn *>(view->getColumnByModelColumn(ColName));
        name->setRowSpan(8);
        name->setEditorType(Qtitan::GridEditor::DelegateAdapter);
        static_cast<Qtitan::GridDelegateAdapterEditorRepository *>(name->editorRepository())->setDelegate(new TileDelegate(this, this));
        view->setRecordPainter(&m_recordPainter);
        view->endUpdate();
        view->setFocusedRowIndex(1);
        layout->addWidget(m_grid);
    }

private:
    TileRecordPainter m_recordPainter;
    Qtitan::CardGrid *m_grid = nullptr;
};

QComboBox *combo(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

Separator separatorFrom(const QString &name)
{
    if (name == u"zebra")
        return Separator::Zebra;
    if (name == u"space")
        return Separator::Space;
    if (name == u"tint")
        return Separator::Tint;
    return Separator::Line;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption designOption(u"design"_s, u"standard | watercolor"_s, u"name"_s, u"standard"_s);
    const QCommandLineOption schemeOption(u"scheme"_s, u"light | dark"_s, u"name"_s, u"light"_s);
    const QCommandLineOption modeOption(u"mode"_s, u"1 | 2"_s, u"n"_s, u"2"_s);
    const QCommandLineOption sepOption(u"sep"_s, u"zebra | line | space | tint"_s, u"name"_s, u"line"_s);
    const QCommandLineOption nameBelowOption(u"name-below"_s, u"2줄 레코드에서 이름을 아래 줄에"_s);
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냅니다."_s, u"file"_s);
    const QCommandLineOption delayOption(u"shot-delay"_s, u"스크린샷까지 기다릴 시간(ms)."_s, u"ms"_s, u"600"_s);
    const QCommandLineOption toggleOption(u"toggle-design-at"_s, u"시작 후 이 시간(ms)에 디자인을 한 번 바꿉니다."_s, u"ms"_s);
    const QCommandLineOption thumbOption(u"thumb"_s, u"섬네일(Qtitan 카드 보기)로 비교합니다."_s);
    parser.addOptions({designOption, schemeOption, modeOption, sepOption, nameBelowOption, shotOption, delayOption, toggleOption, thumbOption});
    parser.process(app);

    auto &theme = fs::ThemeManager::instance();
    theme.setDesign(parser.value(designOption) == u"watercolor"_s ? fs::Design::Watercolor : fs::Design::Standard);
    theme.setScheme(parser.value(schemeOption) == u"dark"_s ? fs::ThemeManager::Scheme::Dark : fs::ThemeManager::Scheme::Light);
    theme.install(app);

    ViewSettings settings;
    settings.mode = parser.value(modeOption).toInt() == 1 ? 1 : 2;
    settings.nameBelow = parser.isSet(nameBelowOption);
    settings.separator = separatorFrom(parser.value(sepOption));

    QWidget window;
    window.setWindowTitle(u"QtitanDataGrid 시험 — FmStyle 적용 판정"_s);
    auto *root = new QVBoxLayout(&window);

    auto *bar = new QHBoxLayout;
    auto *design = combo({u"시안1 · 기본"_s, u"시안2 · 워터컬러"_s}, int(theme.design()), &window);
    auto *scheme = combo({u"라이트"_s, u"다크"_s}, theme.scheme() == fs::ThemeManager::Scheme::Dark ? 1 : 0, &window);
    auto *mode = combo({u"1줄"_s, u"2줄"_s}, settings.mode - 1, &window);
    auto *sep = combo({u"교차 배경"_s, u"구분선"_s, u"여백 · 계층"_s, u"메타 행 틴트"_s}, int(settings.separator), &window);
    auto *namePos = combo({u"이름 위"_s, u"이름 아래"_s}, settings.nameBelow ? 1 : 0, &window);
    for (auto [label, box] : {std::pair{u"디자인"_s, design}, {u"색 구성표"_s, scheme}, {u"레코드"_s, mode},
                              {u"구분"_s, sep}, {u"이름 위치"_s, namePos}}) {
        bar->addWidget(new QLabel(label, &window));
        bar->addWidget(box);
    }
    bar->addStretch();
    root->addLayout(bar);

    QStandardItemModel *model = createModel(&window);
    refreshIcons(model);

    auto *splitter = new QSplitter(&window);
    auto *left = new Pane(model, true, settings, splitter);
    auto *right = new Pane(model, false, settings, splitter);
    if (parser.isSet(thumbOption)) {
        left->hide();
        right->hide();
        splitter->addWidget(new ThumbPane(model, true, splitter));
        splitter->addWidget(new ThumbPane(model, false, splitter));
    } else {
        splitter->addWidget(left);
        splitter->addWidget(right);
    }
    root->addWidget(splitter, 1);

    auto applyAll = [&] {
        left->apply();
        right->apply();
    };
    applyAll();

    QObject::connect(design, &QComboBox::currentIndexChanged, &window, [&](int i) {
        QMetaObject::invokeMethod(&window, [&, i] { theme.setDesign(i == 1 ? fs::Design::Watercolor : fs::Design::Standard); }, Qt::QueuedConnection);
    });
    QObject::connect(scheme, &QComboBox::currentIndexChanged, &window, [&](int i) {
        QMetaObject::invokeMethod(&window, [&, i] { theme.setScheme(i == 1 ? fs::ThemeManager::Scheme::Dark : fs::ThemeManager::Scheme::Light); }, Qt::QueuedConnection);
    });
    QObject::connect(mode, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.mode = i + 1;
        applyAll();
    });
    QObject::connect(sep, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.separator = Separator(i);
        applyAll();
    });
    QObject::connect(namePos, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.nameBelow = i == 1;
        applyAll();
    });
    QObject::connect(&theme, &fs::ThemeManager::changed, &window, [&] {
        refreshIcons(model);
        applyAll();
    });

    window.resize(1440, 520);
    window.show();

    // 실행 중 디자인 전환 확인용: 시작 후 한 번 디자인을 바꾼다.
    if (parser.isSet(toggleOption)) {
        QTimer::singleShot(parser.value(toggleOption).toInt(), &window, [&] {
            theme.setDesign(theme.design() == fs::Design::Watercolor ? fs::Design::Standard : fs::Design::Watercolor);
        });
    }

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
