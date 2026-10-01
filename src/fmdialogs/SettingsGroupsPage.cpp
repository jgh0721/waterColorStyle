// 설정 › 파일 그룹 · 색상(docs/specs/05 §2.1) — 골격은 .ui, 그룹 목록(델리게이트 · 끌어 놓기) · 조건 행 · 대비 태그 ·
// 미리보기는 코드. 미리보기는 메인 창과 같은 Qtitan FileListView에 실제 매처(FileGroupMatcher)를 돌린다.

#include "SettingsPages_p.h"

#include "ui_SettingsGroupsPage.h"

#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmstyle/ColorScheme.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QButtonGroup>
#include <QDropEvent>
#include <QIdentityProxyModel>
#include <QPainter>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QToolButton>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;
using GC = fl::GroupCondition;

enum GroupItemRole { SummaryRole = Qt::UserRole + 1, BuiltinRole, StyleRole };

/// 그룹 스타일을 목록 · 견본용으로 해석한다(다크 = '라이트 색에서 자동'이면 파생).
fl::ResolvedGroupStyle swatchStyle(const fl::GroupStyle &s, const QColor &darkSurface)
{
    fl::ResolvedGroupStyle r;
    r.textLight = s.textLight;
    r.backLight = s.backLight;
    if (s.darkFromLight) {
        if (s.textLight)
            r.textDark = fl::darkFromLightColor(*s.textLight, darkSurface);
        if (s.backLight)
            r.backDark = fl::darkFromLightColor(*s.backLight, darkSurface);
    } else {
        r.textDark = s.textDark;
        r.backDark = s.backDark;
    }
    r.bold = s.bold;
    r.italic = s.italic;
    r.underline = s.underline;
    r.strike = s.strike;
    return r;
}

// ------------------------------------------------------------------------------------- 그룹 목록

/// 그룹 항목(05 §2.1.1) — 높이 46, 번호 16 · 손잡이 10 × 14 · "Aa" 견본 30 × 24 · 이름(+ 기본 제공) / 요약.
class GroupItemDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 46);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const bool square = tc.isWatercolor();
        const QRect r = option.rect;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (index.row() > 0)
            p->fillRect(QRect(r.left(), r.top(), r.width(), 1), tc[T::Grid]);
        if (option.state & QStyle::State_Selected) {
            // 선택 = --accent-soft + 안쪽 1 px --accent(파일 목록의 선택 채움과 다름)
            p->fillRect(r, tc[T::AccentSoft]);
            p->setPen(QPen(tc[T::Accent], 1.0));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
        }
        if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_Selected) == 0) {
            p->setPen(QPen(tc[T::Focus], 1.0, Qt::DotLine));
            p->drawRect(QRectF(r).adjusted(1.5, 1.5, -1.5, -1.5));
        }
        qreal x = r.left() + 6;
        const qreal cy = r.center().y() + 0.5;

        // 번호
        p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 11)));
        p->setPen(tc[T::Fg3]);
        p->drawText(QRectF(x, r.top(), 16, r.height()), Qt::AlignRight | Qt::AlignVCenter, QString::number(index.row() + 1));
        x += 16 + 10;

        // 끌기 손잡이 — 점 6개(2 × 3)
        p->setPen(Qt::NoPen);
        p->setBrush(tc[T::Fg3]);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 2; ++col)
                p->drawEllipse(QPointF(x + 2.5 + col * 5, cy - 5 + row * 5), 1.1, 1.1);
        }
        x += 10 + 10;

        // "Aa" 견본
        const auto style = index.data(StyleRole).value<fl::ResolvedGroupStyle>();
        const bool dark = tc.isDark();
        const QRectF swatch(x, cy - 12, 30, 24);
        const qreal radius = square ? 0 : 5;
        p->setBrush(style.background(dark).value_or(Qt::transparent));
        p->setPen(QPen(tc[T::Line], 1.0));
        p->drawRoundedRect(swatch.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        QFont aa = fs::pixelFont(option.font, 13, style.bold ? QFont::Bold : QFont::Medium);
        aa.setItalic(style.italic);
        aa.setUnderline(style.underline);
        aa.setStrikeOut(style.strike);
        p->setFont(aa);
        p->setPen(style.text(dark).value_or(tc[T::Fg]));
        p->drawText(swatch, Qt::AlignCenter, u"Aa"_s);
        x += 30 + 10;

        // 이름 줄(13 px/18) + 기본 제공 배지 · 요약 줄(11.5 px/15)
        const qreal right = r.right() - 10;
        const qreal top = r.top() + (r.height() - 33) / 2.0;
        const QFont nameFont = fs::pixelFont(option.font, 13);
        p->setFont(nameFont);
        p->setPen(tc[T::Fg]);
        const bool builtin = index.data(BuiltinRole).toBool();
        const QString badge = tr("기본 제공");
        const QSize badgeSize = fm::ui::chipSize(badge, 10.5, 5, 16);
        const qreal nameWidth = right - x - (builtin ? badgeSize.width() + 6 : 0);
        const QString name = QFontMetrics(nameFont).elidedText(index.data().toString(), Qt::ElideRight, int(nameWidth));
        p->drawText(QRectF(x, top, nameWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, name);
        if (builtin) {
            const qreal bx = x + QFontMetrics(nameFont).horizontalAdvance(name) + 6;
            fm::ui::paintChip(p, QRectF(bx, top + 1, badgeSize.width(), badgeSize.height()), badge, fm::ui::ChipKind::Mute, tc,
                              square ? 0 : 3, 10.5);
        }
        const QFont summaryFont = fs::pixelFont(option.font, 11.5);
        p->setFont(summaryFont);
        p->setPen(tc[T::Fg3]);
        p->drawText(QRectF(x, top + 18, right - x, 15), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(summaryFont).elidedText(index.data(SummaryRole).toString(), Qt::ElideRight, int(right - x)));
        p->restore();
    }
};

// ------------------------------------------------------------------------------------- 조건 행

/// 조건 한 행(05 §2.1.2) — [항목 118] [비교 방법 124] [값(mono 12.5)] [× 28]. 값이 해석되지 않으면 그 칸만 경고 모양.
class ConditionRow final : public QWidget
{
public:
    std::function<void(const GC &)> changed;
    std::function<void()> removeRequested;

    explicit ConditionRow(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        m_field = new QComboBox(this);
        m_field->setFixedWidth(118);
        m_field->setAccessibleName(tr("조건 항목"));
        for (GC::Field f : {GC::Field::Extension, GC::Field::Name, GC::Field::Attributes, GC::Field::MimeType, GC::Field::Size,
                            GC::Field::Modified, GC::Field::Created})
            m_field->addItem(fl::conditionFieldLabel(f), int(f));
        m_op = new QComboBox(this);
        m_op->setFixedWidth(124);
        m_op->setAccessibleName(tr("비교 방법"));
        m_value = new QLineEdit(this);
        m_value->setFont(fs::monoFont(12.5));
        m_value->setAccessibleName(tr("조건 값"));
        m_remove = new QToolButton(this);
        m_remove->setAutoRaise(true);
        m_remove->setFixedSize(28, 28);
        m_remove->setToolTip(tr("조건 삭제"));
        m_remove->setAccessibleName(tr("조건 삭제"));
        layout->addWidget(m_field);
        layout->addWidget(m_op);
        layout->addWidget(m_value, 1);
        layout->addWidget(m_remove);
        refreshIcon();
        connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] { refreshIcon(); });

        connect(m_field, &QComboBox::activated, this, [this] {
            fillOps(GC::Field(m_field->currentData().toInt()));
            emitChanged();
        });
        connect(m_op, &QComboBox::activated, this, [this] { emitChanged(); });
        connect(m_value, &QLineEdit::textEdited, this, [this] { emitChanged(); });
        connect(m_remove, &QToolButton::clicked, this, [this] {
            if (removeRequested)
                removeRequested();
        });
    }

    void setCondition(const GC &c)
    {
        const QSignalBlocker b1(m_field);
        const QSignalBlocker b2(m_op);
        m_field->setCurrentIndex(std::max(0, m_field->findData(int(c.field))));
        fillOps(c.field);
        m_op->setCurrentIndex(std::max(0, m_op->findData(int(c.op))));
        if (m_value->text() != c.value)
            m_value->setText(c.value);
        validate(c);
    }

    GC condition() const
    {
        GC c;
        c.field = GC::Field(m_field->currentData().toInt());
        c.op = GC::Op(m_op->currentData().toInt());
        c.value = m_value->text();
        return c;
    }

    QLineEdit *valueEdit() const { return m_value; }

private:
    void fillOps(GC::Field field)
    {
        const QSignalBlocker block(m_op);
        m_op->clear();
        for (GC::Op op : fl::conditionOps(field))
            m_op->addItem(fl::conditionOpLabel(op), int(op));
    }
    void emitChanged()
    {
        const GC c = condition();
        validate(c);
        if (changed)
            changed(c);
    }
    void validate(const GC &c)
    {
        QString error;
        const bool ok = c.value.trimmed().isEmpty() || fl::validateCondition(c, &error);
        fs::setInvalid(m_value, !ok);
        m_value->setToolTip(ok ? QString() : error);
    }
    void refreshIcon()
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        fs::GlyphStateColors colors;
        colors.normal = tc[T::Fg2];
        QColor disabled = tc[T::Fg3];
        disabled.setAlphaF(0.5f);
        colors.disabled = disabled;
        m_remove->setIcon(fs::glyphIcon(fs::Glyph::Close, colors, 10));
    }

    QComboBox *m_field = nullptr;
    QComboBox *m_op = nullptr;
    QLineEdit *m_value = nullptr;
    QToolButton *m_remove = nullptr;
};

// ------------------------------------------------------------------------------------- 대비 태그 · 배지

/// 태그 줄(05 §2.1.3) — 높이 18, 모서리 9, 좌우 6, 10.5 px 600, 간격 4.
class ContrastTags final : public QWidget
{
public:
    struct Tag
    {
        QString text;
        fm::ui::ChipKind kind;
    };
    using QWidget::QWidget;

    void setTags(const QList<Tag> &tags)
    {
        m_tags = tags;
        QStringList texts;
        for (const Tag &t : tags)
            texts.append(t.text);
        setAccessibleDescription(texts.join(u", "_s));
        updateGeometry();
        update();
    }
    QSize sizeHint() const override
    {
        int w = 0;
        for (const Tag &t : m_tags)
            w += fm::ui::chipSize(t.text, 10.5, 6, 18).width() + 4;
        return QSize(std::max(0, w - 4), 18);
    }
    QSize minimumSizeHint() const override { return QSize(0, 18); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        qreal x = 0;
        for (const Tag &t : std::as_const(m_tags)) {
            const QSize s = fm::ui::chipSize(t.text, 10.5, 6, 18);
            fm::ui::paintChip(&p, QRectF(x, 0, s.width(), s.height()), t.text, t.kind, tc, tc.isWatercolor() ? 0 : 9, 10.5);
            x += s.width() + 4;
        }
    }

private:
    QList<Tag> m_tags;
};

/// 한 개짜리 칩(구역 제목 옆 "기본 제공 · 조건은 바꿀 수 없음").
class ChipLabel final : public QWidget
{
public:
    ChipLabel(const QString &text, QWidget *parent)
        : QWidget(parent)
        , m_text(text)
    {
        setAccessibleName(text);
    }
    QSize sizeHint() const override { return fm::ui::chipSize(m_text, 10.5, 5, 16); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        const QSize s = sizeHint();
        fm::ui::paintChip(&p, QRectF(0, (height() - s.height()) / 2.0, s.width(), s.height()), m_text, fm::ui::ChipKind::Mute, tc,
                          tc.isWatercolor() ? 0 : 3, 10.5);
    }

private:
    QString m_text;
};

// ------------------------------------------------------------------------------------- 미리보기 모델

/// 미리보기 — "적용된 그룹" 칸(TypeColumn 자리)을 해석된 그룹 이름으로, 행 상태는 목업대로(선택 행 하나, 커서 없음).
class AppliedGroupsProxy final : public QIdentityProxyModel
{
public:
    using QIdentityProxyModel::QIdentityProxyModel;
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == fl::PreviewStateRole)
            return index.data(fl::MarkedRole).toBool() ? int(fl::PreviewMarked) : 0;
        if (index.column() == fl::TypeColumn && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
            const auto style = index.siblingAtColumn(fl::NameColumn).data(fl::GroupStyleRole).value<fl::ResolvedGroupStyle>();
            return style.groups.isEmpty() ? u"—"_s : style.groups.join(u" + "_s);
        }
        return QIdentityProxyModel::data(index, role);
    }
};

/// 샘플 파일 6개(05 §2.1.4) — 실제 매처에 필요한 메타데이터(크기 · 속성 · 날짜)를 가진다.
QList<fl::FileEntry> previewEntries()
{
    const QDateTime now = QDateTime::currentDateTime();
    auto entry = [&](const QString &stem, const QString &ext, fl::Kind kind, qint64 size, const QDateTime &modified, int attributes,
                     bool marked = false) {
        fl::FileEntry e;
        e.stem = stem;
        e.ext = ext;
        e.kind = kind;
        e.size = size;
        e.modified = modified;
        e.attributes = attributes | fl::Archive;
        e.marked = marked;
        e.path = u"D:\\Work\\"_s + e.fullName();
        return e;
    };
    constexpr qint64 KB = 1024, MB = KB * 1024, GB = MB * 1024;
    return {
        entry(u"BandedPanelView"_s, u"cpp"_s, fl::Kind::Code, qint64(24.1 * KB), now.addSecs(-3 * 3600), 0),
        entry(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols"_s, u"exe"_s, fl::Kind::Exe, qint64(3.74 * GB),
              now.addDays(-4), 0),
        entry(u"placeholder-notes"_s, u"txt"_s, fl::Kind::Doc, 0, now.addDays(-6), 0),
        entry(u"Qt Creator 18"_s, u"lnk"_s, fl::Kind::Sys, qint64(1.4 * KB), now.addDays(-12), 0),
        entry(u"font-pack_Cascadia-Code-NF_2026-07"_s, u"7z"_s, fl::Kind::Zip, 88 * MB, now.addDays(-11), 0, true),
        entry(u"desktop"_s, u"ini"_s, fl::Kind::Sys, 282, now.addDays(-29), fl::Hidden | fl::System),
    };
}

/// 미리보기 열 — `1fr | 72 | 150`: 이름 · 크기 · 적용된 그룹.
fl::ListColumnLayout previewLayout()
{
    using R = fl::ListColumn::Role;
    using L = fl::ListColumn::TwoLine;
    fl::ListColumnLayout layout;
    layout.columns = {
        {fl::IconColumn, R::Icon, QString(), 16, 36, Qt::AlignCenter, false, L::Row0Full},
        {fl::NameColumn, R::Name, QString(), 0, 0, Qt::AlignLeft, true, L::Row0Full},
        {fl::SizeColumn, R::Meta, QString(), 72, -1, Qt::AlignRight, true, L::Row1, false, true},
        {fl::TypeColumn, R::Meta, QCoreApplication::translate("SettingsGroups", "적용된 그룹"), 150, -1, Qt::AlignLeft, true, L::Row1},
    };
    return layout;
}

// ------------------------------------------------------------------------------------- 페이지

class GroupsPage final : public SettingsPage
{
public:
    GroupsPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsGroupsPage>())
    {
        ui->setupUi(this);
        ui->matchSegment->setSegmentSize(fm::ui::SegmentedControl::Small);
        ui->newGroupButton->setGlyph(fm::ui::glyph::PlusSmall);
        ui->addConditionButton->setGlyph(fm::ui::glyph::PlusSmall);
        ui->groupsToolbarLine->setFixedHeight(1);
        for (QToolButton *b : {ui->duplicateButton, ui->deleteButton, ui->upButton, ui->downButton})
            b->setFixedSize(28, 28);
        m_badge = new ChipLabel(tr("기본 제공 · 조건은 바꿀 수 없음"), this);
        ui->definitionHeader->insertWidget(1, m_badge);
        for (auto [layout, name] : {std::pair{ui->textLightLayout, "textLightTags"}, std::pair{ui->textDarkLayout, "textDarkTags"},
                                    std::pair{ui->backLightLayout, "backLightTags"}, std::pair{ui->backDarkLayout, "backDarkTags"}}) {
            auto *tags = new ContrastTags(ui->colorGrid);
            tags->setObjectName(QString::fromLatin1(name));
            layout->addWidget(tags);
            m_tags.append(tags);
        }
        // 추천 색 — 목업 그룹 색(라이트 · 다크)과 연한 배경
        ui->textLightButton->setSuggestions({QColor(0x1D5BC7), QColor(0xA8321F), QColor(0x0F7A6E), QColor(0x6D3FC0), QColor(0x9A5B00),
                                             QColor(0xA3246B), QColor(0x6B717C)});
        ui->textDarkButton->setSuggestions({QColor(0x82AEF6), QColor(0xF08A7A), QColor(0x4FD1BF), QColor(0xB394F0), QColor(0xF0B24D),
                                            QColor(0xF07DB8), QColor(0x8A9099)});
        ui->backLightButton->setSuggestions({QColor(0xFFF1C9), QColor(0xE3EEFD), QColor(0xE3F4EC), QColor(0xFDECEA), QColor(0xF1E9FD)});
        ui->backDarkButton->setSuggestions({QColor(0x3A2F10), QColor(0x14283F), QColor(0x14301F), QColor(0x3A1A16), QColor(0x2A1D3F)});

        buildList();
        bindEditor();
        buildPreview();
        refreshIcons();
        connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] {
            refreshIcons();
            syncFromPending();
        });
    }

    QString pageId() const override { return u"groups"_s; }
    QString title() const override { return tr("파일 그룹 · 색상"); }
    QString description() const override
    {
        return tr("파일을 조건으로 묶고 그룹마다 글자색 · 배경색 · 글꼴 효과를 정합니다. 비워 둔 항목은 목록 기본값을 따릅니다.");
    }
    fm::settings::Sections sections() const override { return Section::Groups; }
    QList<QAbstractButton *> footerButtons() override
    {
        if (!m_import)
            m_import = makeImportExportButton();
        return {m_import};
    }
    QStringList searchKeywords() const override
    {
        QStringList words = SettingsPage::searchKeywords();
        words << tr("그룹") << tr("그룹 정의") << tr("색상과 글꼴 효과") << tr("글자색") << tr("배경색") << tr("굵게") << tr("기울임")
              << tr("밑줄") << tr("취소선") << tr("여러 그룹에 해당하면");
        for (const fl::FileGroup &g : pending().groups.groups)
            words.append(g.name);
        return words;
    }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        if (!ui)
            return;
        const fl::FileGroupSettings &gs = pending().groups;
        m_current = std::clamp(m_current, 0, int(gs.groups.size()) - 1);
        syncList();
        {
            const QSignalBlocker b1(ui->firstOnlyRadio);
            const QSignalBlocker b2(ui->mergeRadio);
            ui->firstOnlyRadio->setChecked(gs.merge == fl::FileGroupSettings::Merge::FirstOnly);
            ui->mergeRadio->setChecked(gs.merge == fl::FileGroupSettings::Merge::PerProperty);
        }
        syncEditor();
        syncPreview();
    }

    /// 목업 보드: 6번 "소스 코드" 선택.
    void showBoardState() override
    {
        m_current = 5;
        syncFromPending();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // 끌어 놓기로 순서 바꾸기 — 모델은 설정에서 다시 만들므로 뷰의 행 이동은 막고 설정만 고친다
        if (watched == ui->groupList->viewport() && event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            const int from = ui->groupList->currentIndex().row();
            const QModelIndex target = ui->groupList->indexAt(drop->position().toPoint());
            int to = target.isValid() ? target.row() : m_listModel->rowCount() - 1;
            drop->setDropAction(Qt::CopyAction);
            drop->accept();
            if (from >= 0 && to >= 0 && from != to)
                moveGroup(from, to);
            return true;
        }
        return SettingsPage::eventFilter(watched, event);
    }

private:
    const fl::FileGroup *currentGroup() const
    {
        const auto &groups = pending().groups.groups;
        return m_current >= 0 && m_current < groups.size() ? &groups.at(m_current) : nullptr;
    }

    /// 지금 그룹을 고친다(기본 제공 그룹의 이름 · 조건은 잠김 — 호출하는 쪽이 막는다).
    void editCurrent(const std::function<void(fl::FileGroup &)> &mutate)
    {
        const int i = m_current;
        session()->edit(Section::Groups, [&](AppSettings &p) {
            if (i >= 0 && i < p.groups.groups.size())
                mutate(p.groups.groups[i]);
        });
    }

    fs::ThemeColors colorsFor(fs::Variant variant) const
    {
        const AppSettings &p = pending();
        const fs::Design design = p.appearance.design;
        return fs::deriveColors(variant, p.theme.scheme.seeds, p.theme.scheme.overridesFor(design, variant), design);
    }

    // ---------------------------------------------------------------- 목록

    void buildList()
    {
        m_listModel = new QStandardItemModel(this);
        ui->groupList->setModel(m_listModel);
        ui->groupList->setItemDelegate(new GroupItemDelegate(ui->groupList));
        ui->groupList->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->groupList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->groupList->setDragDropMode(QAbstractItemView::InternalMove);
        ui->groupList->setDefaultDropAction(Qt::MoveAction);
        ui->groupList->setUniformItemSizes(true);
        ui->groupList->viewport()->installEventFilter(this);
        ui->groupList->setMinimumHeight(46 * 6);
        connect(ui->groupList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
            if (m_syncing || !index.isValid())
                return;
            m_current = index.row();
            syncEditor();
        });

        connect(ui->newGroupButton, &QPushButton::clicked, this, [this] { addGroup(false); });
        connect(ui->duplicateButton, &QToolButton::clicked, this, [this] { addGroup(true); });
        connect(ui->deleteButton, &QToolButton::clicked, this, [this] { deleteGroup(); });
        connect(ui->upButton, &QToolButton::clicked, this, [this] { moveGroup(m_current, m_current - 1); });
        connect(ui->downButton, &QToolButton::clicked, this, [this] { moveGroup(m_current, m_current + 1); });
        // 목록 카드 범위에서만 — 이름 · 값 입력의 Del을 가로채지 않게
        auto shortcut = [this](const QKeySequence &key, auto slot) {
            auto *s = new QShortcut(key, ui->groupsCard, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
            connect(s, &QShortcut::activated, this, slot);
        };
        shortcut(QKeySequence(Qt::CTRL | Qt::Key_D), [this] { addGroup(true); });
        shortcut(QKeySequence(Qt::Key_Delete), [this] { deleteGroup(); });
        shortcut(QKeySequence(Qt::ALT | Qt::Key_Up), [this] { moveGroup(m_current, m_current - 1); });
        shortcut(QKeySequence(Qt::ALT | Qt::Key_Down), [this] { moveGroup(m_current, m_current + 1); });

        auto *merge = new QButtonGroup(this);
        merge->addButton(ui->firstOnlyRadio, int(fl::FileGroupSettings::Merge::FirstOnly));
        merge->addButton(ui->mergeRadio, int(fl::FileGroupSettings::Merge::PerProperty));
        connect(merge, &QButtonGroup::idClicked, this, [this](int id) {
            session()->edit(Section::Groups, [&](AppSettings &p) { p.groups.merge = fl::FileGroupSettings::Merge(id); });
        });
        addCustomItem(ui->groupsCard, [](const AppSettings &a, const AppSettings &b) { return a.groups.groups != b.groups.groups; },
                      [](AppSettings &p, const AppSettings &d) { p.groups.groups = d.groups.groups; }, Section::Groups);
        addCustomItem(ui->mergeBox, [](const AppSettings &a, const AppSettings &b) { return a.groups.merge != b.groups.merge; },
                      [](AppSettings &p, const AppSettings &d) { p.groups.merge = d.groups.merge; }, Section::Groups);
    }

    void syncList()
    {
        const auto &groups = pending().groups.groups;
        const QColor darkSurface = colorsFor(fs::Variant::Dark)[T::Surface];
        m_syncing = true;
        if (m_listModel->rowCount() != groups.size()) {
            m_listModel->clear();
            for (int i = 0; i < groups.size(); ++i)
                m_listModel->appendRow(new QStandardItem());
        }
        for (int i = 0; i < groups.size(); ++i) {
            const fl::FileGroup &g = groups.at(i);
            QStandardItem *item = m_listModel->item(i);
            item->setText(g.name);
            item->setData(fl::groupSummary(g), SummaryRole);
            item->setData(g.builtin, BuiltinRole);
            item->setData(QVariant::fromValue(swatchStyle(g.style, darkSurface)), StyleRole);
            item->setData(u"%1. %2 — %3"_s.arg(i + 1).arg(g.name, fl::groupSummary(g)), Qt::AccessibleTextRole);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
        if (m_current >= 0 && m_current < groups.size()) {
            const QModelIndex index = m_listModel->index(m_current, 0);
            ui->groupList->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        }
        m_syncing = false;
        const fl::FileGroup *g = currentGroup();
        ui->deleteButton->setEnabled(g && !g->builtin);
        ui->upButton->setEnabled(m_current > 0);
        ui->downButton->setEnabled(m_current >= 0 && m_current < groups.size() - 1);
        ui->duplicateButton->setEnabled(g != nullptr);
    }

    void addGroup(bool duplicate)
    {
        const fl::FileGroup *source = currentGroup();
        if (duplicate && !source)
            return;
        fl::FileGroup group;
        if (duplicate) {
            group = *source;
            group.builtin = false;
            group.name = tr("%1 복사본").arg(source->name);
        } else {
            group.name = tr("새 그룹");
            group.conditions = {GC{}};
        }
        const auto &groups = pending().groups.groups;
        int n = 1;
        auto taken = [&](const QString &id) {
            return std::any_of(groups.begin(), groups.end(), [&](const fl::FileGroup &g) { return g.id == id; });
        };
        while (taken(u"group-%1"_s.arg(n)))
            ++n;
        group.id = u"group-%1"_s.arg(n);
        const int at = std::clamp(m_current + 1, 0, int(groups.size()));
        m_current = at;
        session()->edit(Section::Groups, [&](AppSettings &p) { p.groups.groups.insert(at, group); });
        ui->nameEdit->setFocus();
        ui->nameEdit->selectAll();
    }

    void deleteGroup()
    {
        const fl::FileGroup *g = currentGroup();
        if (!g || g->builtin)
            return;
        const int at = m_current;
        session()->edit(Section::Groups, [&](AppSettings &p) { p.groups.groups.removeAt(at); });
        ui->groupList->setFocus();
    }

    void moveGroup(int from, int to)
    {
        const int count = int(pending().groups.groups.size());
        if (from < 0 || from >= count || to < 0 || to >= count || from == to)
            return;
        m_current = to;
        session()->edit(Section::Groups, [&](AppSettings &p) { p.groups.groups.move(from, to); });
    }

    // ---------------------------------------------------------------- 편집

    void bindEditor()
    {
        connect(ui->nameEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
            editCurrent([&](fl::FileGroup &g) { g.name = text; });
        });
        connect(ui->matchSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            editCurrent([&](fl::FileGroup &g) { g.matchAll = i == 1; });
        });
        connect(ui->addConditionButton, &QPushButton::clicked, this, [this] {
            editCurrent([](fl::FileGroup &g) { g.conditions.append(GC{}); });
            if (!m_rows.isEmpty())
                m_rows.last()->valueEdit()->setFocus();
        });
        auto bindColor = [this](fm::ui::ColorPickButton *button, std::optional<QColor> fl::GroupStyle::*field) {
            connect(button, &fm::ui::ColorPickButton::colorChanged, this, [this, field](const std::optional<QColor> &color) {
                editCurrent([&](fl::FileGroup &g) { g.style.*field = color; });
            });
        };
        bindColor(ui->textLightButton, &fl::GroupStyle::textLight);
        bindColor(ui->textDarkButton, &fl::GroupStyle::textDark);
        bindColor(ui->backLightButton, &fl::GroupStyle::backLight);
        bindColor(ui->backDarkButton, &fl::GroupStyle::backDark);
        connect(ui->autoDarkCheck, &QCheckBox::toggled, this, [this](bool on) {
            editCurrent([&](fl::FileGroup &g) { g.style.darkFromLight = on; });
        });
        auto bindEffect = [this](fm::ui::ToggleChip *chip, bool fl::GroupStyle::*field) {
            chip->setCheckable(true);
            connect(chip, &QPushButton::toggled, this, [this, field](bool on) {
                editCurrent([&](fl::FileGroup &g) { g.style.*field = on; });
            });
        };
        bindEffect(ui->boldChip, &fl::GroupStyle::bold);
        bindEffect(ui->italicChip, &fl::GroupStyle::italic);
        bindEffect(ui->underlineChip, &fl::GroupStyle::underline);
        bindEffect(ui->strikeChip, &fl::GroupStyle::strike);
    }

    void syncEditor()
    {
        const fl::FileGroup *g = currentGroup();
        const bool locked = !g || g->builtin;
        m_badge->setVisible(g && g->builtin);
        ui->definitionCard->setEnabled(g != nullptr);
        ui->colorCard->setEnabled(g != nullptr);
        if (!g)
            return;
        if (ui->nameEdit->text() != g->name) {
            const QSignalBlocker block(ui->nameEdit);
            ui->nameEdit->setText(g->name);
        }
        ui->nameEdit->setEnabled(!locked);
        {
            const QSignalBlocker block(ui->matchSegment);
            ui->matchSegment->setCurrentIndex(g->matchAll ? 1 : 0);
        }
        ui->matchSegment->setEnabled(!locked);
        ui->addConditionButton->setEnabled(!locked);

        // 조건 행 — 개수가 같으면 그 자리에서 값만 바꾼다(입력 중인 칸의 포커스 유지)
        if (m_rows.size() != g->conditions.size() || m_rowsGroup != g->id) {
            qDeleteAll(m_rows);
            m_rows.clear();
            for (int i = 0; i < g->conditions.size(); ++i) {
                auto *row = new ConditionRow(ui->conditionBox);
                row->changed = [this, i](const GC &c) {
                    editCurrent([&](fl::FileGroup &group) {
                        if (i < group.conditions.size())
                            group.conditions[i] = c;
                    });
                };
                row->removeRequested = [this, i] {
                    editCurrent([&](fl::FileGroup &group) {
                        if (i < group.conditions.size())
                            group.conditions.removeAt(i);
                    });
                };
                ui->conditionRows->addWidget(row);
                m_rows.append(row);
            }
            m_rowsGroup = g->id;
        }
        for (int i = 0; i < m_rows.size(); ++i) {
            m_rows[i]->setCondition(g->conditions.at(i));
            m_rows[i]->setEnabled(!locked);
        }

        // 색 · 효과
        const fl::GroupStyle &s = g->style;
        const QColor darkSurface = colorsFor(fs::Variant::Dark)[T::Surface];
        const fl::ResolvedGroupStyle resolved = swatchStyle(s, darkSurface);
        for (auto [button, color] : {std::pair{ui->textLightButton, s.textLight}, std::pair{ui->backLightButton, s.backLight},
                                     std::pair{ui->textDarkButton, resolved.textDark}, std::pair{ui->backDarkButton, resolved.backDark}}) {
            const QSignalBlocker block(button);
            button->setColor(color);
        }
        ui->textDarkButton->setEnabled(!s.darkFromLight);
        ui->backDarkButton->setEnabled(!s.darkFromLight);
        {
            const QSignalBlocker block(ui->autoDarkCheck);
            ui->autoDarkCheck->setChecked(s.darkFromLight);
        }
        for (auto [chip, on] : {std::pair{ui->boldChip, s.bold}, std::pair{ui->italicChip, s.italic},
                                std::pair{ui->underlineChip, s.underline}, std::pair{ui->strikeChip, s.strike}}) {
            const QSignalBlocker block(chip);
            chip->setChecked(on);
        }
        refreshTags(resolved);
    }

    /// 대비 태그 — 판정은 반올림 전 값(4.5 미만이면 경고, 표시가 4.5:1이어도).
    void refreshTags(const fl::ResolvedGroupStyle &s)
    {
        using Tag = ContrastTags::Tag;
        using K = fm::ui::ChipKind;
        auto ratio = [](const QColor &a, const QColor &b) {
            const double r = fs::contrastRatio(a, b);
            return std::pair{QString::number(r, 'f', 1), r >= 4.5 ? K::Ok : K::Warn};
        };
        for (const bool dark : {false, true}) {
            const fs::ThemeColors tc = colorsFor(dark ? fs::Variant::Dark : fs::Variant::Light);
            QList<Tag> text, back;
            const std::optional<QColor> fg = s.text(dark);
            const std::optional<QColor> bg = s.background(dark);
            if (fg) {
                const auto [r1, k1] = ratio(*fg, bg.value_or(tc[T::Surface]));
                const QString bgName = bg ? tr("자체 배경") : dark ? tr("어두운 배경") : tr("흰 배경");
                text.append({u"%1 %2:1"_s.arg(bgName, r1), k1});
                const auto [r2, k2] = ratio(*fg, tc[T::Sel]);
                text.append({tr("선택 행 %1:1").arg(r2), k2});
            } else {
                text.append({tr("목록 기본 글자색"), K::Mute});
            }
            if (bg) {
                const auto [r, k] = ratio(fg.value_or(tc[T::Fg]), *bg);
                back.append({tr("글자 대비 %1:1").arg(r), k});
            } else {
                back.append({tr("목록 배경 그대로"), K::Mute});
            }
            m_tags[dark ? 1 : 0]->setTags(text);
            m_tags[dark ? 3 : 2]->setTags(back);
        }
    }

    void refreshIcons()
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        fs::GlyphStateColors colors;
        colors.normal = tc[T::Fg2];
        QColor disabled = tc[T::Fg3];
        disabled.setAlphaF(0.5f);
        colors.disabled = disabled;
        ui->duplicateButton->setIcon(fs::glyphIcon(fs::Glyph::Copy, colors, 14));
        ui->deleteButton->setIcon(fs::glyphIcon(fs::Glyph::Trash, colors, 14));
        ui->upButton->setIcon(fs::glyphIcon(fs::Glyph::ChevronUp, colors, 12));
        ui->downButton->setIcon(fs::glyphIcon(fs::Glyph::ChevronDown, colors, 12));
    }

    // ---------------------------------------------------------------- 미리보기

    void buildPreview()
    {
        m_previewModel = new fl::FileListModel(previewEntries(), this);
        m_previewProxy = new fl::FileSortProxy(this);
        m_previewProxy->setSourceModel(m_previewModel);
        m_previewProxy->sort(-1);
        m_applied = new AppliedGroupsProxy(this);
        m_applied->setSourceModel(m_previewProxy);
        m_preview = new fl::FileListView(ui->previewCard);
        m_preview->setObjectName(u"groupsPreview"_s);
        m_preview->setPreviewMode(true);
        m_preview->setColumnLayout(previewLayout());
        m_preview->setModel(m_applied);
        m_preview->setViewMode(fl::ViewMode::OneLine);
        m_preview->setPaneActive(true);
        m_preview->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_preview->setFixedHeight(m_preview->preferredHeight(m_applied->rowCount()));
        ui->previewLayout->addWidget(m_preview);
    }

    void syncPreview()
    {
        if (!m_preview)
            return;
        const fl::FileGroupSettings &gs = pending().groups;
        if (!m_shownGroups || *m_shownGroups != gs) {
            m_shownGroups = gs;
            m_previewProxy->setGroupMatcher(std::make_shared<const fl::FileGroupMatcher>(gs));
        }
        fl::ListAppearance a;
        a.oneLineSeparator = fl::RecordSeparator::None;
        a.boldSelection = false;
        m_preview->setAppearance(a);
        // 보조 글 — 두 그룹 이상이 함께 적용된 행 번호
        QString note;
        if (gs.merge == fl::FileGroupSettings::Merge::FirstOnly) {
            note = tr("· 위 그룹 하나만 — 처음 맞는 그룹만 적용");
        } else {
            QStringList rows;
            for (int r = 0; r < m_applied->rowCount(); ++r) {
                const auto style = m_applied->index(r, fl::NameColumn).data(fl::GroupStyleRole).value<fl::ResolvedGroupStyle>();
                if (style.groups.size() >= 2)
                    rows.append(QString::number(r + 1));
            }
            note = rows.isEmpty() ? tr("· 속성별로 합치기") : tr("· 속성별로 합치기 — %1행은 두 그룹이 함께 적용").arg(rows.join(u" · "_s));
        }
        ui->previewNote->setText(note);
    }

    std::unique_ptr<Ui::SettingsGroupsPage> ui;
    QStandardItemModel *m_listModel = nullptr;
    ChipLabel *m_badge = nullptr;
    QList<ContrastTags *> m_tags;  // 글자 L · 글자 D · 배경 L · 배경 D
    QList<ConditionRow *> m_rows;
    QString m_rowsGroup;
    QAbstractButton *m_import = nullptr;
    int m_current = 0;
    bool m_syncing = false;
    fl::FileListModel *m_previewModel = nullptr;
    fl::FileSortProxy *m_previewProxy = nullptr;
    AppliedGroupsProxy *m_applied = nullptr;
    fl::FileListView *m_preview = nullptr;
    std::optional<fl::FileGroupSettings> m_shownGroups;
};

} // namespace

SettingsPage *createGroupsPage(SettingsSession *session, QWidget *parent)
{
    return new GroupsPage(session, parent);
}

} // namespace fm::dialogs
