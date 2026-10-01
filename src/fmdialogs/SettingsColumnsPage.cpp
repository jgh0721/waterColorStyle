// 설정 › 열 · 사용자 정의 열(docs/specs/05 §2.2) — 골격은 .ui, 세트 목록 · 열 표(칸 안 편집) · 2줄 배치 미리보기 ·
// 열 편집은 코드. 2줄 미리보기는 메인 창과 같은 Qtitan FileListView에 bandPreviewLayout()을 준다.

#include "SettingsPages_p.h"

#include "ui_SettingsColumnsPage.h"

#include <fmfilelist/ColumnSets.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QCompleter>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QToolButton>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;
using CD = fl::ColumnDef;
using Rule = fl::ColumnSetRule;

enum SetRole { TagRole = Qt::UserRole + 1, TagAutoRole, SummaryRole };
enum ColumnRole { CustomRole = Qt::UserRole + 1, KindTextRole, SourceRole, WidthRole, CheckRole, BandRole, NameRowRole };
enum TableColumn { NumberCol, TitleCol, ValueCol, WidthCol, AlignCol, OneLineCol, BandCol, TableColumnCount };

struct KnownFolder
{
    const char *id;
    const char *label;
};
constexpr KnownFolder kKnownFolders[] = {
    {"Downloads", "다운로드"}, {"Documents", "문서"}, {"Pictures", "사진"},
    {"Music", "음악"},         {"Videos", "동영상"},  {"Desktop", "바탕 화면"},
};

/// 추가할 수 있는 열(기본 필드 + 자주 쓰는 Windows 속성).
struct ColumnChoice
{
    const char *title;
    CD::Kind kind;
    const char *source;
    int width;
    Qt::Alignment align;
    CD::Sort sort;
};
const ColumnChoice kColumnChoices[] = {
    {"확장자", CD::Kind::BuiltinField, "확장자", 64, Qt::AlignLeft, CD::Sort::Text},
    {"종류", CD::Kind::BuiltinField, "종류", 140, Qt::AlignLeft, CD::Sort::Text},
    {"크기", CD::Kind::BuiltinField, "크기", 84, Qt::AlignRight, CD::Sort::Number},
    {"수정한 날짜", CD::Kind::BuiltinField, "수정한 날짜", 136, Qt::AlignLeft, CD::Sort::Date},
    {"만든 날짜", CD::Kind::BuiltinField, "만든 날짜", 136, Qt::AlignLeft, CD::Sort::Date},
    {"접근한 날짜", CD::Kind::BuiltinField, "접근한 날짜", 136, Qt::AlignLeft, CD::Sort::Date},
    {"속성", CD::Kind::BuiltinField, "속성", 52, Qt::AlignLeft, CD::Sort::Text},
    {"경로", CD::Kind::BuiltinField, "경로", 220, Qt::AlignLeft, CD::Sort::Text},
    {"촬영 날짜", CD::Kind::WindowsProperty, "System.Photo.DateTaken", 136, Qt::AlignLeft, CD::Sort::Date},
    {"카메라", CD::Kind::WindowsProperty, "System.Photo.CameraModel", 140, Qt::AlignLeft, CD::Sort::Text},
    {"재생 시간", CD::Kind::WindowsProperty, "System.Media.Duration", 72, Qt::AlignRight, CD::Sort::Number},
    {"작성자", CD::Kind::WindowsProperty, "System.Author", 140, Qt::AlignLeft, CD::Sort::Text},
    {"제목", CD::Kind::WindowsProperty, "System.Title", 180, Qt::AlignLeft, CD::Sort::Text},
    {"태그", CD::Kind::WindowsProperty, "System.Keywords", 140, Qt::AlignLeft, CD::Sort::Text},
    {"파일 버전", CD::Kind::WindowsProperty, "System.FileVersion", 96, Qt::AlignLeft, CD::Sort::NumberTuple},
};

QStringList builtinFields()
{
    return {u"이름 (확장자 포함)"_s, u"이름"_s,      u"확장자"_s,      u"크기"_s, u"종류"_s,
            u"수정한 날짜"_s,        u"만든 날짜"_s, u"접근한 날짜"_s, u"속성"_s, u"경로"_s};
}

QStringList knownProperties()
{
    return {u"System.Photo.DateTaken"_s,    u"System.Photo.CameraModel"_s, u"System.Photo.CameraManufacturer"_s,
            u"System.Image.HorizontalSize"_s, u"System.Image.VerticalSize"_s, u"System.Image.Dimensions"_s,
            u"System.Media.Duration"_s,     u"System.Video.FrameRate"_s,   u"System.Audio.EncodingBitrate"_s,
            u"System.Author"_s,             u"System.Title"_s,             u"System.Keywords"_s,
            u"System.Company"_s,            u"System.FileVersion"_s,       u"System.Size"_s,
            u"System.DateModified"_s,       u"System.DateCreated"_s,       u"System.ItemTypeText"_s};
}

qreal radiusFor(const fs::ThemeColors &tc, qreal r)
{
    return tc.isWatercolor() ? 0 : r;
}

// ------------------------------------------------------------------------------------- 세트 목록

/// 세트 항목(05 §2.2.1) — 최소 높이 56, 이름 + 태그 / 조건 요약, 선택 = --accent-soft + 안쪽 1 px --accent.
class SetItemDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 56);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const QRect r = option.rect;
        p->save();
        if (index.row() > 0)
            p->fillRect(QRect(r.left(), r.top(), r.width(), 1), tc[T::Grid]);
        if (option.state & QStyle::State_Selected) {
            p->fillRect(r, tc[T::AccentSoft]);
            p->setPen(QPen(tc[T::Accent], 1.0));
            p->drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
        }
        const qreal left = r.left() + 12, right = r.right() - 12;
        const qreal top = r.top() + (r.height() - 18 - 2 - 15) / 2.0;
        const QFont nameFont = fs::pixelFont(option.font, 13);
        const QString tag = index.data(TagRole).toString();
        const QSize tagSize = fm::ui::chipSize(tag, 10.5, 5, 16);
        const qreal nameWidth = right - left - tagSize.width() - 6;
        const QString name = QFontMetrics(nameFont).elidedText(index.data().toString(), Qt::ElideRight, int(nameWidth));
        p->setFont(nameFont);
        p->setPen(tc[T::Fg]);
        p->drawText(QRectF(left, top, nameWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, name);
        const qreal tx = left + QFontMetrics(nameFont).horizontalAdvance(name) + 6;
        p->setRenderHint(QPainter::Antialiasing);
        fm::ui::paintChip(p, QRectF(tx, top + 1, tagSize.width(), tagSize.height()), tag,
                          index.data(TagAutoRole).toBool() ? fm::ui::ChipKind::Ok : fm::ui::ChipKind::Mute, tc, radiusFor(tc, 3), 10.5);
        const QFont summaryFont = fs::pixelFont(option.font, 11.5);
        p->setFont(summaryFont);
        p->setPen(tc[T::Fg3]);
        p->drawText(QRectF(left, top + 20, right - left, 15), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(summaryFont).elidedText(index.data(SummaryRole).toString(), Qt::ElideRight, int(right - left)));
        p->restore();
    }
};

// ------------------------------------------------------------------------------------- 열 표

/// 열 표 칸(05 §2.2.3) — 행 30, 선택 = --sel 채움. 너비 · 정렬 · 2줄 위치는 칸 안에서 편집한다.
class ColumnTableDelegate final : public QStyledItemDelegate
{
public:
    std::function<void(int row, int column, const QVariant &value)> commit;

    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 30);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const QRect r = option.rect;
        const QModelIndex first = index.siblingAtColumn(TitleCol);
        p->save();
        p->fillRect(r, (option.state & QStyle::State_Selected) ? tc[T::Sel] : tc[T::Surface]);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Grid]);
        const QRect text = r.adjusted(8, 0, -8, 0);
        switch (index.column()) {
        case NumberCol:
            p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 11.5)));
            p->setPen(tc[T::Fg3]);
            p->drawText(text, Qt::AlignRight | Qt::AlignVCenter, index.data().toString());
            break;
        case TitleCol: {
            const QFont f = fs::pixelFont(option.font, 13);
            p->setFont(f);
            p->setPen(tc[T::Fg]);
            const bool custom = first.data(CustomRole).toBool();
            const QString chip = tr("사용자");
            const QSize chipSize = fm::ui::chipSize(chip, 10.5, 5, 16);
            const int width = text.width() - (custom ? chipSize.width() + 6 : 0);
            const QString name = QFontMetrics(f).elidedText(index.data().toString(), Qt::ElideRight, width);
            p->drawText(QRect(text.left(), text.top(), width, text.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
            if (custom) {
                p->setRenderHint(QPainter::Antialiasing);
                const qreal x = text.left() + QFontMetrics(f).horizontalAdvance(name) + 6;
                fm::ui::paintChip(p, QRectF(x, r.center().y() - 7.5, chipSize.width(), chipSize.height()), chip, fm::ui::ChipKind::Accent,
                                  tc, radiusFor(tc, 3), 10.5);
            }
            break;
        }
        case ValueCol: {
            const QFont kindFont = fs::pixelFont(option.font, 11.5);
            const QString kind = index.data(KindTextRole).toString() + u" · "_s;
            p->setFont(kindFont);
            p->setPen(tc[T::Fg3]);
            p->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, kind);
            const int kx = QFontMetrics(kindFont).horizontalAdvance(kind);
            const QFont mono = fs::monoFont(12);
            p->setFont(mono);
            p->setPen(tc[T::Fg2]);
            p->drawText(text.adjusted(kx, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                        QFontMetrics(mono).elidedText(index.data(SourceRole).toString(), Qt::ElideRight, text.width() - kx));
            break;
        }
        case WidthCol:
        case AlignCol:
            p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 12)));
            p->setPen(tc[T::Fg2]);
            p->drawText(text, (index.column() == WidthCol ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, index.data().toString());
            break;
        case OneLineCol:
            if (index.data(CheckRole).toBool())
                fs::paintGlyph(p, fs::Glyph::Check, QRectF(r.center().x() - 7, r.center().y() - 7, 14, 14), tc[T::AccentFg]);
            break;
        case BandCol: {
            const auto pos = CD::BandPos(index.data(BandRole).toInt());
            const QString label = fl::bandPosLabel(pos);
            const QSize s = fm::ui::chipSize(label, 11.5, 7, 20);
            p->setRenderHint(QPainter::Antialiasing);
            fm::ui::paintChip(p, QRectF(text.left(), r.center().y() - 10, s.width(), s.height()), label,
                              pos == CD::BandPos::Row0Full ? fm::ui::ChipKind::Accent
                              : pos == CD::BandPos::Row1   ? fm::ui::ChipKind::Mute
                                                           : fm::ui::ChipKind::Dashed,
                              tc, radiusFor(tc, 4), 11.5, false);
            break;
        }
        default:
            break;
        }
        p->restore();
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        switch (index.column()) {
        case WidthCol: {
            auto *spin = new QSpinBox(parent);
            spin->setRange(40, 600);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            spin->setFrame(false);
            return spin;
        }
        case AlignCol: {
            auto *combo = new QComboBox(parent);
            combo->addItems({tr("왼쪽"), tr("가운데"), tr("오른쪽")});
            return combo;
        }
        case BandCol: {
            auto *combo = new QComboBox(parent);
            combo->addItem(fl::bandPosLabel(CD::BandPos::Row1), int(CD::BandPos::Row1));
            combo->addItem(fl::bandPosLabel(CD::BandPos::Hidden), int(CD::BandPos::Hidden));
            return combo;
        }
        default:
            return nullptr;
        }
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *spin = qobject_cast<QSpinBox *>(editor))
            spin->setValue(index.data(WidthRole).toInt());
        else if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            if (index.column() == AlignCol)
                combo->setCurrentText(index.data().toString());
            else
                combo->setCurrentIndex(std::max(0, combo->findData(index.data(BandRole).toInt())));
        }
    }
    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override
    {
        if (!commit)
            return;
        if (auto *spin = qobject_cast<QSpinBox *>(editor))
            commit(index.row(), index.column(), spin->value());
        else if (auto *combo = qobject_cast<QComboBox *>(editor))
            commit(index.row(), index.column(), index.column() == AlignCol ? QVariant(combo->currentIndex()) : combo->currentData());
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        editor->setGeometry(option.rect.adjusted(1, 1, -1, -1));
    }
};

// ------------------------------------------------------------------------------------- 칩 한 개

class ChipLabel final : public QWidget
{
public:
    ChipLabel(const QString &text, fm::ui::ChipKind kind, QWidget *parent)
        : QWidget(parent)
        , m_text(text)
        , m_kind(kind)
    {
        setAccessibleName(text);
    }
    QSize sizeHint() const override { return fm::ui::chipSize(m_text, 10.5, 5, 16); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QSize s = sizeHint();
        fm::ui::paintChip(&p, QRectF(0, (height() - s.height()) / 2.0, s.width(), s.height()), m_text, m_kind, tc, radiusFor(tc, 3), 10.5);
    }

private:
    QString m_text;
    fm::ui::ChipKind m_kind;
};

Qt::Alignment alignOf(int index)
{
    return index == 1 ? Qt::AlignHCenter : index == 2 ? Qt::AlignRight : Qt::AlignLeft;
}
int alignIndex(Qt::Alignment a)
{
    return (a & Qt::AlignHCenter) ? 1 : (a & Qt::AlignRight) ? 2 : 0;
}

fl::Kind kindForExtension(const QString &ext)
{
    static const QStringList images{u"jpg"_s, u"jpeg"_s, u"png"_s, u"heic"_s, u"webp"_s, u"arw"_s, u"mp4"_s, u"mkv"_s, u"mov"_s};
    static const QStringList code{u"cpp"_s, u"h"_s, u"hpp"_s, u"cxx"_s, u"cmake"_s, u"txt"_s};
    if (images.contains(ext, Qt::CaseInsensitive))
        return fl::Kind::Img;
    if (code.contains(ext, Qt::CaseInsensitive))
        return fl::Kind::Code;
    if (ext.compare(u"exe"_s, Qt::CaseInsensitive) == 0 || ext.compare(u"msi"_s, Qt::CaseInsensitive) == 0)
        return fl::Kind::Exe;
    return fl::Kind::Doc;
}

// ------------------------------------------------------------------------------------- 페이지

class ColumnsPage final : public SettingsPage
{
public:
    ColumnsPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsColumnsPage>())
    {
        ui->setupUi(this);
        ui->newSetButton->setGlyph(fm::ui::glyph::PlusSmall);
        ui->addColumnButton->setGlyph(fm::ui::glyph::PlusSmall);
        ui->setsToolbarLine->setFixedHeight(1);
        ui->columnsToolbarLine->setFixedHeight(1);
        ui->ruleDivider->setFixedWidth(1);
        ui->ruleDivider->setFixedHeight(20);
        for (QToolButton *b : {ui->duplicateSetButton, ui->deleteSetButton, ui->columnUpButton, ui->columnDownButton, ui->columnRemoveButton})
            b->setFixedSize(28, 28);
        ui->ratioSpin->setFont(fs::withTabularNumbers(ui->ratioSpin->font()));
        ui->pathEdit->setFont(fs::monoFont(12.5));
        ui->sourceEdit->setFont(fs::monoFont(12.5));
        ui->sourceWarning->hide();
        m_customTag = new ChipLabel(tr("사용자 정의 열"), fm::ui::ChipKind::Accent, this);
        ui->editHeader->insertWidget(1, m_customTag);
        m_completerModel = new QStringListModel(this);
        auto *completer = new QCompleter(m_completerModel, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        ui->sourceEdit->setCompleter(completer);

        buildSetList();
        buildRule();
        buildTable();
        buildEditor();
        refreshIcons();
        connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] { refreshIcons(); });
    }

    QString pageId() const override { return u"columns"_s; }
    QString title() const override { return tr("열 · 사용자 정의 열"); }
    QString description() const override
    {
        return tr("폴더 종류마다 열 세트를 두고, 각 열이 1줄과 2줄 표시에서 어디에 놓일지 정합니다.");
    }
    fm::settings::Sections sections() const override { return Section::Columns; }
    QList<QAbstractButton *> footerButtons() override
    {
        if (!m_import)
            m_import = makeImportExportButton();
        return {m_import};
    }
    QStringList searchKeywords() const override
    {
        QStringList words = SettingsPage::searchKeywords();
        words << tr("열 세트") << tr("자동 적용") << tr("열 추가") << tr("사용자 정의 열") << tr("2줄 배치 미리보기") << tr("열 이름")
              << tr("값 종류") << tr("값이 없을 때") << tr("정렬 기준");
        for (const fl::ColumnSet &s : pending().columns.sets)
            words.append(s.name);
        return words;
    }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        const auto &sets = pending().columns.sets;
        m_set = std::clamp(m_set, 0, int(sets.size()) - 1);
        syncSetList();
        syncRule();
        syncTable();
        syncEditor();
        syncBandPreview();
    }

    /// 목업 보드: 세트 "사진 · 영상", 열 4번 "해상도" 선택.
    void showBoardState() override
    {
        m_set = 1;
        m_column = 3;
        syncFromPending();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // 1줄 칸 — Space로 켜고 끈다
        if (watched == ui->columnTable && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space && ui->columnTable->currentIndex().column() == OneLineCol) {
            toggleOneLine(ui->columnTable->currentIndex().row());
            return true;
        }
        return SettingsPage::eventFilter(watched, event);
    }

private:
    const fl::ColumnSet *currentSet() const
    {
        const auto &sets = pending().columns.sets;
        return m_set >= 0 && m_set < sets.size() ? &sets.at(m_set) : nullptr;
    }
    const CD *currentColumn() const
    {
        const fl::ColumnSet *set = currentSet();
        return set && m_column >= 0 && m_column < set->columns.size() ? &set->columns.at(m_column) : nullptr;
    }
    void editSet(const std::function<void(fl::ColumnSet &)> &mutate)
    {
        const int i = m_set;
        session()->edit(Section::Columns, [&](AppSettings &p) {
            if (i >= 0 && i < p.columns.sets.size())
                mutate(p.columns.sets[i]);
        });
    }
    void editColumn(int row, const std::function<void(CD &)> &mutate)
    {
        editSet([&](fl::ColumnSet &set) {
            if (row >= 0 && row < set.columns.size())
                mutate(set.columns[row]);
        });
    }
    std::pair<QStringList, QStringList> groupNamesAndIds() const
    {
        QStringList names, ids;
        for (const fl::FileGroup &g : pending().groups.groups) {
            names.append(g.name);
            ids.append(g.id);
        }
        return {names, ids};
    }
    QString uniqueId(const QString &prefix, const QStringList &taken) const
    {
        int n = 1;
        while (taken.contains(u"%1-%2"_s.arg(prefix).arg(n)))
            ++n;
        return u"%1-%2"_s.arg(prefix).arg(n);
    }

    // ---------------------------------------------------------------- 세트 목록

    void buildSetList()
    {
        m_setModel = new QStandardItemModel(this);
        ui->setList->setModel(m_setModel);
        ui->setList->setItemDelegate(new SetItemDelegate(ui->setList));
        ui->setList->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->setList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->setList->setUniformItemSizes(true);
        connect(ui->setList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
            if (m_syncing || !index.isValid() || index.row() == m_set)
                return;
            m_set = index.row();
            m_column = 0;
            syncFromPending();
        });
        connect(ui->newSetButton, &QPushButton::clicked, this, [this] { addSet(false); });
        connect(ui->duplicateSetButton, &QToolButton::clicked, this, [this] { addSet(true); });
        connect(ui->deleteSetButton, &QToolButton::clicked, this, [this] { deleteSet(); });
        auto shortcut = [this](QWidget *scope, const QKeySequence &key, auto slot) {
            auto *s = new QShortcut(key, scope, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
            connect(s, &QShortcut::activated, this, slot);
        };
        shortcut(ui->setsCard, QKeySequence(Qt::CTRL | Qt::Key_D), [this] { addSet(true); });
        shortcut(ui->setsCard, QKeySequence(Qt::Key_Delete), [this] { deleteSet(); });
        addCustomItem(ui->setsCard, [](const AppSettings &a, const AppSettings &b) { return a.columns.sets != b.columns.sets; },
                      [](AppSettings &p, const AppSettings &d) { p.columns.sets = d.columns.sets; }, Section::Columns);
    }

    void syncSetList()
    {
        const auto &sets = pending().columns.sets;
        const auto [names, ids] = groupNamesAndIds();
        m_syncing = true;
        if (m_setModel->rowCount() != sets.size()) {
            m_setModel->clear();
            for (int i = 0; i < sets.size(); ++i)
                m_setModel->appendRow(new QStandardItem());
        }
        for (int i = 0; i < sets.size(); ++i) {
            const fl::ColumnSet &s = sets.at(i);
            QStandardItem *item = m_setModel->item(i);
            const QString summary = fl::columnSetRuleSummary(s, names, ids);
            item->setText(s.name);
            item->setData(fl::columnSetTag(s), TagRole);
            item->setData(!s.isDefault() && s.autoApply && s.rule.type != Rule::Type::Manual, TagAutoRole);
            item->setData(summary, SummaryRole);
            item->setData(u"%1 (%2) — %3"_s.arg(s.name, fl::columnSetTag(s), summary), Qt::AccessibleTextRole);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        if (m_set >= 0 && m_set < sets.size())
            ui->setList->selectionModel()->setCurrentIndex(m_setModel->index(m_set, 0), QItemSelectionModel::ClearAndSelect);
        ui->setList->setFixedHeight(56 * int(sets.size()) + 2);
        m_syncing = false;
        const fl::ColumnSet *set = currentSet();
        ui->deleteSetButton->setEnabled(set && !set->isDefault());
    }

    void addSet(bool duplicate)
    {
        const auto &sets = pending().columns.sets;
        const fl::ColumnSet *source = currentSet();
        if (sets.isEmpty() || (duplicate && !source))
            return;
        fl::ColumnSet set;
        if (duplicate) {
            set = *source;
            set.name = tr("%1 복사본").arg(source->name);
            if (set.isDefault()) {
                set.rule = Rule{};
                set.autoApply = false;
            }
        } else {
            set.name = tr("새 세트");
            set.columns = sets.first().columns;
        }
        QStringList taken;
        for (const fl::ColumnSet &s : sets)
            taken.append(s.id);
        set.id = uniqueId(u"set"_s, taken);
        const int at = int(sets.size());
        m_set = at;
        m_column = 0;
        session()->edit(Section::Columns, [&](AppSettings &p) { p.columns.sets.append(set); });
    }

    void deleteSet()
    {
        const fl::ColumnSet *set = currentSet();
        if (!set || set->isDefault())
            return;
        const int at = m_set;
        m_column = 0;
        session()->edit(Section::Columns, [&](AppSettings &p) { p.columns.sets.removeAt(at); });
    }

    // ---------------------------------------------------------------- 적용 조건

    void buildRule()
    {
        connect(ui->autoSwitch, &QCheckBox::toggled, this, [this](bool on) {
            editSet([&](fl::ColumnSet &set) {
                set.autoApply = on;
                if (on && set.rule.type == Rule::Type::Manual) {
                    set.rule.type = Rule::Type::GroupRatio;
                    if (set.rule.groupId.isEmpty() && !pending().groups.groups.isEmpty())
                        set.rule.groupId = pending().groups.groups.first().id;
                }
            });
        });
        connect(ui->ruleCombo, &QComboBox::activated, this, [this](int index) {
            const QString key = ui->ruleCombo->itemData(index).toString();
            editSet([&](fl::ColumnSet &set) {
                if (key.startsWith(u"group:")) {
                    set.rule.type = Rule::Type::GroupRatio;
                    set.rule.groupId = key.mid(6);
                } else if (key == u"path") {
                    set.rule.type = Rule::Type::PathWildcard;
                    if (set.rule.pathPattern.isEmpty())
                        set.rule.pathPattern = u"D:\\Work\\*"_s;
                } else if (key.startsWith(u"known:")) {
                    set.rule.type = Rule::Type::KnownFolder;
                    set.rule.knownFolder = key.mid(6);
                }
            });
        });
        connect(ui->ratioSpin, &QSpinBox::valueChanged, this, [this](int v) {
            if (!m_syncing)
                editSet([&](fl::ColumnSet &set) { set.rule.ratioPercent = v; });
        });
        connect(ui->pathEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
            editSet([&](fl::ColumnSet &set) { set.rule.pathPattern = text; });
        });
    }

    void syncRule()
    {
        const fl::ColumnSet *set = currentSet();
        if (!set)
            return;
        m_syncing = true;
        ui->ruleCard->setAccessibleName(tr("%1 세트 적용 조건").arg(set->name));
        const bool isDefault = set->isDefault();
        {
            const QSignalBlocker block(ui->autoSwitch);
            ui->autoSwitch->setChecked(isDefault || set->autoApply);
        }
        ui->autoSwitch->setEnabled(!isDefault);
        ui->autoLabel->setEnabled(!isDefault);
        ui->ruleCombo->setVisible(!isDefault);
        ui->ruleStack->setVisible(!isDefault);
        if (isDefault) {
            ui->rulePrefix->setText(tr("모든 폴더 — 다른 세트가 맞지 않을 때 씁니다"));
            m_syncing = false;
            return;
        }
        // 콤보: 그룹 · {이름} / 경로 · 와일드카드 / 알려진 폴더 · {폴더}
        ui->ruleCombo->clear();
        for (const fl::FileGroup &g : pending().groups.groups)
            ui->ruleCombo->addItem(tr("그룹 · %1").arg(g.name), u"group:"_s + g.id);
        ui->ruleCombo->addItem(tr("경로 · 와일드카드"), u"path"_s);
        for (const KnownFolder &k : kKnownFolders)
            ui->ruleCombo->addItem(tr("알려진 폴더 · %1").arg(QString::fromUtf8(k.label)), u"known:"_s + QString::fromLatin1(k.id));
        QString key;
        switch (set->rule.type) {
        case Rule::Type::PathWildcard: key = u"path"_s; break;
        case Rule::Type::KnownFolder: key = u"known:"_s + set->rule.knownFolder; break;
        default: key = u"group:"_s + set->rule.groupId; break;
        }
        ui->ruleCombo->setCurrentIndex(std::max(0, ui->ruleCombo->findData(key)));
        const bool path = set->rule.type == Rule::Type::PathWildcard;
        const bool known = set->rule.type == Rule::Type::KnownFolder;
        ui->rulePrefix->setText(path || known ? tr("경로가") : tr("폴더의 파일 중"));
        ui->ruleStack->setCurrentWidget(path ? ui->pathPage : known ? ui->knownPage : ui->ratioPage);
        ui->ratioSpin->setValue(set->rule.ratioPercent);
        if (ui->pathEdit->text() != set->rule.pathPattern)
            ui->pathEdit->setText(set->rule.pathPattern);
        const bool enabled = set->autoApply;
        for (QWidget *w : std::initializer_list<QWidget *>{ui->rulePrefix, ui->ruleCombo, ui->ruleStack})
            w->setEnabled(enabled);
        m_syncing = false;
    }

    // ---------------------------------------------------------------- 열 표

    void buildTable()
    {
        m_tableModel = new QStandardItemModel(0, TableColumnCount, this);
        m_tableModel->setHorizontalHeaderLabels({u"#"_s, tr("열"), tr("값"), tr("너비"), tr("정렬"), tr("1줄"), tr("2줄 위치")});
        m_tableModel->setHeaderData(NumberCol, Qt::Horizontal, int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        m_tableModel->setHeaderData(WidthCol, Qt::Horizontal, int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        m_tableModel->setHeaderData(OneLineCol, Qt::Horizontal, int(Qt::AlignCenter), Qt::TextAlignmentRole);
        QTreeView *table = ui->columnTable;
        table->setModel(m_tableModel);
        auto *delegate = new ColumnTableDelegate(table);
        delegate->commit = [this](int row, int column, const QVariant &value) {
            editColumn(row, [&](CD &d) {
                if (column == WidthCol)
                    d.width = value.toInt();
                else if (column == AlignCol)
                    d.align = alignOf(value.toInt());
                else if (column == BandCol)
                    d.bandPos = CD::BandPos(value.toInt());
            });
        };
        table->setItemDelegate(delegate);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
        table->setIndentation(0);
        table->installEventFilter(this);
        fs::setFlatHeader(table);
        QHeaderView *header = table->header();
        header->setStretchLastSection(false);
        header->setSectionResizeMode(QHeaderView::Fixed);
        header->setSectionResizeMode(ValueCol, QHeaderView::Stretch);
        header->setSectionsMovable(false);
        const int widths[] = {28, 112, 0, 58, 58, 38, 104};
        for (int c = 0; c < TableColumnCount; ++c) {
            if (widths[c] > 0)
                header->resizeSection(c, widths[c]);
        }
        connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex &index) {
            if (m_syncing || !index.isValid())
                return;
            m_column = index.row();
            syncEditor();
            syncTableButtons();
        });
        connect(table, &QTreeView::clicked, this, [this](const QModelIndex &index) {
            if (index.column() == OneLineCol)
                toggleOneLine(index.row());
        });

        connect(ui->addColumnButton, &QPushButton::clicked, this, [this] { showAddColumnMenu(); });
        connect(ui->customColumnButton, &QPushButton::clicked, this, [this] { addCustomColumn(); });
        connect(ui->columnUpButton, &QToolButton::clicked, this, [this] { moveColumn(m_column, m_column - 1); });
        connect(ui->columnDownButton, &QToolButton::clicked, this, [this] { moveColumn(m_column, m_column + 1); });
        connect(ui->columnRemoveButton, &QToolButton::clicked, this, [this] { removeColumn(); });
        auto shortcut = [this](const QKeySequence &key, auto slot) {
            auto *s = new QShortcut(key, ui->columnsCard, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
            connect(s, &QShortcut::activated, this, slot);
        };
        shortcut(QKeySequence(Qt::ALT | Qt::Key_Up), [this] { moveColumn(m_column, m_column - 1); });
        shortcut(QKeySequence(Qt::ALT | Qt::Key_Down), [this] { moveColumn(m_column, m_column + 1); });
        shortcut(QKeySequence(Qt::Key_Delete), [this] { removeColumn(); });

        // 2줄 배치 미리보기 — 한 레코드
        m_band = new fl::FileListView(ui->bandCard);
        m_band->setObjectName(u"bandPreview"_s);
        m_band->setPreviewMode(true);
        m_band->setViewMode(fl::ViewMode::TwoLine);
        m_band->setPaneActive(true);
        m_band->setAttribute(Qt::WA_TransparentForMouseEvents);
        fl::ListAppearance a;
        a.twoLineSeparator = fl::RecordSeparator::None;
        m_band->setAppearance(a);
        ui->bandCardLayout->addWidget(m_band);
    }

    void syncTable()
    {
        const fl::ColumnSet *set = currentSet();
        const int count = set ? int(set->columns.size()) : 0;
        m_syncing = true;
        if (m_tableModel->rowCount() != count) {
            m_tableModel->removeRows(0, m_tableModel->rowCount());
            for (int i = 0; i < count; ++i) {
                QList<QStandardItem *> row;
                for (int c = 0; c < TableColumnCount; ++c)
                    row.append(new QStandardItem());
                m_tableModel->appendRow(row);
            }
        }
        m_column = std::clamp(m_column, 0, std::max(0, count - 1));
        for (int i = 0; i < count; ++i) {
            const CD &d = set->columns.at(i);
            const bool nameRow = d.bandPos == CD::BandPos::Row0Full;
            auto item = [&](int c) { return m_tableModel->item(i, c); };
            item(NumberCol)->setText(QString::number(i + 1));
            item(TitleCol)->setText(d.title);
            item(TitleCol)->setData(d.custom, CustomRole);
            item(TitleCol)->setData(nameRow, NameRowRole);
            item(TitleCol)->setData(tr("%1, %2 · %3, %4, %5").arg(d.title, fl::columnKindLabel(d.kind), d.source, fl::columnAlignLabel(d.align),
                                                                   fl::bandPosLabel(d.bandPos)),
                                    Qt::AccessibleTextRole);
            item(ValueCol)->setData(fl::columnKindLabel(d.kind), KindTextRole);
            item(ValueCol)->setData(d.source, SourceRole);
            item(ValueCol)->setToolTip(d.source);
            item(WidthCol)->setText(d.width < 0 ? tr("나머지") : QString::number(d.width));
            item(WidthCol)->setData(d.width, WidthRole);
            item(AlignCol)->setText(fl::columnAlignLabel(d.align));
            item(OneLineCol)->setData(d.showInSingleLine, CheckRole);
            item(OneLineCol)->setData(d.showInSingleLine ? tr("1줄에 표시") : tr("1줄에 표시 안 함"), Qt::AccessibleTextRole);
            item(BandCol)->setData(int(d.bandPos), BandRole);
            item(BandCol)->setData(fl::bandPosLabel(d.bandPos), Qt::AccessibleTextRole);
            for (int c = 0; c < TableColumnCount; ++c) {
                Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
                // 이름 열(행 0 · 전체)은 너비 = 나머지, 2줄 위치 고정
                if (c == AlignCol || ((c == WidthCol || c == BandCol) && !nameRow))
                    flags |= Qt::ItemIsEditable;
                item(c)->setFlags(flags);
            }
        }
        if (count > 0 && ui->columnTable->currentIndex().row() != m_column)  // 칸 편집기를 닫지 않게
            ui->columnTable->selectionModel()->setCurrentIndex(m_tableModel->index(m_column, TitleCol),
                                                                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        ui->columnTable->setFixedHeight(ui->columnTable->header()->sizeHint().height() + 30 * count + 2);
        m_syncing = false;
        syncTableButtons();
    }

    void syncTableButtons()
    {
        const fl::ColumnSet *set = currentSet();
        const CD *d = currentColumn();
        const int count = set ? int(set->columns.size()) : 0;
        ui->columnUpButton->setEnabled(m_column > 0);
        ui->columnDownButton->setEnabled(m_column < count - 1);
        ui->columnRemoveButton->setEnabled(d && d->bandPos != CD::BandPos::Row0Full);
    }

    void toggleOneLine(int row)
    {
        editColumn(row, [](CD &d) { d.showInSingleLine = !d.showInSingleLine; });
    }

    void insertColumn(CD def)
    {
        const fl::ColumnSet *set = currentSet();
        if (!set)
            return;
        QStringList taken;
        for (const CD &d : set->columns)
            taken.append(d.id);
        if (def.id.isEmpty() || taken.contains(def.id))
            def.id = uniqueId(def.custom ? u"custom"_s : u"column"_s, taken);
        const int at = std::clamp(m_column + 1, 0, int(set->columns.size()));
        m_column = at;
        editSet([&](fl::ColumnSet &s) { s.columns.insert(at, def); });
    }

    void showAddColumnMenu()
    {
        const fl::ColumnSet *set = currentSet();
        if (!set)
            return;
        QMenu menu(this);
        bool separated = false;
        for (const ColumnChoice &c : kColumnChoices) {
            const QString source = QString::fromUtf8(c.source);
            const bool present = std::any_of(set->columns.begin(), set->columns.end(), [&](const CD &d) { return d.source == source; });
            if (c.kind == CD::Kind::WindowsProperty && !separated) {
                menu.addSection(tr("Windows 속성"));
                separated = true;
            }
            QAction *action = menu.addAction(QString::fromUtf8(c.title));
            action->setEnabled(!present);
            connect(action, &QAction::triggered, this, [this, c, source] {
                CD d;
                d.title = QString::fromUtf8(c.title);
                d.kind = c.kind;
                d.source = source;
                d.width = c.width;
                d.align = c.align;
                d.sort = c.sort;
                d.bandPos = CD::BandPos::Row1;
                insertColumn(d);
            });
        }
        menu.exec(ui->addColumnButton->mapToGlobal(QPoint(0, ui->addColumnButton->height())));
    }

    void addCustomColumn()
    {
        CD d;
        d.title = tr("새 열");
        d.kind = CD::Kind::Expression;
        d.custom = true;
        d.width = 100;
        d.bandPos = CD::BandPos::Row1;
        d.empty = CD::Empty::Blank;
        insertColumn(d);
        ui->sourceEdit->setFocus();
    }

    void moveColumn(int from, int to)
    {
        const fl::ColumnSet *set = currentSet();
        if (!set || from < 0 || to < 0 || from >= set->columns.size() || to >= set->columns.size() || from == to)
            return;
        m_column = to;
        editSet([&](fl::ColumnSet &s) { s.columns.move(from, to); });
    }

    void removeColumn()
    {
        const CD *d = currentColumn();
        if (!d || d->bandPos == CD::BandPos::Row0Full)
            return;
        const int at = m_column;
        editSet([&](fl::ColumnSet &s) { s.columns.removeAt(at); });
        ui->columnTable->setFocus();
    }

    // ---------------------------------------------------------------- 2줄 배치 미리보기

    void syncBandPreview()
    {
        const fl::ColumnSet *set = currentSet();
        if (!set || !m_band)
            return;
        if (m_bandShown && *m_bandShown == set->columns)
            return;
        m_bandShown = set->columns;
        QList<int> rowColumns;
        const fl::ListColumnLayout layout = fl::bandPreviewLayout(*set, &rowColumns);
        auto *model = new QStandardItemModel(1, layout.requiredColumns(), this);
        QString sample = u"2026-09-14_제주_001.jpg"_s;
        for (const CD &d : set->columns) {
            if (d.bandPos == CD::BandPos::Row0Full && !d.sample.isEmpty())
                sample = d.sample;
        }
        const qsizetype dot = sample.lastIndexOf(u'.');
        const QString stem = dot > 0 ? sample.left(dot) : sample;
        const QString ext = dot > 0 ? sample.mid(dot + 1) : QString();
        const fl::Kind kind = kindForExtension(ext);
        for (int c = 0; c < model->columnCount(); ++c) {
            auto *item = new QStandardItem();
            item->setData(stem, fl::StemRole);
            item->setData(ext, fl::ExtRole);
            item->setData(sample, fl::FullNameRole);
            item->setData(int(kind), fl::KindRole);
            item->setData(false, fl::IsDirRole);
            item->setData(false, fl::IsUpRole);
            item->setData(false, fl::HiddenRole);
            item->setData(false, fl::MarkedRole);
            item->setData(0, fl::PreviewStateRole);  // 선택 · 커서 없이
            if (c == 1)
                item->setText(sample);
            else if (c >= 2 && c - 2 < rowColumns.size()) {
                const CD &d = set->columns.at(rowColumns.at(c - 2));
                item->setText(d.sample.isEmpty() ? u"—"_s : d.sample);
            }
            model->setItem(0, c, item);
        }
        m_band->setColumnLayout(layout);
        m_band->setModel(model);
        delete m_bandModel;
        m_bandModel = model;
        m_band->setFixedHeight(m_band->preferredHeight(1));
    }

    // ---------------------------------------------------------------- 열 편집

    void buildEditor()
    {
        connect(ui->titleEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
            editColumn(m_column, [&](CD &d) { d.title = text; });
        });
        connect(ui->kindSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            if (!m_syncing)
                editColumn(m_column, [&](CD &d) { d.kind = CD::Kind(i); });
        });
        connect(ui->sourceEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
            editColumn(m_column, [&](CD &d) { d.source = text; });
        });
        connect(ui->emptyCombo, &QComboBox::activated, this, [this](int index) {
            const QString key = ui->emptyCombo->itemData(index).toString();
            editColumn(m_column, [&](CD &d) {
                if (key == u"dash")
                    d.empty = CD::Empty::Dash;
                else if (key == u"blank")
                    d.empty = CD::Empty::Blank;
                else {
                    d.empty = CD::Empty::Fallback;
                    d.fallbackColumnId = key.mid(9);
                }
            });
        });
        connect(ui->sortCombo, &QComboBox::activated, this, [this](int index) {
            editColumn(m_column, [&](CD &d) { d.sort = CD::Sort(index); });
        });
    }

    void syncEditor()
    {
        const fl::ColumnSet *set = currentSet();
        const CD *d = currentColumn();
        ui->editCard->setEnabled(d != nullptr);
        if (!d) {
            ui->editTitle->setText(tr("열 편집"));
            m_customTag->hide();
            return;
        }
        m_syncing = true;
        ui->editTitle->setText(tr("열 편집 — %1").arg(d->title));
        m_customTag->setVisible(d->custom);
        if (ui->titleEdit->text() != d->title)
            ui->titleEdit->setText(d->title);
        const bool nameRow = d->bandPos == CD::BandPos::Row0Full;
        ui->kindSegment->setCurrentIndex(int(d->kind));
        ui->kindSegment->setEnabled(!nameRow);
        ui->sourceEdit->setEnabled(!nameRow);
        static const char *sourceLabels[] = {"필드(&X)", "속성 이름(&X)", "식(&X)"};
        ui->sourceLabel->setText(tr(sourceLabels[int(d->kind)]));
        if (ui->sourceEdit->text() != d->source)
            ui->sourceEdit->setText(d->source);
        m_completerModel->setStringList(d->kind == CD::Kind::BuiltinField ? builtinFields() : knownProperties());
        validateSource(*d);

        ui->emptyCombo->clear();
        ui->emptyCombo->addItem(u"—"_s, u"dash"_s);
        ui->emptyCombo->addItem(tr("비워 둠"), u"blank"_s);
        for (const CD &other : set->columns) {
            if (other.id != d->id)
                ui->emptyCombo->addItem(tr("%1로 대신").arg(other.title), u"fallback:"_s + other.id);
        }
        const QString emptyKey = d->empty == CD::Empty::Dash    ? u"dash"_s
                                 : d->empty == CD::Empty::Blank ? u"blank"_s
                                                                : u"fallback:"_s + d->fallbackColumnId;
        ui->emptyCombo->setCurrentIndex(std::max(0, ui->emptyCombo->findData(emptyKey)));
        ui->sortCombo->setCurrentIndex(int(d->sort));
        const bool hasValues = !d->folderValues.isEmpty();
        ui->folderLabel->setVisible(hasValues);
        ui->folderValues->setVisible(hasValues);
        ui->folderValues->setText(d->folderValues.join(u" · "_s));
        m_syncing = false;
    }

    /// 식: 닫히지 않은 [ · 모르는 속성, Windows 속성: 정규 이름 모양(System.xxx) — 입력 아래 경고.
    void validateSource(const CD &d)
    {
        QString warning;
        if (d.kind == CD::Kind::Expression) {
            int depth = 0;
            for (const QChar ch : d.source) {
                if (ch == u'[')
                    ++depth;
                else if (ch == u']')
                    --depth;
                if (depth < 0 || depth > 1)
                    break;
            }
            if (depth != 0) {
                warning = tr("닫히지 않은 [ ]가 있습니다");
            } else {
                static const QRegularExpression placeholder(u"\\[([^\\]]*)\\]"_s);
                QStringList unknown;
                for (auto it = placeholder.globalMatch(d.source); it.hasNext();) {
                    const QString name = it.next().captured(1).trimmed();
                    if (!knownProperties().contains(name) && !builtinFields().contains(name))
                        unknown.append(name);
                }
                if (!unknown.isEmpty())
                    warning = tr("모르는 속성: %1").arg(unknown.join(u", "_s));
            }
        } else if (d.kind == CD::Kind::WindowsProperty) {
            static const QRegularExpression canonical(u"^System(\\.[A-Za-z0-9]+)+$"_s);
            if (!d.source.isEmpty() && !canonical.match(d.source).hasMatch())
                warning = tr("Windows 속성의 정규 이름이 아닙니다(예: System.Photo.DateTaken)");
        }
        fs::setInvalid(ui->sourceEdit, !warning.isEmpty());
        ui->sourceWarning->setText(warning);
        ui->sourceWarning->setVisible(!warning.isEmpty());
    }

    void refreshIcons()
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        fs::GlyphStateColors colors;
        colors.normal = tc[T::Fg2];
        QColor disabled = tc[T::Fg3];
        disabled.setAlphaF(0.5f);
        colors.disabled = disabled;
        ui->duplicateSetButton->setIcon(fs::glyphIcon(fs::Glyph::Copy, colors, 14));
        ui->deleteSetButton->setIcon(fs::glyphIcon(fs::Glyph::Trash, colors, 14));
        ui->columnUpButton->setIcon(fs::glyphIcon(fs::Glyph::ChevronUp, colors, 12));
        ui->columnDownButton->setIcon(fs::glyphIcon(fs::Glyph::ChevronDown, colors, 12));
        ui->columnRemoveButton->setIcon(fs::glyphIcon(fs::Glyph::Close, colors, 10));
    }

    std::unique_ptr<Ui::SettingsColumnsPage> ui;
    QStandardItemModel *m_setModel = nullptr;
    QStandardItemModel *m_tableModel = nullptr;
    QStandardItemModel *m_bandModel = nullptr;
    QStringListModel *m_completerModel = nullptr;
    fl::FileListView *m_band = nullptr;
    ChipLabel *m_customTag = nullptr;
    QAbstractButton *m_import = nullptr;
    std::optional<QList<CD>> m_bandShown;
    int m_set = 0;
    int m_column = 0;
    bool m_syncing = false;
};

} // namespace

SettingsPage *createColumnsPage(SettingsSession *session, QWidget *parent)
{
    return new ColumnsPage(session, parent);
}

} // namespace fm::dialogs
